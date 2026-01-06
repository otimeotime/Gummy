#include "ClientSocket.hpp"

ClientSocket::ClientSocket() : mSocket(new TCPSocket()), mIsConnected(false) {}

ClientSocket::~ClientSocket() {
    Disconnect();
    if (mSocket) {
        delete mSocket;
        mSocket = nullptr;
    }
}

bool ClientSocket::Connect(const std::string& ip, int port) {
    try {
        mSocket->Connect(ip, port);
        mIsConnected = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Connection failed: " << e.what() << std::endl;
        mIsConnected = false;
        return false;
    }
}

void ClientSocket::Disconnect() {
    if (mSocket && mIsConnected) {
        mSocket->Close();
    }
    mIsConnected = false;
}

std::string ClientSocket::Login(const std::string& username, const std::string& password) {
    if (!mIsConnected) return "Not connected to server.";

    ReqAuthenticate req;
    std::memset(&req, 0, sizeof(req));
    std::strncpy(req.username, username.c_str(), sizeof(req.username) - 1);
    std::strncpy(req.password, password.c_str(), sizeof(req.password) - 1);
    req.isLogin = true;

    if (!PacketUtils::SendPacket(mSocket, PacketType::REQ_AUTHENTICATE, req)) {
        return "Failed to send login request.";
    }

    ResAuthenticate res;
    if (PacketUtils::ReceivePacketPayload(mSocket, res)) {
        if (res.isSuccess) {
            return "Success";
        } else {
            return std::string(res.message);
        }
    }
    return "Failed to receive login response.";
}

std::string ClientSocket::Register(const std::string& username, const std::string& password) {
    if (!mIsConnected) return "Not connected to server.";

    ReqAuthenticate req;
    std::memset(&req, 0, sizeof(req));
    std::strncpy(req.username, username.c_str(), sizeof(req.username) - 1);
    std::strncpy(req.password, password.c_str(), sizeof(req.password) - 1);
    req.isLogin = false; // Registration mode

    if (!PacketUtils::SendPacket(mSocket, PacketType::REQ_AUTHENTICATE, req)) {
        return "Failed to send register request.";
    }

    ResAuthenticate res;
    if (PacketUtils::ReceivePacketPayload(mSocket, res)) {
         if (res.isSuccess) {
            return "Success";
        } else {
            return std::string(res.message);
        }
    }
    return "Failed to receive register response.";
}

bool ClientSocket::ChangePassword(const std::string& currentPassword, const std::string& newPassword) {
    if (!mIsConnected) return false;

    ReqChangePassword req;
    std::memset(&req, 0, sizeof(req));
    std::strncpy(req.currentPassword, currentPassword.c_str(), sizeof(req.currentPassword) - 1);
    std::strncpy(req.newPassword, newPassword.c_str(), sizeof(req.newPassword) - 1);

    if (!PacketUtils::SendPacket(mSocket, PacketType::REQ_CHANGE_PASSWORD, req)) {
        return false;
    }

    ResChangePassword res;
    if (PacketUtils::ReceivePacketPayload(mSocket, res)) {
        return res.isSuccess;
    }
    return false;
}

void ClientSocket::Logout() {
    if (!mIsConnected) return;
    
    // Just send the logout request; server usually disconnects or invalidates session
    PacketUtils::SendPacket(mSocket, PacketType::REQ_LOGOUT);
    
    // Do NOT disconnect the socket, as we want to return to Login screen and reuse it.
    // Disconnect(); 
}
std::vector<PlayerStatusInfo> ClientSocket::GetUserList() {
    std::vector<PlayerStatusInfo> result;
    if (!mIsConnected) return result;

    ReqGetUserList req; 
    req.dummy = 0;

    if (!PacketUtils::SendPacket(mSocket, PacketType::REQ_GET_USER_LIST, req)) {
        std::cerr << "Failed to send GetUserList request." << std::endl;
        return result;
    }

    ResGetUserList res; 
    if (PacketUtils::ReceivePacketPayload(mSocket, res)) {
        for (int i=0; i < res.count; ++i) {
            result.push_back(res.players[i]);
        }
    } else {
        std::cerr << "Failed to receive GetUserList response." << std::endl;
    }
    return result;
}
