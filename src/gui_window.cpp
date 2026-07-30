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
    m_updateInfo = AutoUpdater::CheckForRemoteUpdate();
    m_hasUpdate = m_updateInfo.updateAvailable;

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
    int height = 295;
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
            m_hasUpdate = false; // Transition to main 3-column GUI dashboard!
            m_statusText = "Connected  |  Waiting for Counter-Strike 2 to launch...";
        } else {
            m_statusText = "Downloading Update v" + m_updateInfo.latestVersion + " (" + std::to_string(progressPct) + "%)...";
        }
        if (m_hwnd) InvalidateRect(m_hwnd, NULL, TRUE);
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

static void DrawSkeuomorphicCard(HDC hdc, const RECT& rc, COLORREF bgCol, COLORREF highlightCol, COLORREF shadowCol) {
    HBRUSH bgBrush = CreateSolidBrush(bgCol);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    HPEN lightPen = CreatePen(PS_SOLID, 1, highlightCol);
    HPEN darkPen = CreatePen(PS_SOLID, 1, shadowCol);

    // 3D Top-Left Specular Light Bevel
    HPEN oldPen = (HPEN)SelectObject(hdc, lightPen);
    MoveToEx(hdc, rc.left, rc.bottom - 1, NULL);
    LineTo(hdc, rc.left, rc.top);
    LineTo(hdc, rc.right - 1, rc.top);

    // 3D Bottom-Right Drop Shadow Bevel
    SelectObject(hdc, darkPen);
    LineTo(hdc, rc.right - 1, rc.bottom - 1);
    LineTo(hdc, rc.left, rc.bottom - 1);

    SelectObject(hdc, oldPen);
    DeleteObject(lightPen);
    DeleteObject(darkPen);
}

static void DrawSkeuomorphicPill(HDC hdc, const RECT& rc, COLORREF bgCol, COLORREF shadowCol, COLORREF highlightCol) {
    HBRUSH bgBrush = CreateSolidBrush(bgCol);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    HPEN darkPen = CreatePen(PS_SOLID, 1, shadowCol);
    HPEN lightPen = CreatePen(PS_SOLID, 1, highlightCol);

    // Recessed Top-Left Inner Shadow (Debossed look matching user reference)
    HPEN oldPen = (HPEN)SelectObject(hdc, darkPen);
    MoveToEx(hdc, rc.left, rc.bottom - 1, NULL);
    LineTo(hdc, rc.left, rc.top);
    LineTo(hdc, rc.right - 1, rc.top);

    // Recessed Bottom-Right Inner Highlight Rim
    SelectObject(hdc, lightPen);
    LineTo(hdc, rc.right - 1, rc.bottom - 1);
    LineTo(hdc, rc.left, rc.bottom - 1);

    SelectObject(hdc, oldPen);
    DeleteObject(darkPen);
    DeleteObject(lightPen);
}

static void DrawSkeuomorphicStitch(HDC hdc, const RECT& rc, COLORREF stitchCol) {
    HPEN stitchPen = CreatePen(PS_DOT, 1, stitchCol);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN oldPen = (HPEN)SelectObject(hdc, stitchPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);

    Rectangle(hdc, rc.left + 3, rc.top + 3, rc.right - 3, rc.bottom - 3);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(stitchPen);
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

    // Skeuomorphic Pitch Black Stealth Surface
    COLORREF baseBg = RGB(8, 10, 14);
    COLORREF panelBg = RGB(16, 20, 28);
    COLORREF recessedBg = RGB(10, 12, 16);
    COLORREF lightBevel = RGB(55, 68, 88);
    COLORREF darkShadow = RGB(4, 5, 7);
    COLORREF cyanGlow = RGB(0, 229, 255);
    COLORREF matrixGreen = RGB(0, 255, 157);
    COLORREF alertRed = RGB(255, 46, 84);

    HBRUSH bgBrush = CreateSolidBrush(baseBg);
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(memDC, TRANSPARENT);

    if (m_isLoading) {
        // --- SKEUOMORPHIC STARTING SERVICE ANIMATION ---
        HFONT logoFont = CreateFontA(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT titleFont = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT subFont = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        // Aegis-X Header (Electric Cyan #00E5FF)
        SelectObject(memDC, logoFont);
        SetTextColor(memDC, cyanGlow);
        RECT logoRect{ 0, 25, clientRect.right, 55 };
        DrawTextA(memDC, "[ AEGIS-X ULTRA KERNEL SHIELD ]", -1, &logoRect, DT_CENTER | DT_SINGLELINE);

        // Loading Status Text
        SelectObject(memDC, titleFont);
        SetTextColor(memDC, RGB(255, 255, 255));
        RECT titleRect{ 0, 62, clientRect.right, 85 };
        DrawTextA(memDC, m_loadingText.c_str(), -1, &titleRect, DT_CENTER | DT_SINGLELINE);

        // Subtitle Text
        SelectObject(memDC, subFont);
        SetTextColor(memDC, RGB(180, 200, 220));
        RECT subRect{ 0, 88, clientRect.right, 108 };
        DrawTextA(memDC, "Real-Time PCIe DMA & Hypervisor Security (by Sahil)", -1, &subRect, DT_CENTER | DT_SINGLELINE);

        // Skeuomorphic Recessed Progress Track
        RECT barOuter{ 90, 120, clientRect.right - 90, 126 };
        DrawSkeuomorphicPill(memDC, barOuter, recessedBg, darkShadow, lightBevel);

        // Glowing Inner Progress Fill
        int totalW = clientRect.right - 180;
        int fillWidth = (totalW * m_loadingProgress) / 100;
        if (fillWidth > 0) {
            RECT barInner{ 90, 120, 90 + fillWidth, 126 };
            HBRUSH fillBrush = CreateSolidBrush(cyanGlow);
            FillRect(memDC, &barInner, fillBrush);
            DeleteObject(fillBrush);
        }

        // Animated Pulsing Dots
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
        // --- SKEUOMORPHIC HIGH-TACTILE DASHBOARD ---
        HFONT nameFont = CreateFontA(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT badgeFont = CreateFontA(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT subFont = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT titleFont = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT pillFont = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        COLORREF matrixGreen = RGB(0, 255, 128); // Vibrant Neon Green

        if (m_hasUpdate) {
            // --- DEDICATED SKEUOMORPHIC UPDATE POPUP (MATCHES USER HAND-DRAWN SKETCH) ---
            RECT updateCard{ 10, 10, clientRect.right - 10, 200 };
            DrawSkeuomorphicCard(memDC, updateCard, panelBg, lightBevel, darkShadow);
            DrawSkeuomorphicStitch(memDC, updateCard, RGB(35, 45, 60));

            // 1. Header Title
            SelectObject(memDC, nameFont);
            SetTextColor(memDC, cyanGlow);
            RECT headRect{ 20, 26, clientRect.right - 20, 50 };
            DrawTextA(memDC, "[ AEGIS-X SECURITY UPDATE AVAILABLE ]", -1, &headRect, DT_CENTER | DT_SINGLELINE);

            // 2. Info Text Lines
            SelectObject(memDC, titleFont);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT line1Rect{ 20, 58, clientRect.right - 20, 78 };
            std::string l1 = "A new client update is ready to install (v" + m_updateInfo.latestVersion + " - " + std::to_string(m_updateInfo.updateSizeMB) + " MB)";
            DrawTextA(memDC, l1.c_str(), -1, &line1Rect, DT_CENTER | DT_SINGLELINE);

            SelectObject(memDC, subFont);
            SetTextColor(memDC, RGB(160, 220, 255));
            RECT line2Rect{ 20, 84, clientRect.right - 20, 104 };
            DrawTextA(memDC, "Changelog: Enhanced Kernel Guard 2.0 & PCIe DMA Shield", -1, &line2Rect, DT_CENTER | DT_SINGLELINE);

            // 3. Centered Interactive Skeuomorphic Button (Matches User Sketch)
            COLORREF actionBg = m_btnHovered ? cyanGlow : RGB(10, 12, 16);
            COLORREF actionTxt = m_btnHovered ? RGB(8, 10, 14) : cyanGlow;

            HBRUSH actionBrush = CreateSolidBrush(actionBg);
            FillRect(memDC, &m_updateBtnRect, actionBrush);
            DeleteObject(actionBrush);

            HPEN btnPen = CreatePen(PS_SOLID, 2, cyanGlow);
            HPEN oldPen = (HPEN)SelectObject(memDC, btnPen);
            HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
            SelectObject(memDC, nullBrush);
            Rectangle(memDC, m_updateBtnRect.left, m_updateBtnRect.top, m_updateBtnRect.right, m_updateBtnRect.bottom);
            SelectObject(memDC, oldPen);
            DeleteObject(btnPen);

            SelectObject(memDC, nameFont);
            SetTextColor(memDC, actionTxt);
            std::string actionStr = m_updateInfo.isDownloading ? "DOWNLOADING UPDATE..." : "[ UPDATE NOW (20 MB) ]";
            DrawTextA(memDC, actionStr.c_str(), -1, &m_updateBtnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            // LEFT PANEL: Skeuomorphic Stacked Security Cards (X: 10 to 240)
            RECT leftCard{ 10, 10, 240, 200 };
            DrawSkeuomorphicCard(memDC, leftCard, panelBg, lightBevel, darkShadow);
            DrawSkeuomorphicStitch(memDC, leftCard, RGB(35, 45, 60));

            // Player Avatar Box (Square 52x52 at X:22, Y:22)
            RECT avatarRect{ 22, 22, 74, 74 };
            DrawSkeuomorphicPill(memDC, avatarRect, recessedBg, darkShadow, lightBevel);

            bool avatarDrawn = false;
            if (!m_profile.avatarPath.empty()) {
                std::wstring wAvatarPath(m_profile.avatarPath.begin(), m_profile.avatarPath.end());
                Gdiplus::Bitmap avatarImg(wAvatarPath.c_str());
                if (avatarImg.GetLastStatus() == Gdiplus::Ok) {
                    Gdiplus::Graphics graphics(memDC);
                    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    graphics.DrawImage(&avatarImg, 23, 23, 50, 50);
                    avatarDrawn = true;
                }
            }

            if (!avatarDrawn) {
                SelectObject(memDC, nameFont);
                SetTextColor(memDC, cyanGlow);
                std::string initialStr = m_profile.personaName.empty() ? "S" : m_profile.personaName.substr(0, 1);
                DrawTextA(memDC, initialStr.c_str(), -1, &avatarRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Cyan Specular Border around Avatar
            HPEN cyanPen = CreatePen(PS_SOLID, 1, cyanGlow);
            HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
            SelectObject(memDC, cyanPen);
            SelectObject(memDC, nullBrush);
            Rectangle(memDC, 22, 22, 74, 74);
            DeleteObject(cyanPen);

            // Player Name
            SelectObject(memDC, nameFont);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT nameRect{ 82, 22, 232, 44 };
            DrawTextA(memDC, m_profile.personaName.c_str(), -1, &nameRect, DT_LEFT | DT_SINGLELINE);

            // Steam Tag Badge
            SelectObject(memDC, badgeFont);
            SetTextColor(memDC, cyanGlow);
            RECT tagRect{ 82, 44, 232, 60 };
            DrawTextA(memDC, "[ STEAM AUTHENTICATED ]", -1, &tagRect, DT_LEFT | DT_SINGLELINE);

            // ID64 Subtitle
            SelectObject(memDC, subFont);
            SetTextColor(memDC, RGB(180, 205, 235));
            std::string idStr = "ID: " + std::to_string(m_profile.steamId64);
            RECT idRect{ 82, 60, 232, 76 };
            DrawTextA(memDC, idStr.c_str(), -1, &idRect, DT_LEFT | DT_SINGLELINE);

            // Skeuomorphic Hardware Security Card Sub-Panel (Y: 90 to 188)
            RECT hwCard{ 20, 90, 230, 188 };
            DrawSkeuomorphicCard(memDC, hwCard, RGB(12, 15, 22), lightBevel, darkShadow);

            // 3D Sphere Gauge Indicator (Center X: 45, Y: 139)
            COLORREF gaugeColor = m_isViolation ? alertRed : matrixGreen;
            HBRUSH sphereBrush = CreateSolidBrush(gaugeColor);
            HPEN nullP = (HPEN)GetStockObject(NULL_PEN);
            SelectObject(memDC, sphereBrush);
            SelectObject(memDC, nullP);
            Ellipse(memDC, 30, 124, 60, 154);
            DeleteObject(sphereBrush);

            SelectObject(memDC, nameFont);
            SetTextColor(memDC, gaugeColor);
            RECT hwTitle{ 68, 104, 220, 126 };
            DrawTextA(memDC, m_isViolation ? "SECURITY ALERT" : "100% ARMORED", -1, &hwTitle, DT_LEFT | DT_SINGLELINE);

            SelectObject(memDC, subFont);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT hwSub{ 68, 128, 220, 146 };
            DrawTextA(memDC, "Hardware Shield Active", -1, &hwSub, DT_LEFT | DT_SINGLELINE);

            SelectObject(memDC, badgeFont);
            SetTextColor(memDC, cyanGlow);
            RECT hwSub2{ 68, 148, 220, 168 };
            DrawTextA(memDC, "TPM 2.0 / Secure Boot / IOMMU", -1, &hwSub2, DT_LEFT | DT_SINGLELINE);

            // RIGHT PANEL: Skeuomorphic Tactile Remote Surface (X: 250 to 490)
            RECT rightCard{ 250, 10, clientRect.right - 10, 200 };
            DrawSkeuomorphicCard(memDC, rightCard, panelBg, lightBevel, darkShadow);
            DrawSkeuomorphicStitch(memDC, rightCard, RGB(35, 45, 60));

            SelectObject(memDC, titleFont);
            SetTextColor(memDC, cyanGlow);
            RECT rTitle{ 260, 18, clientRect.right - 20, 36 };
            DrawTextA(memDC, "CONTROLS", -1, &rTitle, DT_LEFT | DT_SINGLELINE);

            // Debossed Recessed Pill 1
            RECT pill1{ 258, 40, clientRect.right - 20, 68 };
            DrawSkeuomorphicPill(memDC, pill1, recessedBg, darkShadow, lightBevel);
            SelectObject(memDC, pillFont);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT p1Lbl{ 266, 40, 420, 68 };
            DrawTextA(memDC, "[+] KERNEL GUARD:", -1, &p1Lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SetTextColor(memDC, matrixGreen);
            RECT p1Val{ 420, 40, clientRect.right - 20, 68 };
            DrawTextA(memDC, "PASS", -1, &p1Val, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Debossed Recessed Pill 2
            RECT pill2{ 258, 74, clientRect.right - 20, 102 };
            DrawSkeuomorphicPill(memDC, pill2, recessedBg, darkShadow, lightBevel);
            SelectObject(memDC, pillFont);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT p2Lbl{ 266, 74, 420, 102 };
            DrawTextA(memDC, "[+] PCIe DMA SHIELD:", -1, &p2Lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SetTextColor(memDC, matrixGreen);
            RECT p2Val{ 420, 74, clientRect.right - 20, 102 };
            DrawTextA(memDC, "PASS", -1, &p2Val, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Debossed Recessed Pill 3
            RECT pill3{ 258, 108, clientRect.right - 20, 136 };
            DrawSkeuomorphicPill(memDC, pill3, recessedBg, darkShadow, lightBevel);
            SelectObject(memDC, pillFont);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT p3Lbl{ 266, 108, 420, 136 };
            DrawTextA(memDC, "[+] HYPERVISOR GUARD:", -1, &p3Lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SetTextColor(memDC, matrixGreen);
            RECT p3Val{ 420, 108, clientRect.right - 20, 136 };
            DrawTextA(memDC, "PASS", -1, &p3Val, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Debossed Recessed Pill 4
            RECT pill4{ 258, 142, clientRect.right - 20, 170 };
            DrawSkeuomorphicPill(memDC, pill4, recessedBg, darkShadow, lightBevel);
            SelectObject(memDC, pillFont);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT p4Lbl{ 266, 142, 420, 170 };
            DrawTextA(memDC, "[+] ANTI-TAMPER:", -1, &p4Lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SetTextColor(memDC, matrixGreen);
            RECT p4Val{ 420, 142, clientRect.right - 20, 170 };
            DrawTextA(memDC, "PASS", -1, &p4Val, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        // BOTTOM STATUS BAR (X: 10 to 490, Y: 206 to 246)
        RECT bottomBarRect{ 10, 206, clientRect.right - 10, 246 };
        DrawSkeuomorphicCard(memDC, bottomBarRect, RGB(17, 20, 28), lightBevel, darkShadow);

        SelectObject(memDC, subFont);
        COLORREF dotCol = m_isViolation ? alertRed : (m_isProtected ? matrixGreen : RGB(255, 200, 0));

        HBRUSH dotBr = CreateSolidBrush(dotCol);
        HPEN nullP = (HPEN)GetStockObject(NULL_PEN);
        SelectObject(memDC, dotBr);
        SelectObject(memDC, nullP);
        Ellipse(memDC, 22, 220, 32, 230);
        DeleteObject(dotBr);

        SetTextColor(memDC, RGB(255, 255, 255));
        std::string bottomMsg = "Status: " + m_statusText;
        RECT bottomRect{ 38, 217, clientRect.right - 18, 238 };
        DrawTextA(memDC, bottomMsg.c_str(), -1, &bottomRect, DT_LEFT | DT_SINGLELINE);

        DeleteObject(nameFont);
        DeleteObject(badgeFont);
        DeleteObject(subFont);
        DeleteObject(titleFont);
        DeleteObject(pillFont);
    }

    // Copy to screen DC
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

} // namespace aegisx
