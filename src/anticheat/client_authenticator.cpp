#include "client_authenticator.h"
#include <chrono>

namespace cs2ac::server {

ClientAuthenticator g_ClientAuthenticator;

bool ClientAuthenticator::VerifyPlayerToken(uint64_t steamId64, const std::string& token, const std::string& secretKey) {
    if (token.empty() || steamId64 == 0) return false;

    size_t dotPos = token.find('.');
    if (dotPos == std::string::npos) return false;

    std::string timestampStr = token.substr(0, dotPos);
    uint64_t timestamp = std::stoull(timestampStr);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto currentTimestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    // Verify token age (reject if older than 5 minutes / 300 seconds)
    if (currentTimestamp < timestamp || (currentTimestamp - timestamp) > 300) {
        return false;
    }

    RecordAuthenticatedClient(steamId64);
    return true;
}

void ClientAuthenticator::RecordAuthenticatedClient(uint64_t steamId64) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    m_activeSessions[steamId64] = std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

void ClientAuthenticator::RemoveClient(uint64_t steamId64) {
    m_activeSessions.erase(steamId64);
}

bool ClientAuthenticator::IsClientAuthenticated(uint64_t steamId64) const {
    auto it = m_activeSessions.find(steamId64);
    return (it != m_activeSessions.end());
}

} // namespace cs2ac::server
