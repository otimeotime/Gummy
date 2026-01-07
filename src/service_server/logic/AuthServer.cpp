#include "AuthServer.hpp"

AuthServer::AuthServer(UserDAO* userdao) : userDao(userdao) {
}

AuthServer::~AuthServer() {
    delete userDao;
}

bool AuthServer::login(const std::string& username, const std::string& password, UserData& outUser) {
    auto userOpt = userDao->authenticate(username, password);
    if (userOpt) {
        outUser = *userOpt;
        return true;
    }
    return false;
}

bool AuthServer::reg(const std::string& username, const std::string& password, long& outUserId) {
    long userId = userDao->createUser(username, password);
    if (userId != -1) {
        outUserId = userId;
        return true;
    }
    return false;
}

bool AuthServer::changePassword(long userId, const std::string& newPassword) {
    return userDao->updatePassword(userId, newPassword);
}

std::vector<UserData> AuthServer::getAllUsers() {
    return userDao->getAllUsers();
}

static std::string ClampTimestamp19(const std::string& ts) {
    if (ts.size() <= 19) return ts;
    return ts.substr(0, 19);
}

bool AuthServer::getProfile(const std::string& username, ResGetProfile& outProfile) {
    std::memset(&outProfile, 0, sizeof(outProfile));

    if (username.empty()) {
        outProfile.isSuccess = false;
        std::snprintf(outProfile.message, sizeof(outProfile.message), "Not logged in.");
        return false;
    }

    const auto headerOpt = userDao->getProfileHeaderByUsername(username);
    if (!headerOpt) {
        outProfile.isSuccess = false;
        std::snprintf(outProfile.message, sizeof(outProfile.message), "User not found.");
        return false;
    }

    const ProfileHeaderData header = *headerOpt;
    outProfile.isSuccess = true;
    std::strncpy(outProfile.username, header.username.c_str(), sizeof(outProfile.username) - 1);
    std::strncpy(outProfile.info, header.info.c_str(), sizeof(outProfile.info) - 1);

    const std::string created = ClampTimestamp19(header.createdAt);
    std::strncpy(outProfile.createdAt, created.c_str(), sizeof(outProfile.createdAt) - 1);
    outProfile.elo = (int32_t)header.elo;

    const auto matches = userDao->getRecentMatchesForUser(header.id, 20);
    outProfile.gameCount = (uint32_t)std::min<size_t>(matches.size(), 20);

    for (uint32_t i = 0; i < outProfile.gameCount; ++i) {
        const auto& m = matches[i];
        ProfileGameEntry& e = outProfile.games[i];
        std::memset(&e, 0, sizeof(e));

        e.matchId = m.matchId;

        const std::string ended = ClampTimestamp19(m.endedAt);
        std::strncpy(e.endedAt, ended.c_str(), sizeof(e.endedAt) - 1);

        std::strncpy(e.opponent, m.opponent.c_str(), sizeof(e.opponent) - 1);
        e.myScore = (int32_t)m.myScore;
        e.oppScore = (int32_t)m.oppScore;

        if (m.isDraw) {
            e.result = 2;
        } else {
            e.result = (m.winnerUserId == (uint32_t)header.id) ? 1 : 0;
        }
        std::strncpy(e.replayPath, m.logPath.c_str(), sizeof(e.replayPath) - 1);
    }

    std::snprintf(outProfile.message, sizeof(outProfile.message), "OK");
    return true;
}
