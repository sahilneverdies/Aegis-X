#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>

namespace cs2ac::server {

class ClientAuthenticator {
public:
    ClientAuthenticator() = default;
    ~ClientAuthenticator() = default;

    // Validates player handshake token on connect
    bool VerifyPlayerToken(uint64_t steamId64, const std::string& token, const std::string& secretKey);

    void RecordAuthenticatedClient(uint64_t steamId64);
    void RemoveClient(uint64_t steamId64);
    bool IsClientAuthenticated(uint64_t steamId64) const;

private:
    std::unordered_map<uint64_t, uint64_t> m_activeSessions; // steamId64 -> timestamp
};

extern ClientAuthenticator g_ClientAuthenticator;

} // namespace cs2ac::server
