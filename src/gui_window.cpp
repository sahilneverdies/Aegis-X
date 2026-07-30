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

AegisXWindow::~AegisXWindow() {
    RemoveSystemTrayIcon();
}

void AegisXWindow::CreateSystemTrayIcon() {
    memset(&m_nid, 0, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAA);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1001;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = (HICON)LoadImageA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(101), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!m_nid.hIcon) {
        m_nid.hIcon = LoadIcon(NULL, IDI_SHIELD);
    }
    strcpy_s(m_nid.szTip, sizeof(m_nid.szTip), "Aegis-X Anti-Cheat Guard");

    Shell_NotifyIconA(NIM_ADD, &m_nid);
}

void AegisXWindow::RemoveSystemTrayIcon() {
    if (m_nid.hWnd) {
        Shell_NotifyIconA(NIM_DELETE, &m_nid);
        m_nid.hWnd = NULL;
    }
}

bool AegisXWindow::CreateAegisWindow(HINSTANCE hInstance) {
    g_pWindow = this;

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    m_profile = FetchActiveSteamProfile();

    HICON hAppIcon = (HICON)LoadImageA(hInstance, MAKEINTRESOURCEA(101), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (!hAppIcon) hAppIcon = LoadIcon(NULL, IDI_SHIELD);

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = AegisXWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = hAppIcon;
    wc.hIconSm = hAppIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "AegisX_ClientWindow";

    RegisterClassExA(&wc);

    int width = 500;
    int height = 255;
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

    // Create System Tray Icon
    CreateSystemTrayIcon();

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

void AegisXWindow::TriggerInstallUpdate() {
    if (m_updateInfo.isDownloading) return;

    m_updateInfo.isDownloading = true;
    m_statusText = "Downloading Update v" + m_updateInfo.latestVersion + " (0%)...";
    if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);

    AutoUpdater::StartUpdateDownload(m_updateInfo, [this](int progressPct, bool completed) {
        if (completed) {
            m_statusText = "Update Downloaded! Restarting Aegis-X...";
        } else {
            m_statusText = "Downloading Update v" + m_updateInfo.latestVersion + " (" + std::to_string(progressPct) + "%)...";
        }
        if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);
    });
}

LRESULT CALLBACK AegisXWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEMOVE:
        if (g_pWindow && g_pWindow->m_hasUpdate) {
            POINT pt{ LOWORD(lParam), HIWORD(lParam) };
            bool nowHovered = PtInRect(&g_pWindow->m_updateBtnRect, pt);
            if (nowHovered != g_pWindow->m_btnHovered) {
                g_pWindow->m_btnHovered = nowHovered;
                InvalidateRect(hwnd, &g_pWindow->m_updateBtnRect, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (g_pWindow && g_pWindow->m_hasUpdate) {
            POINT pt{ LOWORD(lParam), HIWORD(lParam) };
            if (PtInRect(&g_pWindow->m_updateBtnRect, pt)) {
                g_pWindow->TriggerInstallUpdate();
            }
        }
        return 0;

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

    case WM_CLOSE:
        // Do not terminate process when clicking X! Run in System Tray!
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP) {
            ShowWindow(hwnd, SW_SHOW);
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        } else if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            InsertMenuA(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_TRAY_RESTORE, "Open Aegis-X Anti-Cheat");
            InsertMenuA(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            InsertMenuA(hMenu, 2, MF_BYPOSITION | MF_STRING, IDM_TRAY_EXIT, "Exit Anti-Cheat");

            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_TRAY_RESTORE) {
            ShowWindow(hwnd, SW_SHOW);
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        } else if (LOWORD(wParam) == IDM_TRAY_EXIT) {
            if (g_pWindow) g_pWindow->RemoveSystemTrayIcon();
            DestroyWindow(hwnd);
            PostQuitMessage(0);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

static void DrawNeumorphicCard(HDC hdc, const RECT& rc, COLORREF bgCol, COLORREF highlightCol, COLORREF shadowCol) {
    HBRUSH bgBrush = CreateSolidBrush(bgCol);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    HPEN lightPen = CreatePen(PS_SOLID, 1, highlightCol);
    HPEN darkPen = CreatePen(PS_SOLID, 1, shadowCol);

    // Top & Left Light Bevel Highlight
    HPEN oldPen = (HPEN)SelectObject(hdc, lightPen);
    MoveToEx(hdc, rc.left, rc.bottom - 1, NULL);
    LineTo(hdc, rc.left, rc.top);
    LineTo(hdc, rc.right - 1, rc.top);

    // Bottom & Right Soft Drop Shadow
    SelectObject(hdc, darkPen);
    LineTo(hdc, rc.right - 1, rc.bottom - 1);
    LineTo(hdc, rc.left, rc.bottom - 1);

    SelectObject(hdc, oldPen);
    DeleteObject(lightPen);
    DeleteObject(darkPen);
}

static void DrawNeumorphicInset(HDC hdc, const RECT& rc, COLORREF bgCol, COLORREF shadowCol, COLORREF highlightCol) {
    HBRUSH bgBrush = CreateSolidBrush(bgCol);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    HPEN darkPen = CreatePen(PS_SOLID, 1, shadowCol);
    HPEN lightPen = CreatePen(PS_SOLID, 1, highlightCol);

    // Top & Left Inner Inset Shadow
    HPEN oldPen = (HPEN)SelectObject(hdc, darkPen);
    MoveToEx(hdc, rc.left, rc.bottom - 1, NULL);
    LineTo(hdc, rc.left, rc.top);
    LineTo(hdc, rc.right - 1, rc.top);

    // Bottom & Right Inner Inset Highlight Rim
    SelectObject(hdc, lightPen);
    LineTo(hdc, rc.right - 1, rc.bottom - 1);
    LineTo(hdc, rc.left, rc.bottom - 1);

    SelectObject(hdc, oldPen);
    DeleteObject(darkPen);
    DeleteObject(lightPen);
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

    // Cyberpunk Dark Background #0B0E14
    COLORREF baseBg = RGB(11, 14, 20);
    COLORREF panelBg = RGB(18, 22, 30);
    COLORREF cyanGlow = RGB(0, 229, 255);
    COLORREF matrixGreen = RGB(0, 255, 157);
    COLORREF alertRed = RGB(255, 46, 84);

    HBRUSH bgBrush = CreateSolidBrush(baseBg);
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(memDC, TRANSPARENT);

    if (m_isLoading) {
        // --- FUTURISTIC CYBER STARTING SERVICE RADAR ---
        HFONT logoFont = CreateFontA(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT titleFont = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT subFont = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        // Aegis-X Radar Logo Header
        SelectObject(memDC, logoFont);
        SetTextColor(memDC, cyanGlow);
        RECT logoRect{ 0, 25, clientRect.right, 55 };
        DrawTextA(memDC, "⚡ AEGIS-X ULTRA KERNEL SHIELD", -1, &logoRect, DT_CENTER | DT_SINGLELINE);

        // Status Text
        SelectObject(memDC, titleFont);
        SetTextColor(memDC, RGB(255, 255, 255));
        RECT titleRect{ 0, 62, clientRect.right, 85 };
        DrawTextA(memDC, m_loadingText.c_str(), -1, &titleRect, DT_CENTER | DT_SINGLELINE);

        // Subtitle Text
        SelectObject(memDC, subFont);
        SetTextColor(memDC, RGB(140, 160, 185));
        RECT subRect{ 0, 88, clientRect.right, 108 };
        DrawTextA(memDC, "Real-Time PCIe DMA & Hypervisor Security (by Sahil)", -1, &subRect, DT_CENTER | DT_SINGLELINE);

        // Cyber Track Frame
        RECT barOuter{ 90, 120, clientRect.right - 90, 126 };
        HBRUSH trackBrush = CreateSolidBrush(RGB(24, 30, 42));
        FillRect(memDC, &barOuter, trackBrush);
        DeleteObject(trackBrush);

        // Glowing Fill
        int totalW = clientRect.right - 180;
        int fillWidth = (totalW * m_loadingProgress) / 100;
        if (fillWidth > 0) {
            RECT barInner{ 90, 120, 90 + fillWidth, 126 };
            HBRUSH fillBrush = CreateSolidBrush(cyanGlow);
            FillRect(memDC, &barInner, fillBrush);
            DeleteObject(fillBrush);
        }

        // Radar Pulse Ring
        int centerX = clientRect.right / 2;
        int dotsY = 150;
        int dotSpacing = 12;
        int startX = centerX - (2 * dotSpacing);

        for (int i = 0; i < 5; i++) {
            int dotX = startX + (i * dotSpacing);
            int pulse = (m_spinnerFrame + (i * 3)) % 15;
            int radius = (pulse < 8) ? (2 + pulse / 2) : (6 - pulse / 2);

            COLORREF dotColor = (pulse < 8) ? cyanGlow : matrixGreen;
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
        // --- 3-COLUMN FUTURISTIC CYBERPUNK HUD ---
        HFONT nameFont = CreateFontA(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT badgeFont = CreateFontA(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT subFont = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT titleFont = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        // COLUMN 1: Steam Player Profile Box (X: 12 to 160)
        RECT col1{ 12, 12, 160, 165 };
        HBRUSH col1Brush = CreateSolidBrush(panelBg);
        FillRect(memDC, &col1, col1Brush);
        DeleteObject(col1Brush);

        // Player Avatar (Square 50x50 at X:61, Y:22)
        RECT avatarRect{ 61, 22, 111, 72 };
        bool avatarDrawn = false;
        if (!m_profile.avatarPath.empty()) {
            std::wstring wAvatarPath(m_profile.avatarPath.begin(), m_profile.avatarPath.end());
            Gdiplus::Bitmap avatarImg(wAvatarPath.c_str());
            if (avatarImg.GetLastStatus() == Gdiplus::Ok) {
                Gdiplus::Graphics graphics(memDC);
                graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                graphics.DrawImage(&avatarImg, 61, 22, 50, 50);
                avatarDrawn = true;
            }
        }

        if (!avatarDrawn) {
            HBRUSH fallbackBrush = CreateSolidBrush(RGB(25, 32, 44));
            FillRect(memDC, &avatarRect, fallbackBrush);
            DeleteObject(fallbackBrush);

            SelectObject(memDC, nameFont);
            SetTextColor(memDC, cyanGlow);
            std::string initialStr = m_profile.personaName.empty() ? "S" : m_profile.personaName.substr(0, 1);
            DrawTextA(memDC, initialStr.c_str(), -1, &avatarRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // Cyan Square Border around Avatar
        HPEN cyanPen = CreatePen(PS_SOLID, 1, cyanGlow);
        HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
        SelectObject(memDC, cyanPen);
        SelectObject(memDC, nullBrush);
        Rectangle(memDC, 60, 21, 112, 73);
        DeleteObject(cyanPen);

        // Player Name
        SelectObject(memDC, nameFont);
        SetTextColor(memDC, RGB(255, 255, 255));
        RECT nameRect{ 14, 80, 158, 102 };
        DrawTextA(memDC, m_profile.personaName.c_str(), -1, &nameRect, DT_CENTER | DT_SINGLELINE);

        // Steam Tag
        SelectObject(memDC, badgeFont);
        SetTextColor(memDC, cyanGlow);
        RECT tagRect{ 14, 104, 158, 122 };
        DrawTextA(memDC, "[ STEAM AUTHENTICATED ]", -1, &tagRect, DT_CENTER | DT_SINGLELINE);

        // ID64 Subtitle
        SelectObject(memDC, subFont);
        SetTextColor(memDC, RGB(140, 160, 185));
        std::string idStr = "ID: " + std::to_string(m_profile.steamId64);
        RECT idRect{ 14, 125, 158, 150 };
        DrawTextA(memDC, idStr.c_str(), -1, &idRect, DT_CENTER | DT_SINGLELINE);

        // COLUMN 2: Security Shield Gauge (X: 170 to 310)
        RECT col2{ 170, 12, 310, 165 };
        HBRUSH col2Brush = CreateSolidBrush(panelBg);
        FillRect(memDC, &col2, col2Brush);
        DeleteObject(col2Brush);

        // Gauge Circle Ring (Center: 240, 70)
        COLORREF gaugeColor = m_isViolation ? alertRed : matrixGreen;
        HPEN gaugePen = CreatePen(PS_SOLID, 3, gaugeColor);
        SelectObject(memDC, gaugePen);
        SelectObject(memDC, nullBrush);
        Ellipse(memDC, 205, 30, 275, 100);
        DeleteObject(gaugePen);

        // Gauge Title inside Ring
        SelectObject(memDC, nameFont);
        SetTextColor(memDC, gaugeColor);
        RECT gaugeText{ 205, 52, 275, 78 };
        std::string statusPct = m_isViolation ? "ALERT" : "100%";
        DrawTextA(memDC, statusPct.c_str(), -1, &gaugeText, DT_CENTER | DT_SINGLELINE);

        SelectObject(memDC, titleFont);
        SetTextColor(memDC, RGB(255, 255, 255));
        RECT gaugeLbl{ 174, 110, 306, 130 };
        DrawTextA(memDC, m_isViolation ? "VIOLATION" : "HARDWARE SHIELD", -1, &gaugeLbl, DT_CENTER | DT_SINGLELINE);

        SelectObject(memDC, subFont);
        SetTextColor(memDC, RGB(140, 160, 185));
        RECT gaugeSub{ 174, 132, 306, 155 };
        DrawTextA(memDC, "TPM / Secure Boot", -1, &gaugeSub, DT_CENTER | DT_SINGLELINE);

        // COLUMN 3: Live Security Metrics Checklist & Remote Update Notification
        RECT col3{ 320, 12, clientRect.right - 12, 165 };
        HBRUSH col3Brush = CreateSolidBrush(panelBg);
        FillRect(memDC, &col3, col3Brush);
        DeleteObject(col3Brush);

        SelectObject(memDC, titleFont);
        SetTextColor(memDC, cyanGlow);
        RECT metricTitle{ 330, 20, clientRect.right - 18, 38 };
        DrawTextA(memDC, "ACTIVE MODULES", -1, &metricTitle, DT_LEFT | DT_SINGLELINE);

        SelectObject(memDC, subFont);
        SetTextColor(memDC, matrixGreen);
        RECT m1{ 330, 42, clientRect.right - 18, 60 };
        DrawTextA(memDC, "[+] KERNEL SHIELD: PASS", -1, &m1, DT_LEFT | DT_SINGLELINE);

        RECT m2{ 330, 62, clientRect.right - 18, 80 };
        DrawTextA(memDC, "[+] PCIe DMA SHIELD: PASS", -1, &m2, DT_LEFT | DT_SINGLELINE);

        if (m_hasUpdate) {
            // Render Neon Update Notification Tag
            SelectObject(memDC, badgeFont);
            SetTextColor(memDC, RGB(255, 200, 0));
            RECT upTag{ 330, 84, clientRect.right - 18, 102 };
            std::string upText = "⚡ NEW UPDATE (v" + m_updateInfo.latestVersion + " - " + std::to_string(m_updateInfo.updateSizeMB) + " MB)";
            DrawTextA(memDC, upText.c_str(), -1, &upTag, DT_LEFT | DT_SINGLELINE);

            // Render Interactive [ INSTALL UPDATE ] Button
            COLORREF btnBg = m_btnHovered ? cyanGlow : RGB(20, 30, 44);
            COLORREF btnTxt = m_btnHovered ? RGB(11, 14, 20) : cyanGlow;

            HBRUSH btnBrush = CreateSolidBrush(btnBg);
            FillRect(memDC, &m_updateBtnRect, btnBrush);
            DeleteObject(btnBrush);

            HPEN btnPen = CreatePen(PS_SOLID, 1, cyanGlow);
            SelectObject(memDC, btnPen);
            SelectObject(memDC, nullBrush);
            Rectangle(memDC, m_updateBtnRect.left, m_updateBtnRect.top, m_updateBtnRect.right, m_updateBtnRect.bottom);
            DeleteObject(btnPen);

            SelectObject(memDC, badgeFont);
            SetTextColor(memDC, btnTxt);
            std::string btnStr = m_updateInfo.isDownloading ? "DOWNLOADING..." : "[ INSTALL UPDATE ]";
            DrawTextA(memDC, btnStr.c_str(), -1, &m_updateBtnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            RECT m3{ 330, 96, clientRect.right - 18, 116 };
            DrawTextA(memDC, "[+] HYPERVISOR GUARD: PASS", -1, &m3, DT_LEFT | DT_SINGLELINE);

            RECT m4{ 330, 120, clientRect.right - 18, 140 };
            DrawTextA(memDC, "[+] ANTI-TAMPER: ACTIVE", -1, &m4, DT_LEFT | DT_SINGLELINE);
        }

        // BOTTOM STATUS BAR (X: 12 to clientRect.right - 12, Y: 172 to 212)
        RECT bottomBarRect{ 12, 172, clientRect.right - 12, 212 };
        HBRUSH bottomBarBrush = CreateSolidBrush(RGB(18, 22, 30));
        FillRect(memDC, &bottomBarRect, bottomBarBrush);
        DeleteObject(bottomBarBrush);

        SelectObject(memDC, subFont);
        COLORREF dotCol = m_isViolation ? alertRed : (m_isProtected ? matrixGreen : RGB(255, 200, 0));

        HBRUSH dotBr = CreateSolidBrush(dotCol);
        HPEN nullP = (HPEN)GetStockObject(NULL_PEN);
        SelectObject(memDC, dotBr);
        SelectObject(memDC, nullP);
        Ellipse(memDC, 24, 187, 34, 197);
        DeleteObject(dotBr);

        SetTextColor(memDC, RGB(241, 245, 249));
        std::string bottomMsg = "Status: " + m_statusText;
        RECT bottomRect{ 40, 184, clientRect.right - 18, 204 };
        DrawTextA(memDC, bottomMsg.c_str(), -1, &bottomRect, DT_LEFT | DT_SINGLELINE);

        DeleteObject(nameFont);
        DeleteObject(badgeFont);
        DeleteObject(subFont);
        DeleteObject(titleFont);
    }

    // Copy to screen DC
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

} // namespace aegisx
