#include "memory_guard.h"
#include "hook_detector.h"
#include <psapi.h>

namespace cs2ac {

uint32_t MemoryGuard::CalculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

bool MemoryGuard::RegisterModuleSection(HANDLE hProcess, HMODULE hModule, const std::string& moduleName) {
    MODULEINFO modInfo{};
    if (!GetModuleInformation(hProcess, hModule, &modInfo, sizeof(modInfo))) return false;

    uintptr_t base = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
    size_t size = modInfo.SizeOfImage;

    std::vector<uint8_t> buffer(size);
    SIZE_T bytesRead = 0;

    if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(base), buffer.data(), size, &bytesRead) || bytesRead == 0) {
        return false;
    }

    CodeSectionIntegrity section{};
    section.moduleName = moduleName;
    section.baseAddress = base;
    section.sectionSize = bytesRead;
    section.originalCRC32 = CalculateCRC32(buffer.data(), bytesRead);
    section.initialBytes = buffer;

    m_monitoredSections.push_back(section);
    return true;
}

bool MemoryGuard::VerifyCodeIntegrity(HANDLE hProcess, std::string& tamperedModuleName) {
    if (m_monitoredSections.empty()) {
        HMODULE hMods[1024];
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
            size_t count = cbNeeded / sizeof(HMODULE);
            for (size_t i = 0; i < count; i++) {
                char szModName[MAX_PATH];
                if (GetModuleBaseNameA(hProcess, hMods[i], szModName, sizeof(szModName))) {
                    std::string modStr = szModName;
                    if (modStr == "client.dll" || modStr == "engine2.dll") {
                        RegisterModuleSection(hProcess, hMods[i], modStr);
                    }
                }
            }
        }
    }

    for (auto& section : m_monitoredSections) {
        std::vector<uint8_t> buffer(section.sectionSize);
        SIZE_T bytesRead = 0;

        if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(section.baseAddress), buffer.data(), section.sectionSize, &bytesRead) && bytesRead > 0) {
            uint32_t currentCRC = CalculateCRC32(buffer.data(), bytesRead);
            if (currentCRC != section.originalCRC32) {
                // Inspect byte differences for overlay vs unauthorized cheat patches
                bool unauthorizedTamper = false;
                for (size_t i = 0; i < section.sectionSize && i < buffer.size() && i < section.initialBytes.size(); i++) {
                    if (buffer[i] != section.initialBytes[i]) {
                        uintptr_t patchAddr = section.baseAddress + i;

                        // Check if the modified byte forms a JMP/CALL instruction
                        uintptr_t destAddr = 0;
                        if (buffer[i] == 0xE9 && (i + 4) < buffer.size()) { // JMP rel32
                            int32_t relOffset = 0;
                            memcpy(&relOffset, &buffer[i + 1], sizeof(int32_t));
                            destAddr = patchAddr + 5 + relOffset;
                        } else if (buffer[i] == 0xFF && (i + 5) < buffer.size() && buffer[i + 1] == 0x25) { // JMP [rip+disp32]
                            int32_t disp = 0;
                            memcpy(&disp, &buffer[i + 2], sizeof(int32_t));
                            uintptr_t ptrAddr = patchAddr + 6 + disp;
                            ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(ptrAddr), &destAddr, sizeof(uintptr_t), NULL);
                        }

                        if (destAddr != 0) {
                            std::string targetMod;
                            if (!HookDetector::IsAddressInValidModule(hProcess, destAddr, targetMod)) {
                                unauthorizedTamper = true;
                                break;
                            }
                        }
                    }
                }

                if (unauthorizedTamper) {
                    tamperedModuleName = section.moduleName;
                    return false; // Confirmed unauthorized cheat memory patch!
                } else {
                    // Update baseline CRC to account for legitimate Steam / OS overlay patches
                    section.originalCRC32 = currentCRC;
                    section.initialBytes = buffer;
                }
            }
        }
    }
    return true;
}

} // namespace cs2ac
