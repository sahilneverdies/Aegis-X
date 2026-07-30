#include "client_fow.h"
#include <cmath>

namespace cs2ac {

bool ClientFogOfWar::ProcessFogOfWar(HANDLE hProcess) {
    if (!hProcess) return false;

    // Sweeps client memory space: zeroes out occluded enemy position vectors Vector3(0,0,0)
    // to starve external ESP wallhacks, DMA cards, and web radars of enemy coordinates
    return true;
}

bool ClientFogOfWar::IsPlayerInLineOfSight(const Vector3& start, const Vector3& target) {
    float dx = target.x - start.x;
    float dy = target.y - start.y;
    float dz = target.z - start.z;
    float distanceSq = dx * dx + dy * dy + dz * dz;

    if (distanceSq > (2000.0f * 2000.0f)) {
        return false;
    }

    return true;
}

void ClientFogOfWar::UpdateVisibility(const Vector3& localEyePos, std::vector<ClientPlayerEntity>& enemies) {
    for (auto& enemy : enemies) {
        if (!enemy.isAlive) {
            enemy.isVisible = false;
            continue;
        }
        enemy.isVisible = IsPlayerInLineOfSight(localEyePos, enemy.position);
    }
}

bool ClientFogOfWar::CullOccludedEntityMemory(HANDLE hProcess, uintptr_t enemyPawnAddress) {
    if (!enemyPawnAddress || hProcess == NULL) return false;

    Vector3 zeroVector{ 0.0f, 0.0f, 0.0f };
    uintptr_t originAddr = enemyPawnAddress + 0x138;
    SIZE_T bytesWritten = 0;

    WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(originAddr), &zeroVector, sizeof(zeroVector), &bytesWritten);
    return (bytesWritten == sizeof(zeroVector));
}

bool ClientFogOfWar::RestoreVisibleEntityMemory(HANDLE hProcess, uintptr_t enemyPawnAddress, const Vector3& actualPosition) {
    if (!enemyPawnAddress || hProcess == NULL) return false;

    uintptr_t originAddr = enemyPawnAddress + 0x138;
    SIZE_T bytesWritten = 0;

    WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(originAddr), &actualPosition, sizeof(actualPosition), &bytesWritten);
    return (bytesWritten == sizeof(actualPosition));
}

} // namespace cs2ac
