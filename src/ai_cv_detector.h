#pragma once

#include <windows.h>
#include <string>

namespace cs2ac {

class AICVDetector {
public:
    AICVDetector() = default;
    ~AICVDetector() = default;

    // Detects DXGI Desktop Duplication API (IDXGIOutputDuplication) frame capture hooks
    bool ScanDXGIDesktopDuplication(DWORD cs2Pid, std::string& description);
};

} // namespace cs2ac
