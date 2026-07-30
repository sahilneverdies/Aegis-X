#include "hypervisor_detector.h"
#include <intrin.h>
#include <windows.h>

namespace cs2ac {

bool HypervisorDetector::CheckHypervisorCPUID(std::string& hypervisorVendor) {
    int cpuInfo[4]{};

    // Standard CPUID leaf 1: check hypervisor feature flag (ECX bit 31)
    __cpuid(cpuInfo, 1);
    if ((cpuInfo[2] & (1 << 31)) == 0) {
        return false; // No hypervisor flag present
    }

    // Hypervisor vendor leaf 0x40000000
    __cpuid(cpuInfo, 0x40000000);
    char vendor[13]{};
    memcpy(vendor, &cpuInfo[1], 4);
    memcpy(vendor + 4, &cpuInfo[2], 4);
    memcpy(vendor + 8, &cpuInfo[3], 4);
    vendor[12] = '\0';

    hypervisorVendor = vendor;
    // Whitelist native Windows 11 Hyper-V / Core Isolation (Microsoft Hv)
    if (hypervisorVendor == "Microsoft Hv") {
        return false;
    }
    return true; // Unauthorized custom hypervisor detected!
}

bool HypervisorDetector::MeasureVMExitLatency(uint64_t& outCycleDifference) {
    int cpuInfo[4]{};

    // Measure cycle count across CPUID call (forces VM-Exit if running inside hypervisor)
    uint64_t t1 = __rdtsc();
    __cpuid(cpuInfo, 0);
    uint64_t t2 = __rdtsc();

    outCycleDifference = t2 - t1;

    // Bare metal native CPUID takes < 150 cycles. Hypervisor VM-Exit takes > 800 cycles.
    return (outCycleDifference > 800);
}

} // namespace cs2ac
