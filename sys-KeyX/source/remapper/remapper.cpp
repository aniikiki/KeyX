#include "remapper.hpp"
#include <cstring>
#include "minIni.h"
#include "switch_model.hpp"
#include "log.h"

// 映射状态标志
static bool s_MappingEnabled = false;

// 按键名称表（用于配置读取）
static constexpr const char* BUTTON_NAMES[] = {
    "A", "B", "X", "Y",
    "Up", "Down", "Left", "Right",
    "L", "R", "ZL", "ZR",
    "StickL", "StickR", "Start", "Select", "Capture"
};

// 按键信息（通用）
struct ButtonInfo {
    const char* name;
    HidcfgDigitalButtonAssignment value;
};

// 统一的按键表
static constexpr ButtonInfo BUTTONS[] = {
    {"A",      HidcfgDigitalButtonAssignment_A},
    {"B",      HidcfgDigitalButtonAssignment_B},
    {"X",      HidcfgDigitalButtonAssignment_X},
    {"Y",      HidcfgDigitalButtonAssignment_Y},
    {"StickL", HidcfgDigitalButtonAssignment_StickL},
    {"StickR", HidcfgDigitalButtonAssignment_StickR},
    {"L",      HidcfgDigitalButtonAssignment_L},
    {"R",      HidcfgDigitalButtonAssignment_R},
    {"ZL",     HidcfgDigitalButtonAssignment_ZL},
    {"ZR",     HidcfgDigitalButtonAssignment_ZR},
    {"Select", HidcfgDigitalButtonAssignment_Select},
    {"Start",  HidcfgDigitalButtonAssignment_Start},
    {"Left",   HidcfgDigitalButtonAssignment_Left},
    {"Up",     HidcfgDigitalButtonAssignment_Up},
    {"Right",  HidcfgDigitalButtonAssignment_Right},
    {"Down",   HidcfgDigitalButtonAssignment_Down},
    {"Capture", HidcfgDigitalButtonAssignment_CaptureButton},
    {"Home",    HidcfgDigitalButtonAssignment_HomeButton},
};

// 查找按键（返回-1表示无效）
int ButtonRemapper::FindButton(const char* name) {
    for (const auto& btn : BUTTONS) {
        if (strcmp(btn.name, name) == 0) return btn.value;
    }
    return -1; // 无效按键
}

// 应用映射（通用）
template<typename ConfigType>
void ButtonRemapper::ApplyMapping(ConfigType& config, const char* from, const char* to) {
    int to_value = FindButton(to);
    if (to_value == -1) return; // 无效的目标按键，跳过
    
    // 检查源按键是否有效，并应用映射
    if (strcmp(from, "A") == 0) config.hardware_button_a = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "B") == 0) config.hardware_button_b = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "X") == 0) config.hardware_button_x = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Y") == 0) config.hardware_button_y = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "StickL") == 0) config.hardware_button_stick_l = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "StickR") == 0) config.hardware_button_stick_r = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "L") == 0) config.hardware_button_l = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "R") == 0) config.hardware_button_r = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "ZL") == 0) config.hardware_button_zl = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "ZR") == 0) config.hardware_button_zr = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Select") == 0) config.hardware_button_select = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Start") == 0) config.hardware_button_start = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Left") == 0) config.hardware_button_left = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Up") == 0) config.hardware_button_up = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Right") == 0) config.hardware_button_right = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Down") == 0) config.hardware_button_down = (HidcfgDigitalButtonAssignment)to_value;
}

// 应用映射到 Embedded 手柄
Result ButtonRemapper::ApplyMappingsToEmbedded(HidsysUniquePadId pad_id, const std::vector<ButtonMapping>& mappings,
                                               const std::vector<ButtonMapping>& captureMappings) {
    HidcfgButtonConfigEmbedded config;
    Result rc = hidsysGetHidButtonConfigEmbedded(pad_id, &config);
    if (R_FAILED(rc)) {
        log_warning("MAPPING pad=%016lX get_embedded_rc=%08X",
                    static_cast<unsigned long>(pad_id.id), rc);
        return rc;
    }
    
    for (const auto& mapping : mappings) {
        ApplyMapping(config, mapping.source, mapping.target);
    }
    
    for (const auto& mapping : captureMappings) {
        if (strcmp(mapping.source, "Capture") == 0) {
            config.hardware_button_capture = FindButton(mapping.target);
        } else {
            ApplyMapping(config, mapping.source, mapping.target);
        }
    }

    rc = hidsysSetHidButtonConfigEmbedded(pad_id, &config);
    if (!captureMappings.empty()) {
        HidcfgButtonConfigEmbedded applied{};
        const Result readRc = hidsysGetHidButtonConfigEmbedded(pad_id, &applied);
        log_info("CAPTURE_MAP pad=%016lX set_rc=%08X read_rc=%08X requested=%u actual=%u",
                 static_cast<unsigned long>(pad_id.id), rc, readRc,
                 config.hardware_button_capture, applied.hardware_button_capture);
    }
    return rc;
}

// 应用映射到 Full 手柄
Result ButtonRemapper::ApplyMappingsToFull(HidsysUniquePadId pad_id, const std::vector<ButtonMapping>& mappings) {
    HidcfgButtonConfigFull config;
    Result rc = hidsysGetHidButtonConfigFull(pad_id, &config);
    if (R_FAILED(rc)) return rc;
    
    for (const auto& mapping : mappings) {
        ApplyMapping(config, mapping.source, mapping.target);
    }
    
    return hidsysSetHidButtonConfigFull(pad_id, &config);
}

// 应用单个映射到左 JoyCon（只处理左侧按键）
void ButtonRemapper::ApplyMappingLeft(HidcfgButtonConfigLeft& config, const char* from, const char* to) {
    int to_value = FindButton(to);
    if (to_value == -1) return;
    
    // 只处理左 JoyCon 拥有的按键
    if (strcmp(from, "Left") == 0) config.hardware_button_left = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Up") == 0) config.hardware_button_up = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Right") == 0) config.hardware_button_right = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Down") == 0) config.hardware_button_down = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "StickL") == 0) config.hardware_button_stick_l = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "L") == 0) config.hardware_button_l = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "ZL") == 0) config.hardware_button_zl = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Select") == 0) config.hardware_button_select = (HidcfgDigitalButtonAssignment)to_value;
    // 忽略右侧按键（A/B/X/Y/R/ZR/StickR/Start 等）
}

// 应用映射到左 JoyCon
Result ButtonRemapper::ApplyMappingsToLeft(HidsysUniquePadId pad_id, const std::vector<ButtonMapping>& mappings) {
    HidcfgButtonConfigLeft config;
    Result rc = hidsysGetHidButtonConfigLeft(pad_id, &config);
    if (R_FAILED(rc)) return rc;
    
    for (const auto& mapping : mappings) {
        ApplyMappingLeft(config, mapping.source, mapping.target);
    }
    
    return hidsysSetHidButtonConfigLeft(pad_id, &config);
}

// 应用单个映射到右 JoyCon（只处理右侧按键）
void ButtonRemapper::ApplyMappingRight(HidcfgButtonConfigRight& config, const char* from, const char* to) {
    int to_value = FindButton(to);
    if (to_value == -1) return;
    
    // 只处理右 JoyCon 拥有的按键
    if (strcmp(from, "A") == 0) config.hardware_button_a = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "B") == 0) config.hardware_button_b = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "X") == 0) config.hardware_button_x = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Y") == 0) config.hardware_button_y = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "StickR") == 0) config.hardware_button_stick_r = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "R") == 0) config.hardware_button_r = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "ZR") == 0) config.hardware_button_zr = (HidcfgDigitalButtonAssignment)to_value;
    else if (strcmp(from, "Start") == 0) config.hardware_button_start = (HidcfgDigitalButtonAssignment)to_value;
    // 忽略左侧按键（十字键/L/ZL/StickL/Select 等）
}

// 应用映射到右 JoyCon
Result ButtonRemapper::ApplyMappingsToRight(HidsysUniquePadId pad_id, const std::vector<ButtonMapping>& mappings) {
    HidcfgButtonConfigRight config;
    Result rc = hidsysGetHidButtonConfigRight(pad_id, &config);
    if (R_FAILED(rc)) return rc;
    
    for (const auto& mapping : mappings) {
        ApplyMappingRight(config, mapping.source, mapping.target);
    }
    
    return hidsysSetHidButtonConfigRight(pad_id, &config);
}

// 从配置文件加载映射关系
void ButtonRemapper::LoadMappingsFromConfig(const char* config_path, std::vector<ButtonMapping>& mappings,
                                            std::vector<ButtonMapping>& captureMappings) {
    for (const char* source : BUTTON_NAMES) {
        ButtonMapping mapping;
        mapping.source = source;
        ini_gets("MAPPING", source, source, mapping.target, sizeof(mapping.target), config_path);
        if (strcmp(source, mapping.target) == 0 || FindButton(mapping.target) == -1) continue;
        if (strcmp(mapping.target, "Home") == 0 && strcmp(source, "Capture") != 0) continue;

        if (strcmp(source, "Capture") == 0 || strcmp(mapping.target, "Capture") == 0) {
            if (IsSwitchLite()) captureMappings.push_back(mapping);
        } else {
            mappings.push_back(mapping);
        }
    }
}

Result ButtonRemapper::SetMapping(const char* config_path) {
    // 读取配置到局部 vector
    std::vector<ButtonMapping> mappings;
    std::vector<ButtonMapping> captureMappings;
    LoadMappingsFromConfig(config_path, mappings, captureMappings);
    log_info("MAPPING config=%s regular=%lu capture=%lu", config_path,
             static_cast<unsigned long>(mappings.size()),
             static_cast<unsigned long>(captureMappings.size()));

    // 如果是空的代表不需要修改配置，直接恢复
    if (mappings.empty() && captureMappings.empty()) return RestoreMapping();

    // 获取所有手柄的ID
    HidsysUniquePadId pad_ids[8];
    s32 total = 0;
    Result rc = hidsysGetUniquePadIds(pad_ids, 8, &total);
    if (R_FAILED(rc) || total == 0) {
        log_warning("MAPPING enumerate rc=%08X count=%d", rc, total);
        return rc;
    }
    
    Result firstError = 0;
    // 遍历所有手柄
    for (s32 i = 0; i < total; i++) {
        // 获取手柄类型
        HidsysUniquePadType pad_type;
        rc = hidsysGetUniquePadType(pad_ids[i], &pad_type);
        if (R_FAILED(rc)) {
            log_warning("MAPPING pad=%016lX type_rc=%08X",
                        static_cast<unsigned long>(pad_ids[i].id), rc);
            continue;
        }
        if (!captureMappings.empty()) {
            log_info("CAPTURE_MAP pad=%016lX type=%u",
                     static_cast<unsigned long>(pad_ids[i].id), static_cast<unsigned int>(pad_type));
        }
        
        // 根据手柄类型应用映射
        if (pad_type == HidsysUniquePadType_Embedded ||
            pad_type == HidsysUniquePadType_DebugPadController) {
            // 两种类型共用内置手柄配置，截图规则已经过 Lite 机型检查。
            rc = ApplyMappingsToEmbedded(pad_ids[i], mappings, captureMappings);
        } else if (mappings.empty()) {
            continue;
        } else if (pad_type == HidsysUniquePadType_FullKeyController) {
            rc = ApplyMappingsToFull(pad_ids[i], mappings);
        } else if (pad_type == HidsysUniquePadType_LeftController) {
            rc = ApplyMappingsToLeft(pad_ids[i], mappings);
        } else if (pad_type == HidsysUniquePadType_RightController) {
            rc = ApplyMappingsToRight(pad_ids[i], mappings);
        } else {
            continue;
        }

        if (R_SUCCEEDED(rc)) s_MappingEnabled = true;
        else if (R_SUCCEEDED(firstError)) firstError = rc;
    }
    
    return firstError;
}



Result ButtonRemapper::RestoreMapping() {
    // 检查映射是否已启用，未启用则无需恢复
    if (!s_MappingEnabled) return 0;
    
    // 恢复所有手柄的默认按键配置
    Result rc = hidsysSetAllDefaultButtonConfig();
    if (R_FAILED(rc)) return rc;
    
    // 禁用自定义按键配置（使用 0 作为 AppletResourceUserId）
    rc = hidsysSetAllCustomButtonConfigEnabled(0, false);
    if (R_SUCCEEDED(rc)) s_MappingEnabled = false;
    
    return rc;
}

