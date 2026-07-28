#pragma once

#include <windows.h>
#include <vector>
#include <string>

namespace cs2ac {

struct VulnerableDriverMatch {
    std::string driverName;
    std::string description;
};

class KernelGuard {
public:
    KernelGuard() = default;
    ~KernelGuard() = default;

    // Scans system drivers for known BYOVD (Bring Your Own Vulnerable Driver) exploit files
    bool ScanBYOVDDrivers(std::vector<VulnerableDriverMatch>& detectedDrivers);

    // Strips unauthorized process handle rights from external processes accessing cs2.exe
    bool EnforceHandleRestrictions(HANDLE hCS2Process);
};

} // namespace cs2ac
