#include "touch_mapper.hpp"

#include <cstring>
#include <minIni.h>

TouchMapper::TouchMapper(const char* configPath) {
    LoadConfig(configPath);
}

u64 TouchMapper::ButtonNameToMask(const char* name) {
    if (strcmp(name, "A") == 0) return HidNpadButton_A;
    if (strcmp(name, "B") == 0) return HidNpadButton_B;
    if (strcmp(name, "X") == 0) return HidNpadButton_X;
    if (strcmp(name, "Y") == 0) return HidNpadButton_Y;
    if (strcmp(name, "StickL") == 0) return HidNpadButton_StickL;
    if (strcmp(name, "StickR") == 0) return HidNpadButton_StickR;
    if (strcmp(name, "L") == 0) return HidNpadButton_L;
    if (strcmp(name, "R") == 0) return HidNpadButton_R;
    if (strcmp(name, "ZL") == 0) return HidNpadButton_ZL;
    if (strcmp(name, "ZR") == 0) return HidNpadButton_ZR;
    if (strcmp(name, "Select") == 0) return HidNpadButton_Minus;
    if (strcmp(name, "Start") == 0) return HidNpadButton_Plus;
    if (strcmp(name, "Left") == 0) return HidNpadButton_Left;
    if (strcmp(name, "Up") == 0) return HidNpadButton_Up;
    if (strcmp(name, "Right") == 0) return HidNpadButton_Right;
    if (strcmp(name, "Down") == 0) return HidNpadButton_Down;
    return 0;
}

void TouchMapper::LoadConfig(const char* configPath) {
    char leftButton[16];
    char rightButton[16];
    ini_gets("TOUCH", "leftbutton", "B", leftButton, sizeof(leftButton), configPath);
    ini_gets("TOUCH", "rightbutton", "A", rightButton, sizeof(rightButton), configPath);

    m_LeftButtonMask = ButtonNameToMask(leftButton);
    m_RightButtonMask = ButtonNameToMask(rightButton);
    if (m_LeftButtonMask == m_RightButtonMask) m_RightButtonMask = 0;
    ResetState();
}

bool TouchMapper::HasFinger(const HidTouchScreenState& state, u32 fingerId) {
    for (s32 i = 0; i < state.count; i++) {
        if (state.touches[i].finger_id == fingerId) return true;
    }
    return false;
}

bool TouchMapper::MinimumPressActive(u64 startTick, u64 nowTick) {
    if (startTick == 0) return false;
    return armTicksToNs(nowTick - startTick) < MIN_PRESS_NS;
}

u64 TouchMapper::Process() {
    memset(&m_TouchState, 0, sizeof(m_TouchState));
    HidTouchScreenState& state = m_TouchState;
    if (hidGetTouchScreenStates(&state, 1) == 0) state.count = 0;
    if (state.count < 0) state.count = 0;
    if (state.count > 16) state.count = 16;

    if (m_WaitForRelease) {
        if (state.count == 0) m_WaitForRelease = false;
        return 0;
    }

    bool wasLeftTracked = m_LeftFingerTracked;
    bool wasRightTracked = m_RightFingerTracked;
    if (m_LeftFingerTracked && !HasFinger(state, m_LeftFingerId)) m_LeftFingerTracked = false;
    if (m_RightFingerTracked && !HasFinger(state, m_RightFingerId)) m_RightFingerTracked = false;

    for (s32 i = 0; i < state.count; i++) {
        const HidTouchState& touch = state.touches[i];
        if ((m_LeftFingerTracked && touch.finger_id == m_LeftFingerId) ||
            (m_RightFingerTracked && touch.finger_id == m_RightFingerId)) {
            continue;
        }

        if (touch.x < SCREEN_MID_X && !m_LeftFingerTracked) {
            m_LeftFingerTracked = true;
            m_LeftFingerId = touch.finger_id;
        } else if (touch.x >= SCREEN_MID_X && !m_RightFingerTracked) {
            m_RightFingerTracked = true;
            m_RightFingerId = touch.finger_id;
        }
    }

    u64 nowTick = armGetSystemTick();
    if (m_LeftFingerTracked && !wasLeftTracked) m_LeftPressStart = nowTick;
    if (m_RightFingerTracked && !wasRightTracked) m_RightPressStart = nowTick;

    bool leftActive = m_LeftFingerTracked || MinimumPressActive(m_LeftPressStart, nowTick);
    bool rightActive = m_RightFingerTracked || MinimumPressActive(m_RightPressStart, nowTick);
    if (!leftActive) m_LeftPressStart = 0;
    if (!rightActive) m_RightPressStart = 0;

    u64 buttons = 0;
    if (leftActive) buttons |= m_LeftButtonMask;
    if (rightActive) buttons |= m_RightButtonMask;
    return buttons;
}

void TouchMapper::ResetState() {
    m_LeftFingerTracked = false;
    m_RightFingerTracked = false;
    m_LeftFingerId = 0;
    m_RightFingerId = 0;
    m_LeftPressStart = 0;
    m_RightPressStart = 0;
    m_WaitForRelease = true;
}
