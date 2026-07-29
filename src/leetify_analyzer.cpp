#include "leetify_analyzer.h"
#include <sstream>
#include <iomanip>

namespace cs2ac {

ProfileAnalysisResult LeetifyAnalyzer::EvaluateProfile(const LeetifyProfileStats& stats) {
    ProfileAnalysisResult result{};
    result.riskLevel = ProfileAnomalyRisk::Normal;
    result.overallAnomalyScore = 0.0f;

    // Baseline Pro Benchmarks:
    // Human elite pro (e.g. donk / m0NESY): Time-To-Damage ~ 150ms - 180ms, Aim Score ~ 90-95, Leetify Rating ~ +7.5
    // Inhuman Anomalies: Time-To-Damage < 100ms or ~0ms, Aim Score = 100, Pre-aim > 98% on occluded enemies

    // 1. Check for Inhuman Reaction Time (< 100ms or 0ms triggerbot)
    if (stats.timeToDamageMs < 100.0f) {
        if (stats.timeToDamageMs <= 10.0f) {
            result.flaggedReasons.push_back("Instant 0ms triggerbot reaction time (" + std::to_string(static_cast<int>(stats.timeToDamageMs)) + "ms)");
            result.overallAnomalyScore += 0.5f;
        } else {
            result.flaggedReasons.push_back("Inhuman reaction time (< 100ms): " + std::to_string(static_cast<int>(stats.timeToDamageMs)) + "ms (Human pro baseline ~150-180ms)");
            result.overallAnomalyScore += 0.35f;
        }
    }

    // 2. Check for Perfect 100 Aim Rating Score
    if (stats.aimScore >= 99.0f) {
        result.flaggedReasons.push_back("Perfect Aim Rating Score (" + std::to_string(static_cast<int>(stats.aimScore)) + "/100)");
        result.overallAnomalyScore += 0.3f;
    }

    // 3. Check for Wallhack Pre-Aim Efficiency (> 95% on occluded unspotted targets)
    if (stats.preaimEfficiency >= 95.0f) {
        result.flaggedReasons.push_back("Inhuman pre-aim placement efficiency (" + std::to_string(static_cast<int>(stats.preaimEfficiency)) + "% on hidden enemies)");
        result.overallAnomalyScore += 0.25f;
    }

    // 4. Check for Rating Outlier exceeding elite pro player benchmarks
    if (stats.leetifyRating > 9.5f) {
        result.flaggedReasons.push_back("Statistical rating outlier (+ " + std::to_string(stats.leetifyRating) + ") exceeding elite CS2 pro benchmarks (donk ~ +7.5)");
        result.overallAnomalyScore += 0.2f;
    }

    // Determine overall risk level
    if (result.overallAnomalyScore >= 0.7f) {
        result.riskLevel = ProfileAnomalyRisk::ConfirmedCheatProfile;
    } else if (result.overallAnomalyScore >= 0.35f) {
        result.riskLevel = ProfileAnomalyRisk::HighRiskInhuman;
    } else if (result.overallAnomalyScore > 0.0f) {
        result.riskLevel = ProfileAnomalyRisk::Suspicious;
    }

    return result;
}

LeetifyProfileStats LeetifyAnalyzer::CalculateLiveTelemetry(uint64_t steamId64, const std::string& name, const std::vector<float>& reactionTimeSamplesMs, float hitRate) {
    LeetifyProfileStats stats{};
    stats.steamId64 = steamId64;
    stats.playerName = name;
    stats.aimScore = std::min(100.0f, hitRate * 100.0f);

    if (reactionTimeSamplesMs.empty()) {
        stats.timeToDamageMs = 200.0f; // Default baseline
    } else {
        float sum = 0.0f;
        for (float sample : reactionTimeSamplesMs) {
            sum += sample;
        }
        stats.timeToDamageMs = sum / reactionTimeSamplesMs.size();
    }

    stats.preaimEfficiency = 80.0f;
    stats.leetifyRating = (stats.aimScore > 95.0f && stats.timeToDamageMs < 100.0f) ? 10.5f : 4.0f;

    return stats;
}

std::string LeetifyAnalyzer::FormatAnomalyReport(const LeetifyProfileStats& stats, const ProfileAnalysisResult& result) {
    std::ostringstream ss;
    ss << "[Aegis-X Telemetry] Player: " << stats.playerName << " (SteamID64: " << stats.steamId64 << ")\n";
    ss << "Aim Score: " << stats.aimScore << "/100 | Time-To-Damage: " << stats.timeToDamageMs << "ms | Rating: +" << stats.leetifyRating << "\n";
    ss << "Flagged Anomaly Reasons:\n";
    for (const auto& reason : result.flaggedReasons) {
        ss << " • " << reason << "\n";
    }
    return ss.str();
}

} // namespace cs2ac
