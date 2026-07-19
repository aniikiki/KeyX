#pragma once

#include <switch.h>

class TouchMapper {
public:
    explicit TouchMapper(const char* configPath);

    void LoadConfig(const char* configPath);
    u64 Process();
    void ResetState();

private:
    static constexpr u32 SCREEN_MID_X = 640;
    static constexpr u64 MIN_PRESS_NS = 50000000ULL;

    u64 m_LeftButtonMask = 0;
    u64 m_RightButtonMask = 0;

    bool m_LeftFingerTracked = false;
    bool m_RightFingerTracked = false;
    u32 m_LeftFingerId = 0;
    u32 m_RightFingerId = 0;
    u64 m_LeftPressStart = 0;
    u64 m_RightPressStart = 0;
    bool m_WaitForRelease = true;
    HidTouchScreenState m_TouchState{};

    static u64 ButtonNameToMask(const char* name);
    static bool HasFinger(const HidTouchScreenState& state, u32 fingerId);
    static bool MinimumPressActive(u64 startTick, u64 nowTick);
};
