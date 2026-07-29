#pragma once

#include <windows.h>

namespace aegisx {

class AntiTamper {
public:
    AntiTamper() = default;
    ~AntiTamper() = default;

    // Protects AegisX_Guard.exe process handle against external termination & injection
    static bool EnableSelfProtection();

    // Enforces strict process mitigation policies (MicrosoftSignedOnly, DynamicCodeOptOut)
    static bool EnforceProcessMitigations();

    // Prevents non-administrative handles from reading or writing AegisX memory
    static bool RestrictProcessAccess();
};

} // namespace aegisx
