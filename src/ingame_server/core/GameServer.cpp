#include "GameServer.hpp"

#include "../../common/network/PacketUtils.hpp"
#include "../../common/network/PacketStructs.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <filesystem>
#include <random>

namespace {
constexpr const char* kDefaultMap = "assets/maps/flatmap.txt";
constexpr uint32_t kPauseDurationMs = 30000;
constexpr uint32_t kPauseResumeCountdownMs = 3000;
constexpr uint32_t kDrawOfferTimeoutMs = 10000;

std::string GetRandomMapFromAssets() {
    namespace fs = std::filesystem;
    std::vector<std::string> maps;
    const std::string mapsDir = "assets/maps";
    try {
        if (fs::exists(mapsDir) && fs::is_directory(mapsDir)) {
            for (const auto& entry : fs::directory_iterator(mapsDir)) {
                 if (entry.path().extension() == ".txt") {
                     maps.push_back(entry.path().string());
                 }
            }
        }
    } catch (...) {}
    
    if (maps.empty()) return kDefaultMap;
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, maps.size() - 1);
    return maps[dis(gen)];
}

const char* CommandToString(uint32_t command) {
    switch (command) {
        case INGAME_CMD_MOVE_LEFT:
            return "MOVE_LEFT";
        case INGAME_CMD_MOVE_RIGHT:
            return "MOVE_RIGHT";
        case INGAME_CMD_STOP:
            return "STOP";
        case INGAME_CMD_ADJUST_ANGLE:
            return "ADJUST_ANGLE";
        case INGAME_CMD_ADJUST_POWER:
            return "ADJUST_POWER";
        case INGAME_CMD_FIRE:
            return "FIRE";
        case INGAME_CMD_POWER_UP:
            return "POWER_UP";
        default:
            return nullptr;
    }
}
}

GameServer::GameServer()
    : mIsRunning(false),
      m_gameRoom(nullptr),
      m_mapLoader(nullptr),
      m_matchId(1),
      m_tick(0) {}

GameServer::~GameServer() {
    Stop();
}

void GameServer::SetMatchId(uint32_t matchId) {
    if (matchId == 0) return;
    std::lock_guard<std::mutex> lock(m_roomMutex);
    m_matchId = matchId;
    m_tick = 0;
    m_matchHasStartTime = false;
    m_matchSaved = false;
    m_playerUserIds[0] = 0;
    m_playerUserIds[1] = 0;
}

namespace {
std::string FormatTimestamp(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}
}

void GameServer::BroadcastPauseSignal(uint32_t requesterId, uint32_t durationMs, uint8_t requesterRemainingUses) {
    ResIngamePauseSignal sig{};
    sig.matchId = m_matchId;
    sig.requesterPlayerId = requesterId;
    sig.durationMs = durationMs;
    sig.requesterRemainingUses = requesterRemainingUses;

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto* c : m_clients) {
        if (!c) continue;
        // Only notify connections that have completed REQ_INGAME_JOIN.
        if (m_fdToPlayerId.find(c->GetFd()) == m_fdToPlayerId.end()) continue;
        PacketUtils::SendPacket(c, PacketType::RES_INGAME_PAUSE_SIGNAL, sig);
    }
}

void GameServer::BroadcastPauseEnd(uint32_t endedByPlayerId) {
    ResIngamePauseEnd msg{};
    msg.matchId = m_matchId;
    msg.endedByPlayerId = endedByPlayerId;

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto* c : m_clients) {
        if (!c) continue;
        if (m_fdToPlayerId.find(c->GetFd()) == m_fdToPlayerId.end()) continue;
        PacketUtils::SendPacket(c, PacketType::RES_INGAME_PAUSE_END, msg);
    }
}

void GameServer::BroadcastDrawSignal(uint32_t requesterId, uint32_t durationMs) {
    ResIngameDrawSignal sig{};
    sig.matchId = m_matchId;
    sig.requesterPlayerId = requesterId;
    sig.durationMs = durationMs;

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto* c : m_clients) {
        if (!c) continue;
        if (m_fdToPlayerId.find(c->GetFd()) == m_fdToPlayerId.end()) continue;
        PacketUtils::SendPacket(c, PacketType::RES_INGAME_DRAW_SIGNAL, sig);
    }
}

void GameServer::BroadcastDrawResult(uint32_t requesterId, uint32_t responderId, uint32_t result, const char* message) {
    ResIngameDrawResult res{};
    res.matchId = m_matchId;
    res.requesterPlayerId = requesterId;
    res.responderPlayerId = responderId;
    res.result = result;
    std::memset(res.message, 0, sizeof(res.message));
    if (message) {
        std::snprintf(res.message, sizeof(res.message), "%s", message);
    }

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto* c : m_clients) {
        if (!c) continue;
        if (m_fdToPlayerId.find(c->GetFd()) == m_fdToPlayerId.end()) continue;
        PacketUtils::SendPacket(c, PacketType::RES_INGAME_DRAW_RESULT, res);
    }
}

void GameServer::BroadcastRematchStatus(uint8_t status, const char* message) {
    ResIngameRematchStatus res{};
    res.status = status;
    {
        std::lock_guard<std::mutex> lock(m_rematchMutex);
        res.acceptedMask = m_rematchAcceptedMask;
    }
    std::memset(res.message, 0, sizeof(res.message));
    if (message) {
        std::snprintf(res.message, sizeof(res.message), "%s", message);
    }

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto* c : m_clients) {
        if (!c) continue;
        if (m_fdToPlayerId.find(c->GetFd()) == m_fdToPlayerId.end()) continue;
        PacketUtils::SendPacket(c, PacketType::RES_INGAME_REMATCH_STATUS, res);
    }
}

void GameServer::EnableRecording(const std::string& path) {
    m_recordPath = path;
}

void GameServer::EnableReplay(const std::string& path) {
    m_replayPath = path;
    m_isReplayMode = true;
}

void GameServer::Stop() {
    mIsRunning = false;
    m_gameServerSocket.Close();

    // Close clients to unblock receive loops; client threads own deletion.
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto* c : m_clients) {
            if (c) c->Close();
        }
        m_clients.clear();
        m_fdToPlayerId.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_pauseActive = false;
        m_pauseRequesterId = UINT32_MAX;
        m_pauseEndingEarly = false;
        m_pauseEndRequestedBy = UINT32_MAX;
        m_pauseUses[0] = 0;
        m_pauseUses[1] = 0;
    }

    {
        std::lock_guard<std::mutex> lock(m_drawMutex);
        m_drawPending = false;
        m_drawRequesterId = UINT32_MAX;
    }

    if (m_gameLoopThread.joinable()) {
        m_gameLoopThread.join();
    }

    if (m_replayThread.joinable()) {
        m_replayThread.join();
    }

    m_replayWriter.Close();
    m_replayReader.Close();

    {
        std::lock_guard<std::mutex> lock(m_roomMutex);
        delete m_gameRoom;
        m_gameRoom = nullptr;

        for (auto* p : m_players) {
            delete p;
        }
        m_players.clear();

        delete m_mapLoader;
        m_mapLoader = nullptr;
    }
}

void GameServer::Run(int port) {
    try {
        if (m_isReplayMode.load()) {
            if (m_replayPath.empty()) {
                throw std::runtime_error("Replay mode enabled but no replay file path provided");
            }
            m_replayReader.Open(m_replayPath);
            m_replayTickRate = m_replayReader.Header().tickRate;
            m_matchId = m_replayReader.Header().matchId;
            m_replayFirstTick = m_replayReader.Header().firstTick;
            m_replayLastTick = m_replayReader.Header().lastTick;
            m_replayMapPath = (m_replayReader.Header().mapPath[0] != '\0') ? std::string(m_replayReader.Header().mapPath) : std::string(kDefaultMap);

            if (m_replayFirstTick == 0 && m_replayReader.FrameCount() > 0) {
                ResIngameState first{};
                if (m_replayReader.ReadFrameAtIndex(0, first)) {
                    m_replayFirstTick = first.tick;
                }
            }
            if (m_replayLastTick == 0 && m_replayReader.FrameCount() > 0) {
                ResIngameState last{};
                if (m_replayReader.ReadFrameAtIndex(m_replayReader.FrameCount() - 1, last)) {
                    m_replayLastTick = last.tick;
                }
            }
            m_replayCurrentIndex = 0;
            m_replayPaused = false;
            m_replaySpeed = 1.0f;
            m_replaySeekPending = false;
        }

        m_gameServerSocket.Bind(port);
        m_gameServerSocket.Listen();
        mIsRunning = true;

        std::cout << "GameServer listening on port " << port << std::endl;

        if (m_isReplayMode.load()) {
            std::cout << "GameServer running in REPLAY mode from " << m_replayPath << std::endl;
            m_replayThread = std::thread(&GameServer::ReplayLoop, this);
        } else {
            if (!m_recordPath.empty()) {
                std::cout << "GameServer recording replay to " << m_recordPath << std::endl;
                m_replayWriter.Open(m_recordPath, 60 /*tickRate*/, m_matchId);
            }
            m_gameLoopThread = std::thread(&GameServer::GameLoop, this);
        }

        while (mIsRunning) {
            TCPSocket* clientSocket = m_gameServerSocket.Accept();
            if (clientSocket == nullptr) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(m_clientsMutex);
                m_clients.push_back(clientSocket);
            }

            std::cout << "GameServer accepted connection." << std::endl;
            std::thread clientThread(&GameServer::HandleClient, this, clientSocket);
            clientThread.detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "GameServer encountered an error: " << e.what() << std::endl;
    }
}

void GameServer::RemoveClient(TCPSocket* clientSocket) {
    if (!clientSocket) return;

    // If a player leaves after game end, the remaining player should not wait indefinitely.
    bool gameOver = false;
    {
        std::lock_guard<std::mutex> lockRoom(m_roomMutex);
        gameOver = (m_gameRoom && m_gameRoom->getState() == GAME_OVER);
    }

    const int fd = clientSocket->GetFd();

    bool shouldNotifyOpponentNoRematch = false;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_fdToPlayerId.erase(fd);

        auto it = std::find(m_clients.begin(), m_clients.end(), clientSocket);
        if (it != m_clients.end()) {
            m_clients.erase(it);
        }

        // If the match ended and there is still someone connected, notify them.
        shouldNotifyOpponentNoRematch = gameOver && !m_isReplayMode.load() && !m_clients.empty();
    }

    if (shouldNotifyOpponentNoRematch) {
        // Stop any rematch window and force clients to disable rematch.
        {
            std::lock_guard<std::mutex> lockRematch(m_rematchMutex);
            m_rematchPending = false;
            m_rematchAcceptedMask = 0;
            m_rematchExpireTime = std::chrono::steady_clock::time_point{};
        }
        BroadcastRematchStatus(2, "opponent_left");
    }
}

void GameServer::HandleClient(TCPSocket* clientSocket) {
    if (!clientSocket) return;

    bool connected = true;

    while (connected && mIsRunning) {
        Packet packet;
        if (!PacketUtils::ReceivePacket(clientSocket, packet)) {
            connected = false;
            break;
        }

        switch (packet.header.type) {
            case PacketType::REQ_INGAME_JOIN: {
                ReqIngameJoin req = packet.GetPayload<ReqIngameJoin>();
                req.username[31] = '\0'; // Safety

                ResIngameJoin res{};
                // If the service server assigned a match id, adopt it for snapshots/replays.
                if (req.matchId != 0) {
                    std::lock_guard<std::mutex> lock(m_roomMutex);
                    m_matchId = req.matchId;
                }
                res.matchId = (req.matchId == 0) ? m_matchId : req.matchId;
                res.playerId = UINT32_MAX;

                // Save username mapping
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    m_fdToName[clientSocket->GetFd()] = req.username;
                }

                bool handledReplayJoin = false;
                ResReplayInfo replayInfo{};
                std::memset(replayInfo.mapPath, 0, sizeof(replayInfo.mapPath));

                uint32_t assignedPlayerId = UINT32_MAX;
                {
                    std::lock_guard<std::mutex> lock(m_roomMutex);

                    if (m_isReplayMode.load()) {
                        // Replay spectators: accept join without creating server-side Player.
                        assignedPlayerId = UINT32_MAX;
                        res.isSuccess = true;
                        res.playerId = UINT32_MAX;
                        std::snprintf(res.message, sizeof(res.message), "Joined replay match %u as spectator", res.matchId);
                        const std::string mp = m_replayMapPath.empty() ? std::string(kDefaultMap) : m_replayMapPath;
                        std::snprintf(replayInfo.mapPath, sizeof(replayInfo.mapPath), "%s", mp.c_str());
                        handledReplayJoin = true;
                    }

                    // Simple restart support: if the previous match ended, reset server-side state
                    // so new clients can join again.
                    if (m_gameRoom && m_gameRoom->getState() == GAME_OVER) {
                        delete m_gameRoom;
                        m_gameRoom = nullptr;

                        for (auto* p : m_players) {
                            delete p;
                        }
                        m_players.clear();

                        delete m_mapLoader;
                        m_mapLoader = nullptr;

                        m_matchId++;
                        m_tick = 0;

                        m_matchHasStartTime = false;
                        m_matchSaved = false;
                        m_playerUserIds[0] = 0;
                        m_playerUserIds[1] = 0;

                        // Reset rematch handshake state for the new match.
                        {
                            std::lock_guard<std::mutex> lockRematch(m_rematchMutex);
                            m_rematchPending = false;
                            m_rematchAcceptedMask = 0;
                            m_rematchExpireTime = std::chrono::steady_clock::time_point{};
                        }
                    }

                    if (!m_mapLoader) {
                        m_mapLoader = new MapLoader();
                        std::string reqMapName = (req.mapName[0] != '\0') ? std::string(req.mapName) : std::string(kDefaultMap);
                        std::string mapPath = reqMapName;

                        if (reqMapName == "RANDOM") {
                            mapPath = GetRandomMapFromAssets();
                            std::cout << "GameServer: Match " << m_matchId << " selected random map: " << mapPath << std::endl;
                        }

                        if (!m_mapLoader->loadMap(mapPath)) {
                            res.isSuccess = false;
                            std::snprintf(res.message, sizeof(res.message), "Failed to load map: %s", mapPath.c_str());
                            PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_JOIN, res);
                            break;
                        }

                        m_currentMapPath = mapPath;
                        if (m_replayWriter.IsOpen()) {
                            m_replayWriter.SetMapPath(mapPath);
                        }
                    }

                    if (m_players.size() >= INGAME_MAX_PLAYERS) {
                        res.isSuccess = false;
                        std::snprintf(res.message, sizeof(res.message), "Room full");
                        PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_JOIN, res);
                        break;
                    }

                    assignedPlayerId = static_cast<uint32_t>(m_players.size());
                    const auto& spawns = m_mapLoader->getSpawnPoints();
                    float sx = (spawns.size() > assignedPlayerId) ? spawns[assignedPlayerId].x : (100.0f + 300.0f * assignedPlayerId);
                    float sy = (spawns.size() > assignedPlayerId) ? spawns[assignedPlayerId].y : 100.0f;
                    bool orient = (assignedPlayerId % 2 == 0);

                    std::string pName = (req.username[0] != '\0') ? std::string(req.username) : ("Player" + std::to_string(assignedPlayerId + 1));
                    Player* p = new Player((int)assignedPlayerId,
                                           pName,
                                           sx,
                                           sy,
                                           orient);
                    m_players.push_back(p);

                    if (assignedPlayerId < INGAME_MAX_PLAYERS) {
                        m_playerUserIds[assignedPlayerId] = req.userId;
                    }

                    if (!m_gameRoom && m_players.size() >= 2) {
                        m_gameRoom = new GameRoom(m_players);
                        m_gameRoom->setMapLoader(m_mapLoader);
                        m_gameRoom->startGame();

                        m_matchStartedAt = std::chrono::system_clock::now();
                        m_matchHasStartTime = true;
                        m_matchSaved = false;
                    }
                }

                if (handledReplayJoin) {
                    {
                        std::lock_guard<std::mutex> lock(m_clientsMutex);
                        // Mark spectators as joined so they can receive replay frames.
                        m_fdToPlayerId[clientSocket->GetFd()] = UINT32_MAX;
                    }

                    PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_JOIN, res);
                    PacketUtils::SendPacket(clientSocket, PacketType::RES_REPLAY_INFO, replayInfo);
                    break;
                }

                if (assignedPlayerId != UINT32_MAX) {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    m_fdToPlayerId[clientSocket->GetFd()] = assignedPlayerId;
                }

                res.isSuccess = (assignedPlayerId != UINT32_MAX);
                res.playerId = assignedPlayerId;
                std::memset(res.mapPath, 0, sizeof(res.mapPath));
                if (res.isSuccess) {
                    std::snprintf(res.message, sizeof(res.message), "Joined match %u as player %u", res.matchId, res.playerId);
                    std::snprintf(res.mapPath, sizeof(res.mapPath), "%s", m_currentMapPath.c_str());
                }
                PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_JOIN, res);
            } break;

            case PacketType::REQ_INGAME_INPUT: {
                if (m_isReplayMode.load()) break;

                // Ignore gameplay input while paused (authoritative).
                {
                    std::lock_guard<std::mutex> lock(m_pauseMutex);
                    if (m_pauseActive) break;
                }

                ReqIngameInput req = packet.GetPayload<ReqIngameInput>();
                const char* cmd = CommandToString(req.command);
                if (!cmd) break;

                std::lock_guard<std::mutex> lock(m_roomMutex);
                if (!m_gameRoom) break;
                m_gameRoom->handleInput((int)req.playerId, cmd, req.value);
            } break;

            case PacketType::REQ_INGAME_CHAT: {
                if (m_isReplayMode.load()) break;

                uint32_t senderId = UINT32_MAX;
                std::string senderName = "Unknown";
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    auto it = m_fdToPlayerId.find(clientSocket->GetFd());
                    if (it != m_fdToPlayerId.end()) senderId = it->second;
                    
                    auto itName = m_fdToName.find(clientSocket->GetFd());
                    if (itName != m_fdToName.end()) senderName = itName->second;
                }

                ReqIngameChat req = packet.GetPayload<ReqIngameChat>();
                req.message[127] = '\0'; // Safety

                ResIngameChat res{};
                res.senderPlayerId = senderId;
                res.isSystem = 0;
                std::snprintf(res.senderName, sizeof(res.senderName), "%s", senderName.c_str());
                std::snprintf(res.message, sizeof(res.message), "%s", req.message);

                // Broadcast to all clients in this server
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    for (auto* c : m_clients) {
                        if (c) PacketUtils::SendPacket(c, PacketType::RES_INGAME_CHAT, res);
                    }
                }
            } break;

            case PacketType::REQ_INGAME_PAUSE_REQUEST: {
                if (m_isReplayMode.load()) break;

                // Determine requester from connection mapping (don't trust client payload).
                uint32_t requesterId = UINT32_MAX;
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    auto it = m_fdToPlayerId.find(clientSocket->GetFd());
                    if (it != m_fdToPlayerId.end()) requesterId = it->second;
                }

                ResIngamePauseResult res{};
                res.isSuccess = 0;
                res.remainingUses = 0;
                std::memset(res.message, 0, sizeof(res.message));

                if (requesterId == UINT32_MAX || requesterId >= INGAME_MAX_PLAYERS) {
                    std::snprintf(res.message, sizeof(res.message), "Not a player");
                    PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_PAUSE_RESULT, res);
                    break;
                }

                // Validate game state and enforce limits.
                bool canPause = false;
                uint8_t remaining = 0;
                {
                    std::lock_guard<std::mutex> lockPause(m_pauseMutex);
                    if (m_pauseActive) {
                        std::snprintf(res.message, sizeof(res.message), "Already paused");
                        remaining = (m_pauseUses[requesterId] >= 3) ? 0 : (uint8_t)(3 - m_pauseUses[requesterId]);
                    } else if (m_pauseUses[requesterId] >= 3) {
                        std::snprintf(res.message, sizeof(res.message), "Pause limit reached");
                        remaining = 0;
                    } else {
                        // Check the room exists and is not over.
                        std::lock_guard<std::mutex> lockRoom(m_roomMutex);
                        if (!m_gameRoom) {
                            std::snprintf(res.message, sizeof(res.message), "Game not started");
                            remaining = (uint8_t)(3 - m_pauseUses[requesterId]);
                        } else if (m_gameRoom->getState() == GAME_OVER) {
                            std::snprintf(res.message, sizeof(res.message), "Game over");
                            remaining = (uint8_t)(3 - m_pauseUses[requesterId]);
                        } else {
                            canPause = true;
                            m_pauseUses[requesterId]++;
                            remaining = (uint8_t)(3 - m_pauseUses[requesterId]);
                            m_pauseActive = true;
                            m_pauseRequesterId = requesterId;
                            m_pauseEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(kPauseDurationMs);
                            m_pauseEndingEarly = false;
                            m_pauseEndRequestedBy = UINT32_MAX;
                        }
                    }
                }

                res.remainingUses = remaining;

                if (!canPause) {
                    PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_PAUSE_RESULT, res);
                    break;
                }

                res.isSuccess = 1;
                std::snprintf(res.message, sizeof(res.message), "Pause started (%us)", kPauseDurationMs / 1000u);
                PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_PAUSE_RESULT, res);

                BroadcastPauseSignal(requesterId, kPauseDurationMs, remaining);
            } break;

            case PacketType::REQ_INGAME_PAUSE_END_EARLY: {
                if (m_isReplayMode.load()) break;

                uint32_t requesterId = UINT32_MAX;
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    auto it = m_fdToPlayerId.find(clientSocket->GetFd());
                    if (it != m_fdToPlayerId.end()) requesterId = it->second;
                }

                bool scheduledCountdown = false;
                uint8_t remainingUses = 0;
                {
                    std::lock_guard<std::mutex> lock(m_pauseMutex);
                    if (m_pauseActive && requesterId != UINT32_MAX && requesterId == m_pauseRequesterId) {
                        // Don't end immediately; schedule a 3-second countdown so both clients see it.
                        if (!m_pauseEndingEarly) {
                            m_pauseEndingEarly = true;
                            m_pauseEndRequestedBy = requesterId;
                            m_pauseEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(kPauseResumeCountdownMs);
                            remainingUses = (m_pauseUses[requesterId] >= 3) ? 0 : (uint8_t)(3 - m_pauseUses[requesterId]);
                            scheduledCountdown = true;
                        }
                    }
                }

                if (scheduledCountdown) {
                    BroadcastPauseSignal(requesterId, kPauseResumeCountdownMs, remainingUses);
                }
            } break;

            case PacketType::REQ_INGAME_DRAW_REQUEST: {
                if (m_isReplayMode.load()) break;

                uint32_t requesterId = UINT32_MAX;
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    auto it = m_fdToPlayerId.find(clientSocket->GetFd());
                    if (it != m_fdToPlayerId.end()) requesterId = it->second;
                }

                auto sendDeniedToRequester = [&](const char* msg) {
                    ResIngameDrawResult res{};
                    res.matchId = m_matchId;
                    res.requesterPlayerId = requesterId;
                    res.responderPlayerId = UINT32_MAX;
                    res.result = (uint32_t)DRAW_RESULT_DENIED;
                    std::memset(res.message, 0, sizeof(res.message));
                    if (msg) std::snprintf(res.message, sizeof(res.message), "%s", msg);
                    PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_DRAW_RESULT, res);
                };

                if (requesterId == UINT32_MAX || requesterId >= INGAME_MAX_PLAYERS) {
                    sendDeniedToRequester("Not a player");
                    break;
                }

                // Validate room state
                {
                    std::lock_guard<std::mutex> lockRoom(m_roomMutex);
                    if (!m_gameRoom || m_gameRoom->getState() == GAME_OVER) {
                        sendDeniedToRequester("Game not active");
                        break;
                    }
                }

                bool accepted = false;
                {
                    std::lock_guard<std::mutex> lock(m_drawMutex);
                    if (m_drawPending) {
                        // Already pending; deny requester only.
                    } else {
                        m_drawPending = true;
                        m_drawRequesterId = requesterId;
                        m_drawExpireTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(kDrawOfferTimeoutMs);
                        accepted = true;
                    }
                }

                if (!accepted) {
                    sendDeniedToRequester("Draw already pending");
                    break;
                }

                BroadcastDrawSignal(requesterId, kDrawOfferTimeoutMs);
            } break;

            case PacketType::REQ_INGAME_DRAW_DECISION: {
                if (m_isReplayMode.load()) break;

                uint32_t responderId = UINT32_MAX;
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    auto it = m_fdToPlayerId.find(clientSocket->GetFd());
                    if (it != m_fdToPlayerId.end()) responderId = it->second;
                }

                ReqIngameDrawDecision req = packet.GetPayload<ReqIngameDrawDecision>();
                (void)req;

                uint32_t requesterId = UINT32_MAX;
                bool pending = false;
                bool expired = false;
                {
                    std::lock_guard<std::mutex> lock(m_drawMutex);
                    pending = m_drawPending;
                    requesterId = m_drawRequesterId;
                    expired = pending && (std::chrono::steady_clock::now() >= m_drawExpireTime);
                    if (expired) {
                        m_drawPending = false;
                        m_drawRequesterId = UINT32_MAX;
                    }
                }

                if (expired && requesterId != UINT32_MAX) {
                    BroadcastDrawResult(requesterId, UINT32_MAX, (uint32_t)DRAW_RESULT_TIMEOUT, "Draw request timed out");
                    break;
                }

                if (!pending || requesterId == UINT32_MAX) {
                    ResIngameDrawResult res{};
                    res.matchId = m_matchId;
                    res.requesterPlayerId = UINT32_MAX;
                    res.responderPlayerId = responderId;
                    res.result = (uint32_t)DRAW_RESULT_DENIED;
                    std::memset(res.message, 0, sizeof(res.message));
                    std::snprintf(res.message, sizeof(res.message), "%s", "No draw pending");
                    PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_DRAW_RESULT, res);
                    break;
                }

                if (responderId == UINT32_MAX || responderId == requesterId) {
                    ResIngameDrawResult res{};
                    res.matchId = m_matchId;
                    res.requesterPlayerId = requesterId;
                    res.responderPlayerId = responderId;
                    res.result = (uint32_t)DRAW_RESULT_DENIED;
                    std::memset(res.message, 0, sizeof(res.message));
                    std::snprintf(res.message, sizeof(res.message), "%s", "Invalid responder");
                    PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_DRAW_RESULT, res);
                    break;
                }

                const bool accept = (req.accept != 0);

                {
                    std::lock_guard<std::mutex> lock(m_drawMutex);
                    // Clear pending regardless of accept/decline
                    m_drawPending = false;
                    m_drawRequesterId = UINT32_MAX;
                }

                if (accept) {
                    {
                        std::lock_guard<std::mutex> lockRoom(m_roomMutex);
                        if (m_gameRoom && m_gameRoom->getState() != GAME_OVER) {
                            m_gameRoom->endGameAsDraw();
                        }
                    }
                    BroadcastDrawResult(requesterId, responderId, (uint32_t)DRAW_RESULT_ACCEPTED, "Draw accepted");
                } else {
                    BroadcastDrawResult(requesterId, responderId, (uint32_t)DRAW_RESULT_DECLINED, "Draw declined");
                }
            } break;

            case PacketType::REQ_INGAME_SURRENDER: {
                if (m_isReplayMode.load()) break;

                uint32_t requesterId = UINT32_MAX;
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    auto it = m_fdToPlayerId.find(clientSocket->GetFd());
                    if (it != m_fdToPlayerId.end()) requesterId = it->second;
                }

                ResIngameSurrenderResult res{};
                res.isSuccess = 0;
                std::memset(res.message, 0, sizeof(res.message));

                if (requesterId == UINT32_MAX || requesterId >= INGAME_MAX_PLAYERS) {
                    std::snprintf(res.message, sizeof(res.message), "%s", "Not a player");
                    PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_SURRENDER_RESULT, res);
                    break;
                }

                bool ok = false;
                {
                    std::lock_guard<std::mutex> lockRoom(m_roomMutex);
                    if (m_gameRoom && m_gameRoom->getState() != GAME_OVER) {
                        m_gameRoom->endGameBySurrender((int)requesterId);
                        ok = true;
                    }
                }

                if (!ok) {
                    std::snprintf(res.message, sizeof(res.message), "%s", "Game not active");
                    PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_SURRENDER_RESULT, res);
                    break;
                }

                // Clear auxiliary states so clients don't stay in pause/draw UI if they linger.
                {
                    std::lock_guard<std::mutex> lockPause(m_pauseMutex);
                    m_pauseActive = false;
                    m_pauseRequesterId = UINT32_MAX;
                    m_pauseEndingEarly = false;
                    m_pauseEndRequestedBy = UINT32_MAX;
                }
                {
                    std::lock_guard<std::mutex> lockDraw(m_drawMutex);
                    m_drawPending = false;
                    m_drawRequesterId = UINT32_MAX;
                }

                res.isSuccess = 1;
                std::snprintf(res.message, sizeof(res.message), "%s", "Surrendered");
                PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_SURRENDER_RESULT, res);
            } break;

            case PacketType::REQ_INGAME_REMATCH_REQUEST: {
                if (m_isReplayMode.load()) break;

                // Determine requester from connection mapping (don't trust client payload).
                uint32_t requesterId = UINT32_MAX;
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    auto it = m_fdToPlayerId.find(clientSocket->GetFd());
                    if (it != m_fdToPlayerId.end()) requesterId = it->second;
                }

                // Only allow rematch when a match exists and is over.
                bool isGameOver = false;
                {
                    std::lock_guard<std::mutex> lockRoom(m_roomMutex);
                    isGameOver = (m_gameRoom && m_gameRoom->getState() == GAME_OVER);
                }
                if (!isGameOver || requesterId == UINT32_MAX || requesterId >= INGAME_MAX_PLAYERS) {
                    // Ignore invalid requests silently.
                    break;
                }

                // If the opponent already left, rematch is impossible.
                bool bothPlayersConnected = false;
                {
                    std::lock_guard<std::mutex> lockClients(m_clientsMutex);
                    bool has0 = false;
                    bool has1 = false;
                    for (const auto& kv : m_fdToPlayerId) {
                        if (kv.second == 0) has0 = true;
                        if (kv.second == 1) has1 = true;
                    }
                    bothPlayersConnected = has0 && has1;
                }
                if (!bothPlayersConnected) {
                    BroadcastRematchStatus(2, "opponent_left");
                    break;
                }

                bool shouldBroadcastStart = false;
                {
                    std::lock_guard<std::mutex> lockRematch(m_rematchMutex);
                    if (!m_rematchPending) {
                        m_rematchPending = true;
                        m_rematchAcceptedMask = 0;
                        m_rematchExpireTime = std::chrono::steady_clock::now() + std::chrono::minutes(1);
                    }
                    m_rematchAcceptedMask |= (uint8_t)(1u << requesterId);
                    if ((m_rematchAcceptedMask & 0x3u) == 0x3u) {
                        shouldBroadcastStart = true;
                    }
                }

                if (shouldBroadcastStart) {
                    BroadcastRematchStatus(1, "start");
                } else {
                    BroadcastRematchStatus(0, "waiting");
                }
            } break;

            case PacketType::REQ_REPLAY_CONTROL: {
                if (!m_isReplayMode.load()) break;

                ReqReplayControl ctl = packet.GetPayload<ReqReplayControl>();
                switch ((ReplayControlCommand)ctl.command) {
                    case REPLAY_CMD_SET_PAUSED: {
                        m_replayPaused = (ctl.value != 0.0f);
                    } break;
                    case REPLAY_CMD_SET_SPEED: {
                        float s = ctl.value;
                        if (s < 0.1f) s = 0.1f;
                        if (s > 16.0f) s = 16.0f;
                        m_replaySpeed = s;
                    } break;
                    case REPLAY_CMD_SEEK_TICK: {
                        m_replaySeekTick = ctl.tick;
                        m_replaySeekPending = true;
                    } break;
                    default:
                        break;
                }

                ResReplayStatus st{};
                const uint32_t idx = m_replayCurrentIndex.load();
                if (m_replayFirstTick != 0) {
                    st.currentTick = m_replayFirstTick + idx;
                } else {
                    st.currentTick = 0;
                }
                st.firstTick = m_replayFirstTick;
                st.lastTick = m_replayLastTick;
                st.isPaused = m_replayPaused.load() ? 1 : 0;
                st.speed = m_replaySpeed.load();
                PacketUtils::SendPacket(clientSocket, PacketType::RES_REPLAY_STATUS, st);
            } break;

            case PacketType::REQ_LOGOUT:
                connected = false;
                break;

            default:
                // Ignore other packet types.
                break;
        }
    }

    RemoveClient(clientSocket);
    clientSocket->Close();
    delete clientSocket;
}

void GameServer::GameLoop() {
    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (mIsRunning) {
        auto now = clock::now();
        std::chrono::duration<float> dt = now - last;
        last = now;

        float deltaSeconds = dt.count();
        if (deltaSeconds < 0.0f) deltaSeconds = 0.0f;
        if (deltaSeconds > 0.1f) deltaSeconds = 0.1f;

        // Pause timeout check (authoritative)
        bool pauseFinished = false;
        uint32_t endedBy = UINT32_MAX;
        {
            std::lock_guard<std::mutex> lock(m_pauseMutex);
            if (m_pauseActive && clock::now() >= m_pauseEndTime) {
                m_pauseActive = false;
                endedBy = m_pauseEndingEarly ? m_pauseEndRequestedBy : UINT32_MAX;
                m_pauseRequesterId = UINT32_MAX;
                m_pauseEndingEarly = false;
                m_pauseEndRequestedBy = UINT32_MAX;
                pauseFinished = true;
            }
        }
        if (pauseFinished) {
            BroadcastPauseEnd(endedBy);
        }

        // Draw offer timeout check
        bool drawTimedOut = false;
        uint32_t drawRequester = UINT32_MAX;
        {
            std::lock_guard<std::mutex> lock(m_drawMutex);
            if (m_drawPending && clock::now() >= m_drawExpireTime) {
                drawTimedOut = true;
                drawRequester = m_drawRequesterId;
                m_drawPending = false;
                m_drawRequesterId = UINT32_MAX;
            }
        }
        if (drawTimedOut && drawRequester != UINT32_MAX) {
            BroadcastDrawResult(drawRequester, UINT32_MAX, (uint32_t)DRAW_RESULT_TIMEOUT, "Draw request timed out");
        }

        // Rematch timeout check (60s window)
        bool rematchTimedOut = false;
        {
            std::lock_guard<std::mutex> lock(m_rematchMutex);
            if (m_rematchPending && (m_rematchAcceptedMask & 0x3u) != 0x3u && clock::now() >= m_rematchExpireTime) {
                rematchTimedOut = true;
                m_rematchPending = false;
                m_rematchAcceptedMask = 0;
                m_rematchExpireTime = std::chrono::steady_clock::time_point{};
            }
        }
        if (rematchTimedOut) {
            BroadcastRematchStatus(2, "timeout");
        }

        {
            std::lock_guard<std::mutex> lock(m_roomMutex);
            bool paused = false;
            {
                std::lock_guard<std::mutex> lockPause(m_pauseMutex);
                paused = m_pauseActive;
            }
            if (m_gameRoom && !paused) {
                m_gameRoom->update(deltaSeconds);
            }
        }

        // Persist match result exactly once when the game ends.
        // Do this outside room lock to avoid blocking the simulation.
        bool shouldSave = false;
        bool isDraw = false;
        uint32_t winnerPlayerId = UINT32_MAX;
        int p0hp = 0;
        int p1hp = 0;
        {
            std::lock_guard<std::mutex> lock(m_roomMutex);
            if (m_gameRoom && m_gameRoom->getState() == GAME_OVER && !m_matchSaved) {
                const auto& players = m_gameRoom->getPlayers();
                int aliveCount = 0;
                for (size_t i = 0; i < players.size() && i < INGAME_MAX_PLAYERS; ++i) {
                    Player* p = players[i];
                    if (!p) continue;
                    if (i == 0) p0hp = (int)p->getHP();
                    if (i == 1) p1hp = (int)p->getHP();
                    if (p->isAlive()) {
                        aliveCount++;
                        winnerPlayerId = (uint32_t)p->getId();
                    }
                }
                isDraw = (aliveCount != 1);
                shouldSave = true;
                // Avoid repeated attempts if DB is down.
                m_matchSaved = true;
            }
        }
        if (shouldSave && !m_isReplayMode.load()) {
            const auto endedAt = std::chrono::system_clock::now();
            const auto startedAt = m_matchHasStartTime ? m_matchStartedAt : endedAt;

            const uint32_t winnerUserId = (!isDraw && winnerPlayerId < INGAME_MAX_PLAYERS)
                                              ? m_playerUserIds[winnerPlayerId]
                                              : 0u;

            std::vector<MatchParticipantRecord> parts;
            for (uint32_t pid = 0; pid < INGAME_MAX_PLAYERS; ++pid) {
                MatchParticipantRecord pr{};
                pr.userId = m_playerUserIds[pid];
                pr.isPlayer = true;
                pr.team = 0;
                pr.turnOrder = (int)pid;
                pr.score = (pid == 0) ? p0hp : p1hp;
                parts.push_back(pr);
            }

            (void)m_matchRecorder.SaveMatch(
                m_matchId,
                FormatTimestamp(startedAt),
                FormatTimestamp(endedAt),
                isDraw,
                winnerUserId,
                m_recordPath,
                parts);
        }

        BroadcastStateSnapshot();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void GameServer::BroadcastStateSnapshot() {
    ResIngameState snapshot{};
    snapshot.matchId = m_matchId;
    snapshot.tick = ++m_tick;

    {
        std::lock_guard<std::mutex> lock(m_roomMutex);

        if (!m_gameRoom) {
            snapshot.roomState = (uint32_t)WAITING_FOR_PLAYERS;
            snapshot.turnTimer = 0.0f;
            snapshot.wind = 0.0f;
            snapshot.terrainModified = 0;
            snapshot.hasExplosion = 0;
            snapshot.explosionX = 0.0f;
            snapshot.explosionY = 0.0f;
            snapshot.explosionRadius = 0.0f;
            snapshot.playerCount = static_cast<uint8_t>(std::min<size_t>(m_players.size(), INGAME_MAX_PLAYERS));
            snapshot.projectileCount = 0;

            for (size_t i = 0; i < snapshot.playerCount; i++) {
                Player* p = m_players[i];
                if (!p) continue;
                const Position pos = p->getPosition();

                snapshot.players[i].id = (uint32_t)p->getId();
                std::memset(snapshot.players[i].name, 0, sizeof(snapshot.players[i].name));
                std::snprintf(snapshot.players[i].name, sizeof(snapshot.players[i].name), "%s", p->getName().c_str());
                snapshot.players[i].hp = (int32_t)p->getHP();
                snapshot.players[i].isAlive = p->isAlive() ? 1 : 0;
                snapshot.players[i].isMyTurn = p->isMyTurn() ? 1 : 0;
                snapshot.players[i].orient = pos.orient ? 1 : 0;
                snapshot.players[i].x = pos.x;
                snapshot.players[i].y = pos.y;
                snapshot.players[i].angle = p->m_angle;
                snapshot.players[i].power = p->m_power;
                snapshot.players[i].stamina = p->getStamina();
            }
        } else {
            snapshot.roomState = (uint32_t)m_gameRoom->getState();
            snapshot.turnTimer = m_gameRoom->getTurnTimer();
            snapshot.wind = m_gameRoom->getWind();
            snapshot.terrainModified = m_gameRoom->consumeTerrainModified() ? 1 : 0;

            float ex = 0.0f, ey = 0.0f, er = 0.0f;
            if (m_gameRoom->consumeLastExplosion(ex, ey, er)) {
                snapshot.hasExplosion = 1;
                snapshot.explosionX = ex;
                snapshot.explosionY = ey;
                snapshot.explosionRadius = er;
            } else {
                snapshot.hasExplosion = 0;
                snapshot.explosionX = 0.0f;
                snapshot.explosionY = 0.0f;
                snapshot.explosionRadius = 0.0f;
            }

            const auto& players = m_gameRoom->getPlayers();
            snapshot.playerCount = static_cast<uint8_t>(std::min<size_t>(players.size(), INGAME_MAX_PLAYERS));

            for (size_t i = 0; i < snapshot.playerCount; i++) {
                Player* p = players[i];
                if (!p) continue;
                const Position pos = p->getPosition();

                snapshot.players[i].id = (uint32_t)p->getId();
                std::memset(snapshot.players[i].name, 0, sizeof(snapshot.players[i].name));
                std::snprintf(snapshot.players[i].name, sizeof(snapshot.players[i].name), "%s", p->getName().c_str());
                snapshot.players[i].hp = (int32_t)p->getHP();
                snapshot.players[i].isAlive = p->isAlive() ? 1 : 0;
                snapshot.players[i].isMyTurn = p->isMyTurn() ? 1 : 0;
                snapshot.players[i].orient = pos.orient ? 1 : 0;
                snapshot.players[i].x = pos.x;
                snapshot.players[i].y = pos.y;
                snapshot.players[i].angle = p->m_angle;
                snapshot.players[i].power = p->m_power;
                snapshot.players[i].stamina = p->getStamina();
            }

            const auto& projectiles = m_gameRoom->getProjectiles();
            const size_t projCount = std::min<size_t>(projectiles.size(), INGAME_MAX_PROJECTILES);
            snapshot.projectileCount = static_cast<uint8_t>(projCount);

            for (size_t i = 0; i < projCount; i++) {
                const auto& pr = projectiles[i];
                snapshot.projectiles[i].isActive = pr.isActive ? 1 : 0;
                snapshot.projectiles[i].x = pr.position.x;
                snapshot.projectiles[i].y = pr.position.y;
                snapshot.projectiles[i].vx = pr.velocity.vx;
                snapshot.projectiles[i].vy = pr.velocity.vy;
                snapshot.projectiles[i].isPowerUp = pr.isPowerUp ? 1 : 0;
            }
        }
    }

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto* c : m_clients) {
        if (!c) continue;
        // Only send snapshots to clients that have completed REQ_INGAME_JOIN.
        if (m_fdToPlayerId.find(c->GetFd()) == m_fdToPlayerId.end()) continue;
        PacketUtils::SendPacket(c, PacketType::RES_INGAME_STATE, snapshot);
    }

    if (m_replayWriter.IsOpen()) {
        m_replayWriter.Append(snapshot);
    }
}

void GameServer::BroadcastReplayFrame(const ResIngameState& frame) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto* c : m_clients) {
        if (!c) continue;
        if (m_fdToPlayerId.find(c->GetFd()) == m_fdToPlayerId.end()) continue;
        PacketUtils::SendPacket(c, PacketType::RES_INGAME_STATE, frame);
    }
}

void GameServer::ReplayLoop() {
    using clock = std::chrono::steady_clock;
    const float baseDt = (m_replayTickRate > 0) ? (1.0f / (float)m_replayTickRate) : (1.0f / 60.0f);

    auto nextSend = clock::now();

    while (mIsRunning) {
        if (m_replayReader.FrameCount() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (m_replaySeekPending.exchange(false)) {
            const uint32_t targetTick = m_replaySeekTick.load();
            uint32_t idx = 0;
            if (m_replayFirstTick != 0 && targetTick >= m_replayFirstTick) {
                idx = targetTick - m_replayFirstTick;
            }
            if (idx >= m_replayReader.FrameCount()) {
                idx = m_replayReader.FrameCount() - 1;
            }
            m_replayCurrentIndex = idx;
            nextSend = clock::now();
        }

        if (m_replayPaused.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        const uint32_t idx = m_replayCurrentIndex.load();
        if (idx >= m_replayReader.FrameCount()) {
            m_replayPaused = true;
            continue;
        }

        ResIngameState frame{};
        if (!m_replayReader.ReadFrameAtIndex(idx, frame)) {
            m_replayPaused = true;
            continue;
        }

        // Ensure matchId matches replay header for clients.
        frame.matchId = m_matchId;

        BroadcastReplayFrame(frame);

        m_replayCurrentIndex = idx + 1;

        float speed = m_replaySpeed.load();
        if (speed < 0.1f) speed = 0.1f;
        const float dt = baseDt / speed;
        nextSend += std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(dt));
        const auto now = clock::now();
        if (nextSend > now) {
            std::this_thread::sleep_until(nextSend);
        } else {
            nextSend = now;
        }
    }
}