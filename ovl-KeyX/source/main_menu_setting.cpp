#include "main_menu_setting.hpp"
#include "main_whitelist.hpp"
#include "turbo_setting.hpp"
#include "remap_setting.hpp"
#include "macro_setting.hpp"
#include "sysmodule.hpp"
#include "refresh.hpp"
#include "about.hpp"

constexpr const char* const descriptions[2][2] = {
    [0] = {
        [0] = "关",
        [1] = "关",
    },
    [1] = {
        [0] = "开",
        [1] = "开",
    },
};

SettingMenu::SettingMenu() {
    m_running = SysModuleManager::isRunning();
    m_hasFlag = SysModuleManager::hasBootFlag();
}

tsl::elm::Element* SettingMenu::createUI() {
    auto frame = new tsl::elm::HeaderOverlayFrame(97);
    frame->setHeader(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
        renderer->drawString("功能设置", false, 20, 50+2, 32, renderer->a(tsl::defaultOverlayColor));
        renderer->drawString("选择设置项  关于插件", false, 20, 50+23, 15, renderer->a(tsl::bannerVersionTextColor));
        renderer->drawString("  白名单", false, 270, 693, 23, renderer->a(tsl::style::color::ColorText));
    }));

    auto list = new tsl::elm::List();
    
    auto ItemBasicKeySetting = new tsl::elm::CategoryHeader(" 基础按键设置");
    list->addItem(ItemBasicKeySetting);

    auto listItemTurbo = new tsl::elm::ListItem("连发设置", ">");
    listItemTurbo->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<SettingTurbo>();
            return true;
        }
        return false;
    });
    list->addItem(listItemTurbo);

    auto listItemMapping = new tsl::elm::ListItem("映射设置", ">");
    listItemMapping->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<SettingRemap>();
            return true;
        }
        return false;
    });
    list->addItem(listItemMapping);

    auto listItemMacro = new tsl::elm::ListItem("脚本设置", ">");
    listItemMacro->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<SettingMacro>();
            return true;
        }
        return false;
    });
    list->addItem(listItemMacro);

    auto ItemModuleManager = new tsl::elm::CategoryHeader(" 功能模块管理 切换自启动 开关");
    list->addItem(ItemModuleManager);

    auto listItemTurboModule = new tsl::elm::ListItem("系统模块", descriptions[m_running][m_hasFlag]);
    listItemTurboModule->setClickListener([listItemTurboModule, this](u64 keys) {
        if (keys & HidNpadButton_A) {
            SysModuleManager::toggleModule();
            m_running = SysModuleManager::isRunning();
            listItemTurboModule->setValue(descriptions[m_running][m_hasFlag]);
            return true;
        }
        if (keys & HidNpadButton_Y) {
            SysModuleManager::toggleBootFlag();
            m_hasFlag = SysModuleManager::hasBootFlag();
            listItemTurboModule->setValue(descriptions[m_running][m_hasFlag]);
            return true;
        }
        return false;
    });
    list->addItem(listItemTurboModule);
        
    frame->setContent(list);
    return frame;
}

bool SettingMenu::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, 
    HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
    
    // 返回前申请刷新主页数据
    if (keysDown & HidNpadButton_B || keysDown & HidNpadButton_Left) {
        Refresh::RefrRequest(Refresh::MainMenu);
        tsl::goBack();
        return true;
    }

    if (keysDown & HidNpadButton_AnyRight) {
        tsl::changeTo<SettingWhitelist>();
        return true;
    }

    
    if (keysDown & HidNpadButton_Minus) {
        tsl::changeTo<AboutPlugin>();
        return true;
    }

    return false;
}
