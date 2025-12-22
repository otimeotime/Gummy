#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>

#include "../../common/network/TCPSocket.hpp"
#include "../logic/GameRoom.hpp"
#include "../logic/MapLoader.hpp"

class GameServer {
private:
    TCPSocket m_gameServerSocket;
    std::atomic<bool> mIsRunning;

    GameRoom* m_gameRoom;
    MapLoader* m_mapLoader;
    std::vector<Player*> m_players;

    std::thread m_gameLoopThread;
    std::mutex m_roomMutex;

    std::mutex m_clientsMutex;
    std::vector<TCPSocket*> m_clients;
    std::unordered_map<int, uint32_t> m_fdToPlayerId;

    uint32_t m_matchId;
    std::atomic<uint32_t> m_tick;

    void HandleClient(TCPSocket* clientSocket);
    void GameLoop();
    void BroadcastStateSnapshot();
    void RemoveClient(TCPSocket* clientSocket);

public:
    GameServer();
    ~GameServer();

    void Run(int port = 9090);
    void Stop();
};


