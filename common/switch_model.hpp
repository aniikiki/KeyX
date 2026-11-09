#pragma once
#include <switch.h>

inline bool IsSwitchLite() {
    static const bool isLite = [] {
        if (R_FAILED(setsysInitialize())) return false;

        SetSysProductModel model{};
        const Result rc = setsysGetProductModel(&model);
        setsysExit();
        return R_SUCCEEDED(rc) && model == SetSysProductModel_Hoag;
    }();
    return isLite;
}
