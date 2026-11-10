#include "autokeyloop.hpp"
#include <cstring>
#include "common.hpp"
#include "remapper.hpp"

namespace {
    // 摇杆伪按键位掩码 (BIT16-23)，必须过滤
    constexpr u64 STICK_PSEUDO_BUTTON_MASK = 0xFF0000ULL;
    
    // 左 JoyCon 按键掩码（十字键、左肩键、左摇杆、SELECT）
    constexpr u64 LEFT_JOYCON_BUTTONS = 
        HidNpadButton_Left | HidNpadButton_Right | HidNpadButton_Up | HidNpadButton_Down |
        HidNpadButton_L | HidNpadButton_ZL | HidNpadButton_StickL |
        HidNpadButton_Minus;
    
    // 右 JoyCon 按键掩码（面键、右肩键、右摇杆、START）
    constexpr u64 RIGHT_JOYCON_BUTTONS = 
        HidNpadButton_A | HidNpadButton_B | HidNpadButton_X | HidNpadButton_Y |
        HidNpadButton_R | HidNpadButton_ZR | HidNpadButton_StickR |
        HidNpadButton_Plus;

    // 判断是否为左 JoyCon
    constexpr bool IsLeftController(HidDeviceType type) {
        return type == HidDeviceType_JoyLeft2 || 
               type == HidDeviceType_JoyLeft4 ||
               type == HidDeviceType_LarkHvcLeft ||
               type == HidDeviceType_LarkNesLeft;
    }
    
    // 判断是否为右 JoyCon
    constexpr bool IsRightController(HidDeviceType type) {
        return type == HidDeviceType_JoyRight1 || 
               type == HidDeviceType_JoyRight5 ||
               type == HidDeviceType_LarkHvcRight ||
               type == HidDeviceType_LarkNesRight;
    }

    // 检测是否为物理连接的JoyCon（通过导轨连接）
    bool isPhysicalJoyCon() {
        u8 interfaceType;
        if (R_SUCCEEDED(hidGetNpadInterfaceType(HidNpadIdType_Handheld, &interfaceType))) {
            return interfaceType == HidNpadInterfaceType_Rail;
        }
        return false;
    }

    // 更新间隔
    constexpr u64 UPDATE_INTERVAL_NS = 1000000ULL;  // 1ms
    
    // 读取手柄状态并应用到结果
    #define READ_NPAD_STATE(StateType, GetFunc, NpadId) \
        do { \
            StateType state; \
            size_t count = GetFunc(NpadId, &state, 1); \
            if (count > 0 && (state.attributes & HidNpadAttribute_IsConnected)) { \
                result.buttons = state.buttons & ~STICK_PSEUDO_BUTTON_MASK; \
                result.analog_stick_l = state.analog_stick_l; \
                result.analog_stick_r = state.analog_stick_r; \
            } \
        } while(0)
}

// 静态成员定义
alignas(0x1000) char AutoKeyLoop::thread_stack[4 * 1024];
alignas(0x1000) u8 AutoKeyLoop::hdls_work_buffer[0x1000];

// 构造函数
AutoKeyLoop::AutoKeyLoop(const char* config_path, const char* macroCfgPath, bool enable_turbo, bool enable_macro, bool enable_touch) {
    // 初始化HDLS工作缓冲区
    Result rc = hiddbgAttachHdlsWorkBuffer(&m_HdlsSessionId, hdls_work_buffer, sizeof(hdls_work_buffer));
    if (R_FAILED(rc)) return;
    
    memset(&m_StateList, 0, sizeof(m_StateList));
    m_HdlsInitialized = true;
    
    // 初始化状态
    m_ShouldExit = false;
    m_IsPaused = false;
    m_PauseStateRestored = false;
    
    // 初始化功能开关
    m_EnableTurbo = enable_turbo;
    m_EnableMacro = enable_macro;
    m_EnableTouch = enable_touch;
    m_TouchWasActive = false;
    // 根据开关创建功能模块
    if (m_EnableTurbo) {
        m_Turbo = std::make_unique<Turbo>(config_path);
        m_isJCRightHand = m_Turbo->IsJCRightHand();
    }
    if (m_EnableMacro) m_Macro = std::make_unique<Macro>(macroCfgPath);
    if (m_EnableTouch) m_TouchMapper = std::make_unique<TouchMapper>(config_path);
    
    // 初始化手柄类型
    m_ControllerType = ControllerType::C_NONE;
    
    // 读取实际生效的按键映射
    UpdateButtonMappings();
    
    // 初始化线程状态
    m_ThreadCreated = false;
    m_ThreadRunning = false;
    memset(&m_Thread, 0, sizeof(Thread));
    
    // 创建线程
    rc = threadCreate(&m_Thread, ThreadFunc, this, thread_stack, sizeof(thread_stack), 44, -2);
    if (R_FAILED(rc)) return;
    m_ThreadCreated = true;
    
    rc = threadStart(&m_Thread);
    if (R_FAILED(rc)) return;
    m_ThreadRunning = true;
}

// 析构函数
AutoKeyLoop::~AutoKeyLoop() {
    // 停止线程
    m_ShouldExit = true;
    if (m_ThreadRunning) threadWaitForExit(&m_Thread);
    if (m_ThreadCreated) threadClose(&m_Thread);
    // 释放HDLS
    if (m_HdlsInitialized) hiddbgReleaseHdlsWorkBuffer(m_HdlsSessionId);
}

// 线程函数
void AutoKeyLoop::ThreadFunc(void* arg) {
    AutoKeyLoop* loop = static_cast<AutoKeyLoop*>(arg);
    loop->MainLoop();
}

// 主循环
void AutoKeyLoop::MainLoop() {
    while (!m_ShouldExit) {
        ProcessResult result{};
        ReadPhysicalInput(result);
        DetermineEvent(result);
        if (result.event != FeatureEvent::PAUSED) m_PauseStateRestored = false;
        switch (result.event) {
            case FeatureEvent::PAUSED:
                if (!m_PauseStateRestored) {
                    // 清除最后一次注入状态并恢复当前物理输入，避免切换连发残留
                    result.OtherButtons = result.buttons;
                    InjectAll(result);
                    m_PauseStateRestored = true;
                }
                // 每100ms重新检查一次，避免恢复焦点后仍固定等待近1秒
                svcSleepThread(100000000ULL);
                continue;
            case FeatureEvent::IDLE:
                break;
            case FeatureEvent::STARTING:
                UpdateButtonMappings();
                hiddbgDumpHdlsStates(m_HdlsSessionId, &m_StateList);
                break;
            case FeatureEvent::Turbo_EXECUTING:
                ApplyHdlsState(result);
                break;
            case FeatureEvent::Macro_EXECUTING:
                ApplyHdlsState(result);
                break;
            case FeatureEvent::Touch_EXECUTING:
                ApplyHdlsState(result);
                break;
            case FeatureEvent::INPUT_EXECUTING:
                ApplyHdlsState(result);
                break;
            case FeatureEvent::FINISHING:
                result.analog_stick_l = {0};
                result.analog_stick_r = {0};
                InjectAll(result);
                break;
        }
        svcSleepThread(UPDATE_INTERVAL_NS);
    }
}

// 判定事件
void AutoKeyLoop::DetermineEvent(ProcessResult& result) {
    /*
        事件判定规则：
        1. 连发或者宏模块内部，会返回应该处于的事件
        2. 检测事件时优先检测宏的事件
        3. 如果宏事件只要不是IDLE，就直接返回宏事件
        4. 在返回之前，会检查宏事件是否为STARTING，且若连发模块启用了，就重置一次连发的状态机参数
        5. 如果宏事件是返回的IDLE，则检查连发模块是否启用，如果启用则返回连发模块的事件
        6. 如果连发模块没有启用，则返回IDLE
    */ 
    if (m_IsPaused || m_ControllerType == ControllerType::C_NONE) {
        if (m_Turbo) {
            if (m_ControllerType == ControllerType::C_NONE) m_Turbo->ResetState();
            else m_Turbo->SynchronizeInput(result.buttons, m_isJoyCon);
        }
        if (m_ControllerType == ControllerType::C_NONE && m_TouchMapper) {
            m_TouchMapper->ResetState();
            m_TouchWasActive = false;
        }
        result.event = FeatureEvent::PAUSED;
        return;
    }
    if (m_Macro) {
        m_Macro->Process(result);
        if (result.event == FeatureEvent::STARTING) {
            if (m_Turbo) m_Turbo->TurboFinishing();
            if (m_TouchMapper) m_TouchMapper->ResetState();
            m_TouchWasActive = false;
        }
        if (result.event != FeatureEvent::IDLE) {
            if (m_Turbo) m_Turbo->SynchronizeInput(result.buttons, m_isJoyCon);
            return;
        }
    }

    u64 physicalButtons = result.buttons;
    u64 touchButtons = m_TouchMapper ? m_TouchMapper->Process() : 0;
    bool touchActive = touchButtons != 0;
    bool touchStarted = touchActive && !m_TouchWasActive;
    bool touchFinished = !touchActive && m_TouchWasActive;
    m_TouchWasActive = touchActive;
    result.buttons |= touchButtons;

    if (m_Turbo) {
        m_Turbo->Process(result, m_isJoyCon);
        if (result.event != FeatureEvent::IDLE) return;
    }

    if (touchActive) {
        result.OtherButtons = result.buttons;
        result.event = touchStarted ? FeatureEvent::STARTING : FeatureEvent::Touch_EXECUTING;
        return;
    }
    if (touchFinished) {
        result.OtherButtons = physicalButtons;
        result.event = FeatureEvent::FINISHING;
        return;
    }
    result.event = FeatureEvent::IDLE;
}


// 暂停
void AutoKeyLoop::Pause() {
    if (m_Turbo) m_Turbo->ResetState();
    if (m_Macro) m_Macro->MacroFinishing();
    if (m_TouchMapper) m_TouchMapper->ResetState();
    m_TouchWasActive = false;
    m_PauseStateRestored = false;
    m_IsPaused = true;
}

// 恢复
void AutoKeyLoop::Resume() {
    m_IsPaused = false;
}

// 更新连发功能
void AutoKeyLoop::UpdateTurboFeature(bool enable, const char* config_path) {
    if (m_EnableTurbo && enable && m_Turbo) {
        m_Turbo->LoadConfig(config_path);
        m_isJCRightHand = m_Turbo->IsJCRightHand();
    }
    else if (m_EnableTurbo && !enable && m_Turbo) m_Turbo.reset();
    else if (!m_EnableTurbo && enable) {
        m_Turbo = std::make_unique<Turbo>(config_path);
        m_isJCRightHand = m_Turbo->IsJCRightHand();
    }
    m_EnableTurbo = enable;
}

// 更新宏功能
void AutoKeyLoop::UpdateMacroFeature(bool enable, const char* macroCfgPath) {
    if (m_EnableMacro && enable && m_Macro) m_Macro->LoadConfig(macroCfgPath);
    else if (m_EnableMacro && !enable && m_Macro) m_Macro.reset();
    else if (!m_EnableMacro && enable) m_Macro = std::make_unique<Macro>(macroCfgPath);
    m_EnableMacro = enable;
}

void AutoKeyLoop::UpdateTouchFeature(bool enable, const char* config_path) {
    if (m_EnableTouch && enable && m_TouchMapper) m_TouchMapper->LoadConfig(config_path);
    else if (m_EnableTouch && !enable && m_TouchMapper) m_TouchMapper.reset();
    else if (!m_EnableTouch && enable) m_TouchMapper = std::make_unique<TouchMapper>(config_path);
    m_EnableTouch = enable;
    m_TouchWasActive = false;
}

// 逆映射必须使用实际生效的配置，避免重放已被系统优先级覆盖的规则。
void AutoKeyLoop::UpdateButtonMappings() {
    ButtonRemapper::RefreshReverseMapping();
}

// 读取物理输入
void AutoKeyLoop::ReadPhysicalInput(ProcessResult& result) {
    m_isJoyCon = false;
    // 先确认手柄类型
    HidNpadIdType npad_id = HidNpadIdType_No1;
    u32 style_set = hidGetNpadStyleSet(npad_id);
    u32 handheld_style = hidGetNpadStyleSet(HidNpadIdType_Handheld);
    // 默认为未知
    m_ControllerType = ControllerType::C_NONE;
    if (style_set & HidNpadStyleTag_NpadFullKey) m_ControllerType = ControllerType::C_PRO;
    else if (style_set & HidNpadStyleTag_NpadJoyDual) m_ControllerType = ControllerType::C_JOYDUAL;
    else if (style_set & HidNpadStyleTag_NpadSystemExt) m_ControllerType = ControllerType::C_SYSTEMEXT;
    else if (handheld_style & HidNpadStyleTag_NpadHandheld){
        m_isJoyCon = isPhysicalJoyCon();
        if (m_isJoyCon) m_ControllerType = ControllerType::C_JOYCON;
        else m_ControllerType = ControllerType::C_LITE;
    }
    // 根据类型读取按键数据
    switch (m_ControllerType) {
        case ControllerType::C_PRO:
            READ_NPAD_STATE(HidNpadFullKeyState, hidGetNpadStatesFullKey, npad_id);
            break;
        case ControllerType::C_JOYDUAL:
            READ_NPAD_STATE(HidNpadJoyDualState, hidGetNpadStatesJoyDual, npad_id);
            break;
        case ControllerType::C_SYSTEMEXT:
            READ_NPAD_STATE(HidNpadSystemExtState, hidGetNpadStatesSystemExt, npad_id);
            break;
        case ControllerType::C_JOYCON:
        case ControllerType::C_LITE:
            READ_NPAD_STATE(HidNpadHandheldState, hidGetNpadStatesHandheld, HidNpadIdType_Handheld);
            break;
        default:
            break;
    }
}


void AutoKeyLoop::ApplyHdlsState(ProcessResult& result) {
    switch (m_ControllerType) {
        case ControllerType::C_PRO:
        case ControllerType::C_SYSTEMEXT:
            InjectPro(result);
            break;
        case ControllerType::C_JOYDUAL:
            InjectJoyDual(result);
            break;
        case ControllerType::C_JOYCON:
            InjectJoyCon(result);
            break;
        case ControllerType::C_LITE:
            InjectLite(result);
            break;
        default:
            break;
    }
}

// Pro手柄注入
void AutoKeyLoop::InjectPro(ProcessResult& result) {
    for (int i = 0; i < m_StateList.total_entries; i++) {
        HidDeviceType device_type = (HidDeviceType)m_StateList.entries[i].device.deviceType;
        HiddbgHdlsState state;
        memset(&state, 0, sizeof(HiddbgHdlsState));
        if (device_type == HidDeviceType_FullKey3) {
            state.buttons = result.OtherButtons;
            state.analog_stick_l = result.analog_stick_l;
            state.analog_stick_r = result.analog_stick_r;
            hiddbgSetHdlsState(m_StateList.entries[i].handle, &state);
            break;
        }
    }
}

// 双JoyCon蓝牙模式
// 已知：蓝牙模式注入延迟（可能我猜的）非常大，反正jc蓝牙模式效果极差基本等于不可用
void AutoKeyLoop::InjectJoyDual(ProcessResult& result) {
    int found_count = 0;
    for (int i = 0; i < m_StateList.total_entries; i++) {
        HidDeviceType device_type = (HidDeviceType)m_StateList.entries[i].device.deviceType;
        HiddbgHdlsState state;
        memset(&state, 0, sizeof(HiddbgHdlsState));
        if (device_type == HidDeviceType_JoyLeft2) {
            found_count++;
            if (result.event == FeatureEvent::Turbo_EXECUTING && !m_isJCRightHand) continue;
            state.buttons = result.OtherButtons & LEFT_JOYCON_BUTTONS;
            state.analog_stick_l = result.analog_stick_l;
            hiddbgSetHdlsState(m_StateList.entries[i].handle, &state);
        }
        else if (device_type == HidDeviceType_JoyRight1) {
            found_count++;
            if (result.event == FeatureEvent::Turbo_EXECUTING && !m_isJCRightHand) continue;
            state.buttons = result.OtherButtons & RIGHT_JOYCON_BUTTONS;
            state.analog_stick_r = result.analog_stick_r;
            hiddbgSetHdlsState(m_StateList.entries[i].handle, &state);
        }
        if (found_count == 2) break;
    }
}


// LITE注入
void AutoKeyLoop::InjectLite(ProcessResult& result) {
    for (int i = 0; i < m_StateList.total_entries; i++) {
        HidDeviceType device_type = (HidDeviceType)m_StateList.entries[i].device.deviceType;
        HiddbgHdlsState state;
        memset(&state, 0, sizeof(HiddbgHdlsState));
        if (device_type == HidDeviceType_DebugPad) {
            u64 buttons = result.OtherButtons;
            ButtonRemapper::ApplyReverseMapping(buttons);
            state.buttons = buttons;
            state.analog_stick_l = result.analog_stick_l;
            state.analog_stick_r = result.analog_stick_r;
            hiddbgSetHdlsState(m_StateList.entries[i].handle, &state);
            break;
        }
    }
}

void AutoKeyLoop::InjectJoyCon(ProcessResult& result) {
    int found_count = 0;
    for (int i = 0; i < m_StateList.total_entries; i++) {
        HidDeviceType device_type = (HidDeviceType)m_StateList.entries[i].device.deviceType;
        HiddbgHdlsState state;
        memset(&state, 0, sizeof(HiddbgHdlsState));
        if (device_type == HidDeviceType_JoyLeft2) {
            found_count++;
            if (result.event == FeatureEvent::Turbo_EXECUTING && m_isJCRightHand) continue;
            state.buttons = result.OtherButtons & LEFT_JOYCON_BUTTONS;
            state.analog_stick_l = result.analog_stick_l;
            hiddbgSetHdlsState(m_StateList.entries[i].handle, &state);
        }
        else if (device_type == HidDeviceType_JoyRight1) {
            found_count++;
            if (result.event == FeatureEvent::Turbo_EXECUTING && !m_isJCRightHand) continue;
            state.buttons = result.OtherButtons & RIGHT_JOYCON_BUTTONS;
            state.analog_stick_r = result.analog_stick_r;
            hiddbgSetHdlsState(m_StateList.entries[i].handle, &state);
        }
        if (found_count == 2) break;
    }
}


// 遍历所有手柄都注入
void AutoKeyLoop::InjectAll(ProcessResult& result) {
    for (int i = 0; i < m_StateList.total_entries; i++) {
        memset(&m_StateList.entries[i].state, 0, sizeof(HiddbgHdlsState));
        HidDeviceType device_type = (HidDeviceType)m_StateList.entries[i].device.deviceType;
        if (IsLeftController(device_type)) {    
            m_StateList.entries[i].state.buttons = result.OtherButtons & LEFT_JOYCON_BUTTONS;
            m_StateList.entries[i].state.analog_stick_l = result.analog_stick_l;
        } else if (IsRightController(device_type)) { 
            m_StateList.entries[i].state.buttons = result.OtherButtons & RIGHT_JOYCON_BUTTONS;;
            m_StateList.entries[i].state.analog_stick_r = result.analog_stick_r;
        } else {  
            u64 buttons = result.OtherButtons;
            if (device_type == HidDeviceType_DebugPad) ButtonRemapper::ApplyReverseMapping(buttons);
            m_StateList.entries[i].state.buttons = buttons;
            m_StateList.entries[i].state.analog_stick_l = result.analog_stick_l;
            m_StateList.entries[i].state.analog_stick_r = result.analog_stick_r;
        }
    }
    hiddbgApplyHdlsStateList(m_HdlsSessionId, &m_StateList); 
}
