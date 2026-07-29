#include "hook_detector.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <algorithm>

namespace cs2ac {

bool HookDetector::IsAddressInValidModule(HANDLE hProcess, uintptr_t address, std::string& outModuleName) {
    HMODULE hMods[1024];
    DWORD cbNeeded;

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        size_t count = cbNeeded / sizeof(HMODULE);
        for (size_t i = 0; i < count; i++) {
            MODULEINFO modInfo{};
            if (GetModuleInformation(hProcess, hMods[i], &modInfo, sizeof(modInfo))) {
                uintptr_t modStart = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
                uintptr_t modEnd = modStart + modInfo.SizeOfImage;

                if (address >= modStart && address < modEnd) {
                    char szModName[MAX_PATH];
                    if (GetModuleBaseNameA(hProcess, hMods[i], szModName, sizeof(szModName))) {
                        outModuleName = szModName;
                    } else {
                        outModuleName = "UnknownModule";
                    }
                    return true;
                }
            }
        }
    }
    outModuleName = "UnbackedMemory";
    return false;
}

bool HookDetector::ScanInlineHooks(HANDLE hProcess, HMODULE hModule, const char* functionName, DetectionDetail& detail) {
    FARPROC pFunc = GetProcAddress(hModule, functionName);
    if (!pFunc) return false;

    uintptr_t funcAddr = reinterpret_cast<uintptr_t>(pFunc);
    BYTE codeBytes[16]{};
    SIZE_T bytesRead = 0;

    if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(funcAddr), codeBytes, sizeof(codeBytes), &bytesRead) || bytesRead < 5) {
        return false;
    }

    bool hooked = false;
    if (codeBytes[0] == 0xE9 || codeBytes[0] == 0xEB) {
        hooked = true;
    } else if (codeBytes[0] == 0xFF && codeBytes[1] == 0x25) {
        hooked = true;
    } else if (codeBytes[0] == 0x68 && codeBytes[5] == 0xC3) {
        hooked = true;
    }

    if (hooked) {
        detail.type = ScanResult::InlineHookDetected;
        detail.address = funcAddr;
        detail.description = std::string("Inline hook detected on function: ") + functionName;
        IsAddressInValidModule(hProcess, funcAddr, detail.moduleName);
        return true;
    }

    return false;
}

bool HookDetector::ScanVirtualTable(HANDLE hProcess, uintptr_t vtableAddress, size_t methodCount, const std::string& tableName, DetectionDetail& detail) {
    std::vector<uintptr_t> methods(methodCount);
    SIZE_T bytesRead = 0;

    if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(vtableAddress), methods.data(), methodCount * sizeof(uintptr_t), &bytesRead)) {
        return false;
    }

    for (size_t i = 0; i < methodCount; i++) {
        uintptr_t methodAddr = methods[i];
        std::string moduleName;
        if (!IsAddressInValidModule(hProcess, methodAddr, moduleName)) {
            detail.type = ScanResult::VMTHookDetected;
            detail.address = methodAddr;
            detail.description = "VMT function pointer #" + std::to_string(i) + " in table '" + tableName + "' points to unbacked memory.";
            detail.moduleName = moduleName;
            return true;
        }
    }
    return false;
}

bool HookDetector::ScanUnbackedExecutableMemory(HANDLE hProcess, std::vector<DetectionDetail>& detections) {
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t address = 0;
    bool foundDetection = false;

    while (VirtualQueryEx(hProcess, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi))) {
        if ((mbi.State == MEM_COMMIT) &&
            (mbi.Protect & (PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_WRITECOPY))) {

            std::string moduleName;
            if (!IsAddressInValidModule(hProcess, reinterpret_cast<uintptr_t>(mbi.BaseAddress), moduleName)) {
                DetectionDetail detail{};
                detail.type = ScanResult::UnbackedExecutableMemory;
                detail.address = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                detail.description = "Unbacked executable memory page detected (size: " + std::to_string(mbi.RegionSize) + " bytes). Potential manual-mapped internal cheat.";
                detail.moduleName = "UnbackedMemory";
                detections.push_back(detail);
                foundDetection = true;
            }
        }
        if (mbi.RegionSize == 0) {
            break;
        }
        uintptr_t nextAddress = address + mbi.RegionSize;
        if (nextAddress <= address) {
            break; // Overflow protection
        }
        address = nextAddress;
    }

    return foundDetection;
}

bool HookDetector::ScanLoadedModules(HANDLE hProcess, std::vector<DetectionDetail>& detections) {
    static const std::vector<std::string> blacklistedNames = {
        "cheat", "hack", "injector", "xenos", "processhacker", "cheatengine", "minhook", "kiero"
    };

    HMODULE hMods[1024];
    DWORD cbNeeded;
    bool foundBlacklist = false;

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        size_t count = cbNeeded / sizeof(HMODULE);
        for (size_t i = 0; i < count; i++) {
            char szModName[MAX_PATH];
            if (GetModuleBaseNameA(hProcess, hMods[i], szModName, sizeof(szModName))) {
                std::string nameLower = szModName;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

                for (const auto& blacklisted : blacklistedNames) {
                    if (nameLower.find(blacklisted) != std::string::npos) {
                        DetectionDetail detail{};
                        detail.type = ScanResult::BlacklistedModuleLoaded;
                        detail.address = reinterpret_cast<uintptr_t>(hMods[i]);
                        detail.description = "Unauthorized/Blacklisted DLL module loaded: " + std::string(szModName);
                        detail.moduleName = szModName;
                        detections.push_back(detail);
                        foundBlacklist = true;
                    }
                }
            }
        }
    }
    return foundBlacklist;
}

bool HookDetector::RunFullScan(HANDLE hProcess, std::vector<DetectionDetail>& detections) {
    detections.clear();

    ScanUnbackedExecutableMemory(hProcess, detections);
    ScanLoadedModules(hProcess, detections);

    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        DetectionDetail detail{};
        if (ScanInlineHooks(hProcess, hUser32, "PeekMessageW", detail)) {
            detections.push_back(detail);
        }
        if (ScanInlineHooks(hProcess, hUser32, "GetAsyncKeyState", detail)) {
            detections.push_back(detail);
        }
    }

    return !detections.empty();
}

} // namespace cs2ac
