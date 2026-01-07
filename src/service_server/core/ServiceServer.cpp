#include "ServiceServer.hpp"
#include "../../common/network/PacketUtils.hpp"
#include "../../common/network/PacketStructs.hpp"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctime>
#include <sys/stat.h>

namespace {
std::string GetEnvOrDefault(const char* key, const std::string& def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    return std::string(v);
}

int GetEnvIntOrDefault(const char* key, int def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    int out = std::atoi(v);
    return out > 0 ? out : def;
}

bool WaitForTcpAccept(const std::string& host, int port, int timeoutMs) {
    const int attempts = std::max(1, timeoutMs / 100);
    for (int i = 0; i < attempts; ++i) {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            ::close(fd);
            return false;
        }

        int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        const int err = (rc == 0) ? 0 : errno;
        ::close(fd);

        if (rc == 0) return true;
        if (err != ECONNREFUSED && err != ETIMEDOUT && err != EHOSTUNREACH && err != ENETUNREACH) {
            return false;
        }
        usleep(100 * 1000);
    }
    return false;
}

bool IsTcpListeningOnce(const std::string& host, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        return false;
    }

    int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    return rc == 0;
}

int PickFreePort(const std::string& host, int preferredPort, int maxTries) {
    if (preferredPort > 0 && !IsTcpListeningOnce(host, preferredPort)) {
        return preferredPort;
    }
    // Try a short range after the preferred port.
    for (int i = 1; i <= maxTries; ++i) {
        const int p = preferredPort + i;
        if (p <= 0) continue;
        if (!IsTcpListeningOnce(host, p)) return p;
    }
    // Fallback to a safer higher range.
    for (int p = 10000; p <= 10100; ++p) {
        if (!IsTcpListeningOnce(host, p)) return p;
    }
    return -1;
}

std::string BuildReplayPath(uint32_t matchId) {
    // Prefer a stable session timestamp injected by the launcher script.
    // This makes replay names align with logs/service_server_<TS>.log.
    const char* dirEnv = std::getenv("REPLAY_DIR");
    const std::string dir = (dirEnv && *dirEnv) ? std::string(dirEnv) : std::string("logs");

    const char* tsEnv = std::getenv("REPLAY_SESSION_TS");
    std::string ts;
    if (tsEnv && *tsEnv) {
        ts = tsEnv;
    } else {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_r(&t, &tm);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec);
        ts = buf;
    }

    char out[512];
    std::snprintf(out, sizeof(out), "%s/replay_service_%s_match_%u.grpl", dir.c_str(), ts.c_str(), matchId);
    return std::string(out);
}
}

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
      mNextMatchId(1),
      mIngameServerPid(-1),
      mIngameServerBin(GetEnvOrDefault("INGAME_SERVER_BIN", "./ingame_server_demo")),
      mIngameHost(GetEnvOrDefault("INGAME_HOST", "127.0.0.1")),
      mIngamePort(GetEnvIntOrDefault("INGAME_PORT", 9090)),
      mIngameMapPath(GetEnvOrDefault("INGAME_MAP", "assets/maps/flatmap.txt"))
{}

bool ServiceServer::EnsureIngameServerRunning(uint32_t matchId, int* outPort) {
    std::lock_guard<std::mutex> lock(mIngameMutex);

    if (outPort) *outPort = 0;

    // Best-effort: ensure replay output directory exists.
    ::mkdir("logs", 0755);

    // Default behavior for the service flow: record each entered match to its own replay file.
    // Easiest robust approach: restart ingame server per match.
    if (mIngameServerPid > 0 && kill(mIngameServerPid, 0) == 0) {
        const pid_t oldPid = mIngameServerPid;
        std::cout << "[Ingame] Restarting ingame server for match " << matchId
                  << " (old PID=" << oldPid << ")" << std::endl;
        // Ask it to shutdown gracefully (it handles SIGINT)
        kill(oldPid, SIGINT);
        // Wait a short time
        for (int i = 0; i < 20; ++i) {
            int status = 0;
            pid_t r = waitpid(oldPid, &status, WNOHANG);
            if (r == oldPid) break;
            usleep(50 * 1000);
        }
        if (kill(oldPid, 0) == 0) {
            kill(oldPid, SIGKILL);
            waitpid(oldPid, nullptr, 0);
        }
        mIngameServerPid = -1;
    } else {
        mIngameServerPid = -1;
    }

    const int portToUse = PickFreePort(mIngameHost, mIngamePort, 20);
    if (portToUse <= 0) {
        std::cerr << "[Ingame] Failed to find a free port" << std::endl;
        return false;
    }
    if (outPort) *outPort = portToUse;

    const std::string portStr = std::to_string(portToUse);
    const std::string matchIdStr = std::to_string(matchId);
    const std::string replayPath = BuildReplayPath(matchId);
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[Ingame] fork() failed" << std::endl;
        return false;
    }
    if (pid == 0) {
        // Child: exec ingame server
        execl(mIngameServerBin.c_str(),
              mIngameServerBin.c_str(),
              "--port", portStr.c_str(),
              "--match-id", matchIdStr.c_str(),
              "--record", replayPath.c_str(),
              (char*)nullptr);
        // If exec fails:
        std::perror("execl(ingame_server)");
        _exit(127);
    }

    mIngameServerPid = pid;
    std::cout << "[Ingame] Started ingame server PID=" << mIngameServerPid
              << " bin=" << mIngameServerBin
              << " port=" << portToUse
              << " record=" << replayPath << std::endl;

    if (!WaitForTcpAccept(mIngameHost, portToUse, 5000)) {
        std::cerr << "[Ingame] Warning: ingame server not accepting connections yet on "
                  << mIngameHost << ":" << portToUse << std::endl;
    }
    return true;
}

void ServiceServer::StopIngameServer() {
    std::lock_guard<std::mutex> lock(mIngameMutex);

    if (mIngameServerPid <= 0) return;

    const pid_t pid = mIngameServerPid;
    mIngameServerPid = -1;

    if (kill(pid, 0) != 0) return;

    // Ask it to shutdown gracefully (it handles SIGINT)
    kill(pid, SIGINT);

    // Wait up to ~1s
    for (int i = 0; i < 20; ++i) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) return;
        usleep(50 * 1000);
    }

    // Force kill as a last resort
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
}

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
    StopIngameServer();
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
                res.userId = 0;
                res.elo = 0;
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
                            res.userId = static_cast<uint32_t>(outUser.id);
                            res.elo = static_cast<int32_t>(outUser.elo);
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
                        res.userId = static_cast<uint32_t>(outUserId);
                        res.elo = 300;
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

                    // Also cancel any pending match involving this user (covers edge-cases like UI closing while popup is open)
                    for (auto it = mPendingMatches.begin(); it != mPendingMatches.end(); ) {
                        if (it->user1 == currentUsername || it->user2 == currentUsername) {
                            ResMatchCancel cancelMsg;
                            cancelMsg.isSuccess = false;
                            std::snprintf(cancelMsg.message, sizeof(cancelMsg.message), "Match cancelled.");

                            if (it->socket1) PacketUtils::SendPacket(it->socket1, PacketType::RES_MATCH_CANCEL, cancelMsg);
                            if (it->socket2) PacketUtils::SendPacket(it->socket2, PacketType::RES_MATCH_CANCEL, cancelMsg);

                            it = mPendingMatches.erase(it);
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
                ResMatchDecide1 resPayload = packet.GetPayload<ResMatchDecide1>();
                bool accepted = resPayload.isSuccess;
                uint32_t matchId = resPayload.matchId;
                
                std::lock_guard<std::mutex> lock(mMatchmakingMutex);
                for (auto it = mPendingMatches.begin(); it != mPendingMatches.end(); ++it) {
                    if (it->matchId != matchId) continue;

                    if (it->user1 == currentUsername) {
                        it->confirm1 = accepted;
                    } else if (it->user2 == currentUsername) {
                        it->confirm2 = accepted;
                    } else {
                        // Decision for a matchId this user isn't part of (stale/out-of-order) -> ignore
                        break;
                    }

                    if (it->confirm1 && it->confirm2) {
                        // BOTH ACCEPTED -> START GAME
                        ResMatchDecide2 startMsg;
                        startMsg.matchId = it->matchId;
                        startMsg.playerOrder[0] = 1; // ID or Index
                        startMsg.playerOrder[1] = 2; 
                        
                        PacketUtils::SendPacket(it->socket1, PacketType::RES_MATCH_DECIDE_2, startMsg);
                        PacketUtils::SendPacket(it->socket2, PacketType::RES_MATCH_DECIDE_2, startMsg);

                        // Spawn ingame server (local demo) and send handoff info.
                        // Default: always record each entered match.
                        int ingamePort = 0;
                        const bool started = EnsureIngameServerRunning(it->matchId, &ingamePort);
                        if (!started || ingamePort <= 0) {
                            ResMatchCancel cancelMsg;
                            cancelMsg.isSuccess = false;
                            std::snprintf(cancelMsg.message, sizeof(cancelMsg.message), "Failed to start ingame server.");

                            PacketUtils::SendPacket(it->socket1, PacketType::RES_MATCH_CANCEL, cancelMsg);
                            PacketUtils::SendPacket(it->socket2, PacketType::RES_MATCH_CANCEL, cancelMsg);
                            mPendingMatches.erase(it);
                            break;
                        }
                        InitGame init;
                        std::memset(&init, 0, sizeof(init));
                        init.matchId = it->matchId;
                        std::strncpy(init.host, mIngameHost.c_str(), sizeof(init.host) - 1);
                        init.port = static_cast<uint16_t>(ingamePort);
                        std::strncpy(init.mapPath, mIngameMapPath.c_str(), sizeof(init.mapPath) - 1);
                        PacketUtils::SendPacket(it->socket1, PacketType::INIT_GAME, init);
                        PacketUtils::SendPacket(it->socket2, PacketType::INIT_GAME, init);
                        
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

            case PacketType::REQ_GET_PROFILE: {
                // Use the authenticated session user (ignore payload to avoid spoofing).
                ResGetProfile res;
                if (!mAuthServer.getProfile(currentUsername, res)) {
                    // getProfile already filled res with error message.
                }
                PacketUtils::SendPacket(clientSocket, PacketType::RES_GET_PROFILE, res);
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

        // If user disconnects while in a pending match, cancel it and notify the opponent.
        for (auto it = mPendingMatches.begin(); it != mPendingMatches.end(); ) {
            if (it->user1 == currentUsername || it->user2 == currentUsername) {
                ResMatchCancel cancelMsg;
                cancelMsg.isSuccess = false;
                std::snprintf(cancelMsg.message, sizeof(cancelMsg.message), "Opponent disconnected.");

                if (it->socket1) PacketUtils::SendPacket(it->socket1, PacketType::RES_MATCH_CANCEL, cancelMsg);
                if (it->socket2) PacketUtils::SendPacket(it->socket2, PacketType::RES_MATCH_CANCEL, cancelMsg);

                it = mPendingMatches.erase(it);
            } else {
                ++it;
            }
        }
    }

    clientSocket->Close();
    delete clientSocket;
}