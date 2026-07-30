#include "external_detector.h"
#include <tlhelp32.h>
#include <algorithm>

namespace cs2ac {

struct EnumParams {
    DWORD cs2Pid;
    RECT cs2Rect;
    std::vector<ExternalDetection>* detections;
};

#include <psapi.h>

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    EnumParams* params = reinterpret_cast<EnumParams*>(lParam);

    if (!IsWindowVisible(hwnd)) return TRUE;

    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);

    if (windowPid == params->cs2Pid || windowPid == 0) return TRUE;

    char className[256]{};
    GetClassNameA(hwnd, className, sizeof(className));
    std::string classStr = className;

    char titleBuf[256]{};
    GetWindowTextA(hwnd, titleBuf, sizeof(titleBuf));
    std::string titleStr = titleBuf;

    std::string titleLower = titleStr;
    std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
    std::string classLower = classStr;
    std::transform(classLower.begin(), classLower.end(), classLower.begin(), ::tolower);

    // Scan window titles and class names for cheat keywords
    std::vector<std::string> cheatKeywords = {
        "cs2-external", "cs2_external", "cs2-esp", "cs2_esp", "esp-recode",
        "cs2_dumper", "cs2-dumper", "aimbot", "wallhack", "triggerbot",
        "bhop_script", "bunnyhop", "injector", "dumper", "kiero", "minhook"
    };

    for (const auto& kw : cheatKeywords) {
        if (titleLower.find(kw) != std::string::npos || classLower.find(kw) != std::string::npos) {
            ExternalDetection det{};
            det.type = "ExternalOverlayDetected";
            det.description = "Unauthorized external cheat window detected: Title='" + titleStr + "'";
            det.windowHandle = hwnd;
            params->detections->push_back(det);
            return TRUE;
        }
    }

    // Whitelist official Steam / Windows / System / Helper window classes & false-positive classes like 'wa'
    if (classStr == "wa" || classStr == "SysShadow" || classStr == "tooltips_class32" ||
        classStr == "IME" || classStr == "MSCTFIME UI" || classStr == "FocusProxy" ||
        classStr == "Valve_Steam_Overlay" || classStr == "vguiPopupWindow" || classStr == "Steam" ||
        classStr == "SteamOverlayHost" || classStr == "CursorVisualClass" || classStr == "ThumbnailDeviceHelperWnd" ||
        classStr == "Button" || classStr == "Shell_TrayWnd" || classStr == "Progman" || classStr == "WorkerW" ||
        titleStr == "wa") {
        return TRUE;
    }

    // Inspect process owner path
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, windowPid);
    if (hProc) {
        char exePath[MAX_PATH]{};
        DWORD szPath = sizeof(exePath);
        bool gotPath = false;

        // Query full process image name
        if (QueryFullProcessImageNameA(hProc, 0, exePath, &szPath)) {
            gotPath = true;
        } else if (GetModuleFileNameExA(hProc, NULL, exePath, sizeof(exePath)) > 0) {
            gotPath = true;
        }

        if (gotPath) {
            std::string procPath = exePath;
            std::transform(procPath.begin(), procPath.end(), procPath.begin(), ::tolower);

            // Whitelist Windows system processes, IDEs, browsers, WhatsApp, Discord, Steam, NVIDIA, AMD
            if (procPath.find("explorer.exe") != std::string::npos ||
                procPath.find("dwm.exe") != std::string::npos ||
                procPath.find("svchost.exe") != std::string::npos ||
                procPath.find("csrss.exe") != std::string::npos ||
                procPath.find("shellexperiencehost.exe") != std::string::npos ||
                procPath.find("searchhost.exe") != std::string::npos ||
                procPath.find("startmenuexperiencehost.exe") != std::string::npos ||
                procPath.find("textinputhost.exe") != std::string::npos ||
                procPath.find("applicationframehost.exe") != std::string::npos ||
                procPath.find("antigravity") != std::string::npos ||
                procPath.find("code.exe") != std::string::npos ||
                procPath.find("devenv.exe") != std::string::npos ||
                procPath.find("whatsapp") != std::string::npos ||
                procPath.find("chrome.exe") != std::string::npos ||
                procPath.find("msedge.exe") != std::string::npos ||
                procPath.find("firefox.exe") != std::string::npos ||
                procPath.find("spotify.exe") != std::string::npos ||
                procPath.find("steam.exe") != std::string::npos ||
                procPath.find("gameoverlayui.exe") != std::string::npos ||
                procPath.find("steamwebhelper.exe") != std::string::npos ||
                procPath.find("discord.exe") != std::string::npos ||
                procPath.find("nvidia") != std::string::npos ||
                procPath.find("nvcontainer.exe") != std::string::npos ||
                procPath.find("radeon") != std::string::npos) {
                CloseHandle(hProc);
                return TRUE;
            }
        }
        CloseHandle(hProc);
    }

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    LONG style = GetWindowLong(hwnd, GWL_STYLE);

    if ((exStyle & WS_EX_TOPMOST) || (exStyle & WS_EX_TRANSPARENT) || (exStyle & WS_EX_LAYERED) || (style & WS_POPUP)) {
        RECT winRect;
        if (GetWindowRect(hwnd, &winRect)) {
            if (winRect.left < params->cs2Rect.right && winRect.right > params->cs2Rect.left &&
                winRect.top < params->cs2Rect.bottom && winRect.bottom > params->cs2Rect.top) {

                ExternalDetection det{};
                det.type = "ExternalOverlayDetected";
                det.description = "Unauthorized external transparent overlay window detected over game screen";
                det.windowHandle = hwnd;
                params->detections->push_back(det);
            }
        }
    }

    return TRUE;
}

bool ExternalDetector::ScanExternalProcesses(DWORD cs2Pid, std::vector<ExternalDetection>& detections) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    std::vector<std::string> cheatKeywords = {
        "cs2-external", "cs2_external", "cs2-esp", "cs2_esp", "esp-recode",
        "cs2_dumper", "cs2-dumper", "aimbot", "wallhack", "triggerbot",
        "bhop_script", "bunnyhop", "injector", "dumper", "kiero", "minhook"
    };

    DWORD currPid = GetCurrentProcessId();

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == cs2Pid || pe.th32ProcessID == currPid || pe.th32ProcessID == 0) {
                continue;
            }

            char szExeA[MAX_PATH]{};
            WideCharToMultiByte(CP_ACP, 0, pe.szExeFile, -1, szExeA, sizeof(szExeA), NULL, NULL);
            std::string exeName = szExeA;
            std::string exeLower = exeName;
            std::transform(exeLower.begin(), exeLower.end(), exeLower.begin(), ::tolower);

            for (const auto& kw : cheatKeywords) {
                if (exeLower.find(kw) != std::string::npos) {
                    ExternalDetection det{};
                    det.type = "ExternalCheatProcessDetected";
                    det.description = "Unauthorized external cheat process detected running on system (" + exeName + ").";
                    det.windowHandle = NULL;
                    detections.push_back(det);
                    break;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return !detections.empty();
}

bool ExternalDetector::ScanExternalOverlays(DWORD cs2Pid, std::vector<ExternalDetection>& detections) {
    if (cs2Pid == 0) return false;

    // 1. Scan external processes for cheat executables
    ScanExternalProcesses(cs2Pid, detections);

    // 2. Scan active windows for overlays
    HWND hCS2Window = NULL;
    HWND hCurr = GetTopWindow(NULL);

    while (hCurr) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hCurr, &pid);
        if (pid == cs2Pid && IsWindowVisible(hCurr)) {
            hCS2Window = hCurr;
            break;
        }
        hCurr = GetNextWindow(hCurr, GW_HWNDNEXT);
    }

    if (hCS2Window) {
        RECT cs2Rect;
        if (GetWindowRect(hCS2Window, &cs2Rect)) {
            EnumParams params{ cs2Pid, cs2Rect, &detections };
            EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&params));
        }
    }

    return !detections.empty();
}

} // namespace cs2ac
