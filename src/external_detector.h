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
    bool ScanCS2ProcessHandles(DWORD cs2Pid, std::vector<ExternalDetection>& detections);
    static bool ScanProcessMemorySignatures(HANDLE hProc, std::string& outSignature);
    static bool IsProcessWhitelisted(const std::string& procPath);
};

} // namespace cs2ac
