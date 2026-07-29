#pragma once

#include <windows.h>
#include <atomic>
#include <thread>

namespace aegisx {

class Watchdog {
public:
    Watchdog() = default;
    ~Watchdog();

    // Starts encrypted heartbeat monitor thread
    bool StartWatchdog(DWORD cs2Pid);

    // Stops watchdog loop
    void StopWatchdog();

private:
    void WatchdogLoop(DWORD cs2Pid);

    std::atomic<bool> m_running{false};
    std::thread m_watchdogThread;
};

} // namespace aegisx
