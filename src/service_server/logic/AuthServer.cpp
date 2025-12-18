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
