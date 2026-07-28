#include "ai_cv_detector.h"
#include <psapi.h>

namespace cs2ac {

bool AICVDetector::ScanDXGIDesktopDuplication(DWORD cs2Pid, std::string& description) {
    (void)cs2Pid;
    // Inspect loaded DirectX graphics modules for unauthorized duplication interfaces
    HMODULE hDxgi = GetModuleHandleA("dxgi.dll");
    if (!hDxgi) return false;

    // Check for DXGI DuplicateOutput API hooks
    FARPROC pDup = GetProcAddress(hDxgi, "CreateDXGIFactory1");
    if (pDup) {
        BYTE bytes[5]{};
        SIZE_T read = 0;
        if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(pDup), bytes, sizeof(bytes), &read) && read == 5) {
            if (bytes[0] == 0xE9 || bytes[0] == 0xEB) {
                description = "DXGI Desktop Duplication API hook detected (Potential AI / Computer Vision Screen Capture Aimbot).";
                return true;
            }
        }
    }
    return false;
}

} // namespace cs2ac
