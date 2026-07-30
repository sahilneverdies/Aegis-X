#pragma once

#include <windows.h>
#include <string>
#include <vector>

#include "updater.h"

namespace aegisx {

struct SteamProfileInfo {
    std::string personaName = "Sahil";
    uint64_t steamId64 = 76561198000000000ULL;
    std::string avatarPath = "";
    bool isLoggedIn = true;
};

#define WM_TRAYICON (WM_USER + 1)
#define IDM_TRAY_RESTORE 2001
#define IDM_TRAY_EXIT 2002

class AegisXWindow {
public:
    AegisXWindow() = default;
    ~AegisXWindow();

    bool CreateAegisWindow(HINSTANCE hInstance);
    void UpdateStatus(const std::string& statusText, bool isProtected, bool isViolation = false, const std::string& violationDetail = "");
    void MessageLoop();
    HWND GetHWND() const { return m_hwnd; }

    void CreateSystemTrayIcon();
    void RemoveSystemTrayIcon();
    void TriggerInstallUpdate();

    static SteamProfileInfo FetchActiveSteamProfile();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint(HWND hwnd);

    HWND m_hwnd = NULL;
    NOTIFYICONDATAA m_nid = {};
    SteamProfileInfo m_profile;
    UpdateInfo m_updateInfo;
    bool m_hasUpdate = true; // Set to true so 20MB Update banner & button display!
    bool m_btnHovered = false;
    RECT m_updateBtnRect{ 120, 112, 380, 156 };

    std::string m_statusText = "Waiting for game to launch...";
    std::string m_violationText = "";
    std::string m_loadingText = "Starting service...";
    bool m_isProtected = false;
    bool m_isViolation = false;
    bool m_isLoading = true;
    int m_loadingProgress = 0;
    int m_spinnerFrame = 0;
};

} // namespace aegisx
