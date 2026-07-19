#include "touch_setting.hpp"
#include "game.hpp"
#include "hiddata.hpp"
#include "ini_helper.hpp"
#include "ipc.hpp"
#include "refresh.hpp"
#include <cstring>

namespace {
    constexpr const char* CONFIG_PATH = "/config/KeyX/config.ini";
    constexpr const char* BUTTON_NAMES[] = {
        "A", "B", "X", "Y", "Up", "Down", "Left", "Right",
        "L", "R", "ZL", "ZR", "StickL", "StickR", "Start", "Select"
    };

    void CopyButtonName(char (&destination)[8], const std::string& source) {
        std::strncpy(destination, source.c_str(), sizeof(destination) - 1);
        destination[sizeof(destination) - 1] = '\0';
    }
}

SettingTouch::SettingTouch() {
    m_DefaultAuto = IniHelper::getBool("TOUCH", "defaultautoenable", false, CONFIG_PATH);
}

tsl::elm::Element* SettingTouch::createUI() {
    auto frame = new tsl::elm::OverlayFrame("触摸映射", "配置屏幕左右区域");
    auto list = new tsl::elm::List();

    list->addItem(new tsl::elm::CategoryHeader(" 调整触摸区域映射"));

    auto globalItem = new tsl::elm::ListItem("全局配置", ">");
    globalItem->setClickListener([](u64 keys) {
        if (!(keys & HidNpadButton_A)) return false;
        tsl::changeTo<SettingTouchConfig>(true, 0);
        return true;
    });
    list->addItem(globalItem);

    u64 currentTitleId = GameMonitor::getCurrentTitleId();
    m_IndependentItem = new tsl::elm::ListItem("独立配置", currentTitleId == 0 ? "\uE14C" : ">");
    m_IndependentItem->setClickListener([](u64 keys) {
        if (!(keys & HidNpadButton_A)) return false;
        u64 titleId = GameMonitor::getCurrentTitleId();
        if (titleId != 0) tsl::changeTo<SettingTouchConfig>(false, titleId);
        return true;
    });
    list->addItem(m_IndependentItem);

    list->addItem(new tsl::elm::CategoryHeader(" 基础功能设置"));
    list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32, s32) {
        renderer->drawString("  自动启用触摸映射（仅全局配置）", false,
                             x + 5, y + 13, 16, tsl::highlightColor2);
    }), 30);

    auto defaultItem = new tsl::elm::ListItem("默认触摸映射", m_DefaultAuto ? "开" : "关");
    defaultItem->setClickListener([this, defaultItem](u64 keys) {
        if (!(keys & HidNpadButton_A)) return false;
        m_DefaultAuto = !m_DefaultAuto;
        IniHelper::setBool("TOUCH", "defaultautoenable", m_DefaultAuto, CONFIG_PATH);
        defaultItem->setValue(m_DefaultAuto ? "开" : "关");
        return true;
    });
    list->addItem(defaultItem);

    list->addItem(new tsl::elm::CategoryHeader(" 使用说明"));
    list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32, s32) {
        renderer->drawString("  左右区域可同时使用，落点决定区域", false,
                             x + 5, y + 13, 16, tsl::highlightColor2);
    }), 30);

    frame->setContent(list);
    return frame;
}

void SettingTouch::update() {
    if (Refresh::RefrConsume(Refresh::OnShow)) {
        m_IndependentItem->setValue(GameMonitor::getCurrentTitleId() == 0 ? "\uE14C" : ">");
    }
}

SettingTouchConfig::SettingTouchConfig(bool isGlobal, u64 currentTitleId)
    : m_IsGlobal(isGlobal) {
    if (m_IsGlobal) {
        std::snprintf(m_ConfigPath, sizeof(m_ConfigPath), "%s", CONFIG_PATH);
    } else {
        GameMonitor::getTitleIdGameName(currentTitleId, m_GameName);
        std::snprintf(m_ConfigPath, sizeof(m_ConfigPath),
                      "/config/KeyX/GameConfig/%016lX.ini", currentTitleId);
    }
    LoadConfig();
}

void SettingTouchConfig::LoadConfig() {
    CopyButtonName(m_LeftButton,
        IniHelper::getString("TOUCH", "leftbutton", "B", m_ConfigPath));
    CopyButtonName(m_RightButton,
        IniHelper::getString("TOUCH", "rightbutton", "A", m_ConfigPath));
}

tsl::elm::Element* SettingTouchConfig::createUI() {
    const char* title = m_IsGlobal ? "全局配置" : "独立配置";
    const char* subtitle = m_IsGlobal ? "设置全局触摸按键" : m_GameName;
    auto frame = new tsl::elm::OverlayFrame(title, subtitle);
    auto list = new tsl::elm::List();

    list->addItem(new tsl::elm::CategoryHeader(" 屏幕区域映射"));

    m_LeftItem = new tsl::elm::ListItem("左半屏", std::string("按键  ") + HidHelper::getButtonIcon(m_LeftButton));
    m_LeftItem->setClickListener([this](u64 keys) {
        if (!(keys & HidNpadButton_A)) return false;
        tsl::changeTo<SettingTouchButton>(m_ConfigPath, true, m_LeftButton, m_RightButton);
        return true;
    });
    list->addItem(m_LeftItem);

    m_RightItem = new tsl::elm::ListItem("右半屏", std::string("按键  ") + HidHelper::getButtonIcon(m_RightButton));
    m_RightItem->setClickListener([this](u64 keys) {
        if (!(keys & HidNpadButton_A)) return false;
        tsl::changeTo<SettingTouchButton>(m_ConfigPath, false, m_RightButton, m_LeftButton);
        return true;
    });
    list->addItem(m_RightItem);

    list->addItem(new tsl::elm::CategoryHeader(" 规则"));
    list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32, s32) {
        renderer->drawString("  两个区域不能映射为同一个按键", false,
                             x + 5, y + 13, 16, tsl::highlightColor2);
    }), 30);
    list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32, s32) {
        renderer->drawString("  目标按键会沿用该按键的连发模式", false,
                             x + 5, y + 13, 16, tsl::highlightColor2);
    }), 30);

    frame->setContent(list);
    return frame;
}

void SettingTouchConfig::update() {
    if (Refresh::RefrConsume(Refresh::TouchConfig)) RefreshItems();
}

void SettingTouchConfig::RefreshItems() {
    LoadConfig();
    m_LeftItem->setValue(std::string("按键  ") + HidHelper::getButtonIcon(m_LeftButton));
    m_RightItem->setValue(std::string("按键  ") + HidHelper::getButtonIcon(m_RightButton));
}

SettingTouchButton::SettingTouchButton(const char* configPath, bool isLeft,
                                       const char* currentButton, const char* otherButton)
    : m_IsLeft(isLeft) {
    std::snprintf(m_ConfigPath, sizeof(m_ConfigPath), "%s", configPath);
    std::snprintf(m_CurrentButton, sizeof(m_CurrentButton), "%s", currentButton);
    std::snprintf(m_OtherButton, sizeof(m_OtherButton), "%s", otherButton);
}

tsl::elm::Element* SettingTouchButton::createUI() {
    auto frame = new tsl::elm::OverlayFrame(
        m_IsLeft ? "左半屏按键" : "右半屏按键", "选择目标按键");
    auto list = new tsl::elm::List();
    list->addItem(new tsl::elm::CategoryHeader(" 可用按键"));

    for (const char* buttonName : BUTTON_NAMES) {
        if (std::strcmp(buttonName, m_OtherButton) == 0) continue;
        bool isCurrent = std::strcmp(buttonName, m_CurrentButton) == 0;
        auto item = new tsl::elm::ListItem(
            isCurrent ? "当前" : "", std::string("按键  ") + HidHelper::getButtonIcon(buttonName));
        item->setClickListener([this, buttonName](u64 keys) {
            if (!(keys & HidNpadButton_A)) return false;
            IniHelper::setString("TOUCH", m_IsLeft ? "leftbutton" : "rightbutton",
                                 buttonName, m_ConfigPath);
            g_ipcManager.sendReloadTouchCommand();
            Refresh::RefrRequest(Refresh::TouchConfig);
            tsl::goBack();
            return true;
        });
        list->addItem(item);
    }

    list->jumpToItem("当前", "");
    frame->setContent(list);
    return frame;
}
