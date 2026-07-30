#include "hook_detector.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <algorithm>

namespace cs2ac {

bool HookDetector::IsAddressInValidModule(HANDLE hProcess, uintptr_t address, std::string& outModuleName) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQueryEx(hProcess, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi))) {
        char szModPath[MAX_PATH]{};
        if (GetModuleFileNameExA(hProcess, reinterpret_cast<HMODULE>(mbi.AllocationBase), szModPath, sizeof(szModPath)) > 0) {
            outModuleName = szModPath;
            return true; // Memory is backed by a valid DLL file on disk
        }
        if (GetModuleFileNameExA(hProcess, reinterpret_cast<HMODULE>(mbi.BaseAddress), szModPath, sizeof(szModPath)) > 0) {
            outModuleName = szModPath;
            return true;
        }
    }

    HMODULE hMods[1024];
    DWORD cbNeeded = 0;
    if (EnumProcessModulesEx(hProcess, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL)) {
        size_t count = cbNeeded / sizeof(HMODULE);
        for (size_t i = 0; i < count; i++) {
            MODULEINFO modInfo{};
            if (GetModuleInformation(hProcess, hMods[i], &modInfo, sizeof(modInfo))) {
                uintptr_t modStart = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
                uintptr_t modEnd = modStart + modInfo.SizeOfImage;

                if (address >= modStart && address < modEnd) {
                    char szModName[MAX_PATH]{};
                    if (GetModuleBaseNameA(hProcess, hMods[i], szModName, sizeof(szModName))) {
                        outModuleName = szModName;
                    } else {
                        outModuleName = "ValidModule";
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

    uintptr_t destinationAddress = 0;
    if (codeBytes[0] == 0xE9) { // JMP rel32
        int32_t relOffset = 0;
        memcpy(&relOffset, &codeBytes[1], sizeof(int32_t));
        destinationAddress = funcAddr + 5 + relOffset;
    } else if (codeBytes[0] == 0xFF && codeBytes[1] == 0x25) { // JMP [rip+disp32]
        int32_t disp = 0;
        memcpy(&disp, &codeBytes[2], sizeof(int32_t));
        uintptr_t ptrAddr = funcAddr + 6 + disp;
        ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(ptrAddr), &destinationAddress, sizeof(uintptr_t), NULL);
    }

    if (destinationAddress != 0) {
        std::string modName;
        if (!IsAddressInValidModule(hProcess, destinationAddress, modName)) {
            // Verify if unbacked target contains PE header 'MZ' (0x5A4D) of a manual-mapped cheat DLL
            WORD magic = 0;
            SIZE_T read = 0;
            if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(destinationAddress), &magic, sizeof(magic), &read) && read == sizeof(magic) && magic == IMAGE_DOS_SIGNATURE) {
                char hexAddr[32];
                snprintf(hexAddr, sizeof(hexAddr), "0x%llX", static_cast<unsigned long long>(destinationAddress));
                detail.type = ScanResult::InlineHookDetected;
                detail.address = destinationAddress;
                detail.description = std::string("Unauthorized manual-mapped cheat hook on ") + functionName + " (PE 'MZ' header at " + hexAddr + ").";
                detail.moduleName = modName;
                return true;
            }
        }
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
        DWORD isExec = mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
        if ((mbi.State == MEM_COMMIT) && isExec && !(mbi.Protect & PAGE_GUARD)) {
            std::string moduleName;
            if (!IsAddressInValidModule(hProcess, reinterpret_cast<uintptr_t>(mbi.BaseAddress), moduleName)) {
                DetectionDetail detail{};
                detail.type = ScanResult::UnbackedExecutableMemory;
                detail.address = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                detail.description = "Manual-mapped internal cheat code detected in unbacked memory at 0x" + std::to_string(detail.address);
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
