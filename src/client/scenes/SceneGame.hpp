#pragma once

#include "../core/GameState.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../core/InputHandler.hpp"

#include "../ui/Button.hpp"
#include "../ui/Text.hpp"
#include "../ui/TextInput.hpp"

#include "../../common/network/TCPSocket.hpp"
#include "../../common/network/PacketUtils.hpp"
#include "../../common/network/PacketStructs.hpp"

#include "../../ingame_server/logic/MapLoader.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <SDL2/SDL_ttf.h>

class SceneGame : public GameState {
public:
    SceneGame(std::string serverIp = "127.0.0.1",
              int serverPort = 9090,
              std::string mapPath = "assets/maps/flatmap.txt",
              std::string username = "",
              uint32_t matchId = 0,
              uint32_t userId = 0,
              bool allowSpectator = true); // Default allow for standalone viewer

    bool onEnter() override;
    bool onExit() override;
    void update() override;
    void render() override;

    // Rematch handshake (used by TerminalScene overlay)
    bool SendRematchRequest();
    bool ConsumeRematchStart();
    uint8_t GetRematchStatus(uint8_t* outAcceptedMask = nullptr, std::string* outMessage = nullptr);
    uint32_t GetPlayerId() const { return m_playerId; }

    std::string getStateID() const override { return "SCENE_GAME"; }

private:
    void ReceiverLoop();
    void SendInput(uint32_t command, float value = 0.0f);

    void SendPauseRequest();
    void SendPauseEndEarly();
    void EnsurePauseUI();
    void DestroyPauseUI();

    void SendDrawRequest();
    void SendDrawDecision(bool accept);
    void EnsureDrawUI();
    void DestroyDrawUI();

    void SendSurrenderRequest();
    void EnsureSurrenderUI();
    void DestroySurrenderUI();

    void SendReplayControl(uint32_t command, uint32_t tick, float value);
    void EnsureReplayUI();
    void DestroyReplayUI();

    void renderHealthBar(const NetPlayerState& player, const std::string& nameLabel);

    void createMapTexture();
    void updateMapTexture();

    void ApplyPendingReplayMap();

    std::string m_serverIp;
    int m_serverPort;

    std::atomic<bool> m_running;
    std::thread m_receiverThread;

    TCPSocket* m_socket;

    uint32_t m_matchId;
    uint32_t m_playerId;
    uint32_t m_userId;
    uint32_t m_seq;

    std::mutex m_stateMutex;
    ResIngameState m_lastState;
    bool m_hasState;

    std::string m_bgTextureID;
    std::string m_playerID;
    std::string m_bulletID;

    std::string m_mapPath;
    std::string m_username;
    bool m_allowSpectator;
    bool m_initFailed = false; // Flag to trigger pop state on update
    MapLoader* m_mapLoader;
    SDL_Texture* m_mapTexture;
    bool m_mapModified;

    std::mutex m_pendingMapMutex;
    bool m_hasPendingMapPath = false;
    std::string m_pendingMapPath;

    TTF_Font* m_font;

    Uint32 m_lastTick;

    bool m_terminalQueued = false;

    // Rematch handshake state (updated by receiver thread)
    std::mutex m_rematchMutex;
    uint8_t m_rematchStatus = 0; // 0 waiting, 1 start, 2 timeout/failed
    uint8_t m_rematchAcceptedMask = 0;
    std::string m_rematchMessage;
    bool m_rematchStartPending = false;

    // Replay spectator UI/state (only used when m_playerId == UINT32_MAX)
    std::mutex m_replayMutex;
    bool m_hasReplayStatus = false;
    ResReplayStatus m_lastReplayStatus{};

    Button* m_btnReplayPlayPause = nullptr;
    Text* m_lblReplayPlayPause = nullptr;
    Button* m_btnReplaySpeedDown = nullptr;
    Text* m_lblReplaySpeedDown = nullptr;
    Button* m_btnReplaySpeedUp = nullptr;
    Text* m_lblReplaySpeedUp = nullptr;
    TextInput* m_inReplaySeekTick = nullptr;
    Button* m_btnReplaySeek = nullptr;
    Text* m_lblReplaySeek = nullptr;
    Text* m_lblReplayStatus = nullptr;

    // In-game pause UI/state (only used when m_playerId != UINT32_MAX)
    std::mutex m_pauseMutex;
    bool m_pauseActive = false;
    uint32_t m_pauseRequesterId = UINT32_MAX;
    Uint32 m_pauseEndTick = 0;
    uint8_t m_pauseRemainingUses = 3;
    int m_pauseLastShownSeconds = -1;

    Uint32 m_pauseToastUntilTick = 0;
    std::string m_pauseToastText;

    Button* m_btnPause = nullptr;
    Text* m_lblPause = nullptr;
    Text* m_lblPauseStatus = nullptr;
    Text* m_lblPauseCountdown = nullptr;
    Text* m_lblPauseHint = nullptr;

    // In-game draw offer UI/state (only used when m_playerId != UINT32_MAX)
    std::mutex m_drawMutex;
    bool m_drawPending = false;
    uint32_t m_drawRequesterId = UINT32_MAX;
    Uint32 m_drawExpireTick = 0;

    Uint32 m_drawToastUntilTick = 0;
    std::string m_drawToastText;

    Button* m_btnDraw = nullptr;
    Text* m_lblDraw = nullptr;
    Button* m_btnDrawAccept = nullptr;
    Text* m_lblDrawAccept = nullptr;
    Button* m_btnDrawDecline = nullptr;
    Text* m_lblDrawDecline = nullptr;
    Text* m_lblDrawStatus = nullptr;

    // In-game surrender UI/state (only used when m_playerId != UINT32_MAX)
    std::mutex m_surrenderMutex;
    bool m_surrenderConfirmActive = false;

    Uint32 m_surrenderToastUntilTick = 0;
    std::string m_surrenderToastText;

    Button* m_btnSurrender = nullptr;
    Text* m_lblSurrender = nullptr;

    Text* m_lblSurrenderConfirm = nullptr;
    Button* m_btnSurrenderYes = nullptr;
    Text* m_lblSurrenderYes = nullptr;
    Button* m_btnSurrenderNo = nullptr;
    Text* m_lblSurrenderNo = nullptr;

    Text* m_lblSurrenderStatus = nullptr;

    Button* m_btnPowerUp = nullptr;
    std::string m_powerUpIconID;
    bool m_powerUpArmed = false;
};
