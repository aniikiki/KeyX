#pragma once

#include <switch.h>
#include <memory>
#include "common.hpp"
#include "turbo.hpp"
#include "macro.hpp"
#include "touch_mapper.hpp"

class AutoKeyLoop {
public:
    // 构造函数
    AutoKeyLoop(const char* config_path, const char* macroCfgPath, bool enable_turbo, bool enable_macro, bool enable_touch);
    
    // 析构函数
    ~AutoKeyLoop();
    
    // 更新连发功能
    void UpdateTurboFeature(bool enable, const char* config_path);
    
    // 更新宏功能
    void UpdateMacroFeature(bool enable, const char* macroCfgPath);

    // 更新触摸映射功能
    void UpdateTouchFeature(bool enable, const char* config_path);
    
    // 读取系统与 KeyX 合并后实际生效的按键映射
    void UpdateButtonMappings();
    
    // 控制接口
    void Pause();
    void Resume();

private:
    // 手柄类型枚举
    enum class ControllerType {
        C_NONE = 0,
        C_PRO,
        C_JOYDUAL,
        C_SYSTEMEXT,
        C_JOYCON,
        C_LITE
    };

    // 默认是采用右手连发
    bool m_isJCRightHand = true;
    bool m_isJoyCon = false;

    // 当前手柄类型
    ControllerType m_ControllerType;
    
    // HDLS 硬件资源
    HiddbgHdlsSessionId m_HdlsSessionId;
    HiddbgHdlsStateList m_StateList;
    bool m_HdlsInitialized;
    
    alignas(0x1000) static u8 hdls_work_buffer[0x1000];
    
    // 线程资源
    Thread m_Thread;
    bool m_ThreadCreated;
    bool m_ThreadRunning;
    bool m_ShouldExit;
    bool m_IsPaused;
    bool m_PauseStateRestored;             // 暂停后是否已恢复物理输入
    
    alignas(0x1000) static char thread_stack[4 * 1024];
    
    // 功能模块
    std::unique_ptr<Turbo> m_Turbo;
    std::unique_ptr<Macro> m_Macro;
    std::unique_ptr<TouchMapper> m_TouchMapper;
    bool m_EnableTurbo;
    bool m_EnableMacro;
    bool m_EnableTouch;
    bool m_TouchWasActive;
    
    
    
    // 内部方法
    static void ThreadFunc(void* arg);
    
    // 主循环（在线程中运行）
    void MainLoop();
    
    // 事件判定
    void DetermineEvent(ProcessResult& result);
    
    // 读取物理输入（从 HID 读取真实手柄状态）
    void ReadPhysicalInput(ProcessResult& result);
    
    // 注入输出（将按键写入 HDLS）
    void ApplyHdlsState(ProcessResult& result);
    
    // 各类型手柄的注入实现
    void InjectPro(ProcessResult& result);
    void InjectJoyDual(ProcessResult& result);
    void InjectLite(ProcessResult& result);
    void InjectJoyCon(ProcessResult& result);
    void InjectAll(ProcessResult& result);

};
