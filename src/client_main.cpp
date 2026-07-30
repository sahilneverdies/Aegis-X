#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#include "hook_detector.h"
#include "external_detector.h"
#include "anti_debug.h"
#include "client_fow.h"
#include "behavior_ac.h"
#include "memory_guard.h"
#include "kernel_guard.h"
#include "ml_behavior_engine.h"
#include "dma_shield.h"
#include "hypervisor_detector.h"
#include "ai_cv_detector.h"
#include "anti_tamper.h"
#include "watchdog.h"

DWORD FindCS2ProcessID() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    DWORD pid = 0;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (wcscmp(pe.szExeFile, L"cs2.exe") == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return pid;
}

void AbortGameAndNotifyUser(const std::vector<cs2ac::DetectionDetail>& detections) {
    DWORD pid = FindCS2ProcessID();
    if (pid != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess) {
            TerminateProcess(hProcess, 0xFACE);
            CloseHandle(hProcess);
        }
    }

    std::string alertMsg = "Aegis-X Protection Suite (by Sahil)\n\n";
    alertMsg += "Game launch blocked or terminated due to security violation:\n\n";

    for (const auto& det : detections) {
        alertMsg += "- " + det.description + "\n";
    }

    alertMsg += "\nPlease disable any unauthorized internal cheats, external overlays, DMA cards, hypervisors, or hooks and try again.";

    MessageBoxA(
        NULL,
        alertMsg.c_str(),
        "Aegis-X Anti-Cheat - Security Violation Detected",
        MB_OK | MB_ICONERROR | MB_SYSTEMMODAL
    );
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 0. Enable unbypassable process self-protection & handle DACL restrictions
    aegisx::AntiTamper::EnableSelfProtection();
    cs2ac::AntiDebug::HideCurrentThread();

    // 1. Check for active debuggers & hardware breakpoints
    if (cs2ac::AntiDebug::IsDebuggerPresentCheck() || cs2ac::AntiDebug::CheckHardwareBreakpoints()) {
        std::vector<cs2ac::DetectionDetail> debugDetections = {
            { cs2ac::ScanResult::DebuggerDetected, "Active debugger or hardware breakpoint detected on system.", 0, "Debugger" }
        };
        AbortGameAndNotifyUser(debugDetections);
        return 1;
    }

    // 2. Scan PCIe bus for hardware DMA cards (CaptainDMA, EnigmaDMA, Screamer, FPGA boards)
    cs2ac::DMAShield dmaShield;
    std::vector<cs2ac::DMADeviceMatch> dmaMatches;
    if (dmaShield.ScanPCIeDMADevices(dmaMatches)) {
        std::vector<cs2ac::DetectionDetail> detections;
        for (const auto& dma : dmaMatches) {
            detections.push_back({ cs2ac::ScanResult::BlacklistedModuleLoaded, dma.description, 0, dma.hardwareID });
        }
        AbortGameAndNotifyUser(detections);
        return 1;
    }

    // 3. Check for Type-1 / Type-2 Hypervisors & VM-Exit timing latency
    cs2ac::HypervisorDetector hypervisorDetector;
    std::string hvVendor;
    if (hypervisorDetector.CheckHypervisorCPUID(hvVendor)) {
        std::vector<cs2ac::DetectionDetail> detections = {
            { cs2ac::ScanResult::DebuggerDetected, "Hypervisor / Virtual Machine cheat detected (Vendor: " + hvVendor + ").", 0, "Hypervisor" }
        };
        AbortGameAndNotifyUser(detections);
        return 1;
    }

    // 4. Scan kernel drivers for blacklisted BYOVD exploit drivers
    cs2ac::KernelGuard kernelGuard;
    std::vector<cs2ac::VulnerableDriverMatch> driverDetections;
    if (kernelGuard.ScanBYOVDDrivers(driverDetections)) {
        std::vector<cs2ac::DetectionDetail> detections;
        for (const auto& drv : driverDetections) {
            detections.push_back({ cs2ac::ScanResult::BlacklistedModuleLoaded, drv.description, 0, drv.driverName });
        }
        AbortGameAndNotifyUser(detections);
        return 1;
    }

    // 5. Continuous Auto-Start Daemon Loop (Detects CS2 startup or launches via Steam)
    DWORD cs2Pid = 0;
    while (cs2Pid == 0) {
        cs2Pid = FindCS2ProcessID();
        if (cs2Pid != 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    HANDLE hCS2 = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, cs2Pid);
    if (!hCS2) {
        MessageBoxA(NULL, "Aegis-X requires Administrator privileges to protect process memory.", "Aegis-X Anti-Cheat", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Apply process handle mitigations & start watchdog heartbeat monitor
    kernelGuard.EnforceHandleRestrictions(hCS2);
    aegisx::Watchdog watchdog;
    watchdog.StartWatchdog(cs2Pid);

    cs2ac::HookDetector detector;
    cs2ac::ExternalDetector extDetector;
    cs2ac::ClientFogOfWar clientFOW;
    cs2ac::MemoryGuard memoryGuard;

    DWORD exitCode = 0;
    while (GetExitCodeProcess(hCS2, &exitCode) && exitCode == STILL_ACTIVE) {
        std::vector<cs2ac::DetectionDetail> detections;

        // 1. Scan internal hooks, VMT hijacking, & RWX unbacked memory
        if (detector.RunFullScan(hCS2, detections)) {
            watchdog.StopWatchdog();
            CloseHandle(hCS2);
            AbortGameAndNotifyUser(detections);
            return 0xFACE;
        }

        // 2. Verify code section CRC32 integrity against byte patching
        std::string tamperedModule;
        if (!memoryGuard.VerifyCodeIntegrity(hCS2, tamperedModule)) {
            watchdog.StopWatchdog();
            CloseHandle(hCS2);
            detections.push_back({ cs2ac::ScanResult::InlineHookDetected, "Memory code patching detected in module: " + tamperedModule, 0, tamperedModule });
            AbortGameAndNotifyUser(detections);
            return 0xFACE;
        }

        // 3. Scan external transparent overlays placed over CS2
        std::vector<cs2ac::ExternalDetection> extDetections;
        if (extDetector.ScanExternalOverlays(cs2Pid, extDetections)) {
            watchdog.StopWatchdog();
            CloseHandle(hCS2);
            std::vector<cs2ac::DetectionDetail> convertedDetections;
            for (const auto& ext : extDetections) {
                convertedDetections.push_back({ cs2ac::ScanResult::BlacklistedModuleLoaded, ext.description, reinterpret_cast<uintptr_t>(ext.windowHandle), "ExternalOverlay" });
            }
            AbortGameAndNotifyUser(convertedDetections);
            return 0xFACE;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    watchdog.StopWatchdog();
    CloseHandle(hCS2);
    return 0;
}
