#include "external_detector.h"
#include <tlhelp32.h>
#include <algorithm>

namespace cs2ac {

struct EnumParams {
    DWORD cs2Pid;
    RECT cs2Rect;
    std::vector<ExternalDetection>* detections;
};

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    EnumParams* params = reinterpret_cast<EnumParams*>(lParam);

    if (!IsWindowVisible(hwnd)) return TRUE;

    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);

    if (windowPid == params->cs2Pid || windowPid == 0) return TRUE;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    if ((exStyle & WS_EX_TOPMOST) && ((exStyle & WS_EX_TRANSPARENT) || (exStyle & WS_EX_LAYERED))) {
        RECT winRect;
        if (GetWindowRect(hwnd, &winRect)) {
            if (winRect.left < params->cs2Rect.right && winRect.right > params->cs2Rect.left &&
                winRect.top < params->cs2Rect.bottom && winRect.bottom > params->cs2Rect.top) {

                char title[256]{};
                GetWindowTextA(hwnd, title, sizeof(title));

                char className[256]{};
                GetClassNameA(hwnd, className, sizeof(className));

                ExternalDetection det{};
                det.type = "ExternalOverlayDetected";
                det.description = "Unauthorized external transparent overlay window detected: Class='" + std::string(className) + "', Title='" + std::string(title) + "'";
                det.windowHandle = hwnd;
                params->detections->push_back(det);
            }
        }
    }

    return TRUE;
}

bool ExternalDetector::ScanExternalOverlays(DWORD cs2Pid, std::vector<ExternalDetection>& detections) {
    if (cs2Pid == 0) return false;

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

    if (!hCS2Window) return false;

    RECT cs2Rect;
    if (!GetWindowRect(hCS2Window, &cs2Rect)) return false;

    EnumParams params{ cs2Pid, cs2Rect, &detections };
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&params));

    return !detections->empty();
}

} // namespace cs2ac
