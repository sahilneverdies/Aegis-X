#pragma once

#include <windows.h>
#include <vector>
#include <string>

namespace cs2ac {

struct ClientDetection {
    std::string moduleName;
    std::string description;
    float confidence;
};

class ClientBehaviorAC {
public:
    ClientBehaviorAC() = default;
    ~ClientBehaviorAC() = default;

    bool AnalyzeAngleDelta(float pitchDelta, float yawDelta, ClientDetection& detection);
    bool AnalyzeJumpSequence(const std::vector<uint32_t>& jumpTicks, ClientDetection& detection);
};

} // namespace cs2ac
