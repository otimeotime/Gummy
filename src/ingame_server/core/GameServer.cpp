#include "GameServer.hpp"

#include "../../common/network/PacketUtils.hpp"
#include "../../common/network/PacketStructs.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

namespace {
constexpr const char* kDefaultMap = "assets/maps/flatmap.txt";

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

    if (m_gameLoopThread.joinable()) {
        m_gameLoopThread.join();
    }

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
        m_gameServerSocket.Bind(port);
        m_gameServerSocket.Listen();
        mIsRunning = true;

        std::cout << "GameServer listening on port " << port << std::endl;

        m_gameLoopThread = std::thread(&GameServer::GameLoop, this);

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

    const int fd = clientSocket->GetFd();
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_fdToPlayerId.erase(fd);

    auto it = std::find(m_clients.begin(), m_clients.end(), clientSocket);
    if (it != m_clients.end()) {
        m_clients.erase(it);
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

                ResIngameJoin res{};
                res.matchId = (req.matchId == 0) ? m_matchId : req.matchId;
                res.playerId = UINT32_MAX;

                uint32_t assignedPlayerId = UINT32_MAX;
                {
                    std::lock_guard<std::mutex> lock(m_roomMutex);

                    if (!m_mapLoader) {
                        m_mapLoader = new MapLoader();
                        std::string mapPath = (req.mapName[0] != '\0') ? std::string(req.mapName) : std::string(kDefaultMap);
                        if (!m_mapLoader->loadMap(mapPath)) {
                            res.isSuccess = false;
                            std::snprintf(res.message, sizeof(res.message), "Failed to load map: %s", mapPath.c_str());
                            PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_JOIN, res);
                            break;
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

                    Player* p = new Player((int)assignedPlayerId,
                                           "Player" + std::to_string(assignedPlayerId + 1),
                                           sx,
                                           sy,
                                           orient);
                    m_players.push_back(p);

                    if (!m_gameRoom && m_players.size() >= 2) {
                        m_gameRoom = new GameRoom(m_players);
                        m_gameRoom->setMapLoader(m_mapLoader);
                        m_gameRoom->startGame();
                    }
                }

                if (assignedPlayerId != UINT32_MAX) {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    m_fdToPlayerId[clientSocket->GetFd()] = assignedPlayerId;
                }

                res.isSuccess = (assignedPlayerId != UINT32_MAX);
                res.playerId = assignedPlayerId;
                if (res.isSuccess) {
                    std::snprintf(res.message, sizeof(res.message), "Joined match %u as player %u", res.matchId, res.playerId);
                }
                PacketUtils::SendPacket(clientSocket, PacketType::RES_INGAME_JOIN, res);
            } break;

            case PacketType::REQ_INGAME_INPUT: {
                ReqIngameInput req = packet.GetPayload<ReqIngameInput>();
                const char* cmd = CommandToString(req.command);
                if (!cmd) break;

                std::lock_guard<std::mutex> lock(m_roomMutex);
                if (!m_gameRoom) break;
                m_gameRoom->handleInput((int)req.playerId, cmd, req.value);
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

        {
            std::lock_guard<std::mutex> lock(m_roomMutex);
            if (m_gameRoom) {
                m_gameRoom->update(deltaSeconds);
            }
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
                snapshot.players[i].hp = (int32_t)p->getHP();
                snapshot.players[i].isAlive = p->isAlive() ? 1 : 0;
                snapshot.players[i].isMyTurn = p->isMyTurn() ? 1 : 0;
                snapshot.players[i].orient = pos.orient ? 1 : 0;
                snapshot.players[i].x = pos.x;
                snapshot.players[i].y = pos.y;
                snapshot.players[i].angle = p->m_angle;
                snapshot.players[i].power = p->m_power;
            }
        } else {
            snapshot.roomState = (uint32_t)m_gameRoom->getState();
            snapshot.turnTimer = m_gameRoom->getTurnTimer();
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
                snapshot.players[i].hp = (int32_t)p->getHP();
                snapshot.players[i].isAlive = p->isAlive() ? 1 : 0;
                snapshot.players[i].isMyTurn = p->isMyTurn() ? 1 : 0;
                snapshot.players[i].orient = pos.orient ? 1 : 0;
                snapshot.players[i].x = pos.x;
                snapshot.players[i].y = pos.y;
                snapshot.players[i].angle = p->m_angle;
                snapshot.players[i].power = p->m_power;
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
            }
        }
    }

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto* c : m_clients) {
        if (!c) continue;
        PacketUtils::SendPacket(c, PacketType::RES_INGAME_STATE, snapshot);
    }
}