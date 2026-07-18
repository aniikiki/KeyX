#pragma once

#include <switch.h>
#include "common.hpp"

class Turbo {
public:
    Turbo(const char* config_path);
    
    // 加载配置
    void LoadConfig(const char* config_path);
    
    // 核心函数：处理输入，填充处理结果（事件+按键数据）
    void Process(ProcessResult& result, bool isJoyCon);
    
    // 停止当前连发周期，但保留切换模式的锁定按键
    void TurboFinishing();

    // 完全重置连发状态（包括切换连发的锁定按键）
    void ResetState();

    // 仅同步物理按键状态，不触发切换（用于宏播放期间）
    void SynchronizeInput(u64 buttons, bool isJoyCon);

    // 获取只允许左边还是右边的手柄联发
    bool IsJCRightHand();

private:

    bool m_isJCRightHand = true;
    // 配置参数
    u64 m_HoldButtonMask;       // 按住连发按键
    u64 m_ToggleButtonMask;     // 切换连发按键
    u64 m_PressDurationNs;      // 按下持续时间（纳秒）
    u64 m_ReleaseDurationNs;    // 松开持续时间（纳秒）
    
    // 运行状态
    bool m_IsActive;            // 是否运行中
    bool m_IsPressed;           // 当前是按下还是松开周期
    u64 m_TurboStartTime;       // 连发开始时间
    u64 m_InitialPressTime;     // 首次按下时间（用于200ms延迟）
    bool m_DelayStart;          // 是否启用延迟启动
    u64 m_LatchedButtons;       // 已锁定的切换连发按键
    u64 m_PreviousToggleButtons;// 上一轮切换键的物理状态
    bool m_NeedsInputSync;      // 恢复/重载后先同步输入，避免误触
    u64 m_ConfigPathHash;       // 当前配置标识，用于识别配置切换
    
    // 事件判定
    FeatureEvent DetermineEvent(u64 active_buttons);
    
    // 事件处理
    void TurboStarting();
    void TurboExecuting(u64 active_buttons, u64 normal_buttons, ProcessResult& result);
    
    // 辅助函数
    bool CheckRelease(u64 active_buttons);
    void GetAllowedButtonMasks(bool isJoyCon, u64& holdMask, u64& toggleMask) const;
};
