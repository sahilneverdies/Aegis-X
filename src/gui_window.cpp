#include "gui_window.h"
#include <uxtheme.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <iostream>

namespace aegisx {

static AegisXWindow* g_pWindow = nullptr;
static ULONG_PTR g_gdiplusToken = 0;

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

    // Retrieve Steam installation path to locate cached avatar PNG
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char steamPath[MAX_PATH]{};
        DWORD sz = sizeof(steamPath);
        if (RegQueryValueExA(hKey, "SteamPath", NULL, NULL, reinterpret_cast<LPBYTE>(steamPath), &sz) == ERROR_SUCCESS) {
            std::string avatarFile = std::string(steamPath) + "/config/avatarcache/" + std::to_string(info.steamId64) + ".png";
            DWORD attrib = GetFileAttributesA(avatarFile.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                info.avatarPath = avatarFile;
            }
        }
        RegCloseKey(hKey);
    }

    return info;
}

bool AegisXWindow::CreateAegisWindow(HINSTANCE hInstance) {
    g_pWindow = this;

    // Initialize GDI+ for PNG image rendering
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

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

    int width = 460;
    int height = 230;
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

    // 30ms animation timer
    SetTimer(m_hwnd, 1, 30, NULL);

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
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

LRESULT CALLBACK AegisXWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
        if (g_pWindow && g_pWindow->m_isLoading) {
            g_pWindow->m_spinnerFrame++;
            g_pWindow->m_loadingProgress += 2;

            if (g_pWindow->m_loadingProgress < 30) {
                g_pWindow->m_loadingText = "Starting service...";
            } else if (g_pWindow->m_loadingProgress < 70) {
                g_pWindow->m_loadingText = "Verifying Kernel Security & Driver Shield...";
            } else if (g_pWindow->m_loadingProgress < 100) {
                g_pWindow->m_loadingText = "Authenticating Steam Profile...";
            } else {
                g_pWindow->m_isLoading = false;
                KillTimer(hwnd, 1);
            }

            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

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

    // Background color #0B0E14 (Cyber Obsidian)
    HBRUSH bgBrush = CreateSolidBrush(RGB(11, 14, 20));
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(memDC, TRANSPARENT);

    if (m_isLoading) {
        // --- AEGIS CYBER STARTING SERVICE ANIMATION ---
        HFONT logoFont = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT titleFont = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT subFont = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        // Aegis-X Header (Electric Cyan #00E5FF)
        SelectObject(memDC, logoFont);
        SetTextColor(memDC, RGB(0, 229, 255));
        RECT logoRect{ 0, 22, clientRect.right, 55 };
        DrawTextA(memDC, "[ AEGIS-X HYPERVISION ]", -1, &logoRect, DT_CENTER | DT_SINGLELINE);

        // Loading Status Text
        SelectObject(memDC, titleFont);
        SetTextColor(memDC, RGB(255, 255, 255));
        RECT titleRect{ 0, 60, clientRect.right, 82 };
        DrawTextA(memDC, m_loadingText.c_str(), -1, &titleRect, DT_CENTER | DT_SINGLELINE);

        // Subtitle Text
        SelectObject(memDC, subFont);
        SetTextColor(memDC, RGB(160, 190, 220));
        RECT subRect{ 0, 85, clientRect.right, 105 };
        DrawTextA(memDC, "Kernel Security & Driver Shield (by Sahil)", -1, &subRect, DT_CENTER | DT_SINGLELINE);

        // Progress Bar Outer Track (#161B22)
        RECT barOuter{ 95, 118, 365, 123 };
        HBRUSH trackBrush = CreateSolidBrush(RGB(22, 27, 34));
        FillRect(memDC, &barOuter, trackBrush);
        DeleteObject(trackBrush);

        // Progress Bar Inner Fill (#00E5FF Electric Cyan)
        int fillWidth = (270 * m_loadingProgress) / 100;
        if (fillWidth > 0) {
            RECT barInner{ 95, 118, 95 + fillWidth, 123 };
            HBRUSH fillBrush = CreateSolidBrush(RGB(0, 229, 255));
            FillRect(memDC, &barInner, fillBrush);
            DeleteObject(fillBrush);
        }

        // Animated Pulsing Cyber Dots
        int centerX = clientRect.right / 2;
        int dotsY = 148;
        int dotSpacing = 12;
        int startX = centerX - (2 * dotSpacing);

        for (int i = 0; i < 5; i++) {
            int dotX = startX + (i * dotSpacing);
            int pulse = (m_spinnerFrame + (i * 3)) % 15;
            int radius = (pulse < 8) ? (2 + pulse / 2) : (6 - pulse / 2);

            COLORREF dotColor = (pulse < 8) ? RGB(0, 229, 255) : RGB(0, 245, 160);
            HBRUSH dotBrush = CreateSolidBrush(dotColor);
            HPEN nullPen = (HPEN)GetStockObject(NULL_PEN);
            SelectObject(memDC, dotBrush);
            SelectObject(memDC, nullPen);
            Ellipse(memDC, dotX - radius, dotsY - radius, dotX + radius, dotsY + radius);
            DeleteObject(dotBrush);
        }

        DeleteObject(logoFont);
        DeleteObject(titleFont);
        DeleteObject(subFont);
    } else {
        // --- HIGH-READABILITY MAIN DASHBOARD UI ---
        HFONT nameFont = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT avatarFont = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT subFont = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT bodyFont = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT cardTitleFont = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        // 1. Render Steam Profile Card (#161B22 Cyber Dark)
        RECT profileCard{ 10, 10, clientRect.right - 10, 68 };
        HBRUSH cardBrush = CreateSolidBrush(RGB(22, 27, 34));
        FillRect(memDC, &profileCard, cardBrush);
        DeleteObject(cardBrush);

        // Avatar Box (#1F2633 with Electric Cyan Border)
        RECT avatarRect{ 18, 18, 60, 60 };
        HBRUSH avatarBrush = CreateSolidBrush(RGB(31, 38, 51));
        FillRect(memDC, &avatarRect, avatarBrush);
        DeleteObject(avatarBrush);

        bool avatarDrawn = false;
        if (!m_profile.avatarPath.empty()) {
            std::wstring wAvatarPath(m_profile.avatarPath.begin(), m_profile.avatarPath.end());
            Gdiplus::Bitmap avatarImg(wAvatarPath.c_str());
            if (avatarImg.GetLastStatus() == Gdiplus::Ok) {
                Gdiplus::Graphics graphics(memDC);
                graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                graphics.DrawImage(&avatarImg, 18, 18, 42, 42);
                avatarDrawn = true;
            }
        }

        if (!avatarDrawn) {
            // Fallback: Player Initial Avatar Badge (e.g. 'S' for Sahil)
            SelectObject(memDC, avatarFont);
            SetTextColor(memDC, RGB(0, 229, 255)); // Electric Cyan
            std::string initialStr = m_profile.personaName.empty() ? "S" : m_profile.personaName.substr(0, 1);
            DrawTextA(memDC, initialStr.c_str(), -1, &avatarRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // Draw Cyan Border around avatar
        HPEN cyanPen = CreatePen(PS_SOLID, 1, RGB(0, 229, 255));
        HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
        SelectObject(memDC, cyanPen);
        SelectObject(memDC, nullBrush);
        Rectangle(memDC, 17, 17, 61, 61);
        DeleteObject(cyanPen);

        // Steam Persona Name (Bright Pure White #FFFFFF)
        SelectObject(memDC, nameFont);
        SetTextColor(memDC, RGB(255, 255, 255));
        RECT nameTextRect{ 68, 18, clientRect.right - 18, 40 };
        DrawTextA(memDC, m_profile.personaName.c_str(), -1, &nameTextRect, DT_LEFT | DT_SINGLELINE);

        // Steam ID / Status Subtitle (Bright Ice Cyan #80F2FF)
        SelectObject(memDC, subFont);
        SetTextColor(memDC, RGB(128, 242, 255));
        std::string subText = "[ STEAM AUTHENTICATED ]   |   ID64: " + std::to_string(m_profile.steamId64);
        RECT subTextRect{ 68, 42, clientRect.right - 18, 62 };
        DrawTextA(memDC, subText.c_str(), -1, &subTextRect, DT_LEFT | DT_SINGLELINE);

        // 2. Render Security Status Banner (#0D281E Dark Emerald for valid, #2E0F17 for violation)
        RECT bannerCard{ 10, 76, clientRect.right - 10, 145 };
        COLORREF bannerBg = m_isViolation ? RGB(46, 15, 23) : RGB(13, 40, 30);
        COLORREF bannerBorder = m_isViolation ? RGB(255, 46, 84) : RGB(0, 245, 160);
        HBRUSH bannerBrush = CreateSolidBrush(bannerBg);
        FillRect(memDC, &bannerCard, bannerBrush);
        DeleteObject(bannerBrush);

        // Banner Title (Bright Emerald Green #00F5A0)
        SelectObject(memDC, cardTitleFont);
        SetTextColor(memDC, bannerBorder);
        std::string bannerTitle = m_isViolation ? "[!] Security Violation Detected" : "[ HARDWARE && DRIVER SHIELD ACTIVE ]";
        RECT bannerTitleRect{ 20, 85, clientRect.right - 20, 104 };
        DrawTextA(memDC, bannerTitle.c_str(), -1, &bannerTitleRect, DT_LEFT | DT_SINGLELINE);

        // Banner Body Text (Clear Light Emerald #E2F9EE)
        SelectObject(memDC, bodyFont);
        SetTextColor(memDC, RGB(226, 249, 238));
        std::string bannerBody = m_isViolation ?
            (m_violationText.empty() ? "Unauthorized cheat behavior detected. CS2 process terminated." : m_violationText) :
            "Aegis-X Real-Time Driver Shield, Anti-Tamper && Kernel Watchdog are fully operational.";
        RECT bannerBodyRect{ 20, 106, clientRect.right - 20, 138 };
        DrawTextA(memDC, bannerBody.c_str(), -1, &bannerBodyRect, DT_LEFT | DT_WORDBREAK);

        // 3. Render Bottom Status Bar
        SelectObject(memDC, subFont);
        COLORREF statusDotColor = m_isViolation ? RGB(255, 46, 84) : (m_isProtected ? RGB(0, 245, 160) : RGB(255, 200, 0));

        // Draw clean GDI Status Dot
        HBRUSH statusDotBrush = CreateSolidBrush(statusDotColor);
        HPEN nullPen = (HPEN)GetStockObject(NULL_PEN);
        SelectObject(memDC, statusDotBrush);
        SelectObject(memDC, nullPen);
        Ellipse(memDC, 16, 157, 26, 167);
        DeleteObject(statusDotBrush);

        // Bright High-Contrast Status Text (#F1F5F9)
        SetTextColor(memDC, RGB(241, 245, 249));
        std::string bottomText = "Aegis-X Engine v3.0  |  Status: " + m_statusText;
        RECT statusTextRect{ 32, 154, clientRect.right - 10, 174 };
        DrawTextA(memDC, bottomText.c_str(), -1, &statusTextRect, DT_LEFT | DT_SINGLELINE);

        DeleteObject(nameFont);
        DeleteObject(avatarFont);
        DeleteObject(subFont);
        DeleteObject(bodyFont);
        DeleteObject(cardTitleFont);
    }

    // Copy to screen DC
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

} // namespace aegisx
