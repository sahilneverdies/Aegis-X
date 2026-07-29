#include "watchdog.h"
#include <chrono>

namespace aegisx {

Watchdog::~Watchdog() {
    StopWatchdog();
}

bool Watchdog::StartWatchdog(DWORD cs2Pid) {
    if (m_running) return true;
    m_running = true;
    m_watchdogThread = std::thread(&Watchdog::WatchdogLoop, this, cs2Pid);
    return true;
}

void Watchdog::StopWatchdog() {
    if (m_running) {
        m_running = false;
        if (m_watchdogThread.joinable()) {
            m_watchdogThread.join();
        }
    }
}

void Watchdog::WatchdogLoop(DWORD cs2Pid) {
    ULONGLONG lastTick = GetTickCount64();

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ULONGLONG currentTick = GetTickCount64();
        ULONGLONG delta = currentTick - lastTick;

        // If thread execution was suspended/frozen for > 2000ms -> Anti-Cheat suspension bypass attempt detected!
        if (delta > 2000) {
            HANDLE hCS2 = OpenProcess(PROCESS_TERMINATE, FALSE, cs2Pid);
            if (hCS2) {
                TerminateProcess(hCS2, 0xDEAD);
                CloseHandle(hCS2);
            }
            ExitProcess(0xDEAD);
        }

        lastTick = currentTick;
    }
}

} // namespace aegisx
