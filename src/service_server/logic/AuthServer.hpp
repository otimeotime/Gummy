#ifndef AUTHSERVER_HPP
#define AUTHSERVER_HPP

#pragma once
#include "../common/network/Packet.hpp"
#include "UserDAO.hpp"
#include "DatabaseServer.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <sys/socket.h>

class AuthServer {
private:
    UserDAO* userDao;
public:
    AuthServer(UserDAO* userdao);

    ~AuthServer();

    bool login(const std::string& username, const std::string& password, UserData& outUser);

    bool reg(const std::string& username, const std::string& password, long& outUserId);

    bool changePassword(long userId, const std::string& newPassword);
    
};

#endif // AUTHSERVER_HPP