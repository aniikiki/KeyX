#pragma once
#include <switch.h>

class ButtonRemapper {
public:
    // 以系统当前映射为基础，应用普通按键规则。
    static Result SetMapping(const char* config_path);

    // 只撤销 KeyX 修改的字段，保留系统映射及其启用状态。
    static Result RestoreMapping();

    // Lite 的 HDLS 注入使用实际生效的内置手柄映射。
    static Result RefreshReverseMapping();
    static void ApplyReverseMapping(u64& buttons);
};
