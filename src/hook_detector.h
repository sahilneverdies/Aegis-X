#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

namespace cs2ac {

enum class ScanResult {
    Clean = 0,
    InlineHookDetected,
    VMTHookDetected,
    UnbackedExecutableMemory,
    BlacklistedModuleLoaded,
    DebuggerDetected,
    BhopDetected,
    AimbotDetected,
    AimlockDetected,
    AntiaimDetected
};

struct DetectionDetail {
    ScanResult type;
    std::string description;
    uintptr_t address;
    std::string moduleName;
};

class HookDetector {
public:
    HookDetector() = default;
    ~HookDetector() = default;

    bool RunFullScan(HANDLE hProcess, std::vector<DetectionDetail>& detections);
    bool ScanInlineHooks(HANDLE hProcess, HMODULE hModule, const char* functionName, DetectionDetail& detail);
    bool ScanVirtualTable(HANDLE hProcess, uintptr_t vtableAddress, size_t methodCount, const std::string& tableName, DetectionDetail& detail);
    bool ScanUnbackedExecutableMemory(HANDLE hProcess, std::vector<DetectionDetail>& detections);
    bool ScanLoadedModules(HANDLE hProcess, std::vector<DetectionDetail>& detections);

    static bool IsAddressInValidModule(HANDLE hProcess, uintptr_t address, std::string& outModuleName);
};

} // namespace cs2ac
