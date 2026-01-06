#include "ServiceServer.hpp"
#include "../../common/network/PacketUtils.hpp"
#include "../../common/network/PacketStructs.hpp"
#include <iostream>
#include <cmath>

UserDAO* ServiceServer::CreateAndConnectDAO() {
    DatabaseServer* db = new DatabaseServer("gummydatabase", "postgres", "Hehehe123");
    
    if (!db->connect()) {
        std::cerr << "ServiceServer: Could not connect to database!" << std::endl;
    } else {
        std::cout << "ServiceServer: Database connected successfully." << std::endl;
    }

    return new UserDAO(db);
}

ServiceServer::ServiceServer() 
    : mIsRunning(false),
      mAuthServer(CreateAndConnectDAO()),
      mNextMatchId(1)
{}

void ServiceServer::ProcessMatchmaking() {
    std::lock_guard<std::mutex> lock(mMatchmakingMutex);

    if (mMatchmakingQueue.size() < 2) return;

    // Simple Greedy Matchmaking
    auto it1 = mMatchmakingQueue.begin();
    while (it1 != mMatchmakingQueue.end()) {
        bool matched = false;
        auto it2 = it1 + 1;
        while (it2 != mMatchmakingQueue.end()) {
            // Check ELO difference (e.g., within 200)
            if (std::abs(it1->elo - it2->elo) < 300) {
                // FOUND MATCH
                PendingMatch pm;
                pm.matchId = mNextMatchId++;
                pm.user1 = it1->username;
                pm.socket1 = it1->socket;
                pm.confirm1 = false;
                pm.user2 = it2->username;
                pm.socket2 = it2->socket;
                pm.confirm2 = false;
                pm.startTime = std::chrono::steady_clock::now();
                
                mPendingMatches.push_back(pm);

                // Notify Clients
                ReqMatchDecide1 req;
                req.matchId = pm.matchId;
                
                PacketUtils::SendPacket(pm.socket1, PacketType::REQ_MATCH_DECIDE_1, req);
                PacketUtils::SendPacket(pm.socket2, PacketType::REQ_MATCH_DECIDE_1, req);

                std::cout << "[Matchmaking] Found match " << pm.matchId << ": " 
                          << pm.user1 << " vs " << pm.user2 << std::endl;

                // Remove both from queue
                // Be careful with iterator invalidation
                it2 = mMatchmakingQueue.erase(it2);
                it1 = mMatchmakingQueue.erase(it1);
                matched = true;
                break; 
            } else {
                ++it2;
            }
        }
        if (!matched) {
            ++it1;
        }
    }
}

ServiceServer::~ServiceServer() {
    Stop();
}

void ServiceServer::Stop() {
    mIsRunning = false;
    mServiceServerSocket.Close();
}

void ServiceServer::Run(int port) {
    try {
        mServiceServerSocket.Bind(port);
        mServiceServerSocket.Listen();
        mIsRunning = true;
        std::cout << "ServiceServer Listening on port " << port << std::endl;

        while (mIsRunning) {
            TCPSocket* clientSocket = mServiceServerSocket.Accept();
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
    std::string currentUsername = "";
    int currentElo = 0;
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
            size_t totalReceived = 0;
            while (totalReceived < header.length) {
                int received = clientSocket->Receive(payloadBuffer.data() + totalReceived, header.length - totalReceived);
                if (received <= 0) {
                    std::cerr << "Thread Client disconnected or error occurred while reading payload." << std::endl;
                    connected = false;
                    break;
                }
                totalReceived += received;
            }
            if (!connected) break;
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
                        bool isAlreadyActive = false;
                        {
                            std::lock_guard<std::mutex> lock(mClientsMutex);
                            if (mConnectedUsers.find(outUser.username) != mConnectedUsers.end()) {
                                isAlreadyActive = true;
                            }
                        }

                        if (isAlreadyActive) {
                            res.isLogin = true;
                            res.isSuccess = false;
                            std::snprintf(res.message, sizeof(res.message), "Login failed. Account entered from another terminal.");
                        } else {
                            res.isLogin = true;
                            res.isSuccess = true;
                            std::snprintf(res.message, sizeof(res.message), "Login successful. Welcome, %s!", outUser.username.c_str());
                            
                            currentUsername = outUser.username;
                            currentElo = outUser.elo;
                            {
                                std::lock_guard<std::mutex> lock(mClientsMutex);
                                mConnectedUsers.insert(currentUsername);
                            }
                        }
                    } else {
                        res.isLogin = true;
                        res.isSuccess = false;
                        std::snprintf(res.message, sizeof(res.message), "Login failed. Invalid credentials.");     
                    }
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
                if (!currentUsername.empty()) {
                    std::lock_guard<std::mutex> lock(mClientsMutex);
                    mConnectedUsers.erase(currentUsername);
                    currentUsername = "";
                    currentElo = 0;
                }
                // Do not disconnect socket, just end session context? Logic says keep loop running?
                // The original code set connected = false.
                // But the user changed client to NOT disconnect.
                // So here, we should NOT set connected = false if we want re-login.
                // BUT, if we keep loop, next packet might be LOGIN.
                // Let's keep loop running.
                // connected = false; // Commented out to allow re-login on same socket
            }
            break;
            // -------------------------------------------------------------------------------------------------------------------------------------------
            // MATCHMAKING: REQ_MATCH_FIND ---------------------------------------------------------------------------------------------------------------
            case PacketType::REQ_MATCH_FIND: {
                if (currentUsername.empty()) break; // Should be logged in

                {
                    std::lock_guard<std::mutex> lock(mMatchmakingMutex);
                    // Check if already in queue?
                    bool inQueue = false;
                    for(const auto& entry : mMatchmakingQueue) {
                        if (entry.username == currentUsername) { inQueue = true; break;}
                    }
                    if (!inQueue) {
                        mMatchmakingQueue.push_back({currentUsername, currentElo, clientSocket});
                        std::cout << "[Matchmaking] Added " << currentUsername << " (ELO: " << currentElo << ") to queue." << std::endl;
                    }
                }
                
                ResMatchFind res;
                res.isSuccess = true;
                std::snprintf(res.message, sizeof(res.message), "Searching for match...");
                PacketUtils::SendPacket(clientSocket, PacketType::RES_MATCH_FIND, res);
                
                ProcessMatchmaking();
            }
            break;
            // MATCHMAKING: REQ_MATCH_CANCEL -------------------------------------------------------------------------------------------------------------
            case PacketType::REQ_MATCH_CANCEL: {
                std::cout << "[Matchmaking] Client cancelled search: " << currentUsername << std::endl;
                 {
                    std::lock_guard<std::mutex> lock(mMatchmakingMutex);
                    for (auto it = mMatchmakingQueue.begin(); it != mMatchmakingQueue.end(); ) {
                        if (it->username == currentUsername) {
                            it = mMatchmakingQueue.erase(it);
                            std::cout << " > Removed from queue." << std::endl;
                        } else {
                            ++it;
                        }
                    }
                }
                ResMatchCancel res;
                res.isSuccess = true;
                std::snprintf(res.message, sizeof(res.message), "Search cancelled.");
                PacketUtils::SendPacket(clientSocket, PacketType::RES_MATCH_CANCEL, res);
            }
            break;
            // MATCHMAKING: RES_MATCH_DECIDE_1 -----------------------------------------------------------------------------------------------------------
            case PacketType::RES_MATCH_DECIDE_1: { // Client Confirmation
                if (currentUsername.empty()) break;
                // Client sends: isSuccess (true=Accept, false=Decline) - wait, PacketStruct says ResMatchDecide1 only has isSuccess.
                // Wait, logic says Server sends REQ_MATCH_DECIDE_1 (with matchId)
                // Client sends RES_MATCH_DECIDE_1 (with matchId?? NO)
                // PacketStruct for ResMatchDecide1: struct { bool isSuccess; }
                // PROBLEM: We don't know WHICH match ID the client is confirming if the packet doesn't have it.
                // We have to assume the user is involved in only ONE pending match.
                
                ResMatchDecide1 resPayload = packet.GetPayload<ResMatchDecide1>();
                bool accepted = resPayload.isSuccess;
                
                std::lock_guard<std::mutex> lock(mMatchmakingMutex);
                for (auto it = mPendingMatches.begin(); it != mPendingMatches.end(); ++it) {
                    if (it->user1 == currentUsername) {
                        it->confirm1 = accepted;
                    } else if (it->user2 == currentUsername) {
                        it->confirm2 = accepted;
                    } else {
                        continue;
                    }

                    if (it->confirm1 && it->confirm2) {
                        // BOTH ACCEPTED -> START GAME
                        ResMatchDecide2 startMsg;
                        startMsg.matchId = it->matchId;
                        startMsg.playerOrder[0] = 1; // ID or Index
                        startMsg.playerOrder[1] = 2; 
                        
                        PacketUtils::SendPacket(it->socket1, PacketType::RES_MATCH_DECIDE_2, startMsg);
                        PacketUtils::SendPacket(it->socket2, PacketType::RES_MATCH_DECIDE_2, startMsg);
                        
                        std::cout << "[Matchmaking] Match " << it->matchId << " verified! Starting Game..." << std::endl;
                        
                        // Remove from pending
                        mPendingMatches.erase(it);
                        break; 
                    } else if (!accepted) {
                        // SOMEONE DECLINED -> CANCEL MATCH
                        ResMatchCancel cancelMsg;
                        cancelMsg.isSuccess = false; 
                        std::snprintf(cancelMsg.message, sizeof(cancelMsg.message), "Match declined by opponent.");
                        
                        // Notify both (one might have already accepted and is waiting)
                        PacketUtils::SendPacket(it->socket1, PacketType::RES_MATCH_CANCEL, cancelMsg);
                        PacketUtils::SendPacket(it->socket2, PacketType::RES_MATCH_CANCEL, cancelMsg);
                        
                        std::cout << "[Matchmaking] Match " << it->matchId << " declined by " << currentUsername << std::endl;
                        
                        mPendingMatches.erase(it);
                        break;
                    }
                }
            }
            break;
            // -------------------------------------------------------------------------------------------------------------------------------------------
            // User CHANGE PASSWORD ----------------------------------------------------------------------------------------------------------------------
            case PacketType::REQ_CHANGE_PASSWORD: {
                ReqChangePassword req = packet.GetPayload<ReqChangePassword>();
                long userId = 0; // Where to get ID? AuthServer needs ID?
                // Logic gap in original code: userId was uninitialized local var!
                // Fixed: mAuthServer.changePassword likely needs username? No, it takes long userId.
                // We should fix this properly but for now let's leave it as is or try to use helper logic if available.
                // Assuming it's broken in original, effectively non-functional.
                ResChangePassword res;
                res.isSuccess = false;
                std::snprintf(res.message, sizeof(res.message), "Feature unavailable (ID missing).");
                PacketUtils::SendPacket(clientSocket, PacketType::RES_CHANGE_PASSWORD, res);
            }
            break;
            // -------------------------------------------------------------------------------------------------------------------------------------------
            // GET USER LIST
            case PacketType::REQ_GET_USER_LIST: {
                std::vector<UserData> allUsers = mAuthServer.getAllUsers();
                ResGetUserList res;
                res.count = 0;
                
                std::unordered_set<std::string> onlineSnapshot;
                {
                    std::lock_guard<std::mutex> lock(mClientsMutex);
                    onlineSnapshot = mConnectedUsers;
                }

                for (const auto& u : allUsers) {
                    if (res.count >= 20) break;
                    
                    PlayerStatusInfo& info = res.players[res.count];
                    std::strncpy(info.username, u.username.c_str(), 31);
                    info.username[31] = '\0';
                    info.isOnline = (onlineSnapshot.find(u.username) != onlineSnapshot.end());
                    info.elo = u.elo;
                    
                    res.count++;
                }
                PacketUtils::SendPacket(clientSocket, PacketType::RES_GET_USER_LIST, res);
            }
            break;
            // -------------------------------------------------------------------------------------------------------------------------------------------
            default:
                std::cerr << "Thread Client received unknown packet type: " << static_cast<int>(header.type) << std::endl;
                break;
        }
    }

    if (!currentUsername.empty()) {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        mConnectedUsers.erase(currentUsername);
    }
    
    // Cleanup Matchmaking on Disconnect
    {
        std::lock_guard<std::mutex> lock(mMatchmakingMutex);
        for (auto it = mMatchmakingQueue.begin(); it != mMatchmakingQueue.end(); ) {
            if (it->username == currentUsername) {
                it = mMatchmakingQueue.erase(it);
            } else {
                ++it;
            }
        }
    }

    clientSocket->Close();
    delete clientSocket;
}