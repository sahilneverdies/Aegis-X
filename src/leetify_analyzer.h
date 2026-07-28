#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace cs2ac {

struct LeetifyProfileStats {
    uint64_t steamId64;
    std::string playerName;
    float aimScore;             // 0 to 100 (e.g. 100 = perfect aimbot score)
    float timeToDamageMs;       // Median reaction time in ms (e.g. < 100ms or 0ms = triggerbot/aimbot)
    float preaimEfficiency;     // 0% to 100% pre-aim placement on unspotted enemies
    float leetifyRating;        // Overall rating (Pro benchmark e.g. donk ~ +7.5)
    float crosshairPlacementDeg;// Median degrees off target when peeking
};

enum class ProfileAnomalyRisk {
    Normal = 0,
    Suspicious,
    HighRiskInhuman,
    ConfirmedCheatProfile
};

struct ProfileAnalysisResult {
    ProfileAnomalyRisk riskLevel;
    std::vector<std::string> flaggedReasons;
    float overallAnomalyScore; // 0.0 to 1.0
};

class LeetifyAnalyzer {
public:
    LeetifyAnalyzer() = default;
    ~LeetifyAnalyzer() = default;

    // Evaluates player statistics against professional benchmarks (donk / m0NESY baselines)
    ProfileAnalysisResult EvaluateProfile(const LeetifyProfileStats& stats);

    // Calculates real-time telemetry from live match events (No API key required!)
    LeetifyProfileStats CalculateLiveTelemetry(uint64_t steamId64, const std::string& name, const std::vector<float>& reactionTimeSamplesMs, float hitRate);

    // Formats a diagnostic string for server/admin announcements
    std::string FormatAnomalyReport(const LeetifyProfileStats& stats, const ProfileAnalysisResult& result);
};

} // namespace cs2ac
