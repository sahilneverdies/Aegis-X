#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include "hook_detector.h"

namespace cs2ac {

class ClientBehaviorAC {
public:
    ClientBehaviorAC();
    ~ClientBehaviorAC() = default;

    bool ScanClientBehavior(HANDLE hCS2, std::vector<DetectionDetail>& detections);

private:
    DWORD m_lastJumpTick = 0;
    std::vector<uint32_t> m_jumpHistory;
    POINT m_lastMousePos{ 0, 0 };
    DWORD m_lastMouseTime = 0;
    size_t m_consecutiveSnaps = 0;
};

} // namespace cs2ac
