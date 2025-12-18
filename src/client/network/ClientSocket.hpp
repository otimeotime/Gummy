#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <iostream>

#include "../../common/network/TCPSocket.hpp"
#include "../../common/network/Packet.hpp"
#include "../../common/network/PacketType.hpp"
#include "../../common/network/PacketStructs.hpp"
#include "../../common/network/PacketUtils.hpp"
#include "../../common/network/PacketUtils.hpp"

class ClientSocket {
private:
    TCPSocket* mSocket;
    bool mIsConnected;

public:
    ClientSocket();
    ~ClientSocket();

    bool Connect(const std::string& ip, int port);
    void Disconnect();

    std::string Login(const std::string& username, const std::string& password);
    std::string Register(const std::string& username, const std::string& password);
    bool ChangePassword(const std::string& currentPassword, const std::string& newPassword);
    void Logout();
    
    bool IsConnected() const { return mIsConnected; }
};

#endif // CLIENT_SOCKET_H