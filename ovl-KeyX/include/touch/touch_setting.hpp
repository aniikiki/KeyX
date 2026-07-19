#pragma once
#include <tesla.hpp>

class SettingTouch : public tsl::Gui {
public:
    SettingTouch();
    virtual tsl::elm::Element* createUI() override;
    virtual void update() override;

private:
    bool m_DefaultAuto = false;
    tsl::elm::ListItem* m_IndependentItem = nullptr;
};

class SettingTouchConfig : public tsl::Gui {
public:
    SettingTouchConfig(bool isGlobal, u64 currentTitleId);
    virtual tsl::elm::Element* createUI() override;
    virtual void update() override;

private:
    bool m_IsGlobal;
    char m_GameName[64]{};
    char m_ConfigPath[64]{};
    char m_LeftButton[8]{};
    char m_RightButton[8]{};
    tsl::elm::ListItem* m_LeftItem = nullptr;
    tsl::elm::ListItem* m_RightItem = nullptr;

    void LoadConfig();
    void RefreshItems();
};

class SettingTouchButton : public tsl::Gui {
public:
    SettingTouchButton(const char* configPath, bool isLeft, const char* currentButton,
                       const char* otherButton);
    virtual tsl::elm::Element* createUI() override;

private:
    char m_ConfigPath[64]{};
    char m_CurrentButton[8]{};
    char m_OtherButton[8]{};
    bool m_IsLeft;
};
