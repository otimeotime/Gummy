#ifndef SERVICE_SERVER_H
#define SERVICE_SERVER_H

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <map>
#include <chrono>
#include "../../common/network/TCPSocket.hpp"
#include "../logic/AuthServer.hpp" 

struct MatchmakingEntry {
    std::string username;
    int elo;
    TCPSocket* socket;
};

struct PendingMatch {
    uint32_t matchId;
    std::string user1;
    TCPSocket* socket1;
    bool confirm1;
    
    std::string user2;
    TCPSocket* socket2;
    bool confirm2;
    
    std::chrono::steady_clock::time_point startTime;
};

class ServiceServer {
private:
    TCPSocket mServiceServerSocket;
    std::atomic<bool> mIsRunning;
    
    AuthServer mAuthServer;

    std::mutex mClientsMutex;
    std::unordered_set<std::string> mConnectedUsers;

    // Matchmaking
    std::mutex mMatchmakingMutex;
    std::vector<MatchmakingEntry> mMatchmakingQueue;
    std::vector<PendingMatch> mPendingMatches;
    uint32_t mNextMatchId;

    void HandleClient(TCPSocket* clientSocket);
    void ProcessMatchmaking();
    void CheckPendingMatches(); // Optional: Timeout logic

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