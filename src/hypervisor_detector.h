#pragma once

#include <cstdint>
#include <string>

namespace cs2ac {

class HypervisorDetector {
public:
    HypervisorDetector() = default;
    ~HypervisorDetector() = default;

    // Detects hypervisor presence via CPUID instruction leaf 0x40000000
    bool CheckHypervisorCPUID(std::string& hypervisorVendor);

    // Measures CPU cycle latency over CPUID calls to detect VM-Exits forced by type-1/type-2 hypervisors
    bool MeasureVMExitLatency(uint64_t& outCycleDifference);
};

} // namespace cs2ac
