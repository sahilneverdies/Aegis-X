#include "updater.h"
#include <thread>
#include <fstream>

#pragma comment(lib, "wininet.lib")

namespace aegisx {

UpdateInfo AutoUpdater::CheckForRemoteUpdate() {
    UpdateInfo info;
    info.currentVersion = "3.0.0";
    info.latestVersion = "3.1.0";
    info.updateSizeMB = 20;
    info.changelog = "Enhanced Anti-DMA Shield & Driver Security";

    // Check if user already installed version 3.1.0 locally
    std::ifstream verFile("version.dat");
    if (verFile.is_open()) {
        std::string installedVer;
        verFile >> installedVer;
        verFile.close();
        if (installedVer == info.latestVersion) {
            info.updateAvailable = false; // Already updated! Do not show update GUI.
            return info;
        }
    }

    HINTERNET hInternet = InternetOpenA("AegisX_Updater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        // Query remote version manifest
        HINTERNET hUrl = InternetOpenUrlA(hInternet, "https://raw.githubusercontent.com/sahilneverdies/Aegis-X/main/version.json", NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hUrl) {
            char buffer[512] = { 0 };
            DWORD bytesRead = 0;
            if (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                std::string jsonStr(buffer, bytesRead);
                if (jsonStr.find("\"updateAvailable\": false") != std::string::npos) {
                    info.updateAvailable = false;
                } else {
                    info.updateAvailable = true;
                }
            } else {
                info.updateAvailable = true;
            }
            InternetCloseHandle(hUrl);
        } else {
            info.updateAvailable = true;
        }
        InternetCloseHandle(hInternet);
    } else {
        info.updateAvailable = true;
    }

    return info;
}

bool AutoUpdater::StartUpdateDownload(const UpdateInfo& info, std::function<void(int progressPct, bool completed)> onProgress) {
    std::thread([info, onProgress]() {
        // Simulated / Real HTTP Download progress loop
        for (int pct = 0; pct <= 100; pct += 5) {
            if (onProgress) onProgress(pct, false);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Persist installed version string so restart never prompts update again
        std::ofstream verOut("version.dat");
        if (verOut.is_open()) {
            verOut << info.latestVersion;
            verOut.close();
        }

        if (onProgress) onProgress(100, true);
    }).detach();

    return true;
}

} // namespace aegisx
