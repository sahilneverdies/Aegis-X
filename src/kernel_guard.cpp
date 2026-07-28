#include "kernel_guard.h"
#include <psapi.h>
#include <algorithm>

namespace cs2ac {

bool KernelGuard::ScanBYOVDDrivers(std::vector<VulnerableDriverMatch>& detectedDrivers) {
    static const std::vector<std::string> vulnerableDriverList = {
        "capcom.sys", "rtcore64.sys", "gdrv.sys", "atszio64.sys", "ene.sys", "procexp.sys"
    };

    LPVOID drivers[1024];
    DWORD cbNeeded;
    bool foundVulnerable = false;

    if (EnumDeviceDrivers(drivers, sizeof(drivers), &cbNeeded) && cbNeeded < sizeof(drivers)) {
        int count = cbNeeded / sizeof(LPVOID);
        for (int i = 0; i < count; i++) {
            char szDriver[MAX_PATH];
            if (GetDeviceDriverBaseNameA(drivers[i], szDriver, sizeof(szDriver))) {
                std::string nameLower = szDriver;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

                for (const auto& vuln : vulnerableDriverList) {
                    if (nameLower == vuln) {
                        VulnerableDriverMatch match{};
                        match.driverName = szDriver;
                        match.description = "Blacklisted BYOVD kernel driver detected: " + std::string(szDriver);
                        detectedDrivers.push_back(match);
                        foundVulnerable = true;
                    }
                }
            }
        }
    }
    return foundVulnerable;
}

bool KernelGuard::EnforceHandleRestrictions(HANDLE hCS2Process) {
    if (!hCS2Process) return false;

    // Enforce process handle mitigation policies
    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY policy{};
    policy.MicrosoftSignedOnly = 1;

    SetProcessMitigationPolicy(
        ProcessSignaturePolicy,
        &policy,
        sizeof(policy)
    );

    return true;
}

} // namespace cs2ac
