#include "ml_behavior_engine.h"
#include <cmath>
#include <numeric>

namespace cs2ac {

float MLBehaviorEngine::CalculateCurveEntropy(const std::vector<TrajectoryPoint>& trajectory) {
    if (trajectory.size() < 4) return 1.0f; // Need sufficient trajectory samples

    std::vector<float> angularAccelerations;
    for (size_t i = 2; i < trajectory.size(); i++) {
        float d1 = trajectory[i - 1].yaw - trajectory[i - 2].yaw;
        float d2 = trajectory[i].yaw - trajectory[i - 1].yaw;
        float accel = std::abs(d2 - d1);
        angularAccelerations.push_back(accel);
    }

    if (angularAccelerations.empty()) return 1.0f;

    // Calculate variance
    float mean = std::accumulate(angularAccelerations.begin(), angularAccelerations.end(), 0.0f) / angularAccelerations.size();
    float variance = 0.0f;
    for (float a : angularAccelerations) {
        variance += (a - mean) * (a - mean);
    }
    variance /= angularAccelerations.size();

    // Pure linear / Bézier humanizer aimbots exhibit unrealistically zero variance in angular acceleration
    return variance;
}

bool MLBehaviorEngine::DetectHardwareMacroController(const std::vector<uint64_t>& inputTimestampsMs) {
    if (inputTimestampsMs.size() < 10) return false;

    // Analyze delta distribution for fixed-interval hardware micro-delays (e.g. KMBox / Arduino millisecond timer loop)
    std::vector<uint64_t> deltas;
    for (size_t i = 1; i < inputTimestampsMs.size(); i++) {
        deltas.push_back(inputTimestampsMs[i] - inputTimestampsMs[i - 1]);
    }

    // Check for identical repeat deltas (hardware clock loop)
    size_t identicalDeltas = 0;
    for (size_t i = 1; i < deltas.size(); i++) {
        if (deltas[i] == deltas[i - 1] && deltas[i] > 0) {
            identicalDeltas++;
        }
    }

    // If over 70% of timing deltas are identical millisecond intervals -> Hardware macro controller detected
    return (identicalDeltas > (deltas.size() * 0.7));
}

} // namespace cs2ac
