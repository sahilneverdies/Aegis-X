#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace aegisx {

struct SteamProfileInfo {
    std::string personaName = "Sahil";
    uint64_t steamId64 = 76561198000000000ULL;
    std::string avatarPath = "";
    bool isLoggedIn = true;
};

class AegisXWindow {
public:
    AegisXWindow() = default;
    ~AegisXWindow() = default;

    bool CreateAegisWindow(HINSTANCE hInstance);
    void UpdateStatus(const std::string& statusText, bool isProtected, bool isViolation = false, const std::string& violationDetail = "");
    void MessageLoop();
    HWND GetHWND() const { return m_hwnd; }

    static SteamProfileInfo FetchActiveSteamProfile();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint(HWND hwnd);

    HWND m_hwnd = NULL;
    SteamProfileInfo m_profile;
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
