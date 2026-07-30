#include "external_detector.h"
#include <tlhelp32.h>
#include <algorithm>
#include <psapi.h>

namespace cs2ac {

struct EnumParams {
    DWORD cs2Pid;
    RECT cs2Rect;
    std::vector<ExternalDetection>* detections;
};

bool ExternalDetector::IsProcessWhitelisted(const std::string& procPath) {
    std::string pathLower = procPath;
    std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

    std::vector<std::string> whitelist = {
        "explorer.exe", "dwm.exe", "svchost.exe", "csrss.exe", "shellexperiencehost.exe",
        "searchhost.exe", "startmenuexperiencehost.exe", "textinputhost.exe", "applicationframehost.exe",
        "antigravity", "code.exe", "devenv.exe", "whatsapp", "chrome.exe", "msedge.exe",
        "firefox.exe", "spotify.exe", "steam.exe", "gameoverlayui.exe", "steamwebhelper.exe",
        "discord.exe", "nvidia", "nvcontainer.exe", "radeon", "aegisx"
    };

    for (const auto& w : whitelist) {
        if (pathLower.find(w) != std::string::npos) return true;
    }
    return false;
}

bool ExternalDetector::ScanProcessMemorySignatures(HANDLE hProc, std::string& outSignature) {
    if (!hProc) return false;

    // Signatures of CS2 memory dumpers, offsets, and external cheat frameworks
    std::vector<std::pair<std::string, std::string>> signatures = {
        { "dwLocalPlayerPawn", "CS2 Memory Dumper Offset (dwLocalPlayerPawn)" },
        { "dwEntityList", "CS2 Entity List Offset (dwEntityList)" },
        { "dwViewMatrix", "CS2 View Matrix Offset (dwViewMatrix)" },
        { "C_CSPlayerPawn", "CS2 Player Pawn Entity Class Signature" },
        { "cs2-external-esp", "CS2 External ESP Signature" },
        { "cs2_dumper", "CS2 Offset Dumper Signature" },
        { "kiero", "DirectX/Kiero Hook Framework" },
        { "MinHook", "MinHook In-Process Detour Engine" }
    };

    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t addr = 0;

    while (VirtualQueryEx(hProc, reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
        if ((mbi.State == MEM_COMMIT) && (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE))) {
            if (mbi.RegionSize > 0 && mbi.RegionSize <= 10 * 1024 * 1024) { // Only scan regions up to 10MB
                std::vector<char> buffer(mbi.RegionSize);
                SIZE_T bytesRead = 0;
                if (ReadProcessMemory(hProc, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead) && bytesRead > 0) {
                    std::string memChunk(buffer.data(), bytesRead);
                    for (const auto& sig : signatures) {
                        if (memChunk.find(sig.first) != std::string::npos) {
                            outSignature = sig.second;
                            return true;
                        }
                    }
                }
            }
        }
        if (mbi.RegionSize == 0) break;
        uintptr_t nextAddr = addr + mbi.RegionSize;
        if (nextAddr <= addr) break;
        addr = nextAddr;
    }

    return false;
}

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
        if (QueryFullProcessImageNameA(hProc, 0, exePath, &szPath)) {
            if (ExternalDetector::IsProcessWhitelisted(exePath)) {
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

            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
            if (hProc) {
                char exePath[MAX_PATH]{};
                DWORD szPath = sizeof(exePath);
                if (QueryFullProcessImageNameA(hProc, 0, exePath, &szPath)) {
                    std::string procPath = exePath;
                    if (IsProcessWhitelisted(procPath)) {
                        CloseHandle(hProc);
                        continue;
                    }
                }

                // 1. Keyword check in filename
                bool matchedKeyword = false;
                for (const auto& kw : cheatKeywords) {
                    if (exeLower.find(kw) != std::string::npos) {
                        matchedKeyword = true;
                        ExternalDetection det{};
                        det.type = "ExternalCheatProcessDetected";
                        det.description = "Unauthorized external cheat process detected running on system (" + exeName + ").";
                        det.windowHandle = NULL;
                        detections.push_back(det);
                        break;
                    }
                }

                // 2. Name-Independent Memory Signature Scan (Detects cheat even if renamed to app.exe / 123.exe / game.exe!)
                if (!matchedKeyword) {
                    std::string sigFound;
                    if (ScanProcessMemorySignatures(hProc, sigFound)) {
                        ExternalDetection det{};
                        det.type = "ExternalCheatSignatureDetected";
                        det.description = "Unauthorized cheat memory signature detected inside process '" + exeName + "' (" + sigFound + ").";
                        det.windowHandle = NULL;
                        detections.push_back(det);
                    }
                }

                CloseHandle(hProc);
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return !detections.empty();
}

bool ExternalDetector::ScanExternalOverlays(DWORD cs2Pid, std::vector<ExternalDetection>& detections) {
    if (cs2Pid == 0) return false;

    // 1. Scan external processes for cheat executables & memory signatures
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
        if (!GetWindowRect(hCS2Window, &cs2Rect)) return !detections.empty();

        EnumParams params{ cs2Pid, cs2Rect, &detections };
        EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&params));
    }

    return !detections.empty();
}

} // namespace cs2ac
