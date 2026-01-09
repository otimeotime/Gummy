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

#include <chrono>

#include "../database/MatchRecorder.hpp"

#include "../replay/ReplayFile.hpp"

#include "../../common/network/TCPSocket.hpp"
#include "../logic/GameRoom.hpp"
#include "../logic/MapLoader.hpp"

class GameServer {
private:
    TCPSocket m_gameServerSocket;
    std::atomic<bool> mIsRunning;

    // Modes
    std::atomic<bool> m_isReplayMode{false};
    std::string m_recordPath;
    std::string m_replayPath;

    // Recording
    ReplayWriter m_replayWriter;

    // Replay playback state (only used in replay mode)
    ReplayReader m_replayReader;
    std::thread m_replayThread;
    std::atomic<bool> m_replayPaused{false};
    std::atomic<float> m_replaySpeed{1.0f};
    std::atomic<bool> m_replaySeekPending{false};
    std::atomic<uint32_t> m_replaySeekTick{0};
    std::atomic<uint32_t> m_replayCurrentIndex{0};
    uint32_t m_replayFirstTick{0};
    uint32_t m_replayLastTick{0};
    uint16_t m_replayTickRate{60};
    std::string m_replayMapPath;

    GameRoom* m_gameRoom;
    MapLoader* m_mapLoader;
    std::string m_currentMapPath;
    std::vector<Player*> m_players;

    std::thread m_gameLoopThread;
    std::mutex m_roomMutex;

    std::mutex m_clientsMutex;
    std::vector<TCPSocket*> m_clients;
    std::unordered_map<int, uint32_t> m_fdToPlayerId;
    std::unordered_map<int, std::string> m_fdToName;

    // Pause state (authoritative)
    std::mutex m_pauseMutex;
    bool m_pauseActive = false;
    uint32_t m_pauseRequesterId = UINT32_MAX;
    std::chrono::steady_clock::time_point m_pauseEndTime{};
    bool m_pauseEndingEarly = false;
    uint32_t m_pauseEndRequestedBy = UINT32_MAX;
    uint8_t m_pauseUses[INGAME_MAX_PLAYERS] = {0, 0};

    // Draw offer state (authoritative)
    std::mutex m_drawMutex;
    bool m_drawPending = false;
    uint32_t m_drawRequesterId = UINT32_MAX;
    std::chrono::steady_clock::time_point m_drawExpireTime{};

    // Rematch handshake state (authoritative)
    std::mutex m_rematchMutex;
    bool m_rematchPending = false;
    uint8_t m_rematchAcceptedMask = 0;
    std::chrono::steady_clock::time_point m_rematchExpireTime{};

    uint32_t m_matchId;
    std::atomic<uint32_t> m_tick;

    // Match persistence state
    uint32_t m_playerUserIds[INGAME_MAX_PLAYERS] = {0, 0};
    std::chrono::system_clock::time_point m_matchStartedAt{};
    bool m_matchHasStartTime = false;
    bool m_matchSaved = false;
    MatchRecorder m_matchRecorder;

    void BroadcastPauseSignal(uint32_t requesterId, uint32_t durationMs, uint8_t requesterRemainingUses);
    void BroadcastPauseEnd(uint32_t endedByPlayerId);

    void BroadcastDrawSignal(uint32_t requesterId, uint32_t durationMs);
    void BroadcastDrawResult(uint32_t requesterId, uint32_t responderId, uint32_t result, const char* message);

    void BroadcastRematchStatus(uint8_t status, const char* message);

    void HandleClient(TCPSocket* clientSocket);
    void GameLoop();
    void BroadcastStateSnapshot();
    void RemoveClient(TCPSocket* clientSocket);

    void ReplayLoop();
    void BroadcastReplayFrame(const ResIngameState& frame);

public:
    GameServer();
    ~GameServer();

    void SetMatchId(uint32_t matchId);

    void EnableRecording(const std::string& path);
    void EnableReplay(const std::string& path);

    void Run(int port = 9090);
    void Stop();
};


