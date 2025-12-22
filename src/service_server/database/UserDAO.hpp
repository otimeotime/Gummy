#ifndef USERDAO_HPP
#define USERDAO_HPP

#pragma once
#include "DatabaseServer.hpp"
#include <string>
#include <optional>
#include <iostream>

// Updated UserData struct to include 'elo' and 'info'
struct UserData {
    long id;
    std::string username;
    int elo;
    std::string password;
};

class UserDAO {
private:
    DatabaseServer* db;

public:
    UserDAO(DatabaseServer* database);

    bool deleteUser(const u_int32_t userid);

    long createUser(const std::string& username, const std::string& password);

    std::optional<UserData> authenticate(const std::string& username, const std::string& password);

    bool updateElo(long userId, int newElo);

    bool updatePassword(long userId, const std::string& newPassword);

    bool updateUsername(long userId, const std::string& newUsername);
    
    std::optional<UserData> getUserById(long userId);
};

#endif // USERDAO_HPP