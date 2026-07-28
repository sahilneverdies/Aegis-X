#pragma once

#include <windows.h>
#include <vector>
#include <string>

namespace cs2ac {

struct DMADeviceMatch {
    std::string deviceName;
    std::string hardwareID;
    std::string description;
};

class DMAShield {
public:
    DMAShield() = default;
    ~DMAShield() = default;

    // Scans PCIe device configuration spaces for FPGA DMA boards (CaptainDMA, EnigmaDMA, Screamer, Xilinx)
    bool ScanPCIeDMADevices(std::vector<DMADeviceMatch>& detectedDevices);

    // Checks IOMMU / VT-d hardware memory virtualization status
    bool VerifyIOMMUProtectionStatus();
};

} // namespace cs2ac
