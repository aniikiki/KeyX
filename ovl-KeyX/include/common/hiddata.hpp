#pragma once
#include <switch.h>
#include <cstring>
#include "switch_model.hpp"

/**
 * HID 按键数据工具集
 * 提供按键图标、名称转换、掩码查询等常用功能
 */

// ===== 按键 Unicode 图标定义 =====
namespace ButtonIcon {
    constexpr const char* A      = "";  // A 按键
    constexpr const char* B      = "";  // B 按键
    constexpr const char* X      = "";  // X 按键
    constexpr const char* Y      = "";  // Y 按键
    constexpr const char* Up     = "";  // 方向键上
    constexpr const char* Down   = "";  // 方向键下
    constexpr const char* Left   = "";  // 方向键左
    constexpr const char* Right  = "";  // 方向键右
    constexpr const char* L      = "";  // L 按键
    constexpr const char* R      = "";  // R 按键
    constexpr const char* ZL     = "";  // ZL 按键
    constexpr const char* ZR     = "";  // ZR 按键
    constexpr const char* StickL = "";  // 左摇杆按下
    constexpr const char* StickR = "";  // 右摇杆按下
    constexpr const char* Start  = "";  // Start/Plus
    constexpr const char* Select = "";  // Select/Minus
    constexpr const char* Capture = "截图";
    constexpr const char* Home = "主页";
    constexpr const char* Unknown = ""; // 未知按键
}

// ===== 按键名称常量 =====
namespace ButtonName {
    constexpr const char* A      = "A";
    constexpr const char* B      = "B";
    constexpr const char* X      = "X";
    constexpr const char* Y      = "Y";
    constexpr const char* Up     = "Up";
    constexpr const char* Down   = "Down";
    constexpr const char* Left   = "Left";
    constexpr const char* Right  = "Right";
    constexpr const char* L      = "L";
    constexpr const char* R      = "R";
    constexpr const char* ZL     = "ZL";
    constexpr const char* ZR     = "ZR";
    constexpr const char* StickL = "StickL";
    constexpr const char* StickR = "StickR";
    constexpr const char* Start  = "Start";
    constexpr const char* Select = "Select";
}

// 按键位标志别名（引用系统定义）
enum ButtonMask : u64 {
    BTN_A      = HidNpadButton_A,      
    BTN_B      = HidNpadButton_B,
    BTN_X      = HidNpadButton_X,
    BTN_Y      = HidNpadButton_Y,
    BTN_STICKL = HidNpadButton_StickL,
    BTN_STICKR = HidNpadButton_StickR,
    BTN_L      = HidNpadButton_L,
    BTN_R      = HidNpadButton_R,
    BTN_ZL     = HidNpadButton_ZL,
    BTN_ZR     = HidNpadButton_ZR,
    BTN_START  = HidNpadButton_Plus,
    BTN_SELECT = HidNpadButton_Minus,
    BTN_LEFT   = HidNpadButton_Left,
    BTN_UP     = HidNpadButton_Up,
    BTN_RIGHT  = HidNpadButton_Right,
    BTN_DOWN   = HidNpadButton_Down
};


// ===== 按键映射相关定义 =====
namespace MappingDef {

    // 按键映射结构体
    struct ButtonMapping {
        const char* source;   
        char target[8];       
    };

    // 支持映射的按键总数
    constexpr int BUTTON_COUNT = 17;

    inline int GetButtonCount() {
        return IsSwitchLite() ? BUTTON_COUNT : BUTTON_COUNT - 1;
    }

    inline const char* GetDisplayTarget(const ButtonMapping& mapping) {
        const bool isHome = strcmp(mapping.target, "Home") == 0;
        if (isHome && strcmp(mapping.source, "Capture") != 0) return mapping.source;
        if (!IsSwitchLite() && (strcmp(mapping.target, "Capture") == 0 || isHome)) return mapping.source;
        return mapping.target;
    }

}

// ===== 预设组合键定义 =====
namespace MacroHotkeys {

    constexpr u64 Hotkeys[] = {
        BTN_ZL | BTN_ZR | BTN_A,
        BTN_ZL | BTN_ZR | BTN_B,
        BTN_ZL | BTN_ZR | BTN_X,
        BTN_ZL | BTN_ZR | BTN_Y,
        BTN_ZL | BTN_ZR | BTN_R,
        BTN_ZL | BTN_ZR | BTN_L,
        BTN_ZL | BTN_ZR | BTN_UP,
        BTN_ZL | BTN_ZR | BTN_DOWN,
        BTN_ZL | BTN_ZR | BTN_LEFT,
        BTN_ZL | BTN_ZR | BTN_RIGHT
    };
    
    constexpr int HotkeysCount = sizeof(Hotkeys) / sizeof(Hotkeys[0]);

}

// ===== 连发按键配置 =====
namespace TurboConfig {
    
    // 连发按键配置结构体
    struct ButtonConfig {
        const char* name; 
        u64 flag;          
    };
    
    // 支持连发的按键列表
    constexpr ButtonConfig Buttons[] = {
        {"按键", BTN_A},
        {"按键", BTN_B},
        {"按键", BTN_X},
        {"按键", BTN_Y},
        {"按键", BTN_L},
        {"按键", BTN_R},
        {"按键", BTN_ZL},
        {"按键", BTN_ZR},
        {"方向键上", BTN_UP},
        {"方向键下", BTN_DOWN},
        {"方向键左", BTN_LEFT},
        {"方向键右", BTN_RIGHT}
    };
    
    constexpr int ButtonCount = sizeof(Buttons) / sizeof(Buttons[0]);
}

// ===== 按键工具函数 =====
namespace HidHelper {
    /**
     * 根据按键名称获取对应的 Unicode 图标
     * @param buttonName 按键名称字符串（如 "A", "B", "Up"）
     * @return 对应的 Unicode 图标字符串，未知按键返回 ButtonIcon::Unknown
     */
    inline const char* getButtonIcon(const char* buttonName) {
        if (strcmp(buttonName, "A") == 0)      return ButtonIcon::A;
        if (strcmp(buttonName, "B") == 0)      return ButtonIcon::B;
        if (strcmp(buttonName, "X") == 0)      return ButtonIcon::X;
        if (strcmp(buttonName, "Y") == 0)      return ButtonIcon::Y;
        if (strcmp(buttonName, "Up") == 0)     return ButtonIcon::Up;
        if (strcmp(buttonName, "Down") == 0)   return ButtonIcon::Down;
        if (strcmp(buttonName, "Left") == 0)   return ButtonIcon::Left;
        if (strcmp(buttonName, "Right") == 0)  return ButtonIcon::Right;
        if (strcmp(buttonName, "L") == 0)      return ButtonIcon::L;
        if (strcmp(buttonName, "R") == 0)      return ButtonIcon::R;
        if (strcmp(buttonName, "ZL") == 0)     return ButtonIcon::ZL;
        if (strcmp(buttonName, "ZR") == 0)     return ButtonIcon::ZR;
        if (strcmp(buttonName, "StickL") == 0) return ButtonIcon::StickL;
        if (strcmp(buttonName, "StickR") == 0) return ButtonIcon::StickR;
        if (strcmp(buttonName, "Start") == 0)  return ButtonIcon::Start;
        if (strcmp(buttonName, "Select") == 0) return ButtonIcon::Select;
        if (strcmp(buttonName, "Capture") == 0) return ButtonIcon::Capture;
        if (strcmp(buttonName, "Home") == 0) return ButtonIcon::Home;
        return ButtonIcon::Unknown;
    }
    
    /**
     * 根据按键名称获取对应的 HidNpadButton 掩码值
     * @param buttonName 按键名称字符串（如 "A", "B", "Up"）
     * @return 对应的按键掩码值，未知按键返回 0
     */
    inline u64 getButtonFlag(const char* buttonName) {
        if (strcmp(buttonName, "A") == 0)      return BTN_A;
        if (strcmp(buttonName, "B") == 0)      return BTN_B;
        if (strcmp(buttonName, "X") == 0)      return BTN_X;
        if (strcmp(buttonName, "Y") == 0)      return BTN_Y;
        if (strcmp(buttonName, "Up") == 0)     return BTN_UP;
        if (strcmp(buttonName, "Down") == 0)   return BTN_DOWN;
        if (strcmp(buttonName, "Left") == 0)   return BTN_LEFT;
        if (strcmp(buttonName, "Right") == 0)  return BTN_RIGHT;
        if (strcmp(buttonName, "L") == 0)      return BTN_L;
        if (strcmp(buttonName, "R") == 0)      return BTN_R;
        if (strcmp(buttonName, "ZL") == 0)     return BTN_ZL;
        if (strcmp(buttonName, "ZR") == 0)     return BTN_ZR;
        if (strcmp(buttonName, "StickL") == 0) return BTN_STICKL;
        if (strcmp(buttonName, "StickR") == 0) return BTN_STICKR;
        if (strcmp(buttonName, "Start") == 0)  return BTN_START;
        if (strcmp(buttonName, "Select") == 0) return BTN_SELECT;
        return 0;
    }
    
    /**
     * 根据 HidNpadButton 掩码获取对应的 Unicode 图标
     * @param buttonMask 单个按键的掩码值
     * @return 对应的 Unicode 图标字符串，未知按键返回 ButtonIcon::Unknown
     */
    inline const char* getIconByMask(u64 buttonMask) {
        if (buttonMask == BTN_A)     return ButtonIcon::A;
        if (buttonMask == BTN_B)     return ButtonIcon::B;
        if (buttonMask == BTN_X)     return ButtonIcon::X;
        if (buttonMask == BTN_Y)     return ButtonIcon::Y;
        if (buttonMask == BTN_UP)    return ButtonIcon::Up;
        if (buttonMask == BTN_DOWN)  return ButtonIcon::Down;
        if (buttonMask == BTN_LEFT)  return ButtonIcon::Left;
        if (buttonMask == BTN_RIGHT) return ButtonIcon::Right;
        if (buttonMask == BTN_L)     return ButtonIcon::L;
        if (buttonMask == BTN_R)     return ButtonIcon::R;
        if (buttonMask == BTN_ZL)    return ButtonIcon::ZL;
        if (buttonMask == BTN_ZR)    return ButtonIcon::ZR;
        if (buttonMask == BTN_STICKL) return ButtonIcon::StickL;
        if (buttonMask == BTN_STICKR) return ButtonIcon::StickR;
        if (buttonMask == BTN_START)  return ButtonIcon::Start;
        if (buttonMask == BTN_SELECT) return ButtonIcon::Select;
        return ButtonIcon::Unknown;
    }
    
    /**
     * 根据 HidNpadButton 掩码获取对应的按键名称
     * @param buttonMask 单个按键的掩码值
     * @return 对应的按键名称字符串，未知按键返回空字符串
     */
    inline const char* getNameByMask(u64 buttonMask) {
        if (buttonMask == BTN_A)     return ButtonName::A;
        if (buttonMask == BTN_B)     return ButtonName::B;
        if (buttonMask == BTN_X)     return ButtonName::X;
        if (buttonMask == BTN_Y)     return ButtonName::Y;
        if (buttonMask == BTN_UP)    return ButtonName::Up;
        if (buttonMask == BTN_DOWN)  return ButtonName::Down;
        if (buttonMask == BTN_LEFT)  return ButtonName::Left;
        if (buttonMask == BTN_RIGHT) return ButtonName::Right;
        if (buttonMask == BTN_L)     return ButtonName::L;
        if (buttonMask == BTN_R)     return ButtonName::R;
        if (buttonMask == BTN_ZL)    return ButtonName::ZL;
        if (buttonMask == BTN_ZR)    return ButtonName::ZR;
        if (buttonMask == BTN_STICKL) return ButtonName::StickL;
        if (buttonMask == BTN_STICKR) return ButtonName::StickR;
        if (buttonMask == BTN_START)  return ButtonName::Start;
        if (buttonMask == BTN_SELECT) return ButtonName::Select;
        return "";
    }

    /**
     * 根据组合键掩码生成图标字符串
     * @param mask 按键组合掩码
     * @param separator 分隔符（默认为 "+"）
     * @return 图标组合字符串，如 "🅐+🅑+🅧"
     */
    inline std::string getCombinedIcons(u64 mask, const char* separator = "+") {
        if (mask == 0) return "";
        
        struct ButtonMap { u64 flag; const char* icon; };
        const ButtonMap maps[] = {
            {BTN_ZL,    ButtonIcon::ZL},
            {BTN_ZR,    ButtonIcon::ZR},
            {BTN_L,     ButtonIcon::L},
            {BTN_R,     ButtonIcon::R},
            {BTN_A,     ButtonIcon::A},
            {BTN_B,     ButtonIcon::B},
            {BTN_X,     ButtonIcon::X},
            {BTN_Y,     ButtonIcon::Y},
            {BTN_UP,    ButtonIcon::Up},
            {BTN_DOWN,  ButtonIcon::Down},
            {BTN_LEFT,  ButtonIcon::Left},
            {BTN_RIGHT, ButtonIcon::Right},
            {BTN_STICKL, ButtonIcon::StickL},
            {BTN_STICKR, ButtonIcon::StickR},
            {BTN_START,  ButtonIcon::Start},
            {BTN_SELECT, ButtonIcon::Select}
        };
        
        std::string result;
        for (const auto& m : maps) {
            if (mask & m.flag) {
                if (!result.empty()) result += separator;
                result += m.icon;
            }
        }
        return result;
    }
}
