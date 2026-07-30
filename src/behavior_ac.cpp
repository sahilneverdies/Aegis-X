#include "behavior_ac.h"
#include <cmath>
#include <chrono>

namespace cs2ac {

ClientBehaviorAC::ClientBehaviorAC() {
    GetCursorPos(&m_lastMousePos);
    m_lastMouseTime = GetTickCount();
}

bool ClientBehaviorAC::ScanClientBehavior(HANDLE hCS2, std::vector<DetectionDetail>& detections) {
    bool detected = false;
    DWORD now = GetTickCount();

    // 1. BHOP & AUTOSTRAFE BEHAVIORAL SCAN
    bool isSpaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    if (isSpaceDown) {
        if (now - m_lastJumpTick < 160 && now - m_lastJumpTick > 10) {
            m_jumpHistory.push_back(now);
            if (m_jumpHistory.size() > 10) {
                m_jumpHistory.erase(m_jumpHistory.begin());
            }

            // Check for repeated frame-perfect hop deltas (14-35ms intervals)
            size_t framePerfectCount = 0;
            for (size_t i = 1; i < m_jumpHistory.size(); i++) {
                DWORD delta = m_jumpHistory[i] - m_jumpHistory[i - 1];
                if (delta >= 14 && delta <= 35) {
                    framePerfectCount++;
                }
            }

            if (framePerfectCount >= 4) {
                DetectionDetail det{};
                det.type = ScanResult::BhopDetected;
                det.description = "Automated Bunnyhop / Autostrafe script detected (" + std::to_string(framePerfectCount) + " frame-perfect hops).";
                det.moduleName = "BehaviorAC";
                detections.push_back(det);
                detected = true;
            }
        }
        m_lastJumpTick = now;
    }

    // 2. AIMBOT & AIMLOCK SNAP BEHAVIORAL SCAN
    bool isFiring = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    POINT currMousePos;
    if (GetCursorPos(&currMousePos)) {
        int dx = currMousePos.x - m_lastMousePos.x;
        int dy = currMousePos.y - m_lastMousePos.y;
        double dist = std::sqrt(dx * dx + dy * dy);
        DWORD timeDelta = now - m_lastMouseTime;

        if (timeDelta > 0 && isFiring) {
            double mouseSpeed = (dist / (double)timeDelta) * 1000.0; // px/sec

            // Inhuman snap (> 3500 px/sec instant directional snap onto target)
            if (dist > 180 && mouseSpeed > 3500.0) {
                m_consecutiveSnaps++;
                if (m_consecutiveSnaps >= 2) {
                    DetectionDetail det{};
                    det.type = ScanResult::AimbotDetected;
                    det.description = "Aimbot snap / Aimlock target tracking detected (Velocity: " + std::to_string((int)mouseSpeed) + " px/s).";
                    det.moduleName = "BehaviorAC";
                    detections.push_back(det);
                    detected = true;
                }
            } else {
                if (m_consecutiveSnaps > 0) m_consecutiveSnaps--;
            }

            // 3. ANTIAIM / SPINBOT BEHAVIORAL SCAN
            if (dist > 450 && mouseSpeed > 8000.0) {
                DetectionDetail det{};
                det.type = ScanResult::AntiaimDetected;
                det.description = "Antiaim / Spinbot irregular view angle rotation detected.";
                det.moduleName = "BehaviorAC";
                detections.push_back(det);
                detected = true;
            }
        }

        m_lastMousePos = currMousePos;
        m_lastMouseTime = now;
    }

    return detected;
}

} // namespace cs2ac
