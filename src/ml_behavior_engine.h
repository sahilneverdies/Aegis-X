#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace cs2ac {

struct TrajectoryPoint {
    float pitch;
    float yaw;
    uint64_t timestampMs;
};

class MLBehaviorEngine {
public:
    MLBehaviorEngine() = default;
    ~MLBehaviorEngine() = default;

    // Evaluates mouse trajectory curve variance & entropy to detect humanized algorithmic aimbots
    float CalculateCurveEntropy(const std::vector<TrajectoryPoint>& trajectory);

    // Detects hardware input microcontrollers (KMBox / Arduino) via subtick micro-timing analysis
    bool DetectHardwareMacroController(const std::vector<uint64_t>& inputTimestampsMs);
};

} // namespace cs2ac
