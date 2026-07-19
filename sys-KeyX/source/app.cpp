#include <switch.h>
#include "app.hpp"
#include <errno.h>
#include <sys/stat.h>
#include <minIni.h>
#include <cstdlib>

#define CONFIG_DIR "/config/KeyX"
#define CONFIG_PATH "/config/KeyX/config.ini"

// 检查文件是否存在
bool App::FileExists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

// 初始化配置路径（确保配置目录存在）
bool App::InitializeConfigPath() {
    if (mkdir(CONFIG_DIR, 0777) == 0) return true;
    if (errno == EEXIST) return true;
    return false;
}

// App类的实现
App::App() {
    if (!InitializeConfigPath()) return;
    if (!InitializeIPC()) return;
    m_loop_error = false;
}

App::~App() {
}

// 初始化IPC服务
bool App::InitializeIPC() {

    // 加载白名单
    GameMonitor::LoadWhitelist();

    // 创建IPC服务
    ipc_server = std::make_unique<IPCServer>();

    // 设置退出回调
    ipc_server->SetExitCallback([this]() {
        ButtonRemapper::RestoreMapping();
        m_loop_error = true;
    });
    
    // 设置开启连发回调
    ipc_server->SetEnableAutoFireCallback([this]() {
        m_CurrentAutoEnable = true;
        if (m_GameInFocus && !m_OverlayOpen) {
            if (autokey_loop) UpdateTurboConfig();
            else StartAutoKey();
        }
    });
    
    // 设置关闭连发回调
    ipc_server->SetDisableAutoFireCallback([this]() {
        m_CurrentAutoEnable = false;
        if (!m_CurrentAutoMacroEnable && !m_CurrentTouchEnable) StopAutoKey();
        else UpdateTurboConfig();
    });
    
    // 设置开启宏回调
    ipc_server->SetEnableMacroCallback([this]() {
        m_CurrentAutoMacroEnable = true;
        if (m_GameInFocus && !m_OverlayOpen) {
            if (autokey_loop) UpdateMacroConfig();
            else StartAutoKey();
        }
    });
    
    // 设置关闭宏回调
    ipc_server->SetDisableMacroCallback([this]() {
        m_CurrentAutoMacroEnable = false;
        if (!m_CurrentAutoEnable && !m_CurrentTouchEnable) StopAutoKey();
        else UpdateMacroConfig();
    });

    ipc_server->SetEnableTouchCallback([this]() {
        m_CurrentTouchEnable = true;
        // 已有循环时立即补建触摸模块；Tesla 打开期间没有循环，关闭后统一重建。
        if (autokey_loop) UpdateTouchConfig();
        else if (m_GameInFocus && !m_OverlayOpen) StartAutoKey();
    });

    ipc_server->SetDisableTouchCallback([this]() {
        m_CurrentTouchEnable = false;
        if (!m_CurrentAutoEnable && !m_CurrentAutoMacroEnable) StopAutoKey();
        else UpdateTouchConfig();
    });
    
    // 设置开启映射回调
    ipc_server->SetEnableMappingCallback([this]() {
        m_CurrentAutoRemapEnable = true;
        if (m_GameInFocus && !m_OverlayOpen) ButtonRemapper::SetMapping(m_ConfigPath);
        UpdateButtonMappingConfig();
    });
    
    // 设置关闭映射回调
    ipc_server->SetDisableMappingCallback([this]() {
        m_CurrentAutoRemapEnable = false;
        ButtonRemapper::RestoreMapping();
    });
    
    // 设置重载全部配置
    ipc_server->SetReloadBasicCallback([this]() {
        LoadGameConfig(m_CurrentTid);
        if (m_GameInFocus && !m_OverlayOpen && m_CurrentAutoRemapEnable) ButtonRemapper::SetMapping(m_ConfigPath);
        if (m_GameInFocus && !m_OverlayOpen && HasAutoKeyFeature() && !autokey_loop) StartAutoKey();
        UpdateTurboConfig();
        UpdateMacroConfig();
        UpdateTouchConfig();
        UpdateButtonMappingConfig();
        if (!HasAutoKeyFeature() && autokey_loop) StopAutoKey();
    });
    
    // 设置重载连发配置回调
    ipc_server->SetReloadAutoFireCallback([this]() {
        UpdateTurboConfig();
    });
    
    // 设置重载宏配置回调
    ipc_server->SetReloadMacroCallback([this]() {
        UpdateMacroConfig();
    });

    ipc_server->SetReloadTouchCallback([this]() {
        UpdateTouchConfig();
    });
    
    // 设置重载映射配置回调
    ipc_server->SetReloadMappingCallback([this]() {
        if (m_GameInFocus && !m_OverlayOpen && m_CurrentAutoRemapEnable) ButtonRemapper::SetMapping(m_ConfigPath);
        UpdateButtonMappingConfig();
    });

    // 重载白名单
    ipc_server->SetReloadWhitelistCallback([this]() {
        GameMonitor::LoadWhitelist();
    });

    ipc_server->SetPauseInputCallback([this]() {
        m_OverlayOpen = true;
        // Tesla 需要直接读取实体按键。仅暂停线程会让最后一次 HDLS 状态继续
        // 占用输入，导致停止键等按键偶发丢失；销毁循环可完整释放 HDLS。
        if (autokey_loop) StopAutoKey();
        if (m_CurrentAutoRemapEnable) ButtonRemapper::RestoreMapping();
    });

    ipc_server->SetResumeInputCallback([this]() {
        m_OverlayOpen = false;
        if (!m_GameInFocus) return;

        if (HasAutoKeyFeature()) {
            if (autokey_loop) ResumeAutoKey();
            else StartAutoKey();
        }
        if (m_CurrentAutoRemapEnable) ButtonRemapper::SetMapping(m_ConfigPath);
    });

    // 启动服务
    if (!ipc_server->Start("keyLoop")) {
        ipc_server.reset();
        return false;
    }

    return true;
}


void App::Loop() {
    while (!m_loop_error) {
        GameStateResult game = GameMonitor::GetState();
        switch (game.event) {
            case GameEvent::Idle:
                // 无游戏运行
                for (int i = 0; i < 10 && !m_loop_error; ++i) svcSleepThread(100000000ULL);
                continue;
            case GameEvent::Running:
                OnGameRunning(game.tid);
                if (autokey_loop || m_CurrentAutoRemapEnable) svcSleepThread(100000000ULL);
                else for (int i = 0; i < 5 && !m_loop_error; ++i) svcSleepThread(100000000ULL);
                continue;
            case GameEvent::Launched:
                OnGameLaunched(game.tid);
                break;
            case GameEvent::Exited:
                OnGameExited();
                break;
            default:
                break;
        }
        svcSleepThread(100000000ULL);  // 100ms
    }
}

// 处理游戏启动事件
void App::OnGameLaunched(u64 tid) {
    m_FirstLaunch = true;
    m_GameInFocus = true;
    m_CurrentTid = tid;
    LoadGameConfig(tid);
    if (!m_OverlayOpen && HasAutoKeyFeature()) StartAutoKey();
    if (!m_OverlayOpen && m_CurrentAutoRemapEnable) ButtonRemapper::SetMapping(m_ConfigPath);
}

// 处理游戏运行事件
void App::OnGameRunning(u64 tid) {
    // 获取游戏焦点状态(只有在焦点变化的时候才会获取到在焦点或者不在，不然获取的是无变化)
    FocusState focus = FocusMonitor::GetState(tid);
    switch (focus) {
        case FocusState::InFocus:
            m_GameInFocus = true;
            if (!m_OverlayOpen && HasAutoKeyFeature() && autokey_loop) ResumeAutoKey();
            else if (!m_OverlayOpen && HasAutoKeyFeature() && !autokey_loop) StartAutoKey();
            else if (!HasAutoKeyFeature() && autokey_loop) StopAutoKey();
            if (!m_OverlayOpen && m_CurrentAutoRemapEnable) ButtonRemapper::SetMapping(m_ConfigPath);
            break;
        case FocusState::OutOfFocus:
            m_GameInFocus = false;
            if (autokey_loop) PauseAutoKey();
            if (m_CurrentAutoRemapEnable) ButtonRemapper::RestoreMapping();
            break;
        default:
            break;
    }
}

// 处理游戏退出事件
void App::OnGameExited() {
    m_GameInFocus = false;
    if (autokey_loop) StopAutoKey();
    m_CurrentTid = 0;
}

// 加载游戏配置（读取并缓存配置参数）
void App::LoadGameConfig(u64 tid) {
    LoadBasicConfig(tid);
}

// 加载基础配置（确定配置路径）
void App::LoadBasicConfig(u64 tid) {
    snprintf(m_GameConfigPath, sizeof(m_GameConfigPath), "/config/KeyX/GameConfig/%016lX.ini", tid);
    m_CurrentGlobConfig = ini_getbool("AUTOFIRE", "globconfig", 1, m_GameConfigPath);
    if (m_FirstLaunch && m_CurrentGlobConfig) {
        bool defaultAutoEnable = ini_getbool("AUTOFIRE", "defaultautoenable", 0, CONFIG_PATH);
        bool defaultRemapEnable = ini_getbool("MAPPING", "defaultautoenable", 0, CONFIG_PATH);
        bool defaultTouchEnable = ini_getbool("TOUCH", "defaultautoenable", 0, CONFIG_PATH);
        ini_putl("AUTOFIRE", "autoenable", defaultAutoEnable, CONFIG_PATH);
        ini_putl("MAPPING", "autoenable", defaultRemapEnable, CONFIG_PATH);
        ini_putl("TOUCH", "autoenable", defaultTouchEnable, CONFIG_PATH);
    }
    m_FirstLaunch = false;
    // 此处用来设定是否使用全局配置，还是独立配置
    // 功能的详细参数，是m_ConfigPath
    // 功能的开关，是SwitchConfigPath
    // 宏的开关，是 m_GameConfigPath
    m_ConfigPath = m_CurrentGlobConfig ? CONFIG_PATH : m_GameConfigPath;
    const char* switchConfigPath = m_CurrentGlobConfig ? CONFIG_PATH : m_GameConfigPath;
    m_CurrentAutoEnable = ini_getbool("AUTOFIRE", "autoenable", 0, switchConfigPath);
    m_CurrentAutoRemapEnable = ini_getbool("MAPPING", "autoenable", 0, switchConfigPath);
    m_CurrentTouchEnable = ini_getbool("TOUCH", "autoenable", 0, switchConfigPath);
    m_CurrentAutoMacroEnable = ini_getbool("MACRO", "autoenable", 0, m_GameConfigPath);  // 宏只读取独立配置
}

bool App::HasAutoKeyFeature() const {
    return m_CurrentAutoEnable || m_CurrentAutoMacroEnable || m_CurrentTouchEnable;
}

// 开启按键模块
bool App::StartAutoKey() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    // 如果已经创建，则不重复创建
    if (autokey_loop) return true;
    autokey_loop = std::make_unique<AutoKeyLoop>(
        m_ConfigPath, m_GameConfigPath,
        m_CurrentAutoEnable, m_CurrentAutoMacroEnable, m_CurrentTouchEnable);
    return true;
}

// 退出按键模块
void App::StopAutoKey() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) autokey_loop.reset();
}

// 暂停连发
void App::PauseAutoKey() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) autokey_loop->Pause();
}

// 恢复连发
void App::ResumeAutoKey() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) autokey_loop->Resume();
}

// 更新连发配置
void App::UpdateTurboConfig() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) {
        autokey_loop->UpdateTurboFeature(m_CurrentAutoEnable, m_ConfigPath);
    }
}

// 更新宏配置
void App::UpdateMacroConfig() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) {
        autokey_loop->UpdateMacroFeature(m_CurrentAutoMacroEnable, m_GameConfigPath);
    }
}

void App::UpdateTouchConfig() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) {
        autokey_loop->UpdateTouchFeature(m_CurrentTouchEnable, m_ConfigPath);
    }
}

// 更新按键映射配置
void App::UpdateButtonMappingConfig() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) {
        autokey_loop->UpdateButtonMappings(m_ConfigPath);
    }
}
