#pragma once

#include <windows.h>
#include <wininet.h>
#include <string>
#include <functional>

namespace aegisx {

struct UpdateInfo {
    bool updateAvailable = false;
    std::string latestVersion = "3.1.0";
    std::string currentVersion = "3.0.0";
    int updateSizeMB = 20;
    std::string downloadUrl = "https://github.com/sahilneverdies/Aegis-X/releases/download/v3.1.0/AegisX_ClientGuard.exe";
    std::string changelog = "Enhanced Anti-DMA & Kernel Guard 2.0";
    bool isDownloading = false;
    int downloadProgress = 0;
};

class AutoUpdater {
public:
    static UpdateInfo CheckForRemoteUpdate();
    static bool StartUpdateDownload(const UpdateInfo& info, std::function<void(int progressPct, bool completed)> onProgress);
};

} // namespace aegisx
