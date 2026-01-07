#ifndef USERDAO_HPP
#define USERDAO_HPP

#pragma once
#include "DatabaseServer.hpp"
#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <iostream>
#include <mutex>

// Updated UserData struct to include 'elo' and 'info'
struct UserData {
    long id;
    std::string username;
    int elo;
    std::string password;
};

struct ProfileHeaderData {
    long id = 0;
    std::string username;
    int elo = 0;
    std::string info;
    std::string createdAt; // "YYYY-MM-DD HH:MM:SS"
};

struct RecentMatchData {
    uint32_t matchId = 0;
    std::string endedAt;   // "YYYY-MM-DD HH:MM:SS" or empty
    std::string opponent;  // may be empty
    int myScore = 0;
    int oppScore = 0;
    bool isDraw = false;
    uint32_t winnerUserId = 0; // 0 if draw/unknown
    std::string logPath;
};

class UserDAO {
private:
    DatabaseServer* db;
    std::mutex m_dbMutex;

public:
    UserDAO(DatabaseServer* database);

    bool deleteUser(const u_int32_t userid);

    long createUser(const std::string& username, const std::string& password);

    std::vector<UserData> getAllUsers();

    std::optional<UserData> authenticate(const std::string& username, const std::string& password);

    bool updateElo(long userId, int newElo);

    bool updatePassword(long userId, const std::string& newPassword);

    bool updateUsername(long userId, const std::string& newUsername);
    
    std::optional<UserData> getUserById(long userId);

    std::optional<ProfileHeaderData> getProfileHeaderByUsername(const std::string& username);
    std::vector<RecentMatchData> getRecentMatchesForUser(long userId, int limit = 20);
};

#endif // USERDAO_HPP