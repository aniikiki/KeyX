#include "turbo_config.hpp"
#include "game.hpp"
#include "ini_helper.hpp"
#include "ipc.hpp"
#include "hiddata.hpp"
#include "refresh.hpp"

namespace {
    constexpr int DELAY_START_MIN_MS = 50;
    constexpr int DELAY_START_MAX_MS = 1000;
    constexpr int DELAY_START_STEP_MS = 10;
    constexpr int DELAY_START_STEPS =
        (DELAY_START_MAX_MS - DELAY_START_MIN_MS) / DELAY_START_STEP_MS + 1;
    constexpr int TURBO_FREQUENCY_MIN = 1;
    constexpr int TURBO_FREQUENCY_MAX = 10;
    constexpr int TURBO_FREQUENCY_STEPS =
        TURBO_FREQUENCY_MAX - TURBO_FREQUENCY_MIN + 1;

    u64 s_HoldTurboButtons = 0;
    u64 s_ToggleTurboButtons = 0;
    u64 s_StopTurboButton = 0;
    u64 s_TriggerTurboButton = 0;

    enum class TurboButtonMode {
        Off,
        Hold,
        Toggle,
        Stop,
        Trigger,
    };

    TurboButtonMode GetButtonMode(u64 flag) {
        if (s_TriggerTurboButton & flag) return TurboButtonMode::Trigger;
        if (s_StopTurboButton & flag) return TurboButtonMode::Stop;
        if (s_ToggleTurboButtons & flag) return TurboButtonMode::Toggle;
        if (s_HoldTurboButtons & flag) return TurboButtonMode::Hold;
        return TurboButtonMode::Off;
    }

    const char* GetModeText(TurboButtonMode mode) {
        switch (mode) {
            case TurboButtonMode::Hold: return "按住";
            case TurboButtonMode::Toggle: return "切换";
            case TurboButtonMode::Stop: return "停止键";
            case TurboButtonMode::Trigger: return "功能键";
            default: return "关闭";
        }
    }

    tsl::Color GetModeColor(TurboButtonMode mode) {
        switch (mode) {
            case TurboButtonMode::Hold: return {0x00, 0xDD, 0xFF, 0xFF};
            case TurboButtonMode::Toggle: return {0xFF, 0xD0, 0x00, 0xFF};
            case TurboButtonMode::Stop: return {0xFF, 0x55, 0x75, 0xFF};
            case TurboButtonMode::Trigger: return {0x7C, 0xFF, 0x6B, 0xFF};
            default: return tsl::style::color::ColorDescription;
        }
    }

    void SaveButtonMasks(const char* configPath) {
        IniHelper::setString("AUTOFIRE", "buttons", std::to_string(s_HoldTurboButtons), configPath);
        IniHelper::setString("AUTOFIRE", "togglebuttons", std::to_string(s_ToggleTurboButtons), configPath);
        IniHelper::setString("AUTOFIRE", "stopbutton", std::to_string(s_StopTurboButton), configPath);
        IniHelper::setString("AUTOFIRE", "triggerbutton", std::to_string(s_TriggerTurboButton), configPath);
    }
    
    int FrequencyFromDurations(int pressMs, int releaseMs) {
        int cycleMs = pressMs + releaseMs;
        if (cycleMs <= 0) return TURBO_FREQUENCY_MAX;

        int frequency = (1000 + cycleMs / 2) / cycleMs;
        if (frequency < TURBO_FREQUENCY_MIN) return TURBO_FREQUENCY_MIN;
        if (frequency > TURBO_FREQUENCY_MAX) return TURBO_FREQUENCY_MAX;
        return frequency;
    }

    void DurationsFromFrequency(int frequency, int& pressMs, int& releaseMs) {
        int cycleMs = (1000 + frequency / 2) / frequency;
        pressMs = (cycleMs + 1) / 2;
        releaseMs = cycleMs - pressMs;
    }
}

SettingTurboConfig::SettingTurboConfig(bool isGlobal, u64 currentTitleId)  
    : m_isGlobal(isGlobal)
{
    if (!m_isGlobal) {
        // 获取当前游戏名称
        GameMonitor::getTitleIdGameName(currentTitleId, m_gameName);
        // 生成当前游戏配置文件路径 示例：/config/KeyX/GameConfig/01007EF00011E000.ini
        snprintf(m_ConfigPath, sizeof(m_ConfigPath), "/config/KeyX/GameConfig/%016lX.ini", currentTitleId);
    }
    else {
        // 生成全局配置文件路径 示例：/config/KeyX/config.ini
        snprintf(m_ConfigPath, sizeof(m_ConfigPath), "/config/KeyX/config.ini");
    }

    // 从现有按下/松开时间换算频率，兼容旧版三档配置。
    int press = IniHelper::getInt("AUTOFIRE", "presstime", 50, m_ConfigPath);
    int release = IniHelper::getInt("AUTOFIRE", "fireinterval", 50, m_ConfigPath);
    m_TurboFrequency = FrequencyFromDurations(press, release);
    // 读取防止误触配置（0=关闭，1=开启）
    m_DelayStart = IniHelper::getInt("AUTOFIRE", "delaystart", 1, m_ConfigPath);
    m_DelayStartMs = IniHelper::getInt("AUTOFIRE", "delaystartms", 200, m_ConfigPath);
    if (m_DelayStartMs < DELAY_START_MIN_MS) m_DelayStartMs = DELAY_START_MIN_MS;
    if (m_DelayStartMs > DELAY_START_MAX_MS) m_DelayStartMs = DELAY_START_MAX_MS;
    // 读取互斥的按住/切换连发/停止键/功能键配置
    std::string holdButtons = IniHelper::getString("AUTOFIRE", "buttons", "0", m_ConfigPath);
    std::string toggleButtons = IniHelper::getString("AUTOFIRE", "togglebuttons", "0", m_ConfigPath);
    std::string stopButton = IniHelper::getString("AUTOFIRE", "stopbutton", "0", m_ConfigPath);
    std::string triggerButton = IniHelper::getString("AUTOFIRE", "triggerbutton", "0", m_ConfigPath);
    s_HoldTurboButtons = std::strtoull(holdButtons.c_str(), nullptr, 10);
    s_ToggleTurboButtons = std::strtoull(toggleButtons.c_str(), nullptr, 10);
    s_StopTurboButton = std::strtoull(stopButton.c_str(), nullptr, 10);
    s_TriggerTurboButton = std::strtoull(triggerButton.c_str(), nullptr, 10);
    s_HoldTurboButtons &= ~s_ToggleTurboButtons;
    s_StopTurboButton &= ~(s_HoldTurboButtons | s_ToggleTurboButtons);
    if (s_StopTurboButton != 0) s_StopTurboButton &= (~s_StopTurboButton + 1ULL);
    s_TriggerTurboButton &= ~(s_HoldTurboButtons | s_ToggleTurboButtons | s_StopTurboButton);
    if (s_TriggerTurboButton != 0)
        s_TriggerTurboButton &= (~s_TriggerTurboButton + 1ULL);
}

tsl::elm::Element* SettingTurboConfig::createUI() {
    const char* title = m_isGlobal ? "全局配置" : "独立配置";
    const char* subtitle = m_isGlobal ? "设置全局默认连发参数" : m_gameName;
    
    auto frame = new tsl::elm::OverlayFrame(title, subtitle);
    auto list = new tsl::elm::List();
    
    auto listItemTurboKeySetting = new tsl::elm::CategoryHeader(" 连发参数配置");
    list->addItem(listItemTurboKeySetting);

    auto listItemTurboKey = new tsl::elm::ListItem("连发按键", ">");
    listItemTurboKey->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<SettingTurboButton>(m_ConfigPath, m_isGlobal);
            return true;
        }
        return false;
    });
    list->addItem(listItemTurboKey);

    auto listItemTurboFrequency = new tsl::elm::StepTrackBarV2(
        "连发频率", "", TURBO_FREQUENCY_STEPS,
        TURBO_FREQUENCY_MIN, TURBO_FREQUENCY_MAX, "次/秒");
    listItemTurboFrequency->setSimpleCallback([this](s16 value, s16) {
        m_TurboFrequency = value;
        int pressMs = 0;
        int releaseMs = 0;
        DurationsFromFrequency(m_TurboFrequency, pressMs, releaseMs);
        IniHelper::setInt("AUTOFIRE", "presstime", pressMs, m_ConfigPath);
        IniHelper::setInt("AUTOFIRE", "fireinterval", releaseMs, m_ConfigPath);
        g_ipcManager.sendReloadAutoFireCommand();
    });
    listItemTurboFrequency->setProgress(
        static_cast<u8>(m_TurboFrequency - TURBO_FREQUENCY_MIN));
    list->addItem(listItemTurboFrequency);

    list->addItem(new tsl::elm::CategoryHeader(" 延迟启动连发功能避免误触"));

    auto listItemDelayStart = new tsl::elm::ListItem("防止误触", m_DelayStart ? "开" : "关");
    listItemDelayStart->setClickListener([listItemDelayStart, this](u64 keys) {
        if (keys & HidNpadButton_A) {
            m_DelayStart = !m_DelayStart;
            IniHelper::setInt("AUTOFIRE", "delaystart", m_DelayStart ? 1 : 0, m_ConfigPath);
            g_ipcManager.sendReloadAutoFireCommand();
            listItemDelayStart->setValue(m_DelayStart ? "开" : "关");
            return true;
        }
        return false;
    });
    list->addItem(listItemDelayStart);

    auto listItemDelayDuration = new tsl::elm::StepTrackBarV2(
        "防误触时间", "", DELAY_START_STEPS,
        DELAY_START_MIN_MS, DELAY_START_MAX_MS, "ms");
    listItemDelayDuration->setSimpleCallback([this](s16 value, s16) {
        m_DelayStartMs = value;
        IniHelper::setInt("AUTOFIRE", "delaystartms", m_DelayStartMs, m_ConfigPath);
        g_ipcManager.sendReloadAutoFireCommand();
    });
    listItemDelayDuration->setProgress(
        static_cast<u8>((m_DelayStartMs - DELAY_START_MIN_MS) / DELAY_START_STEP_MS));
    list->addItem(listItemDelayDuration);
    

    // 为新增的延迟滑条留出空间，同时保持按键布局完整可见
    s32 buttonDisplayHeight = 180;
    // 添加按键布局显示区域：蓝色=按住，黄色=切换，红色=停止键，绿色=功能键
    auto buttonDisplay = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* r, s32 x, s32 y, s32 w, s32 h) {
        tsl::Color whiteColor = {0xFF, 0xFF, 0xFF, 0xFF};
        auto buttonColor = [&](u64 flag) {
            TurboButtonMode mode = GetButtonMode(flag);
            return mode == TurboButtonMode::Off ? whiteColor : GetModeColor(mode);
        };
        
        const s32 buttonSize = 30 * 0.8;   // 按钮大小
        const s32 rowSpacing = 10;   // 行间距
        
        // === 水平位置计算 ===
        // 左侧：方向键组
        s32 dpadLeftX = x + 40;      // 方向键左
        s32 dpadCenterX = x + 80;    // 方向键上/下
        s32 dpadRightX = x + 120;    // 方向键右
        
        // L/ZL列：在方向键右的右边，保持间距
        s32 lColumnX = dpadRightX + 35;  // L和ZL垂直对齐
        
        // 右侧：ABXY按键组
        s32 aButtonX = x + w - 40;    // A按键
        s32 rightColumnX = x + w - 80;  // X和B按键（垂直对齐）
        s32 yButtonX = x + w - 120;   // Y按键
        
        // R/ZR列：在Y左边，保持间距
        s32 rColumnX = yButtonX - 35;  // R和ZR垂直对齐
        
        // === 垂直位置计算 ===
        // 总高度 = 5行×30px + 4个间距×10px = 190px
        const s32 layoutTotalHeight = 5 * buttonSize + 4 * rowSpacing;
        
        // 垂直居中（drawString的y参数是文字底部，需加buttonSize偏移）
        s32 baseY = y + (h - layoutTotalHeight) / 2 + buttonSize;
        
        // === 根据每个按键的连发模式显示颜色 ===
        // 行1: ZL, ZR
        s32 row1Y = baseY;
        r->drawString(ButtonIcon::ZL, false, lColumnX, row1Y, buttonSize, 
            a(buttonColor(BTN_ZL)));
        r->drawString(ButtonIcon::ZR, false, rColumnX, row1Y, buttonSize, 
            a(buttonColor(BTN_ZR)));
        
        // 行2: L, R
        s32 row2Y = baseY + buttonSize + rowSpacing;
        r->drawString(ButtonIcon::L, false, lColumnX, row2Y + 10, buttonSize, 
            a(buttonColor(BTN_L)));
        r->drawString(ButtonIcon::R, false, rColumnX, row2Y + 10, buttonSize, 
            a(buttonColor(BTN_R)));
        
        // 行3: 方向键上, X
        s32 row3Y = baseY + 2 * (buttonSize + rowSpacing);
        r->drawString(ButtonIcon::Up, false, dpadCenterX, row3Y, buttonSize, 
            a(buttonColor(BTN_UP)));
        r->drawString(ButtonIcon::X, false, rightColumnX, row3Y, buttonSize, 
            a(buttonColor(BTN_X)));
        
        // 行4: 方向键左右, Y/A
        s32 row4Y = baseY + 3 * (buttonSize + rowSpacing);
        r->drawString(ButtonIcon::Left, false, dpadLeftX, row4Y, buttonSize, 
            a(buttonColor(BTN_LEFT)));
        r->drawString(ButtonIcon::Right, false, dpadRightX, row4Y, buttonSize, 
            a(buttonColor(BTN_RIGHT)));
        r->drawString(ButtonIcon::Y, false, yButtonX, row4Y, buttonSize, 
            a(buttonColor(BTN_Y)));
        r->drawString(ButtonIcon::A, false, aButtonX, row4Y, buttonSize, 
            a(buttonColor(BTN_A)));
        
        // 行5: 方向键下, B
        s32 row5Y = baseY + 4 * (buttonSize + rowSpacing);
        r->drawString(ButtonIcon::Down, false, dpadCenterX, row5Y, buttonSize, 
            a(buttonColor(BTN_DOWN)));
        r->drawString(ButtonIcon::B, false, rightColumnX, row5Y, buttonSize, 
            a(buttonColor(BTN_B)));
    });
    
    list->addItem(buttonDisplay, buttonDisplayHeight);
    
    frame->setContent(list);
    return frame;
}


SettingTurboButton::SettingTurboButton(const char* configPath, bool isGlobal)
    : m_isGlobal(isGlobal)
{
    snprintf(m_configPath, sizeof(m_configPath), "%s", configPath);
}

tsl::elm::Element* SettingTurboButton::createUI() {
    const char* subtitle = m_isGlobal ? "全局配置" : "独立配置";
    
    // 使用 HeaderOverlayFrame 以支持自定义头部
    auto frame = new tsl::elm::HeaderOverlayFrame(97);
    
    // 添加自定义头部（包含标题、副标题和底部右键提示）
    frame->setHeader(new tsl::elm::CustomDrawer([subtitle](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
        renderer->drawString("连发按键", false, 20, 50+2, 32, renderer->a(tsl::defaultOverlayColor));
        renderer->drawString(subtitle, false, 20, 50+23, 15, renderer->a(tsl::bannerVersionTextColor));
        renderer->drawString("\uE0EE  重置", false, 270, 693, 23, renderer->a(tsl::style::color::ColorText));
    }));
    auto list = new tsl::elm::List();
    list->addItem(new tsl::elm::CategoryHeader("关闭 → 按住 → 切换 → 停止键 → 功能键"));
    m_buttonItems.clear();
    for (const auto& btn : TurboConfig::Buttons) {
        const char* icon = HidHelper::getIconByMask(btn.flag);
        std::string buttonName = std::string(btn.name) + "  " + icon;
        TurboButtonMode mode = GetButtonMode(btn.flag);
        auto item = new tsl::elm::ListItem(buttonName, GetModeText(mode));
        item->setValueColor(GetModeColor(mode));
        item->setClickListener([this, btn](u64 keys) {
            if (!(keys & HidNpadButton_A)) return false;

            switch (GetButtonMode(btn.flag)) {
                case TurboButtonMode::Off:
                    s_HoldTurboButtons |= btn.flag;
                    s_ToggleTurboButtons &= ~btn.flag;
                    s_StopTurboButton &= ~btn.flag;
                    s_TriggerTurboButton &= ~btn.flag;
                    break;
                case TurboButtonMode::Hold:
                    s_HoldTurboButtons &= ~btn.flag;
                    s_ToggleTurboButtons |= btn.flag;
                    s_StopTurboButton &= ~btn.flag;
                    s_TriggerTurboButton &= ~btn.flag;
                    break;
                case TurboButtonMode::Toggle:
                    s_HoldTurboButtons &= ~btn.flag;
                    s_ToggleTurboButtons &= ~btn.flag;
                    s_StopTurboButton = btn.flag; // 新停止键自动替换旧停止键
                    s_TriggerTurboButton &= ~btn.flag;
                    break;
                case TurboButtonMode::Stop:
                    s_StopTurboButton = 0;
                    s_TriggerTurboButton = btn.flag; // 新功能键自动替换旧功能键
                    break;
                case TurboButtonMode::Trigger:
                    s_TriggerTurboButton = 0;
                    break;
            }

            SaveButtonMasks(m_configPath);
            g_ipcManager.sendReloadAutoFireCommand();
            Refresh::RefrRequest(Refresh::MainMenu);
            RefreshButtonItems();
            return true;
        });
        m_buttonItems.push_back(item);
        list->addItem(item);
    }
    
    frame->setContent(list);
    return frame;
}

void SettingTurboButton::update() {
    if (Refresh::RefrConsume(Refresh::TurboButton)) {
        RefreshButtonItems();
    }
}

void SettingTurboButton::RefreshButtonItems() {
    for (int i = 0; i < TurboConfig::ButtonCount && i < static_cast<int>(m_buttonItems.size()); i++) {
        TurboButtonMode mode = GetButtonMode(TurboConfig::Buttons[i].flag);
        m_buttonItems[i]->setValue(GetModeText(mode));
        m_buttonItems[i]->setValueColor(GetModeColor(mode));
    }
}

bool SettingTurboButton::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, 
    HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
    
    // 监控右键，执行重置功能
    if (keysDown & HidNpadButton_Right) {
        s_HoldTurboButtons = 0;
        s_ToggleTurboButtons = 0;
        s_StopTurboButton = 0;
        s_TriggerTurboButton = 0;
        SaveButtonMasks(m_configPath);
        Refresh::RefrRequest(Refresh::TurboButton);
        g_ipcManager.sendReloadAutoFireCommand();
        return true;
    }
    
    return false;
}
