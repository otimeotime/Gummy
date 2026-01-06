#ifndef SERVICE_SERVER_H
#define SERVICE_SERVER_H

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include "../../common/network/TCPSocket.hpp"
#include "../logic/AuthServer.hpp" 

class ServiceServer {
private:
    TCPSocket mServiceServerSocket;
    std::atomic<bool> mIsRunning;
    
    AuthServer mAuthServer;

    std::mutex mClientsMutex;
    std::unordered_set<std::string> mConnectedUsers;

    void HandleClient(TCPSocket* clientSocket);


public:
    ServiceServer();
    ~ServiceServer();

    UserDAO* CreateAndConnectDAO();
    // The main loop that waits for connections
    void Run(int port);
    
    // Signal to stop the server
    void Stop();
};

#endif