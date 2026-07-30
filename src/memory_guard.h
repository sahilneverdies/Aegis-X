#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

namespace cs2ac {

struct CodeSectionIntegrity {
    std::string moduleName;
    uintptr_t baseAddress;
    size_t sectionSize;
    uint32_t originalCRC32;
    std::vector<uint8_t> initialBytes;
};

class MemoryGuard {
public:
    MemoryGuard() = default;
    ~MemoryGuard() = default;

    // Registers a module's .text section for continuous CRC32 integrity verification
    bool RegisterModuleSection(HANDLE hProcess, HMODULE hModule, const std::string& moduleName);

    // Verifies registered module code sections against byte patching and memory tampering
    bool VerifyCodeIntegrity(HANDLE hProcess, std::string& tamperedModuleName);

    // Calculates CRC32 checksum over a memory buffer
    static uint32_t CalculateCRC32(const uint8_t* data, size_t length);

private:
    std::vector<CodeSectionIntegrity> m_monitoredSections;
};

} // namespace cs2ac
