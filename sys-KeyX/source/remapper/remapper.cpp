#include "remapper.hpp"
#include "minIni.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <vector>

namespace {
    template<typename Config>
    struct ButtonBinding {
        const char* name;
        HidcfgDigitalButtonAssignment assignment;
        u64 mask;
        u32 Config::* field;
    };

    template<typename Config>
    constexpr auto FullButtonBindings() {
        return std::array<ButtonBinding<Config>, 16>{{
            {"A", HidcfgDigitalButtonAssignment_A, HidNpadButton_A, &Config::hardware_button_a},
            {"B", HidcfgDigitalButtonAssignment_B, HidNpadButton_B, &Config::hardware_button_b},
            {"X", HidcfgDigitalButtonAssignment_X, HidNpadButton_X, &Config::hardware_button_x},
            {"Y", HidcfgDigitalButtonAssignment_Y, HidNpadButton_Y, &Config::hardware_button_y},
            {"Up", HidcfgDigitalButtonAssignment_Up, HidNpadButton_Up, &Config::hardware_button_up},
            {"Down", HidcfgDigitalButtonAssignment_Down, HidNpadButton_Down, &Config::hardware_button_down},
            {"Left", HidcfgDigitalButtonAssignment_Left, HidNpadButton_Left, &Config::hardware_button_left},
            {"Right", HidcfgDigitalButtonAssignment_Right, HidNpadButton_Right, &Config::hardware_button_right},
            {"L", HidcfgDigitalButtonAssignment_L, HidNpadButton_L, &Config::hardware_button_l},
            {"R", HidcfgDigitalButtonAssignment_R, HidNpadButton_R, &Config::hardware_button_r},
            {"ZL", HidcfgDigitalButtonAssignment_ZL, HidNpadButton_ZL, &Config::hardware_button_zl},
            {"ZR", HidcfgDigitalButtonAssignment_ZR, HidNpadButton_ZR, &Config::hardware_button_zr},
            {"StickL", HidcfgDigitalButtonAssignment_StickL, HidNpadButton_StickL, &Config::hardware_button_stick_l},
            {"StickR", HidcfgDigitalButtonAssignment_StickR, HidNpadButton_StickR, &Config::hardware_button_stick_r},
            {"Start", HidcfgDigitalButtonAssignment_Start, HidNpadButton_Plus, &Config::hardware_button_start},
            {"Select", HidcfgDigitalButtonAssignment_Select, HidNpadButton_Minus, &Config::hardware_button_select}
        }};
    }

    constexpr auto EMBEDDED_BUTTONS = FullButtonBindings<HidcfgButtonConfigEmbedded>();
    constexpr auto FULL_BUTTONS = FullButtonBindings<HidcfgButtonConfigFull>();
    constexpr std::array<ButtonBinding<HidcfgButtonConfigLeft>, 8> LEFT_BUTTONS{{
        {"Up", HidcfgDigitalButtonAssignment_Up, HidNpadButton_Up, &HidcfgButtonConfigLeft::hardware_button_up},
        {"Down", HidcfgDigitalButtonAssignment_Down, HidNpadButton_Down, &HidcfgButtonConfigLeft::hardware_button_down},
        {"Left", HidcfgDigitalButtonAssignment_Left, HidNpadButton_Left, &HidcfgButtonConfigLeft::hardware_button_left},
        {"Right", HidcfgDigitalButtonAssignment_Right, HidNpadButton_Right, &HidcfgButtonConfigLeft::hardware_button_right},
        {"L", HidcfgDigitalButtonAssignment_L, HidNpadButton_L, &HidcfgButtonConfigLeft::hardware_button_l},
        {"ZL", HidcfgDigitalButtonAssignment_ZL, HidNpadButton_ZL, &HidcfgButtonConfigLeft::hardware_button_zl},
        {"StickL", HidcfgDigitalButtonAssignment_StickL, HidNpadButton_StickL, &HidcfgButtonConfigLeft::hardware_button_stick_l},
        {"Select", HidcfgDigitalButtonAssignment_Select, HidNpadButton_Minus, &HidcfgButtonConfigLeft::hardware_button_select}
    }};
    constexpr std::array<ButtonBinding<HidcfgButtonConfigRight>, 8> RIGHT_BUTTONS{{
        {"A", HidcfgDigitalButtonAssignment_A, HidNpadButton_A, &HidcfgButtonConfigRight::hardware_button_a},
        {"B", HidcfgDigitalButtonAssignment_B, HidNpadButton_B, &HidcfgButtonConfigRight::hardware_button_b},
        {"X", HidcfgDigitalButtonAssignment_X, HidNpadButton_X, &HidcfgButtonConfigRight::hardware_button_x},
        {"Y", HidcfgDigitalButtonAssignment_Y, HidNpadButton_Y, &HidcfgButtonConfigRight::hardware_button_y},
        {"R", HidcfgDigitalButtonAssignment_R, HidNpadButton_R, &HidcfgButtonConfigRight::hardware_button_r},
        {"ZR", HidcfgDigitalButtonAssignment_ZR, HidNpadButton_ZR, &HidcfgButtonConfigRight::hardware_button_zr},
        {"StickR", HidcfgDigitalButtonAssignment_StickR, HidNpadButton_StickR, &HidcfgButtonConfigRight::hardware_button_stick_r},
        {"Start", HidcfgDigitalButtonAssignment_Start, HidNpadButton_Plus, &HidcfgButtonConfigRight::hardware_button_start}
    }};

    struct ButtonRule {
        HidcfgDigitalButtonAssignment source;
        HidcfgDigitalButtonAssignment target;
    };

    template<typename Config>
    struct SavedConfig {
        HidsysUniquePadId pad_id;
        Config original;
        Config applied;
    };

    struct ReverseMapping {
        u64 target = 0;
        u64 source = 0;
    };

    std::mutex s_MappingMutex;
    std::vector<SavedConfig<HidcfgButtonConfigEmbedded>> s_EmbeddedConfigs;
    std::vector<SavedConfig<HidcfgButtonConfigFull>> s_FullConfigs;
    std::vector<SavedConfig<HidcfgButtonConfigLeft>> s_LeftConfigs;
    std::vector<SavedConfig<HidcfgButtonConfigRight>> s_RightConfigs;
    std::array<ReverseMapping, 16> s_ReverseMappings{};

    std::vector<ButtonRule> LoadMappings(const char* config_path) {
        std::vector<ButtonRule> rules;
        for (const auto& source : EMBEDDED_BUTTONS) {
            char target[8];
            ini_gets("MAPPING", source.name, source.name, target, sizeof(target), config_path);
            for (const auto& destination : EMBEDDED_BUTTONS) {
                if (strcmp(target, destination.name) == 0 && source.assignment != destination.assignment) {
                    rules.push_back({source.assignment, destination.assignment});
                    break;
                }
            }
        }
        return rules;
    }

    template<typename Config, size_t Count>
    void UndoKeyXFields(Config& current, const SavedConfig<Config>& saved,
                       const std::array<ButtonBinding<Config>, Count>& bindings) {
        for (const auto& button : bindings) {
            if (saved.original.*button.field != saved.applied.*button.field &&
                current.*button.field == saved.applied.*button.field) {
                current.*button.field = saved.original.*button.field;
            }
        }
    }

    template<typename Config, size_t Count>
    Result ApplyMappings(HidsysUniquePadId pad_id, const std::vector<ButtonRule>& rules,
                         std::vector<SavedConfig<Config>>& saved_configs,
                         const std::array<ButtonBinding<Config>, Count>& bindings,
                         Result (*get_config)(HidsysUniquePadId, Config*),
                         Result (*set_config)(HidsysUniquePadId, const Config*)) {
        Config current{};
        Result rc = get_config(pad_id, &current);
        if (R_FAILED(rc)) return rc;

        auto saved = std::find_if(saved_configs.begin(), saved_configs.end(), [pad_id](const auto& entry) {
            return entry.pad_id.id == pad_id.id;
        });
        Config original = current;
        if (saved != saved_configs.end()) UndoKeyXFields(original, *saved, bindings);

        Config applied = original;
        for (const auto& button : bindings) {
            // 系统已重映射的实体按键保持原样。
            if (original.*button.field != button.assignment) continue;
            for (const auto& rule : rules) {
                if (rule.source == button.assignment) {
                    applied.*button.field = rule.target;
                    break;
                }
            }
        }

        if (memcmp(&current, &applied, sizeof(Config)) != 0) {
            rc = set_config(pad_id, &applied);
            if (R_FAILED(rc)) return rc;
        }

        if (memcmp(&original, &applied, sizeof(Config)) == 0) {
            if (saved != saved_configs.end()) saved_configs.erase(saved);
        } else if (saved == saved_configs.end()) {
            saved_configs.push_back({pad_id, original, applied});
        } else {
            saved->original = original;
            saved->applied = applied;
        }
        return 0;
    }

    template<typename Config, size_t Count>
    Result RestoreConfigs(std::vector<SavedConfig<Config>>& saved_configs,
                          const std::array<ButtonBinding<Config>, Count>& bindings,
                          Result (*get_config)(HidsysUniquePadId, Config*),
                          Result (*set_config)(HidsysUniquePadId, const Config*)) {
        Result first_error = 0;
        for (auto saved = saved_configs.begin(); saved != saved_configs.end();) {
            Config current{};
            Result rc = get_config(saved->pad_id, &current);
            if (R_SUCCEEDED(rc)) {
                Config restored = current;
                UndoKeyXFields(restored, *saved, bindings);
                if (memcmp(&current, &restored, sizeof(Config)) != 0) {
                    rc = set_config(saved->pad_id, &restored);
                }
            }
            if (R_SUCCEEDED(rc)) {
                saved = saved_configs.erase(saved);
            } else {
                // 保留失败项，下次暂停、退出或重新应用时继续恢复。
                if (R_SUCCEEDED(first_error)) first_error = rc;
                ++saved;
            }
        }
        return first_error;
    }

    void BuildReverseMapping(const HidcfgButtonConfigEmbedded& config) {
        s_ReverseMappings = {};
        for (size_t i = 0; i < EMBEDDED_BUTTONS.size(); ++i) {
            const auto& target = EMBEDDED_BUTTONS[i];
            u64 source = 0;
            for (const auto& candidate : EMBEDDED_BUTTONS) {
                if (config.*candidate.field == target.assignment &&
                    (source == 0 || candidate.mask == target.mask)) {
                    source = candidate.mask;
                }
            }
            if (source == 0 && config.hardware_button_capture == target.assignment) {
                source = HiddbgNpadButton_Capture;
            }
            if (source != 0 && source != target.mask) {
                s_ReverseMappings[i] = {target.mask, source};
            }
        }
    }

    Result RefreshReverseMappingLocked() {
        s_ReverseMappings = {};
        HidsysUniquePadId pad_ids[8];
        s32 total = 0;
        Result rc = hidsysGetUniquePadIds(pad_ids, 8, &total);
        if (R_FAILED(rc)) return rc;

        HidcfgButtonConfigEmbedded config{};
        bool have_embedded = false;
        Result first_error = 0;
        for (s32 i = 0; i < total; ++i) {
            HidsysUniquePadType type;
            rc = hidsysGetUniquePadType(pad_ids[i], &type);
            if (R_SUCCEEDED(rc) &&
                (type == HidsysUniquePadType_DebugPadController ||
                 (type == HidsysUniquePadType_Embedded && !have_embedded))) {
                HidcfgButtonConfigEmbedded candidate{};
                rc = hidsysGetHidButtonConfigEmbedded(pad_ids[i], &candidate);
                if (R_SUCCEEDED(rc)) {
                    config = candidate;
                    have_embedded = true;
                    if (type == HidsysUniquePadType_DebugPadController) break;
                }
            }
            if (R_FAILED(rc) && R_SUCCEEDED(first_error)) first_error = rc;
        }
        if (have_embedded) BuildReverseMapping(config);
        return first_error;
    }

    Result RestoreMappingsLocked() {
        const Result results[] = {
            RestoreConfigs(s_EmbeddedConfigs, EMBEDDED_BUTTONS, hidsysGetHidButtonConfigEmbedded, hidsysSetHidButtonConfigEmbedded),
            RestoreConfigs(s_FullConfigs, FULL_BUTTONS, hidsysGetHidButtonConfigFull, hidsysSetHidButtonConfigFull),
            RestoreConfigs(s_LeftConfigs, LEFT_BUTTONS, hidsysGetHidButtonConfigLeft, hidsysSetHidButtonConfigLeft),
            RestoreConfigs(s_RightConfigs, RIGHT_BUTTONS, hidsysGetHidButtonConfigRight, hidsysSetHidButtonConfigRight)
        };
        RefreshReverseMappingLocked();
        for (Result rc : results) {
            if (R_FAILED(rc)) return rc;
        }
        return 0;
    }
}

Result ButtonRemapper::SetMapping(const char* config_path) {
    std::lock_guard<std::mutex> lock(s_MappingMutex);
    const auto rules = LoadMappings(config_path);
    if (rules.empty()) return RestoreMappingsLocked();

    HidsysUniquePadId pad_ids[8];
    s32 total = 0;
    Result rc = hidsysGetUniquePadIds(pad_ids, 8, &total);
    if (R_FAILED(rc)) return rc;

    Result first_error = 0;
    for (s32 i = 0; i < total; ++i) {
        HidsysUniquePadType type;
        rc = hidsysGetUniquePadType(pad_ids[i], &type);
        if (R_SUCCEEDED(rc)) {
            switch (type) {
                case HidsysUniquePadType_Embedded:
                case HidsysUniquePadType_DebugPadController:
                    rc = ApplyMappings(pad_ids[i], rules, s_EmbeddedConfigs, EMBEDDED_BUTTONS,
                                       hidsysGetHidButtonConfigEmbedded, hidsysSetHidButtonConfigEmbedded);
                    break;
                case HidsysUniquePadType_FullKeyController:
                    rc = ApplyMappings(pad_ids[i], rules, s_FullConfigs, FULL_BUTTONS,
                                       hidsysGetHidButtonConfigFull, hidsysSetHidButtonConfigFull);
                    break;
                case HidsysUniquePadType_LeftController:
                    rc = ApplyMappings(pad_ids[i], rules, s_LeftConfigs, LEFT_BUTTONS,
                                       hidsysGetHidButtonConfigLeft, hidsysSetHidButtonConfigLeft);
                    break;
                case HidsysUniquePadType_RightController:
                    rc = ApplyMappings(pad_ids[i], rules, s_RightConfigs, RIGHT_BUTTONS,
                                       hidsysGetHidButtonConfigRight, hidsysSetHidButtonConfigRight);
                    break;
            }
        }
        if (R_FAILED(rc) && R_SUCCEEDED(first_error)) first_error = rc;
    }
    RefreshReverseMappingLocked();
    return first_error;
}

Result ButtonRemapper::RestoreMapping() {
    std::lock_guard<std::mutex> lock(s_MappingMutex);
    return RestoreMappingsLocked();
}

Result ButtonRemapper::RefreshReverseMapping() {
    std::lock_guard<std::mutex> lock(s_MappingMutex);
    return RefreshReverseMappingLocked();
}

void ButtonRemapper::ApplyReverseMapping(u64& buttons) {
    std::lock_guard<std::mutex> lock(s_MappingMutex);
    u64 to_clear = 0;
    u64 to_set = 0;
    for (const auto& mapping : s_ReverseMappings) {
        if (buttons & mapping.target) {
            to_clear |= mapping.target;
            to_set |= mapping.source;
        }
    }
    buttons = (buttons & ~to_clear) | to_set;
}
