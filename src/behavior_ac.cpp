#include "behavior_ac.h"
#include <cmath>

namespace cs2ac {

bool ClientBehaviorAC::AnalyzeAngleDelta(float pitchDelta, float yawDelta, ClientDetection& detection) {
    float totalDelta = std::sqrt(pitchDelta * pitchDelta + yawDelta * yawDelta);

    if (totalDelta > 45.0f) {
        detection.moduleName = "AimbotSnap";
        detection.description = "Client-side snap aimbot detected (Angle delta: " + std::to_string(totalDelta) + " deg/tick).";
        detection.confidence = 0.98f;
        return true;
    }

    return false;
}

bool ClientBehaviorAC::AnalyzeJumpSequence(const std::vector<uint32_t>& jumpTicks, ClientDetection& detection) {
    if (jumpTicks.size() < 5) return false;

    size_t perfectHops = 0;
    for (size_t i = 1; i < jumpTicks.size(); i++) {
        if ((jumpTicks[i] - jumpTicks[i - 1]) == 1) {
            perfectHops++;
        }
    }

    if (perfectHops >= 4) {
        detection.moduleName = "BhopScript";
        detection.description = "Client-side automated bunnyhop script detected (" + std::to_string(perfectHops) + " consecutive frame-perfect jumps).";
        detection.confidence = 0.95f;
        return true;
    }

    return false;
}

} // namespace cs2ac
