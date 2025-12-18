#include "ServiceServer.hpp"
#include "../../common/network/PacketUtils.hpp"
#include "../../common/network/PacketStructs.hpp"
#include <iostream>

ServiceServer::~ServiceServer() {
    Stop();
}

void ServiceServer::Stop() {
    mIsRunning = false;
    mServerSocket.Close();
}

void ServiceServer::Run(int port) {
    try {
        mServerSocket.Bind(port);
        mServerSocket.Listen();
        mIsRunning = true;
        std::cout << "ServiceServer Listening on port " << port << std::endl;

        while (mIsRunning) {
            TCPSocket* clientSocket = mServerSocket.Accept();
            if (clientSocket == nullptr) {
                continue;
            }
            std::cout << "ServiceServer Accepted connection from client." << std::endl;
            std::thread clientThread(&ServiceServer::HandleClient, this, clientSocket);
            clientThread.detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "ServiceServer encountered an error: " << e.what() << std::endl;
    }
}

void ServiceServer::HandleClient(TCPSocket* clientSocket) {
    bool connected = true;
    std::vector<char> headerBuffer(sizeof(Header));

    while (connected && mIsRunning) {
        int bytesRead = clientSocket->Receive(headerBuffer.data(), headerBuffer.size());
        if (bytesRead <= 0) {
            std::cout << "Thread Client disconnected or error occurred." << std::endl;
            connected = false;
            break;
        }

        Header header;
        if (!PacketUtils::ReadHeader(headerBuffer.data(), bytesRead, header)) {
            std::cout << "Thread Client failed to read header." << std::endl;
            connected = false;
            break;
        }

        std::vector<char> payloadBuffer(header.length);
        if (header.length > 0) {
            bytesRead = clientSocket->Receive(payloadBuffer.data(), payloadBuffer.size());
            if (bytesRead <= 0) {
                std::cerr << "Thread Client disconnected or error occurred while reading payload." << std::endl;
                connected = false;
                break;
            }
        }

        Packet packet;
        packet.header = header;
        packet.payload = payloadBuffer;

        switch (header.type) {
            // User try to LOGIN or REGISTER -------------------------------------------------------------------------------------------------------------
            case PacketType::REQ_AUTHENTICATE: {
                ReqAuthenticate req = packet.GetPayload<ReqAuthenticate>();
                UserData outUser;
                ResAuthenticate res;
                if (req.isLogin) {
                    if (mAuthServer.login(req.username, req.password, outUser)) {
                        res.isLogin = true;
                        res.isSuccess = true;
                        strcpy(res.message, "Login successful.");
                        std::snprintf(res.message, sizeof(res.message), "Login successful. Welcome, %s!", outUser.username.c_str());
                    } else {
                        res.isLogin = true;
                        res.isSuccess = false;
                        std::snprintf(res.message, sizeof(res.message), "Login failed. Invalid credentials.");     
                    }
                    PacketUtils::SendPacket(clientSocket, PacketType::RES_AUTHENTICATE, res);
                } else {
                    long outUserId;
                    if (mAuthServer.reg(req.username, req.password, outUserId)) {
                        res.isLogin = false;
                        res.isSuccess = true;
                        std::snprintf(res.message, sizeof(res.message), "Registration successful. Welcome, %s!", req.username);
                    } else {
                        res.isLogin = false;
                        res.isSuccess = false;
                        std::snprintf(res.message, sizeof(res.message), "Registration failed.");
                    }
                }
                PacketUtils::SendPacket(clientSocket, PacketType::RES_AUTHENTICATE, res);
            }
            break;
            // -------------------------------------------------------------------------------------------------------------------------------------------
            // User LOGOUT -------------------------------------------------------------------------------------------------------------------------------
            case PacketType::REQ_LOGOUT: {
                std::cout << "Client requested logout." << std::endl;
                connected = false; 
            }
            break;
            // -------------------------------------------------------------------------------------------------------------------------------------------
            // User CHANGE PASSWORD ----------------------------------------------------------------------------------------------------------------------
            case PacketType::REQ_CHANGE_PASSWORD: {
                ReqChangePassword req = packet.GetPayload<ReqChangePassword>();
                long userId;
                ResChangePassword res;
                if (mAuthServer.changePassword(userId, req.newPassword)) {
                    res.isSuccess = true;
                    std::snprintf(res.message, sizeof(res.message), "Password change successful.");
                } else {
                    res.isSuccess = false;
                    std::snprintf(res.message, sizeof(res.message), "Password change failed.");
                }
                PacketUtils::SendPacket(clientSocket, PacketType::RES_CHANGE_PASSWORD, res);
            }
            break;
            // -------------------------------------------------------------------------------------------------------------------------------------------
            default:
                std::cerr << "Thread Client received unknown packet type: " << static_cast<int>(header.type) << std::endl;
                break;
        }
    }

    clientSocket->Close();
    delete clientSocket;
}