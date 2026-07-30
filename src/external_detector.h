#pragma once

#include <windows.h>
#include <vector>
#include <string>

namespace cs2ac {

struct ExternalDetection {
    std::string type;
    std::string description;
    HWND windowHandle;
};

class ExternalDetector {
public:
    ExternalDetector() = default;
    ~ExternalDetector() = default;

    bool ScanExternalOverlays(DWORD cs2Pid, std::vector<ExternalDetection>& detections);
    bool ScanExternalProcesses(DWORD cs2Pid, std::vector<ExternalDetection>& detections);
};

} // namespace cs2ac
