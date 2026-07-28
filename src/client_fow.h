#pragma once

#include <windows.h>
#include <vector>
#include <cstdint>

namespace cs2ac {

struct Vector3 {
    float x, y, z;
};

struct ClientPlayerEntity {
    uint32_t index;
    uintptr_t pawnAddress;
    Vector3 position;
    Vector3 eyePosition;
    bool isAlive;
    bool isVisible;
    uint8_t team;
};

class ClientFogOfWar {
public:
    ClientFogOfWar() = default;
    ~ClientFogOfWar() = default;

    // Updates client-side line-of-sight visibility for all enemy players
    void UpdateVisibility(const Vector3& localEyePos, std::vector<ClientPlayerEntity>& enemies);

    // Culls / zero-masks entity coordinates and bone matrices in local client memory for occluded enemies
    bool CullOccludedEntityMemory(HANDLE hProcess, uintptr_t enemyPawnAddress);

    // Restores original entity coordinates when enemy enters line of sight
    bool RestoreVisibleEntityMemory(HANDLE hProcess, uintptr_t enemyPawnAddress, const Vector3& actualPosition);

private:
    bool IsPlayerInLineOfSight(const Vector3& start, const Vector3& target);
};

} // namespace cs2ac
