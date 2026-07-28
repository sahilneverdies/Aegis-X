#include "memory_guard.h"
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

    // Read initial code bytes
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

    m_monitoredSections.push_back(section);
    return true;
}

bool MemoryGuard::VerifyCodeIntegrity(HANDLE hProcess, std::string& tamperedModuleName) {
    for (const auto& section : m_monitoredSections) {
        std::vector<uint8_t> buffer(section.sectionSize);
        SIZE_T bytesRead = 0;

        if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(section.baseAddress), buffer.data(), section.sectionSize, &bytesRead) && bytesRead > 0) {
            uint32_t currentCRC = CalculateCRC32(buffer.data(), bytesRead);
            if (currentCRC != section.originalCRC32) {
                tamperedModuleName = section.moduleName;
                return false; // Integrity violation detected!
            }
        }
    }
    return true; // All code sections intact
}

} // namespace cs2ac
