#include "turbo.hpp"
#include <minIni.h>
#include <cstdlib>


namespace {
    constexpr u64 INPUT_SETTLE_NS = 30000000ULL;
    constexpr int MIN_DELAY_START_MS = 50;
    constexpr int MAX_DELAY_START_MS = 1000;

    u64 HashConfigPath(const char* path) {
        // FNV-1a：只保存8字节配置标识，避免在常驻模块中复制完整路径
        u64 hash = 14695981039346656037ULL;
        while (*path != '\0') {
            hash ^= static_cast<u8>(*path++);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

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
}


// 构造函数
Turbo::Turbo(const char* config_path) {
    m_HoldButtonMask = 0;
    m_ToggleButtonMask = 0;
    m_StopButtonMask = 0;
    m_PressDurationNs = 100 * 1000000ULL;   // 默认100ms
    m_ReleaseDurationNs = 100 * 1000000ULL; // 默认100ms
    m_IsActive = false;
    m_IsPressed = false;
    m_TurboStartTime = 0;
    m_InitialPressTime = 0;
    m_DelayStartDurationNs = 200 * 1000000ULL;
    m_LatchedButtons = 0;
    m_PreviousToggleButtons = 0;
    m_StopButtonWasPressed = false;
    m_NeedsInputSync = true;
    m_ConfigPathHash = 0;
    m_StopInputConsumed = false;
    // 自动加载配置
    LoadConfig(config_path);
}

// 加载配置
void Turbo::LoadConfig(const char* config_path) {
    // 读取互斥的按住/切换连发/停止键掩码；旧配置中的 buttons 仍为按住连发
    char hold_buttons_str[32];
    char toggle_buttons_str[32];
    char stop_button_str[32];
    ini_gets("AUTOFIRE", "buttons", "0", hold_buttons_str, sizeof(hold_buttons_str), config_path);
    ini_gets("AUTOFIRE", "togglebuttons", "0", toggle_buttons_str, sizeof(toggle_buttons_str), config_path);
    ini_gets("AUTOFIRE", "stopbutton", "0", stop_button_str, sizeof(stop_button_str), config_path);
    u64 hold_buttons = strtoull(hold_buttons_str, nullptr, 10);
    u64 toggle_buttons = strtoull(toggle_buttons_str, nullptr, 10);
    u64 stop_button = strtoull(stop_button_str, nullptr, 10);
    hold_buttons &= ~toggle_buttons;  // 配置异常重叠时，切换模式优先
    stop_button &= ~(hold_buttons | toggle_buttons);
    if (stop_button != 0) stop_button &= (~stop_button + 1ULL); // 停止键只允许一个

    u64 config_path_hash = HashConfigPath(config_path);
    bool config_changed = m_ConfigPathHash != 0 && m_ConfigPathHash != config_path_hash;
    m_ConfigPathHash = config_path_hash;
    m_HoldButtonMask = hold_buttons;
    m_ToggleButtonMask = toggle_buttons;
    m_StopButtonMask = stop_button;
    if (config_changed) {
        m_LatchedButtons = 0;
    } else {
        m_LatchedButtons &= m_ToggleButtonMask;
    }
    m_PreviousToggleButtons = 0;
    m_StopButtonWasPressed = false;
    m_NeedsInputSync = true;
    m_StopInputConsumed = false;
    // 读取时间参数（毫秒转纳秒）
    int press_ms = ini_getl("AUTOFIRE", "presstime", 100, config_path);
    int release_ms = ini_getl("AUTOFIRE", "fireinterval", 100, config_path);
    m_PressDurationNs = (u64)press_ms * 1000000ULL;
    m_ReleaseDurationNs = (u64)release_ms * 1000000ULL;
    // 读取防止误触开关
    m_DelayStart = ini_getbool("AUTOFIRE", "delaystart", 1, config_path);
    int delay_start_ms = ini_getl("AUTOFIRE", "delaystartms", 200, config_path);
    if (delay_start_ms < MIN_DELAY_START_MS) delay_start_ms = MIN_DELAY_START_MS;
    if (delay_start_ms > MAX_DELAY_START_MS) delay_start_ms = MAX_DELAY_START_MS;
    m_DelayStartDurationNs = static_cast<u64>(delay_start_ms) * 1000000ULL;
    m_isJCRightHand = ini_getbool("AUTOFIRE", "IsJCRightHand", 1, "/config/KeyX/config.ini");

}

void Turbo::GetAllowedButtonMasks(bool isJoyCon, u64& holdMask, u64& toggleMask, u64& stopMask) const {
    holdMask = m_HoldButtonMask;
    toggleMask = m_ToggleButtonMask;
    stopMask = m_StopButtonMask;
    if (!isJoyCon) return;

    u64 sideMask = m_isJCRightHand ? RIGHT_JOYCON_BUTTONS : LEFT_JOYCON_BUTTONS;
    holdMask &= sideMask;
    toggleMask &= sideMask;
    stopMask &= sideMask;
}

void Turbo::SynchronizeInput(u64 buttons, bool isJoyCon) {
    u64 holdMask = 0;
    u64 toggleMask = 0;
    u64 stopMask = 0;
    GetAllowedButtonMasks(isJoyCon, holdMask, toggleMask, stopMask);
    (void)holdMask;
    m_LatchedButtons &= toggleMask;
    m_PreviousToggleButtons = buttons & toggleMask;
    m_StopButtonWasPressed = (buttons & stopMask) != 0;
    m_NeedsInputSync = false;
    m_StopInputConsumed = false;
}

// 获取只允许左边还是右边的手柄联发
bool Turbo::IsJCRightHand() {
    return m_isJCRightHand;
}

// 核心函数：处理输入
void Turbo::Process(ProcessResult& result, bool isJoyCon) {
    u64 holdMask = 0;
    u64 toggleMask = 0;
    u64 stopMask = 0;
    GetAllowedButtonMasks(isJoyCon, holdMask, toggleMask, stopMask);
    m_LatchedButtons &= toggleMask;

    // 切换键只负责开启；关闭统一由独立停止键完成。
    u64 physical_toggle_buttons = result.buttons & toggleMask;
    u64 physical_stop_button = result.buttons & stopMask;
    bool stop_button_is_pressed = physical_stop_button != 0;
    bool stop_pressed = false;
    if (m_NeedsInputSync) {
        m_PreviousToggleButtons = physical_toggle_buttons;
        m_StopButtonWasPressed = stop_button_is_pressed;
        m_NeedsInputSync = false;
    } else {
        u64 newly_pressed = physical_toggle_buttons & ~m_PreviousToggleButtons & ~m_LatchedButtons;
        m_LatchedButtons |= newly_pressed;
        stop_pressed = stop_button_is_pressed && !m_StopButtonWasPressed;
        m_PreviousToggleButtons = physical_toggle_buttons;
        m_StopButtonWasPressed = stop_button_is_pressed;
    }

    bool stop_action_started = false;
    bool stop_cycle_finished = false;
    bool had_latched_buttons = m_LatchedButtons != 0;
    // 只有确实存在切换连发时才消费停止键；否则它仍是普通按键。
    if (stop_pressed && had_latched_buttons && !m_StopInputConsumed) {
        m_LatchedButtons = 0;
        m_StopInputConsumed = true;
        stop_action_started = true;
    }

    // 屏蔽这次停止按压，直到实体键或触摸真正松开。
    if (m_StopInputConsumed && !stop_button_is_pressed) {
        m_StopInputConsumed = false;
        stop_cycle_finished = true;
    }

    // 按住和切换模式可以同时工作，但每个按键只属于其中一种。
    u64 hold_buttons = result.buttons & holdMask;
    u64 active_buttons = hold_buttons | m_LatchedButtons;
    u64 normal_buttons = result.buttons & ~(holdMask | toggleMask);
    if (m_StopInputConsumed || stop_cycle_finished)
        normal_buttons &= ~stopMask;

    bool force_stop_output = stop_action_started || m_StopInputConsumed || stop_cycle_finished;

    // 没有按住连发需要继续时，立即结束切换连发状态。
    if (stop_action_started && hold_buttons == 0) TurboFinishing();

    // 停止键完全松开后恢复 HDLS 状态；仍按着的按住连发不受影响。
    if (stop_cycle_finished && active_buttons == 0) {
        result.event = FeatureEvent::FINISHING;
        result.OtherButtons = normal_buttons;
        return;
    }

    result.event = DetermineEvent(active_buttons);
    switch (result.event) {
        case FeatureEvent::IDLE:
        case FeatureEvent::PAUSED:
            if (force_stop_output) {
                result.event = FeatureEvent::INPUT_EXECUTING;
                result.OtherButtons = normal_buttons | hold_buttons;
            }
            return;
        case FeatureEvent::STARTING:
            TurboStarting();
            return;
        case FeatureEvent::Turbo_EXECUTING:
            TurboExecuting(active_buttons, normal_buttons, result);
            return;
        case FeatureEvent::FINISHING:
            TurboFinishing();
            // 配置为切换连发的物理按压只作为控制输入，不额外透传一次
            result.OtherButtons = normal_buttons;
            return;
        default:
            return;
    }
}

// 事件判定
FeatureEvent Turbo::DetermineEvent(u64 active_buttons) {
    bool has_autokey = (active_buttons != 0);
    bool turbo_active = m_IsActive;
    if (turbo_active && CheckRelease(active_buttons)) return FeatureEvent::FINISHING;
    else if (turbo_active) return FeatureEvent::Turbo_EXECUTING;
    else if (has_autokey) {
        // 切换连发应立即启动；防误触延迟仅作用于按住连发
        if (m_LatchedButtons != 0) return FeatureEvent::STARTING;
        if (m_InitialPressTime == 0) m_InitialPressTime = armGetSystemTick();
        u64 elapsed_ns = armTicksToNs(armGetSystemTick() - m_InitialPressTime);
        if (m_DelayStart && elapsed_ns < m_DelayStartDurationNs) return FeatureEvent::IDLE;
        m_InitialPressTime = 0;
        return FeatureEvent::STARTING;
    }
    m_InitialPressTime = 0;
    return FeatureEvent::IDLE;
}

// 事件处理：启动连发
void Turbo::TurboStarting() {
    m_IsActive = true;
    m_IsPressed = true;
    m_TurboStartTime = armGetSystemTick();
}

// 事件处理：连发运行
void Turbo::TurboExecuting(u64 active_buttons, u64 normal_buttons, ProcessResult& result) {
    // 用绝对时间计算当前应该是按下还是松开
    u64 elapsed_ns = armTicksToNs(armGetSystemTick() - m_TurboStartTime);
    u64 cycle_ns = m_PressDurationNs + m_ReleaseDurationNs;
    u64 pos_in_cycle = elapsed_ns % cycle_ns;
    m_IsPressed = (pos_in_cycle < m_PressDurationNs);
    result.OtherButtons = m_IsPressed ? (normal_buttons | active_buttons) : normal_buttons;
}

// 事件处理：停止连发
void Turbo::TurboFinishing() {
    m_IsActive = false;
    m_IsPressed = false;
    m_TurboStartTime = 0;
    m_InitialPressTime = 0;
}

void Turbo::ResetState() {
    TurboFinishing();
    m_LatchedButtons = 0;
    m_PreviousToggleButtons = 0;
    m_StopButtonWasPressed = false;
    m_NeedsInputSync = true;
    m_StopInputConsumed = false;
}

// 检测真松开（仅在按下周期检测，避免污染）
bool Turbo::CheckRelease(u64 active_buttons) {
    if (!m_IsPressed) return false;
    // 用绝对时间计算当前周期内的位置
    u64 elapsed_ns = armTicksToNs(armGetSystemTick() - m_TurboStartTime);
    u64 cycle_ns = m_PressDurationNs + m_ReleaseDurationNs;
    u64 pos_in_cycle = elapsed_ns % cycle_ns;
    // 按下周期开始后30ms内不检测松开
    if (pos_in_cycle < INPUT_SETTLE_NS) return false;
    if (active_buttons == 0) return true;
    return false;
}
