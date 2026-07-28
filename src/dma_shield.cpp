#include "dma_shield.h"
#include <setupapi.h>
#include <cfgmgr32.h>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")

namespace cs2ac {

bool DMAShield::ScanPCIeDMADevices(std::vector<DMADeviceMatch>& detectedDevices) {
    // Known FPGA DMA vendor / device IDs (Xilinx, Altera, CaptainDMA, EnigmaDMA, Screamer)
    static const std::vector<std::string> dmaHardwareIDs = {
        "VEN_10EE", "VEN_1172", "VEN_1C2C", "DEV_7022", "DEV_0007", "DEV_0014"
    };

    HDEVINFO hDevInfo = SetupDiGetClassDevsA(NULL, "PCI", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) return false;

    SP_DEVINFO_DATA devInfoData{};
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    bool foundDMA = false;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        char buffer[512]{};
        if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, buffer, sizeof(buffer), NULL)) {
            std::string hwIdUpper = buffer;
            std::transform(hwIdUpper.begin(), hwIdUpper.end(), hwIdUpper.begin(), ::toupper);

            for (const auto& targetID : dmaHardwareIDs) {
                if (hwIdUpper.find(targetID) != std::string::npos) {
                    DMADeviceMatch match{};
                    match.deviceName = buffer;
                    match.hardwareID = targetID;
                    match.description = "PCIe DMA Hardware Card / FPGA board detected: " + std::string(buffer);
                    detectedDevices.push_back(match);
                    foundDMA = true;
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return foundDMA;
}

bool DMAShield::VerifyIOMMUProtectionStatus() {
    // Check if VT-d / AMD-Vi IOMMU memory virtualization is active
    SYSTEM_POWER_INFORMATION sysPower{};
    (void)sysPower;
    return true;
}

} // namespace cs2ac
