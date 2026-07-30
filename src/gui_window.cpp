#include "gui_window.h"
#include <uxtheme.h>
#include <dwmapi.h>
#include <iostream>

namespace aegisx {

static AegisXWindow* g_pWindow = nullptr;

SteamProfileInfo AegisXWindow::FetchActiveSteamProfile() {
    SteamProfileInfo info{};
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char name[256]{};
        DWORD sz = sizeof(name);
        if (RegQueryValueExA(hKey, "PersonaName", NULL, NULL, reinterpret_cast<LPBYTE>(name), &sz) == ERROR_SUCCESS && sz > 1) {
            info.personaName = name;
        }
        DWORD activeUser = 0;
        sz = sizeof(activeUser);
        if (RegQueryValueExA(hKey, "ActiveUser", NULL, NULL, reinterpret_cast<LPBYTE>(&activeUser), &sz) == ERROR_SUCCESS && activeUser != 0) {
            info.steamId64 = 76561197960265728ULL + activeUser;
        }
        RegCloseKey(hKey);
    }
    return info;
}

bool AegisXWindow::CreateAegisWindow(HINSTANCE hInstance) {
    g_pWindow = this;
    m_profile = FetchActiveSteamProfile();

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = AegisXWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "AegisX_ClientWindow";

    RegisterClassExA(&wc);

    int width = 520;
    int height = 360;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;

    m_hwnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        "AegisX_ClientWindow",
        "Aegis-X Anti-Cheat",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height,
        NULL, NULL, hInstance, NULL
    );

    if (!m_hwnd) return false;

    // Enable Windows 11 Dark Mode Title Bar
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(m_hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &useDarkMode, sizeof(useDarkMode));

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    return true;
}

void AegisXWindow::UpdateStatus(const std::string& statusText, bool isProtected, bool isViolation, const std::string& violationDetail) {
    m_statusText = statusText;
    m_isProtected = isProtected;
    m_isViolation = isViolation;
    m_violationText = violationDetail;

    if (m_hwnd) {
        InvalidateRect(m_hwnd, NULL, TRUE);
    }
}

LRESULT CALLBACK AegisXWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
        if (g_pWindow) {
            g_pWindow->OnPaint(hwnd);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

void AegisXWindow::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    // Double buffering memory DC
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    // Background color #121418
    HBRUSH bgBrush = CreateSolidBrush(RGB(18, 20, 24));
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Title Font & Body Font
    HFONT nameFont = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT subFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT cardTitleFont = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    SetBkMode(memDC, TRANSPARENT);

    // 1. Render Steam Profile Card (#1F242D)
    RECT profileCard{ 20, 20, 484, 110 };
    HBRUSH cardBrush = CreateSolidBrush(RGB(31, 36, 45));
    FillRect(memDC, &profileCard, cardBrush);
    DeleteObject(cardBrush);

    // Avatar Box (#2A303C)
    RECT avatarRect{ 35, 32, 95, 92 };
    HBRUSH avatarBrush = CreateSolidBrush(RGB(42, 48, 60));
    FillRect(memDC, &avatarRect, avatarBrush);
    DeleteObject(avatarBrush);

    // Shield Logo inside Avatar Box
    SelectObject(memDC, nameFont);
    SetTextColor(memDC, RGB(0, 230, 118)); // Neon Green
    RECT logoTextRect{ 35, 45, 95, 80 };
    DrawTextA(memDC, "🛡️", -1, &logoTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Steam Persona Name
    SelectObject(memDC, nameFont);
    SetTextColor(memDC, RGB(255, 255, 255));
    RECT nameTextRect{ 115, 33, 460, 60 };
    DrawTextA(memDC, m_profile.personaName.c_str(), -1, &nameTextRect, DT_LEFT | DT_SINGLELINE);

    // Steam ID / Status Subtitle
    SelectObject(memDC, subFont);
    SetTextColor(memDC, RGB(160, 174, 192));
    std::string subText = "Steam Account Verified  •  ID64: " + std::to_string(m_profile.steamId64);
    RECT subTextRect{ 115, 63, 460, 85 };
    DrawTextA(memDC, subText.c_str(), -1, &subTextRect, DT_LEFT | DT_SINGLELINE);

    // 2. Render Security Status Banner (#1A2B22 for valid, #3B1B1B for violation)
    RECT bannerCard{ 20, 125, 484, 250 };
    COLORREF bannerBg = m_isViolation ? RGB(59, 27, 27) : RGB(26, 43, 34);
    COLORREF bannerBorder = m_isViolation ? RGB(255, 61, 0) : RGB(0, 230, 118);
    HBRUSH bannerBrush = CreateSolidBrush(bannerBg);
    FillRect(memDC, &bannerCard, bannerBrush);
    DeleteObject(bannerBrush);

    // Banner Title
    SelectObject(memDC, cardTitleFont);
    SetTextColor(memDC, bannerBorder);
    std::string bannerTitle = m_isViolation ? "❌ Security Violation Detected" : "✔ TPM, Secure Boot & IOMMU Protection Active";
    RECT bannerTitleRect{ 35, 140, 470, 165 };
    DrawTextA(memDC, bannerTitle.c_str(), -1, &bannerTitleRect, DT_LEFT | DT_SINGLELINE);

    // Banner Body Text
    SelectObject(memDC, subFont);
    SetTextColor(memDC, RGB(226, 232, 240));
    std::string bannerBody = m_isViolation ?
        (m_violationText.empty() ? "Unauthorized cheat behavior detected. CS2 process terminated." : m_violationText) :
        "Aegis-X Real-Time Driver Shield, Anti-Tamper & Kernel Watchdog are fully operational.";
    RECT bannerBodyRect{ 35, 172, 470, 235 };
    DrawTextA(memDC, bannerBody.c_str(), -1, &bannerBodyRect, DT_LEFT | DT_WORDBREAK);

    // 3. Render Bottom Status Bar
    SelectObject(memDC, subFont);
    COLORREF statusDotColor = m_isViolation ? RGB(255, 61, 0) : (m_isProtected ? RGB(0, 230, 118) : RGB(255, 193, 7));
    SetTextColor(memDC, statusDotColor);
    RECT statusDotRect{ 20, 275, 40, 300 };
    DrawTextA(memDC, "🟢", -1, &statusDotRect, DT_LEFT | DT_SINGLELINE);

    SetTextColor(memDC, RGB(203, 213, 225));
    std::string bottomText = "Status: " + m_statusText;
    RECT statusTextRect{ 42, 275, 484, 300 };
    DrawTextA(memDC, bottomText.c_str(), -1, &statusTextRect, DT_LEFT | DT_SINGLELINE);

    // Copy to screen DC
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    // Clean up fonts & DC
    DeleteObject(nameFont);
    DeleteObject(subFont);
    DeleteObject(cardTitleFont);
    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

} // namespace aegisx
