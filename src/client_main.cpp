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

    std::vector<std::string> uniqueDescriptions;
    for (const auto& det : detections) {
        if (std::find(uniqueDescriptions.begin(), uniqueDescriptions.end(), det.description) == uniqueDescriptions.end()) {
            uniqueDescriptions.push_back(det.description);
        }
    }

    size_t count = 0;
    for (const auto& desc : uniqueDescriptions) {
        alertMsg += "- " + desc + "\n";
        if (++count >= 5) break;
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

    // Outer continuous daemon loop - Aegis-X stays running 24/7 in the background
    while (true) {
        // 1. Check for active debuggers & hardware breakpoints
        if (cs2ac::AntiDebug::IsDebuggerPresentCheck() || cs2ac::AntiDebug::CheckHardwareBreakpoints()) {
            std::vector<cs2ac::DetectionDetail> debugDetections = {
                { cs2ac::ScanResult::DebuggerDetected, "Active debugger or hardware breakpoint detected on system.", 0, "Debugger" }
            };
            AbortGameAndNotifyUser(debugDetections);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
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
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        // 3. Check for Type-1 / Type-2 Hypervisors & VM-Exit timing latency
        cs2ac::HypervisorDetector hypervisorDetector;
        std::string hvVendor;
        if (hypervisorDetector.CheckHypervisorCPUID(hvVendor)) {
            std::vector<cs2ac::DetectionDetail> detections = {
                { cs2ac::ScanResult::DebuggerDetected, "Hypervisor / Virtual Machine cheat detected (Vendor: " + hvVendor + ").", 0, "Hypervisor" }
            };
            AbortGameAndNotifyUser(detections);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
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
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        // 5. Continuous Auto-Start Daemon Loop (Polls for CS2 launch in background)
        DWORD cs2Pid = 0;
        while (cs2Pid == 0) {
            cs2Pid = FindCS2ProcessID();
            if (cs2Pid != 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        HANDLE hCS2 = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, cs2Pid);
        if (!hCS2) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // Apply process handle mitigations & start watchdog heartbeat monitor
        kernelGuard.EnforceHandleRestrictions(hCS2);
        aegisx::Watchdog watchdog;
        watchdog.StartWatchdog(cs2Pid);

        cs2ac::HookDetector detector;
        cs2ac::ExternalDetector extDetector;
        cs2ac::ClientFogOfWar clientFOW;
        cs2ac::MemoryGuard memoryGuard;
        cs2ac::AICVDetector cvDetector;

        DWORD exitCode = 0;
        bool cheatDetected = false;

        while (GetExitCodeProcess(hCS2, &exitCode) && exitCode == STILL_ACTIVE) {
            std::vector<cs2ac::DetectionDetail> detections;

            // Scan internal hooks, VMT hijacking, & RWX unbacked memory
            if (detector.RunFullScan(hCS2, detections)) {
                cheatDetected = true;
                watchdog.StopWatchdog();
                CloseHandle(hCS2);
                AbortGameAndNotifyUser(detections);
                break;
            }

            // Verify code section integrity against byte patching
            std::string tamperedModule;
            if (!memoryGuard.VerifyCodeIntegrity(hCS2, tamperedModule)) {
                cheatDetected = true;
                watchdog.StopWatchdog();
                CloseHandle(hCS2);
                detections.push_back({ cs2ac::ScanResult::InlineHookDetected, "Memory code patching detected in module: " + tamperedModule, 0, tamperedModule });
                AbortGameAndNotifyUser(detections);
                break;
            }

            // Scan external transparent overlays placed over CS2
            std::vector<cs2ac::ExternalDetection> extDetections;
            if (extDetector.ScanExternalOverlays(cs2Pid, extDetections)) {
                cheatDetected = true;
                watchdog.StopWatchdog();
                CloseHandle(hCS2);
                std::vector<cs2ac::DetectionDetail> convertedDetections;
                for (const auto& ext : extDetections) {
                    convertedDetections.push_back({ cs2ac::ScanResult::BlacklistedModuleLoaded, ext.description, reinterpret_cast<uintptr_t>(ext.windowHandle), "ExternalOverlay" });
                }
                AbortGameAndNotifyUser(convertedDetections);
                break;
            }

            // Scan DXGI Desktop Duplication API hooks for AI vision aimbots
            std::string cvDesc;
            if (cvDetector.ScanDXGIDesktopDuplication(cs2Pid, cvDesc)) {
                cheatDetected = true;
                watchdog.StopWatchdog();
                CloseHandle(hCS2);
                detections.push_back({ cs2ac::ScanResult::BlacklistedModuleLoaded, cvDesc, 0, "AICVDetector" });
                AbortGameAndNotifyUser(detections);
                break;
            }

            // Update client Fog-Of-War culling
            (void)clientFOW;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (!cheatDetected) {
            watchdog.StopWatchdog();
            CloseHandle(hCS2);
        }

        // Brief sleep before resetting daemon loop for next CS2 launch
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
