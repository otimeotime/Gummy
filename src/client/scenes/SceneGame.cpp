#include "SceneGame.hpp"

#include "TerminalScene.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <algorithm>

namespace {
bool g_showHitboxes = true;
bool g_prevToggleHitboxes = false;

void DrawCircleOutline(SDL_Renderer* renderer, int cx, int cy, int r) {
    // Simple parametric circle; good enough for a debug overlay.
    const int segments = 64;
    double prevX = cx + r;
    double prevY = cy;
    for (int i = 1; i <= segments; i++) {
        const double t = (2.0 * M_PI * i) / segments;
        const double x = cx + r * std::cos(t);
        const double y = cy + r * std::sin(t);
        SDL_RenderDrawLine(renderer, (int)prevX, (int)prevY, (int)x, (int)y);
        prevX = x;
        prevY = y;
    }
}
} // namespace

SceneGame::SceneGame(std::string serverIp, int serverPort, std::string mapPath, std::string username, uint32_t matchId, uint32_t userId, bool allowSpectator)
    : m_serverIp(std::move(serverIp)),
      m_serverPort(serverPort),
      m_running(false),
      m_socket(nullptr),
      m_matchId(matchId == 0 ? 1u : matchId),
      m_playerId(UINT32_MAX),
      m_userId(userId),
      m_seq(0),
      m_hasState(false),
      m_bgTextureID(""),
      m_playerID(""),
      m_bulletID(""),
      m_mapPath(std::move(mapPath)),
      m_username(std::move(username)),
      m_allowSpectator(allowSpectator),
            m_mapLoader(nullptr),
            m_mapTexture(nullptr),
            m_mapModified(true),
            m_font(nullptr),
      m_lastTick(0) {
    std::memset(&m_lastState, 0, sizeof(m_lastState));
    std::memset(&m_lastReplayStatus, 0, sizeof(m_lastReplayStatus));
}

void SceneGame::SendPauseRequest() {
    if (!m_socket || !m_socket->IsValid()) return;
    if (m_playerId == UINT32_MAX) return;

    ReqIngamePauseRequest req{};
    req.matchId = m_matchId;
    PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_PAUSE_REQUEST, req);
}

void SceneGame::SendPauseEndEarly() {
    if (!m_socket || !m_socket->IsValid()) return;
    if (m_playerId == UINT32_MAX) return;

    ReqIngamePauseEndEarly req{};
    req.matchId = m_matchId;
    PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_PAUSE_END_EARLY, req);
}

void SceneGame::SendDrawRequest() {
    if (!m_socket || !m_socket->IsValid()) return;
    if (m_playerId == UINT32_MAX) return;

    ReqIngameDrawRequest req{};
    req.matchId = m_matchId;
    PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_DRAW_REQUEST, req);
}

void SceneGame::SendDrawDecision(bool accept) {
    if (!m_socket || !m_socket->IsValid()) return;
    if (m_playerId == UINT32_MAX) return;

    ReqIngameDrawDecision req{};
    req.matchId = m_matchId;
    req.accept = accept ? 1 : 0;
    PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_DRAW_DECISION, req);
}

void SceneGame::SendSurrenderRequest() {
    if (!m_socket || !m_socket->IsValid()) return;
    if (m_playerId == UINT32_MAX) return;

    ReqIngameSurrender req{};
    req.matchId = m_matchId;
    PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_SURRENDER, req);
}

bool SceneGame::SendRematchRequest() {
    if (!m_socket || !m_socket->IsValid()) return false;
    if (m_playerId == UINT32_MAX) return false;

    ReqIngameRematchRequest req{};
    req.matchId = m_matchId;
    return PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_REMATCH_REQUEST, req);
}

bool SceneGame::ConsumeRematchStart() {
    std::lock_guard<std::mutex> lock(m_rematchMutex);
    if (!m_rematchStartPending) return false;
    m_rematchStartPending = false;
    return true;
}

uint8_t SceneGame::GetRematchStatus(uint8_t* outAcceptedMask, std::string* outMessage) {
    std::lock_guard<std::mutex> lock(m_rematchMutex);
    if (outAcceptedMask) *outAcceptedMask = m_rematchAcceptedMask;
    if (outMessage) *outMessage = m_rematchMessage;
    return m_rematchStatus;
}

void SceneGame::EnsurePauseUI() {
    if (m_btnPause) return;

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    TextureManager::getInstance()->load(TextureManager::spritePath("button.png"), "btn_generic", renderer);

    const int btnW = 160;
    const int btnH = 45;
    const int margin = 20;
    const int x = 1280 - margin - btnW;
    const int y = 720 - margin - btnH;

    m_btnPause = new Button((float)x, (float)y, btnW, btnH, "btn_generic", [this]() {
        bool paused = false;
        uint32_t requester = UINT32_MAX;
        uint8_t remaining = 0;
        {
            std::lock_guard<std::mutex> lock(m_pauseMutex);
            paused = m_pauseActive;
            requester = m_pauseRequesterId;
            remaining = m_pauseRemainingUses;
        }

        if (!paused) {
            if (remaining == 0) {
                std::lock_guard<std::mutex> lock(m_pauseMutex);
                m_pauseToastText = "Pause limit reached";
                m_pauseToastUntilTick = SDL_GetTicks() + 2500;
                return;
            }
            SendPauseRequest();
            return;
        }

        // Only the requester can end the pause early.
        if (requester == m_playerId) {
            SendPauseEndEarly();
        }
    }, 181, 73);

    m_lblPause = new Text((float)x + 48, (float)y + 12, "assets/font.ttf", 18, "Pause", {255, 255, 255, 255});
    // Use in-repo font for the pause overlay (falls back if missing).
    m_lblPauseStatus = new Text(20, 20, "assets/font.ttf", 22, "", {255, 255, 255, 255});
    m_lblPauseCountdown = new Text(640, 300, "assets/font.ttf", 96, "", {255, 255, 255, 255});
    m_lblPauseHint = new Text(640, 420, "assets/font.ttf", 28, "", {255, 255, 255, 255});
}

void SceneGame::DestroyPauseUI() {
    if (m_btnPause) { m_btnPause->clean(); delete m_btnPause; m_btnPause = nullptr; }
    if (m_lblPause) { m_lblPause->clean(); delete m_lblPause; m_lblPause = nullptr; }
    if (m_lblPauseStatus) { m_lblPauseStatus->clean(); delete m_lblPauseStatus; m_lblPauseStatus = nullptr; }
    if (m_lblPauseCountdown) { m_lblPauseCountdown->clean(); delete m_lblPauseCountdown; m_lblPauseCountdown = nullptr; }
    if (m_lblPauseHint) { m_lblPauseHint->clean(); delete m_lblPauseHint; m_lblPauseHint = nullptr; }
}

void SceneGame::EnsureDrawUI() {
    if (m_btnDraw) return;

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    TextureManager::getInstance()->load(TextureManager::spritePath("button.png"), "btn_generic", renderer);

    const int btnW = 160;
    const int btnH = 45;
    const int margin = 20;
    const int gap = 10;
    const int pauseX = 1280 - margin - btnW;
    const int y = 720 - margin - btnH;
    const int drawX = pauseX - gap - btnW;

    m_btnDraw = new Button((float)drawX, (float)y, btnW, btnH, "btn_generic", [this]() {
        bool pending = false;
        {
            std::lock_guard<std::mutex> lock(m_drawMutex);
            pending = m_drawPending;
        }
        if (pending) return;

        // Optimistically flip UI; server will confirm via signal or reset via result.
        {
            std::lock_guard<std::mutex> lock(m_drawMutex);
            m_drawPending = true;
            m_drawRequesterId = m_playerId;
            m_drawExpireTick = SDL_GetTicks() + 10000;
        }
        SendDrawRequest();
    }, 181, 73);
    m_lblDraw = new Text(0, 0, "assets/font.ttf", 18, "Draw", {255, 255, 255, 255});

    // Accept / Decline buttons (only shown for the non-requester)
    const int smallW = (btnW - gap) / 2;
    m_btnDrawAccept = new Button((float)drawX, (float)y, smallW, btnH, "btn_generic", [this]() {
        SendDrawDecision(true);
    }, 181, 73);
    m_lblDrawAccept = new Text(0, 0, "assets/font.ttf", 18, "Accept", {255, 255, 255, 255});

    m_btnDrawDecline = new Button((float)(drawX + smallW + gap), (float)y, smallW, btnH, "btn_generic", [this]() {
        SendDrawDecision(false);
    }, 181, 73);
    m_lblDrawDecline = new Text(0, 0, "assets/font.ttf", 18, "Decline", {255, 255, 255, 255});

    // Use in-repo font for notification/status text.
    m_lblDrawStatus = new Text(0, 0, "assets/font.ttf", 22, "", {255, 255, 255, 255});
}

void SceneGame::EnsureSurrenderUI() {
    if (m_btnSurrender) return;

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    TextureManager::getInstance()->load(TextureManager::spritePath("button.png"), "btn_generic", renderer);

    const int btnW = 160;
    const int btnH = 45;

    // Base Surrender button (positioned each frame)
    m_btnSurrender = new Button(0, 0, btnW, btnH, "btn_generic", [this]() {
        // Don't allow surrender while paused.
        {
            std::lock_guard<std::mutex> lock(m_pauseMutex);
            if (m_pauseActive) return;
        }
        std::lock_guard<std::mutex> lock(m_surrenderMutex);
        m_surrenderConfirmActive = true;
    }, 181, 73);
    m_lblSurrender = new Text(0, 0, "assets/font.ttf", 18, "Surrender", {255, 255, 255, 255});

    // Confirmation overlay
    m_lblSurrenderConfirm = new Text(0, 0, "assets/font.ttf", 44, "Are you sure?", {255, 255, 255, 255});

    const int gap = 12;
    const int smallW = (btnW - gap) / 2;
    m_btnSurrenderYes = new Button(0, 0, smallW, btnH, "btn_generic", [this]() {
        {
            std::lock_guard<std::mutex> lock(m_surrenderMutex);
            m_surrenderConfirmActive = false;
        }
        SendSurrenderRequest();
    }, 181, 73);
    m_lblSurrenderYes = new Text(0, 0, "assets/font.ttf", 18, "Yes", {255, 255, 255, 255});

    m_btnSurrenderNo = new Button(0, 0, smallW, btnH, "btn_generic", [this]() {
        std::lock_guard<std::mutex> lock(m_surrenderMutex);
        m_surrenderConfirmActive = false;
    }, 181, 73);
    m_lblSurrenderNo = new Text(0, 0, "assets/font.ttf", 18, "No", {255, 255, 255, 255});

    // Top-right toast/status
    m_lblSurrenderStatus = new Text(0, 0, "assets/font.ttf", 22, "", {255, 255, 255, 255});
}

void SceneGame::DestroySurrenderUI() {
    if (m_btnSurrender) { m_btnSurrender->clean(); delete m_btnSurrender; m_btnSurrender = nullptr; }
    if (m_lblSurrender) { m_lblSurrender->clean(); delete m_lblSurrender; m_lblSurrender = nullptr; }
    if (m_lblSurrenderConfirm) { m_lblSurrenderConfirm->clean(); delete m_lblSurrenderConfirm; m_lblSurrenderConfirm = nullptr; }
    if (m_btnSurrenderYes) { m_btnSurrenderYes->clean(); delete m_btnSurrenderYes; m_btnSurrenderYes = nullptr; }
    if (m_lblSurrenderYes) { m_lblSurrenderYes->clean(); delete m_lblSurrenderYes; m_lblSurrenderYes = nullptr; }
    if (m_btnSurrenderNo) { m_btnSurrenderNo->clean(); delete m_btnSurrenderNo; m_btnSurrenderNo = nullptr; }
    if (m_lblSurrenderNo) { m_lblSurrenderNo->clean(); delete m_lblSurrenderNo; m_lblSurrenderNo = nullptr; }
    if (m_lblSurrenderStatus) { m_lblSurrenderStatus->clean(); delete m_lblSurrenderStatus; m_lblSurrenderStatus = nullptr; }
}

void SceneGame::DestroyDrawUI() {
    if (m_btnDraw) { m_btnDraw->clean(); delete m_btnDraw; m_btnDraw = nullptr; }
    if (m_lblDraw) { m_lblDraw->clean(); delete m_lblDraw; m_lblDraw = nullptr; }
    if (m_btnDrawAccept) { m_btnDrawAccept->clean(); delete m_btnDrawAccept; m_btnDrawAccept = nullptr; }
    if (m_lblDrawAccept) { m_lblDrawAccept->clean(); delete m_lblDrawAccept; m_lblDrawAccept = nullptr; }
    if (m_btnDrawDecline) { m_btnDrawDecline->clean(); delete m_btnDrawDecline; m_btnDrawDecline = nullptr; }
    if (m_lblDrawDecline) { m_lblDrawDecline->clean(); delete m_lblDrawDecline; m_lblDrawDecline = nullptr; }
    if (m_lblDrawStatus) { m_lblDrawStatus->clean(); delete m_lblDrawStatus; m_lblDrawStatus = nullptr; }
}

void SceneGame::SendReplayControl(uint32_t command, uint32_t tick, float value) {
    if (!m_socket || !m_socket->IsValid()) return;

    ReqReplayControl c{};
    c.command = command;
    c.tick = tick;
    c.value = value;
    PacketUtils::SendPacket(m_socket, PacketType::REQ_REPLAY_CONTROL, c);
}

void SceneGame::EnsureReplayUI() {
    if (m_btnReplayPlayPause) return;

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    TextureManager::getInstance()->load(TextureManager::spritePath("button.png"), "btn_generic", renderer);

    const int x0 = 30;
    const int y0 = 80;

    m_btnReplayPlayPause = new Button(x0, y0, 140, 40, "btn_generic", [this]() {
        bool paused = false;
        float speed = 1.0f;
        {
            std::lock_guard<std::mutex> lock(m_replayMutex);
            paused = m_hasReplayStatus ? (m_lastReplayStatus.isPaused != 0) : false;
            speed = m_hasReplayStatus ? m_lastReplayStatus.speed : 1.0f;
        }
        SendReplayControl((uint32_t)REPLAY_CMD_SET_PAUSED, 0, paused ? 0.0f : 1.0f);
        // keep speed unchanged; server returns status in response
        (void)speed;
    }, 181, 73);
    m_lblReplayPlayPause = new Text(0, 0, "assets/font.ttf", 18, "Play/Pause", {255, 255, 255, 255});
    m_btnReplayPlayPause->centerObject(m_lblReplayPlayPause);

    m_btnReplaySpeedDown = new Button(x0 + 150, y0, 60, 40, "btn_generic", [this]() {
        float speed = 1.0f;
        {
            std::lock_guard<std::mutex> lock(m_replayMutex);
            speed = m_hasReplayStatus ? m_lastReplayStatus.speed : 1.0f;
        }
        speed *= 0.5f;
        if (speed < 0.25f) speed = 0.25f;
        SendReplayControl((uint32_t)REPLAY_CMD_SET_SPEED, 0, speed);
    }, 181, 73);
    m_lblReplaySpeedDown = new Text(0, 0, "assets/font.ttf", 18, "-", {255, 255, 255, 255});
    m_btnReplaySpeedDown->centerObject(m_lblReplaySpeedDown);

    m_btnReplaySpeedUp = new Button(x0 + 220, y0, 60, 40, "btn_generic", [this]() {
        float speed = 1.0f;
        {
            std::lock_guard<std::mutex> lock(m_replayMutex);
            speed = m_hasReplayStatus ? m_lastReplayStatus.speed : 1.0f;
        }
        speed *= 2.0f;
        if (speed > 16.0f) speed = 16.0f;
        SendReplayControl((uint32_t)REPLAY_CMD_SET_SPEED, 0, speed);
    }, 181, 73);
    m_lblReplaySpeedUp = new Text(0, 0, "assets/font.ttf", 18, "+", {255, 255, 255, 255});
    m_btnReplaySpeedUp->centerObject(m_lblReplaySpeedUp);

    m_inReplaySeekTick = new TextInput(x0 + 300, y0, 160, 40, "assets/font.ttf", 18);
    m_btnReplaySeek = new Button(x0 + 470, y0, 90, 40, "btn_generic", [this]() {
        if (!m_inReplaySeekTick) return;
        const std::string s = m_inReplaySeekTick->getString();
        uint32_t tick = 0;
        try {
            tick = (uint32_t)std::stoul(s);
        } catch (...) {
            return;
        }
        SendReplayControl((uint32_t)REPLAY_CMD_SEEK_TICK, tick, 0.0f);
    }, 181, 73);
    m_lblReplaySeek = new Text(0, 0, "assets/font.ttf", 18, "Jump", {255, 255, 255, 255});
    m_btnReplaySeek->centerObject(m_lblReplaySeek);

    m_lblReplayStatus = new Text(x0, y0 + 50, "assets/font.ttf", 16, "Replay: connecting...", {255, 255, 0, 255});
}

void SceneGame::DestroyReplayUI() {
    if (m_btnReplayPlayPause) { m_btnReplayPlayPause->clean(); delete m_btnReplayPlayPause; m_btnReplayPlayPause = nullptr; }
    if (m_lblReplayPlayPause) { m_lblReplayPlayPause->clean(); delete m_lblReplayPlayPause; m_lblReplayPlayPause = nullptr; }
    if (m_btnReplaySpeedDown) { m_btnReplaySpeedDown->clean(); delete m_btnReplaySpeedDown; m_btnReplaySpeedDown = nullptr; }
    if (m_lblReplaySpeedDown) { m_lblReplaySpeedDown->clean(); delete m_lblReplaySpeedDown; m_lblReplaySpeedDown = nullptr; }
    if (m_btnReplaySpeedUp) { m_btnReplaySpeedUp->clean(); delete m_btnReplaySpeedUp; m_btnReplaySpeedUp = nullptr; }
    if (m_lblReplaySpeedUp) { m_lblReplaySpeedUp->clean(); delete m_lblReplaySpeedUp; m_lblReplaySpeedUp = nullptr; }
    if (m_inReplaySeekTick) { m_inReplaySeekTick->clean(); delete m_inReplaySeekTick; m_inReplaySeekTick = nullptr; }
    if (m_btnReplaySeek) { m_btnReplaySeek->clean(); delete m_btnReplaySeek; m_btnReplaySeek = nullptr; }
    if (m_lblReplaySeek) { m_lblReplaySeek->clean(); delete m_lblReplaySeek; m_lblReplaySeek = nullptr; }
    if (m_lblReplayStatus) { m_lblReplayStatus->clean(); delete m_lblReplayStatus; m_lblReplayStatus = nullptr; }
}

bool SceneGame::onEnter() {
    m_lastTick = SDL_GetTicks();

    m_bgTextureID = "background";
    m_playerID = "player";
    m_bulletID = "bullet";
    m_powerUpIconID = "power_up_icon";
    if (!TextureManager::getInstance()->load("assets/power_up.png", m_powerUpIconID, Game::getInstance()->getRenderer())) {
        std::cerr << "SceneGame: failed to load power-up icon" << std::endl;
    }
    if (!TextureManager::getInstance()->load("assets/sprites/gameplay_background.png", m_bgTextureID, Game::getInstance()->getRenderer())) {
        std::cerr << "SceneGame: failed to load background" << std::endl;
        // Use fallback if needed, or allow continuing to try connecting
        // return false; 
    }
    if (!TextureManager::getInstance()->load(TextureManager::spritePath("player.png"), m_playerID, Game::getInstance()->getRenderer())) {
        std::cerr << "SceneGame: failed to load player texture" << std::endl;
        return false;
    }
    if (!TextureManager::getInstance()->load(TextureManager::spritePath("bullet.png"), m_bulletID, Game::getInstance()->getRenderer())) {
        std::cerr << "SceneGame: failed to load bullet texture" << std::endl;
        // Don't hard-fail: we can render projectiles as rectangles.
    }

    m_font = TTF_OpenFont("assets/font.ttf", 20);
    if (!m_font) {
        std::cout << "SceneGame: Warning: Failed to load font (assets/font.ttf)" << std::endl;
    }

    // Local terrain (visual only). Server-side terrain deformation is not replicated yet.
    m_mapLoader = new MapLoader();
    if (!m_mapLoader->loadMap(m_mapPath)) {
        std::cerr << "SceneGame: failed to load map (visual layer): " << m_mapPath << std::endl;
    } else {
        createMapTexture();
        m_mapModified = false;
    }

    try {
        m_socket = new TCPSocket();
        m_socket->Connect(m_serverIp, m_serverPort);
        std::cout << "SceneGame connected to " << m_serverIp << ":" << m_serverPort << std::endl;

        ReqIngameJoin join{};
        join.matchId = m_matchId;
        join.userId = m_userId;
        std::snprintf(join.username, sizeof(join.username), "%s", m_username.c_str());
        std::snprintf(join.mapName, sizeof(join.mapName), "%s", m_mapPath.c_str());

        if (!PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_JOIN, join)) {
            std::cerr << "SceneGame: failed to send join" << std::endl;
            return false;
        }

        Packet resp;
        if (!PacketUtils::ReceivePacket(m_socket, resp) || resp.header.type != PacketType::RES_INGAME_JOIN) {
            std::cerr << "SceneGame: join failed (no response)" << std::endl;
            return false;
        }

        ResIngameJoin joined = resp.GetPayload<ResIngameJoin>();
        if (!joined.isSuccess) {
            std::cerr << "SceneGame: join rejected: " << joined.message << std::endl;
            return false;
        }

        m_matchId = joined.matchId;
        m_playerId = joined.playerId;

        // Check if server selected a different map (e.g. via RANDOM request)
        if (joined.mapPath[0] != '\0') {
            std::string sMapPath(joined.mapPath);
            if (sMapPath != m_mapPath) {
                std::cout << "SceneGame: Server enforced map: " << sMapPath << " (was " << m_mapPath << ")" << std::endl;
                m_mapPath = sMapPath;
                if (m_mapLoader) delete m_mapLoader;
                m_mapLoader = new MapLoader();
                if (!m_mapLoader->loadMap(m_mapPath)) {
                    std::cerr << "SceneGame: failed to reload forced map: " << m_mapPath << std::endl;
                } else {
                    createMapTexture();
                    m_mapModified = false;
                }
            }
        }

        std::cout << "SceneGame: joined match " << m_matchId << " as player " << m_playerId << std::endl;

        if (m_playerId == UINT32_MAX) {
            // Check explicit spectator permission
            if (!m_allowSpectator) {
                std::cerr << "SceneGame Error: Connected as spectator but spectator mode disallowed!" << std::endl;
                std::cerr << "           (Connected to port " << m_serverPort << " which might be a zombie replay server)" << std::endl;
                m_initFailed = true;
                return false;
            }
            EnsureReplayUI();
            // Trigger an initial status response (harmless in live server; ignored there)
            SendReplayControl((uint32_t)REPLAY_CMD_SET_SPEED, 0, 1.0f);
        } else {
            EnsurePauseUI();
        }

        EnsureChatUI(); // Everyone gets chat

        m_running = true;
        m_receiverThread = std::thread(&SceneGame::ReceiverLoop, this);
        return true;

    } catch (const std::exception& e) {
        std::cerr << "SceneGame: exception connecting: " << e.what() << std::endl;
        return false;
    }
}

bool SceneGame::onExit() {
    m_running = false;

    if (m_socket) {
        m_socket->Close();
    }

    if (m_receiverThread.joinable()) {
        m_receiverThread.join();
    }

    if (m_socket) {
        delete m_socket;
        m_socket = nullptr;
    }

    if (m_mapTexture) {
        SDL_DestroyTexture(m_mapTexture);
        m_mapTexture = nullptr;
    }

    if (m_mapLoader) {
        delete m_mapLoader;
        m_mapLoader = nullptr;
    }

    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }

    DestroyReplayUI();
    DestroyPauseUI();
    DestroyDrawUI();
    DestroySurrenderUI();
    DestroyChatUI();

    if (m_btnPowerUp) { m_btnPowerUp->clean(); delete m_btnPowerUp; m_btnPowerUp = nullptr; }

    TextureManager::getInstance()->clearFromTextureMap(m_bgTextureID);
    TextureManager::getInstance()->clearFromTextureMap(m_playerID);
    TextureManager::getInstance()->clearFromTextureMap(m_bulletID);
    TextureManager::getInstance()->clearFromTextureMap(m_powerUpIconID);

    return true;
}

void SceneGame::createMapTexture() {
    if (!m_mapLoader) return;

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    const int width = m_mapLoader->getWidth();
    const int height = m_mapLoader->getHeight();
    if (width <= 0 || height <= 0) return;

    if (m_mapTexture) {
        SDL_DestroyTexture(m_mapTexture);
        m_mapTexture = nullptr;
    }

    m_mapTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!m_mapTexture) {
        std::cerr << "SceneGame: failed to create map texture: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_SetTextureBlendMode(m_mapTexture, SDL_BLENDMODE_BLEND);

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_mapTexture, NULL, &pixels, &pitch) != 0 || !pixels) {
        std::cerr << "SceneGame: failed to lock map texture: " << SDL_GetError() << std::endl;
        return;
    }

    for (int y = 0; y < height; y++) {
        auto* row = reinterpret_cast<Uint32*>(reinterpret_cast<Uint8*>(pixels) + y * pitch);
        for (int x = 0; x < width; x++) {
            row[x] = m_mapLoader->isSolid((float)x, (float)y) ? 0x3C280DFF : 0x00000000;
        }
    }

    SDL_UnlockTexture(m_mapTexture);
}

void SceneGame::updateMapTexture() {
    if (!m_mapTexture || !m_mapLoader) return;

    const int width = m_mapLoader->getWidth();
    const int height = m_mapLoader->getHeight();
    if (width <= 0 || height <= 0) return;

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_mapTexture, NULL, &pixels, &pitch) != 0 || !pixels) {
        std::cerr << "SceneGame: failed to lock map texture: " << SDL_GetError() << std::endl;
        return;
    }

    for (int y = 0; y < height; y++) {
        auto* row = reinterpret_cast<Uint32*>(reinterpret_cast<Uint8*>(pixels) + y * pitch);
        for (int x = 0; x < width; x++) {
            row[x] = m_mapLoader->isSolid((float)x, (float)y) ? 0x3C280DFF : 0x00000000;
        }
    }

    SDL_UnlockTexture(m_mapTexture);
}

void SceneGame::ReceiverLoop() {
    while (m_running && m_socket && m_socket->IsValid()) {
        Packet p;
        if (!PacketUtils::ReceivePacket(m_socket, p)) {
            break;
        }

        if (p.header.type == PacketType::RES_INGAME_REMATCH_STATUS) {
            ResIngameRematchStatus st = p.GetPayload<ResIngameRematchStatus>();
            {
                std::lock_guard<std::mutex> lock(m_rematchMutex);
                m_rematchStatus = st.status;
                m_rematchAcceptedMask = st.acceptedMask;
                m_rematchMessage = (st.message[0] != '\0') ? std::string(st.message) : std::string();
                if (st.status == 1) {
                    m_rematchStartPending = true;
                }
            }
            continue;
        }

        if (p.header.type == PacketType::RES_INGAME_PAUSE_SIGNAL) {
            ResIngamePauseSignal sig = p.GetPayload<ResIngamePauseSignal>();
            {
                std::lock_guard<std::mutex> lock(m_pauseMutex);
                const bool wasPaused = m_pauseActive;
                m_pauseActive = true;
                m_pauseRequesterId = sig.requesterPlayerId;
                m_pauseEndTick = SDL_GetTicks() + (Uint32)sig.durationMs;
                m_pauseLastShownSeconds = -1;
                if (m_playerId != UINT32_MAX && sig.requesterPlayerId == m_playerId) {
                    m_pauseRemainingUses = sig.requesterRemainingUses;
                }
                
                std::string name = "Unknown";
                {
                    std::lock_guard<std::mutex> stateLock(m_stateMutex);
                    if (m_hasState) {
                         for(int i=0; i<m_lastState.playerCount; ++i) {
                             if(m_lastState.players[i].id == sig.requesterPlayerId) {
                                 name = m_lastState.players[i].name;
                                 break;
                             }
                         }
                    }
                }
                if (name.empty()) name = "Player " + std::to_string(sig.requesterPlayerId);

                std::string msg;
                // If duration is short (less than 5s), it's a resume countdown.
                if (sig.durationMs <= 5000) {
                   msg = "[System] " + name + " unpaused. Resuming in " + std::to_string(sig.durationMs/1000) + "s...";
                } else {
                   msg = "[System] " + name + " paused the game.";
                }
                
                {
                    std::lock_guard<std::mutex> chatLock(m_chatMutex);
                    m_chatLog.push_back(msg);
                    if (m_chatLog.size() > 8) m_chatLog.erase(m_chatLog.begin());
                    m_chatLogUpdated = true;
                }
            }
            continue;
        }
        if (p.header.type == PacketType::RES_INGAME_PAUSE_END) {
            ResIngamePauseEnd msgEnd = p.GetPayload<ResIngamePauseEnd>();
            {
                std::lock_guard<std::mutex> lock(m_pauseMutex);
                m_pauseActive = false;
                m_pauseRequesterId = UINT32_MAX;
                m_pauseEndTick = 0;
                m_pauseLastShownSeconds = -1;
                
                std::string name = "";
                if (msgEnd.endedByPlayerId != UINT32_MAX) {
                    {
                        std::lock_guard<std::mutex> stateLock(m_stateMutex);
                        if (m_hasState) {
                            for(int i=0; i<m_lastState.playerCount; ++i) {
                                if(m_lastState.players[i].id == msgEnd.endedByPlayerId) {
                                    name = m_lastState.players[i].name;
                                    break;
                                }
                            }
                        }
                    }
                    if (name.empty()) name = "Player " + std::to_string(msgEnd.endedByPlayerId);
                } else {
                    name = "System";
                }

                std::string msg = "[System]: " + name + " resumed the game.";
                {
                    std::lock_guard<std::mutex> chatLock(m_chatMutex);
                    m_chatLog.push_back(msg);
                    if (m_chatLog.size() > 8) m_chatLog.erase(m_chatLog.begin());
                    m_chatLogUpdated = true;
                }
            }
            continue;
        }
        if (p.header.type == PacketType::RES_INGAME_PAUSE_RESULT) {
            ResIngamePauseResult res = p.GetPayload<ResIngamePauseResult>();
            {
                std::lock_guard<std::mutex> lock(m_pauseMutex);
                m_pauseRemainingUses = res.remainingUses;
                
                // This payload doesn't have ID, it is unicast to requester only.
                // We use "You" or generic message.
                std::string txt = (res.isSuccess) ? "Pause request accepted." : "Pause request denied.";
                if (res.message[0] != '\0') txt += std::string(" (") + res.message + ")";

                {
                    std::lock_guard<std::mutex> chatLock(m_chatMutex);
                    m_chatLog.push_back("[System]: " + txt);
                    if (m_chatLog.size() > 8) m_chatLog.erase(m_chatLog.begin());
                    m_chatLogUpdated = true;
                }
            }
            continue;
        }

        if (p.header.type == PacketType::RES_INGAME_DRAW_SIGNAL) {
            ResIngameDrawSignal sig = p.GetPayload<ResIngameDrawSignal>();
            {
                std::lock_guard<std::mutex> lock(m_drawMutex);
                m_drawPending = true;
                m_drawRequesterId = sig.requesterPlayerId;
                m_drawExpireTick = SDL_GetTicks() + (Uint32)sig.durationMs;

                std::string name = "Unknown";
                {
                    std::lock_guard<std::mutex> stateLock(m_stateMutex);
                    if (m_hasState) {
                         for(int i=0; i<m_lastState.playerCount; ++i) {
                             if(m_lastState.players[i].id == sig.requesterPlayerId) {
                                 name = m_lastState.players[i].name;
                                 break;
                             }
                         }
                    }
                }
                if (name.empty()) name = "Player " + std::to_string(sig.requesterPlayerId);

                std::string msg = "[System]: " + name + " requested a draw.";

                {
                    std::lock_guard<std::mutex> chatLock(m_chatMutex);
                    m_chatLog.push_back(msg);
                    if (m_chatLog.size() > 8) m_chatLog.erase(m_chatLog.begin());
                    m_chatLogUpdated = true;
                }
            }
            continue;
        }

        if (p.header.type == PacketType::RES_INGAME_DRAW_RESULT) {
            ResIngameDrawResult res = p.GetPayload<ResIngameDrawResult>();
            {
                std::lock_guard<std::mutex> lock(m_drawMutex);
                m_drawPending = false;
                m_drawRequesterId = UINT32_MAX;
                m_drawExpireTick = 0;
                
                std::string msg;
                if (res.result == 0) { // ACCEPTED
                    msg = "[System]: Draw Accepted.";
                } else if (res.result == 1) { // DECLINED
                     std::string responder = "Unknown";
                     if (res.responderPlayerId != UINT32_MAX) {
                        std::lock_guard<std::mutex> stateLock(m_stateMutex);
                        if (m_hasState) {
                             for(int i=0; i<m_lastState.playerCount; ++i) {
                                 if(m_lastState.players[i].id == res.responderPlayerId) {
                                     responder = m_lastState.players[i].name;
                                     break;
                                 }
                             }
                        }
                        if (responder.empty()) responder = "Player " + std::to_string(res.responderPlayerId);
                     }
                     msg = "[System]: " + responder + " declined the draw.";
                } else if (res.result == 2) { // TIMEOUT
                     msg = "[System]: Draw request timed out.";
                } else {
                     msg = "[System]: Draw request denied.";
                }

                {
                    std::lock_guard<std::mutex> chatLock(m_chatMutex);
                    m_chatLog.push_back(msg);
                    if (m_chatLog.size() > 8) m_chatLog.erase(m_chatLog.begin());
                    m_chatLogUpdated = true;
                }
            }
            continue;
        }

        if (p.header.type == PacketType::RES_INGAME_SURRENDER_RESULT) {
            ResIngameSurrenderResult res = p.GetPayload<ResIngameSurrenderResult>();
            {
                std::lock_guard<std::mutex> lock(m_surrenderMutex);
                m_surrenderConfirmActive = false;
                
                // This is unicast.
                std::string txt = (res.isSuccess) ? "You surrendered." : "Surrender failed.";
                if (res.message[0] != '\0') txt += " (" + std::string(res.message) + ")";
                
                {
                    std::lock_guard<std::mutex> chatLock(m_chatMutex);
                    m_chatLog.push_back("[System]: " + txt);
                    if (m_chatLog.size() > 8) m_chatLog.erase(m_chatLog.begin());
                    m_chatLogUpdated = true;
                }
            }
            continue;
        }

        if (p.header.type == PacketType::RES_INGAME_CHAT) {
            ResIngameChat c = p.GetPayload<ResIngameChat>();
            std::string sender = (c.senderName[0] != '\0') ? c.senderName : "Player";
            std::string msg = c.message;
            std::string line = sender + ": " + msg;
            
            {
                std::lock_guard<std::mutex> lock(m_chatMutex);
                m_chatLog.push_back(line);
                if (m_chatLog.size() > 8) m_chatLog.erase(m_chatLog.begin());
                m_chatLogUpdated = true;
            }
            continue;
        }

        if (p.header.type == PacketType::RES_REPLAY_INFO) {
            ResReplayInfo info = p.GetPayload<ResReplayInfo>();
            if (m_playerId == UINT32_MAX) {
                const std::string mp = (info.mapPath[0] != '\0') ? std::string(info.mapPath) : std::string("assets/maps/flatmap.txt");
                {
                    std::lock_guard<std::mutex> lock(m_pendingMapMutex);
                    m_pendingMapPath = mp;
                    m_hasPendingMapPath = true;
                }
            }
            continue;
        }
        if (p.header.type == PacketType::RES_REPLAY_STATUS) {
            ResReplayStatus st = p.GetPayload<ResReplayStatus>();
            {
                std::lock_guard<std::mutex> lock(m_replayMutex);
                m_lastReplayStatus = st;
                m_hasReplayStatus = true;
            }
            continue;
        }

        if (p.header.type != PacketType::RES_INGAME_STATE) continue;

        ResIngameState s = p.GetPayload<ResIngameState>();
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (s.hasExplosion) {
                m_pendingExplosions.push_back({s.explosionX, s.explosionY, s.explosionRadius});
            }
            m_lastState = s;
            m_hasState = true;
        }
    }

    m_running = false;
}

void SceneGame::ApplyPendingReplayMap() {
    std::string mp;
    {
        std::lock_guard<std::mutex> lock(m_pendingMapMutex);
        if (!m_hasPendingMapPath) return;
        mp = m_pendingMapPath;
        m_hasPendingMapPath = false;
    }

    if (mp.empty() || mp == m_mapPath) return;

    std::cout << "[SceneGame] Replay map updated: " << mp << std::endl;
    m_mapPath = mp;

    if (m_mapTexture) {
        SDL_DestroyTexture(m_mapTexture);
        m_mapTexture = nullptr;
    }
    if (m_mapLoader) {
        delete m_mapLoader;
        m_mapLoader = nullptr;
    }

    m_mapLoader = new MapLoader();
    if (!m_mapLoader->loadMap(m_mapPath)) {
        std::cerr << "SceneGame: failed to load replay map (visual layer): " << m_mapPath << std::endl;
        return;
    }
    createMapTexture();
    m_mapModified = false;
}

void SceneGame::SendInput(uint32_t command, float value) {
    if (!m_socket || !m_socket->IsValid()) return;
    if (m_playerId == UINT32_MAX) return;

    ReqIngameInput in{};
    in.matchId = m_matchId;
    in.playerId = m_playerId;
    in.seq = ++m_seq;
    in.command = command;
    in.value = value;
    PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_INPUT, in);
}

void SceneGame::update() {
    ApplyPendingReplayMap();

    bool chatConsumed = HandleChatInput();
    if (m_inChat) m_inChat->update();
    UpdateChatDisplay();
    
    // Ensure we track key states even if chat blocks input, to avoid "stuck" keys or accidental triggers on release/focus loss.
    const bool isSpacePressed = InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_SPACE);
    const bool isEnterPressed = InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_RETURN);

    if (chatConsumed || (m_inChat && m_inChat->hasFocus())) {
        m_prevSpace = isSpacePressed;
        m_prevEnter = isEnterPressed;
        return; 
    }

    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_ESCAPE)) {
        Game::getInstance()->quit();
        return;
    }

    // Debug: toggle hitbox overlay (client-side only). Allow toggling anytime.
    const bool toggleHitboxes = InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_H);
    if (toggleHitboxes && !g_prevToggleHitboxes) {
        g_showHitboxes = !g_showHitboxes;
    }
    g_prevToggleHitboxes = toggleHitboxes;

    // Keep a local copy of server state for decision-making.
    ResIngameState state{};
    bool hasState = false;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_hasState) {
            state = m_lastState;
            hasState = true;
        }
    }

    // Spectator replay controls (only when the server assigned UINT32_MAX)
    if (m_playerId == UINT32_MAX) {
        EnsureReplayUI();

        if (m_btnReplayPlayPause) m_btnReplayPlayPause->update();
        if (m_btnReplaySpeedDown) m_btnReplaySpeedDown->update();
        if (m_btnReplaySpeedUp) m_btnReplaySpeedUp->update();
        if (m_inReplaySeekTick) m_inReplaySeekTick->update();
        if (m_btnReplaySeek) m_btnReplaySeek->update();

        // Update status text
        if (m_lblReplayStatus) {
            ResReplayStatus st{};
            bool ok = false;
            {
                std::lock_guard<std::mutex> lock(m_replayMutex);
                ok = m_hasReplayStatus;
                st = m_lastReplayStatus;
            }
            if (ok) {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "Replay: tick=%u  speed=%.2fx  paused=%s  range=[%u..%u]",
                              st.currentTick, st.speed, (st.isPaused ? "yes" : "no"), st.firstTick, st.lastTick);
                m_lblReplayStatus->setText(buf);
            } else {
                m_lblReplayStatus->setText("Replay: waiting for status...");
            }
        }

        // No gameplay inputs when spectating
        return;
    }

    // Surrender confirmation has the highest priority: while it's open,
    // only allow Yes/No interactions and block other inputs.
    EnsureSurrenderUI();
    {
        bool confirm = false;
        {
            std::lock_guard<std::mutex> lock(m_surrenderMutex);
            confirm = m_surrenderConfirmActive;
        }
        if (confirm) {
            if (m_btnSurrenderYes) m_btnSurrenderYes->update();
            if (m_btnSurrenderNo) m_btnSurrenderNo->update();
            return;
        }
    }

    EnsurePauseUI();
    if (m_btnPause) m_btnPause->update();

    EnsureDrawUI();

    // Draw offer local timeout fallback (server should also send a result)
    {
        std::lock_guard<std::mutex> lock(m_drawMutex);
        if (m_drawPending && m_drawExpireTick != 0 && SDL_GetTicks() >= m_drawExpireTick) {
            m_drawPending = false;
            m_drawRequesterId = UINT32_MAX;
            m_drawExpireTick = 0;
            m_drawToastText = "Draw request timed out";
            m_drawToastUntilTick = SDL_GetTicks() + 2000;
        }
    }

    // Draw UI state machine
    bool drawPending = false;
    uint32_t drawRequester = UINT32_MAX;
    {
        std::lock_guard<std::mutex> lock(m_drawMutex);
        drawPending = m_drawPending;
        drawRequester = m_drawRequesterId;
    }

    // Don't show draw controls while paused overlay is active.
    bool pausedNow = false;
    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        pausedNow = m_pauseActive;
    }

    if (!pausedNow) {
        if (drawPending && drawRequester != UINT32_MAX && drawRequester != m_playerId) {
            if (m_btnDrawAccept) m_btnDrawAccept->update();
            if (m_btnDrawDecline) m_btnDrawDecline->update();
        } else {
            if (m_btnDraw) m_btnDraw->update();
        }
    }

    // Surrender button update (disabled while paused).
    if (!pausedNow) {
        if (m_btnSurrender) m_btnSurrender->update();
    }

    // Power-up button update (available anytime for live players, except paused/confirm overlays)
    if (!pausedNow) {
        if (!m_btnPowerUp) {
            const int size = 48;
            const int margin = 20;
            const int x = 1280 - margin - size;
            const int y = 720 / 2 - size / 2;
            SDL_Color orange{255, 165, 0, 255};
            m_btnPowerUp = new Button((float)x, (float)y, size, size, "btn_generic",
                                      [this]() {
                                          m_powerUpArmed = true;
                                          SendInput(INGAME_CMD_POWER_UP);
                                      },
                                      181, 73, orange, 3);
        }
        // Toggle indicator via opacity: dim when OFF, bright when ARMED.
        if (m_btnPowerUp) {
            m_btnPowerUp->setAlpha(m_powerUpArmed ? 255 : 120);
        }
        if (m_btnPowerUp) m_btnPowerUp->update();
    }

    // Pause UI label + enable state + positioning
    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        const bool isRequester = (m_pauseRequesterId == m_playerId);
        if (m_btnPause) {
            if (m_pauseActive) {
                m_btnPause->setEnabled(isRequester);
            } else {
                m_btnPause->setEnabled(m_pauseRemainingUses > 0);
            }
        }
        if (m_lblPause) {
            if (m_pauseActive) {
                m_lblPause->setText(isRequester ? "Resume" : "Paused");
            } else {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Pause (%u left)", (unsigned)m_pauseRemainingUses);
                m_lblPause->setText(buf);
            }
        }

        // Position: bottom-right when playing; centered under timer when paused.
        if (m_btnPause && m_lblPause) {
            const int btnW = m_btnPause->getWidth();
            const int btnH = m_btnPause->getHeight();
            int bx = 0;
            int by = 0;
            if (m_pauseActive) {
                bx = 640 - btnW / 2;
                by = 360 + 90;
            } else {
                const int margin = 20;
                bx = 1280 - margin - btnW;
                by = 720 - margin - btnH;
            }
            m_btnPause->setPosition((float)bx, (float)by);

            const int lw = m_lblPause->getWidth();
            const int lh = m_lblPause->getHeight();
            m_lblPause->setPosition((float)(bx + (btnW - lw) / 2), (float)(by + (btnH - lh) / 2));
        }
    }

    // Position Draw + Surrender buttons (bottom-right row)
    if (m_btnDraw && m_lblDraw && m_btnSurrender && m_lblSurrender) {
        const int btnW = m_btnPause ? m_btnPause->getWidth() : 160;
        const int btnH = m_btnPause ? m_btnPause->getHeight() : 45;
        const int margin = 20;
        const int gap = 10;

        const int pauseX = 1280 - margin - btnW;
        const int y = 720 - margin - btnH;
        const int drawX = pauseX - gap - btnW;
        const int surrenderX = drawX - gap - btnW;

        m_btnDraw->setPosition((float)drawX, (float)y);
        {
            const int lw = m_lblDraw->getWidth();
            const int lh = m_lblDraw->getHeight();
            m_lblDraw->setPosition((float)(drawX + (btnW - lw) / 2), (float)(y + (btnH - lh) / 2));
        }

        m_btnSurrender->setPosition((float)surrenderX, (float)y);
        {
            const int lw = m_lblSurrender->getWidth();
            const int lh = m_lblSurrender->getHeight();
            m_lblSurrender->setPosition((float)(surrenderX + (btnW - lw) / 2), (float)(y + (btnH - lh) / 2));
        }
    }

    // Compute dt for power charging.
    Uint32 now = SDL_GetTicks();
    float dt = (m_lastTick == 0) ? 0.0f : (float)(now - m_lastTick) / 1000.0f;
    m_lastTick = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;

    // If paused, don't send gameplay inputs.
    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        if (m_pauseActive) {
            // Update pause status text roughly once per second
            if (m_lblPauseStatus) {
                const Uint32 tnow = SDL_GetTicks();
                int remainingMs = (m_pauseEndTick > tnow) ? (int)(m_pauseEndTick - tnow) : 0;
                int remainingSec = (remainingMs + 999) / 1000;
                if (remainingSec != m_pauseLastShownSeconds) {
                    m_pauseLastShownSeconds = remainingSec;
                    char buf[128];
                    std::snprintf(buf, sizeof(buf), "Paused: %ds", remainingSec);
                    m_lblPauseStatus->setText(buf);
                }
            }
            return;
        }
    }

    bool isMyTurn = false;
    uint32_t roomState = 0;
    if (hasState) {
        roomState = state.roomState;
        for (uint8_t i = 0; i < state.playerCount && i < INGAME_MAX_PLAYERS; i++) {
            if (state.players[i].id == m_playerId) {
                isMyTurn = (state.players[i].isMyTurn != 0);
                break;
            }
        }
    }

    // End game: rely on the authoritative server state to avoid clients disagreeing
    // during join/startup when snapshots may temporarily include only one player.
    if (!m_terminalQueued && hasState && state.roomState == 3u) {
        m_terminalQueued = true;
        // Determine winner/draw from the final authoritative snapshot.
        int aliveCount = 0;
        const NetPlayerState* lastAlive = nullptr;
        for (uint8_t i = 0; i < state.playerCount && i < INGAME_MAX_PLAYERS; i++) {
            const auto& pl = state.players[i];
            if (pl.isAlive) {
                aliveCount++;
                lastAlive = &pl;
            }
        }

        std::string resultText;
        if (aliveCount == 1 && lastAlive) {
            // Match the in-game name labeling logic.
            resultText = std::string(lastAlive->name);
            if (resultText.empty()) {
                resultText = std::string("Player") + std::to_string((int)lastAlive->id + 1);
            }
            if (lastAlive->id == m_playerId && !m_username.empty()) {
                resultText = m_username;
            }
            resultText = std::string("WINNER: ") + resultText;
        } else {
            resultText = "DRAW";
        }

        Game::getInstance()->getStateMachine()->requestPushState(
            new TerminalScene(m_serverIp, m_serverPort, m_mapPath, m_username, resultText));
        return;
    }

    // Only send gameplay inputs during our turn and when the room is PLAYING_TURN.
    // RoomState values currently: 0 waiting, 1 playing, 2 firing, 3 game over.
    const bool canAct = hasState && (roomState == 1u) && isMyTurn;

    if (!canAct) {
        // Still send STOP to avoid leaving stale velocity.
        SendInput(INGAME_CMD_STOP);
        return;
    }

    bool moving = false;

    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_A)) {
        SendInput(INGAME_CMD_MOVE_LEFT);
        moving = true;
    } else if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_D)) {
        SendInput(INGAME_CMD_MOVE_RIGHT);
        moving = true;
    }

    if (!moving) {
        SendInput(INGAME_CMD_STOP);
    }

    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_W)) {
        SendInput(INGAME_CMD_ADJUST_ANGLE, 0.5f);
    }
    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_S)) {
        SendInput(INGAME_CMD_ADJUST_ANGLE, -0.5f);
    }

    // Power hold mechanic:
    // - Press SPACE: reset power to 0, start charging.
    // - Hold SPACE: keep increasing power.
    // - Release SPACE: keep the last power value (no more updates).
    if (isSpacePressed && !m_prevSpace) {
        // Reset power on a new charge attempt (server clamps to [0..100]).
        SendInput(INGAME_CMD_ADJUST_POWER, -1000.0f);
    }
    if (isSpacePressed) {
        SendInput(INGAME_CMD_ADJUST_POWER, 60.0f * dt);
    }
    m_prevSpace = isSpacePressed;

    if (isEnterPressed && !m_prevEnter) {
        // Consume locally (server is authoritative, but this keeps the UI indicator correct).
        m_powerUpArmed = false;
        SendInput(INGAME_CMD_FIRE);
    }
    m_prevEnter = isEnterPressed;
}

void SceneGame::render() {
    if (m_initFailed) {
        SDL_Renderer* renderer = Game::getInstance()->getRenderer();
        if (renderer) {
           // Provide visual feedback if we are stuck for a frame
           SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
           SDL_RenderClear(renderer);
        }
        return;
    }

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    if (!renderer) return;

    bool paused = false;
    uint32_t pauseRequesterId = UINT32_MAX;
    Uint32 pauseEndTick = 0;
    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        paused = m_pauseActive;
        pauseRequesterId = m_pauseRequesterId;
        pauseEndTick = m_pauseEndTick;
    }

    auto RenderWorld = [&]() {
        TextureManager::getInstance()->drawScaled(
            m_bgTextureID,
            0,
            0,
            1280,
            720,
            Game::getInstance()->getRenderer());

        // Visual terrain layer (updated via server explosion events).
        if (m_mapTexture) {
            SDL_RenderCopy(Game::getInstance()->getRenderer(), m_mapTexture, NULL, NULL);
        }

        ResIngameState state{};
        bool hasState = false;
        std::vector<ExplosionEvent> explosionsToApply;

        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (m_hasState) {
                state = m_lastState;
                hasState = true;
            }
            explosionsToApply = std::move(m_pendingExplosions);
            m_pendingExplosions.clear(); 
        }

        if (!hasState) {
            if (m_playerId == UINT32_MAX) {
                EnsureReplayUI();
                if (m_btnReplayPlayPause) m_btnReplayPlayPause->draw();
                if (m_lblReplayPlayPause) m_lblReplayPlayPause->draw();
                if (m_btnReplaySpeedDown) m_btnReplaySpeedDown->draw();
                if (m_lblReplaySpeedDown) m_lblReplaySpeedDown->draw();
                if (m_btnReplaySpeedUp) m_btnReplaySpeedUp->draw();
                if (m_lblReplaySpeedUp) m_lblReplaySpeedUp->draw();
                if (m_inReplaySeekTick) m_inReplaySeekTick->draw();
                if (m_btnReplaySeek) m_btnReplaySeek->draw();
                if (m_lblReplaySeek) m_lblReplaySeek->draw();
                if (m_lblReplayStatus) m_lblReplayStatus->draw();
            }
            return;
        }

        // Apply explosion events from server to our local terrain.
        if (!explosionsToApply.empty() && m_mapLoader) {
            for (const auto& ex : explosionsToApply) {
                m_mapLoader->applyExplosion(ex.x, ex.y, ex.radius);
            }
            m_mapModified = true;
        }
        // Fallback for replay (which might still rely on single-state hasExplosion if not using queue logic perfectly)
        // or just double safety: if state says explosion but queue missed it (unlikely with above logic), apply it?
        // Actually, with Replay, we might need check. In Replay Mode, ReceiverLoop isn't running the same way if read from file?
        // Wait, Replay is usually handled via server connection in this codebase (SceneGame connects to server in Replay Mode too).
        // BUT if this is a "live" replay or the server is just streaming states.
        // Let's stick to queue. It is populated in ReceiverLoop which handles ALL incoming packets.
        
        if (m_mapModified) {
            updateMapTexture();
            m_mapModified = false;
        }

        // Draw players.
        for (uint8_t i = 0; i < state.playerCount && i < INGAME_MAX_PLAYERS; i++) {
            const auto& pl = state.players[i];
            if (!pl.isAlive) continue;

            SDL_RendererFlip flip = (pl.orient == 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            TextureManager::getInstance()->drawScaled(
                m_playerID,
                (int)pl.x,
                (int)pl.y,
                32,
                32,
                Game::getInstance()->getRenderer(),
                0.0,
                flip);

            // Debug hitbox overlay.
            if (g_showHitboxes) {
                if (renderer) {
                    // Sprite bounds
                    SDL_Rect rect{(int)pl.x, (int)pl.y, 32, 32};
                    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                    SDL_RenderDrawRect(renderer, &rect);

                    // Approx projectile hitbox (server collision radius is ~20)
                    const int cx = (int)pl.x + 16;
                    const int cy = (int)pl.y + 16;
                    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
                    DrawCircleOutline(renderer, cx, cy, 20);
                }
            }

            std::string nameLabel = std::string(pl.name);
            if (nameLabel.empty()) {
                nameLabel = std::string("Player") + std::to_string((int)pl.id + 1);
            }
            if (pl.id == m_playerId && !m_username.empty()) {
                nameLabel = m_username;
            }
            renderHealthBar(pl, nameLabel);
        }

    // Draw projectiles.
    for (uint8_t i = 0; i < state.projectileCount && i < INGAME_MAX_PROJECTILES; i++) {
        const auto& pr = state.projectiles[i];
        if (!pr.isActive) continue;

        // Power-up projectile: orange (not yellow).
        if (pr.isPowerUp) {
            SDL_Rect r{(int)pr.x - 8, (int)pr.y - 8, 16, 16};
            SDL_SetRenderDrawColor(Game::getInstance()->getRenderer(), 255, 165, 0, 255);
            SDL_RenderFillRect(Game::getInstance()->getRenderer(), &r);
        } else {
            // Normal projectile: sprite if loaded; otherwise a small white rect.
            TextureManager::getInstance()->drawScaled(
                m_bulletID,
                (int)pr.x - 8,
                (int)pr.y - 8,
                16,
                16,
                Game::getInstance()->getRenderer());

            SDL_Rect r{(int)pr.x - 2, (int)pr.y - 2, 4, 4};
            SDL_SetRenderDrawColor(Game::getInstance()->getRenderer(), 255, 255, 255, 255);
            SDL_RenderFillRect(Game::getInstance()->getRenderer(), &r);
        }
    }

    // HUD: local-player-only indicators.
    NetPlayerState me{};
    bool foundMe = false;
    if (m_playerId != UINT32_MAX) {
        for (uint8_t i = 0; i < state.playerCount && i < INGAME_MAX_PLAYERS; i++) {
            if (state.players[i].id == m_playerId) {
                me = state.players[i];
                foundMe = true;
                break;
            }
        }

        if (!foundMe) return;
    }

        if (!renderer) return;

    const int angleInt = foundMe ? (int)me.angle : 0;
    const int powerInt = foundMe ? (int)me.power : 0;
    const bool isMyTurn = foundMe ? ((me.isMyTurn != 0) && (state.roomState == 1u)) : false;

    float timeLeft = state.turnTimer;
    if (timeLeft < 0.0f) timeLeft = 0.0f;
    int secondsLeft = (int)(timeLeft + 0.999f);

    auto drawText = [&](const std::string& text, int x, int y) {
        if (!m_font) return;
        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Surface* surface = TTF_RenderText_Solid(m_font, text.c_str(), textColor);
        if (!surface) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
        if (tex) {
            SDL_Rect rect = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, tex, NULL, &rect);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surface);
    };

    auto drawTextScaled = [&](const std::string& text, int x, int y, float scale, int* outW, int* outH) {
        if (outW) *outW = 0;
        if (outH) *outH = 0;
        if (!m_font) return;
        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Surface* surface = TTF_RenderText_Solid(m_font, text.c_str(), textColor);
        if (!surface) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
        if (tex) {
            const int w = (int)std::lround(surface->w * scale);
            const int h = (int)std::lround(surface->h * scale);
            SDL_Rect rect = {x, y, w, h};
            SDL_RenderCopy(renderer, tex, NULL, &rect);
            SDL_DestroyTexture(tex);
            if (outW) *outW = w;
            if (outH) *outH = h;
        }
        SDL_FreeSurface(surface);
    };

    // 1) Angle: red line only during our turn, numeric value at line tip.
    if (isMyTurn) {
        constexpr float kPi = 3.14159265358979323846f;
        const float rad = me.angle * (kPi / 180.0f);
        const float directionMult = (me.orient != 0) ? 1.0f : -1.0f;
        const float lineLen = 70.0f;

        const int startX = (int)me.x + 16;
        const int startY = (int)me.y + 16;
        const int tipX = startX + (int)(std::cos(rad) * lineLen * directionMult);
        const int tipY = startY + (int)(-std::sin(rad) * lineLen);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderDrawLine(renderer, startX, startY, tipX, tipY);

        drawText(std::to_string(angleInt), tipX + 6, tipY - 10);
    }

    // 2) Power: single-line UI: "Power: <bar> value"
    {
        const int barX = 10;
        const int barY = 10;
        const int barW = 160;
        const int barH = 12;
        const float ratio = (powerInt <= 0) ? 0.0f : (powerInt >= 100 ? 1.0f : (powerInt / 100.0f));
        const int fillW = (int)(barW * ratio);

        int labelW = 0;
        int labelH = 0;
        if (m_font) {
            // Measure so we can place the bar right after the label.
            TTF_SizeText(m_font, "Power", &labelW, &labelH);
        }

        const int labelX = barX;
        const int labelY = barY + (barH - labelH) / 2;
        const int barOffsetX = barX + labelW + 8;
        const int valueX = barOffsetX + barW + 8;

        drawText("Power", labelX, labelY);

        SDL_Rect bg{barOffsetX, barY, barW, barH};
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_RenderFillRect(renderer, &bg);

        SDL_Rect fill{barOffsetX, barY, fillW, barH};
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_RenderFillRect(renderer, &fill);

        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderDrawRect(renderer, &bg);

        drawText(std::to_string(powerInt), valueX, labelY);
    }

    // 3) Timer: top-center, larger font size, "TIME" then newline then number.
    {
        const float scale = 2.0f;
        const std::string label = "TIME";
        const std::string value = std::to_string(secondsLeft);

        int labelW = 0, labelH = 0;
        int valueW = 0, valueH = 0;
        if (m_font) {
            TTF_SizeText(m_font, label.c_str(), &labelW, &labelH);
            TTF_SizeText(m_font, value.c_str(), &valueW, &valueH);
            labelW = (int)std::lround(labelW * scale);
            labelH = (int)std::lround(labelH * scale);
            valueW = (int)std::lround(valueW * scale);
            valueH = (int)std::lround(valueH * scale);
        }

        const int blockW = std::max(labelW, valueW);
        const int screenW = 1280;
        const int x = (screenW - blockW) / 2;
        const int y = 8;

        drawTextScaled(label, x + (blockW - labelW) / 2, y, scale, nullptr, nullptr);
        drawTextScaled(value, x + (blockW - valueW) / 2, y + labelH + 2, scale, nullptr, nullptr);
    }

    // 4) Wind: top-left numeric indicator with direction glyph.
    {
        const float w = state.wind * 10.0f;
        const char dir = (w > 0.0f) ? '>' : '<';
        const int mag = (int)std::lround(std::fabs(w));
        drawText(std::string("WIND ") + dir + " " + std::to_string(mag), 10, 34);
    }

    // Replay UI overlay for spectator.
    if (m_playerId == UINT32_MAX) {
        EnsureReplayUI();
        if (m_btnReplayPlayPause) m_btnReplayPlayPause->draw();
        if (m_lblReplayPlayPause) m_lblReplayPlayPause->draw();
        if (m_btnReplaySpeedDown) m_btnReplaySpeedDown->draw();
        if (m_lblReplaySpeedDown) m_lblReplaySpeedDown->draw();
        if (m_btnReplaySpeedUp) m_btnReplaySpeedUp->draw();
        if (m_lblReplaySpeedUp) m_lblReplaySpeedUp->draw();
        if (m_inReplaySeekTick) m_inReplaySeekTick->draw();
        if (m_btnReplaySeek) m_btnReplaySeek->draw();
        if (m_lblReplaySeek) m_lblReplaySeek->draw();
        if (m_lblReplayStatus) m_lblReplayStatus->draw();
    }

    };

    SDL_SetRenderTarget(renderer, NULL);
    RenderWorld();

    // If paused: darken the screen (no blur)
    if (m_playerId != UINT32_MAX && paused) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
        SDL_Rect full{0, 0, 1280, 720};
        SDL_RenderFillRect(renderer, &full);
    }

    // Surrender confirm overlay (darken the screen)
    bool surrenderConfirm = false;
    {
        std::lock_guard<std::mutex> lock(m_surrenderMutex);
        surrenderConfirm = m_surrenderConfirmActive;
    }
    if (m_playerId != UINT32_MAX && surrenderConfirm) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
        SDL_Rect full{0, 0, 1280, 720};
        SDL_RenderFillRect(renderer, &full);

        EnsureSurrenderUI();

        // Center confirm text
        if (m_lblSurrenderConfirm) {
            const int w = m_lblSurrenderConfirm->getWidth();
            const int h = m_lblSurrenderConfirm->getHeight();
            m_lblSurrenderConfirm->setPosition(640.0f - (float)w / 2.0f, 300.0f - (float)h / 2.0f);
            m_lblSurrenderConfirm->draw();
        }

        // Yes/No buttons centered under text
        const int btnW = 160;
        const int btnH = 45;
        const int gap = 12;
        const int smallW = (btnW - gap) / 2;
        const int x = 640 - btnW / 2;
        const int y = 360;

        if (m_btnSurrenderYes) {
            m_btnSurrenderYes->setPosition((float)x, (float)y);
            m_btnSurrenderYes->draw();
        }
        if (m_lblSurrenderYes) {
            const int lw = m_lblSurrenderYes->getWidth();
            const int lh = m_lblSurrenderYes->getHeight();
            m_lblSurrenderYes->setPosition((float)(x + (smallW - lw) / 2), (float)(y + (btnH - lh) / 2));
            m_lblSurrenderYes->draw();
        }

        if (m_btnSurrenderNo) {
            m_btnSurrenderNo->setPosition((float)(x + smallW + gap), (float)y);
            m_btnSurrenderNo->draw();
        }
        if (m_lblSurrenderNo) {
            const int lw = m_lblSurrenderNo->getWidth();
            const int lh = m_lblSurrenderNo->getHeight();
            m_lblSurrenderNo->setPosition((float)(x + smallW + gap + (smallW - lw) / 2), (float)(y + (btnH - lh) / 2));
            m_lblSurrenderNo->draw();
        }
    }

    // Pause UI overlay for live players
    if (m_playerId != UINT32_MAX) {
        EnsurePauseUI();

        // Draw the button (bottom-right normally; centered-under-timer while paused)
        if (!surrenderConfirm) {
            if (m_btnPause) m_btnPause->draw();
            if (m_lblPause) m_lblPause->draw();
        }

        const Uint32 now = SDL_GetTicks();
        if (paused && !surrenderConfirm) {
            int remainingMs = (pauseEndTick > now) ? (int)(pauseEndTick - now) : 0;
            int remainingSec = (remainingMs + 999) / 1000;
            if (remainingSec < 0) remainingSec = 0;

            // Big centered timer (MM:SS)
            if (m_lblPauseCountdown) {
                const int mm = remainingSec / 60;
                const int ss = remainingSec % 60;
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
                m_lblPauseCountdown->setText(buf);
                const int w = m_lblPauseCountdown->getWidth();
                const int h = m_lblPauseCountdown->getHeight();
                m_lblPauseCountdown->setPosition(640.0f - (float)w / 2.0f, 360.0f - (float)h / 2.0f - 40.0f);
                m_lblPauseCountdown->draw();
            }

            // 3-2-1 notification right before resume
            if (m_lblPauseHint) {
                if (remainingSec > 0 && remainingSec <= 3) {
                    m_lblPauseHint->setText(std::string("Resuming in ") + std::to_string(remainingSec));
                } else {
                    m_lblPauseHint->setText("Paused");
                }
                const int w = m_lblPauseHint->getWidth();
                const int h = m_lblPauseHint->getHeight();
                m_lblPauseHint->setPosition(640.0f - (float)w / 2.0f, 360.0f + 40.0f);
                m_lblPauseHint->draw();
            }

            // Optional toast (request denied, etc.)
        }
    }

    // Draw UI overlay for live players (asynchronous; does not pause gameplay)
    if (m_playerId != UINT32_MAX) {
        EnsureDrawUI();
        EnsureSurrenderUI();
        bool pausedNow = false;
        {
            std::lock_guard<std::mutex> lock(m_pauseMutex);
            pausedNow = m_pauseActive;
        }
        if (!pausedNow && !surrenderConfirm) {
            // Power-up button: small square, right-center, orange stroke + icon.
            const int size = 48;
            const int puMargin = 20;
            const int puX = 1280 - puMargin - size;
            const int puY = 720 / 2 - size / 2;
            const int pad = 6;

            if (m_btnPowerUp) {
                m_btnPowerUp->setPosition((float)puX, (float)puY);
                m_btnPowerUp->draw();
            }
            TextureManager::getInstance()->drawScaled(
                m_powerUpIconID,
                puX + pad,
                puY + pad,
                size - 2 * pad,
                size - 2 * pad,
                renderer);

            bool drawPending = false;
            uint32_t requester = UINT32_MAX;
            {
                std::lock_guard<std::mutex> lock(m_drawMutex);
                drawPending = m_drawPending;
                requester = m_drawRequesterId;
            }

            const int btnW = m_btnDraw ? m_btnDraw->getWidth() : 160;
            const int btnH = m_btnDraw ? m_btnDraw->getHeight() : 45;
            const int margin = 20;
            const int gap = 10;
            const int pauseX = 1280 - margin - btnW;
            const int y = 720 - margin - btnH;
            const int drawX = pauseX - gap - btnW;

            if (drawPending && requester == m_playerId) {
                if (m_btnDraw) {
                    m_btnDraw->setEnabled(false);
                    m_btnDraw->setPosition((float)drawX, (float)y);
                    m_btnDraw->draw();
                }
                if (m_lblDraw) {
                    m_lblDraw->setText("Wait for response");
                    const int lw = m_lblDraw->getWidth();
                    const int lh = m_lblDraw->getHeight();
                    m_lblDraw->setPosition((float)(drawX + (btnW - lw) / 2), (float)(y + (btnH - lh) / 2));
                    m_lblDraw->draw();
                }
            } else if (drawPending && requester != UINT32_MAX && requester != m_playerId) {
                const int smallW = (btnW - gap) / 2;
                if (m_btnDrawAccept) {
                    m_btnDrawAccept->setPosition((float)drawX, (float)y);
                    m_btnDrawAccept->draw();
                }
                if (m_lblDrawAccept) {
                    const int lw = m_lblDrawAccept->getWidth();
                    const int lh = m_lblDrawAccept->getHeight();
                    m_lblDrawAccept->setPosition((float)(drawX + (smallW - lw) / 2), (float)(y + (btnH - lh) / 2));
                    m_lblDrawAccept->draw();
                }
                if (m_btnDrawDecline) {
                    m_btnDrawDecline->setPosition((float)(drawX + smallW + gap), (float)y);
                    m_btnDrawDecline->draw();
                }
                if (m_lblDrawDecline) {
                    const int lw = m_lblDrawDecline->getWidth();
                    const int lh = m_lblDrawDecline->getHeight();
                    m_lblDrawDecline->setPosition((float)(drawX + smallW + gap + (smallW - lw) / 2), (float)(y + (btnH - lh) / 2));
                    m_lblDrawDecline->draw();
                }
            } else {
                if (m_btnDraw) {
                    m_btnDraw->setEnabled(true);
                    m_btnDraw->setPosition((float)drawX, (float)y);
                    m_btnDraw->draw();
                }
                if (m_lblDraw) {
                    m_lblDraw->setText("Draw");
                    const int lw = m_lblDraw->getWidth();
                    const int lh = m_lblDraw->getHeight();
                    m_lblDraw->setPosition((float)(drawX + (btnW - lw) / 2), (float)(y + (btnH - lh) / 2));
                    m_lblDraw->draw();
                }
            }

            // Surrender button (always visible when not paused)
            if (m_btnSurrender) {
                m_btnSurrender->setEnabled(true);
                m_btnSurrender->draw();
            }
            if (m_lblSurrender) {
                m_lblSurrender->setText("Surrender");
                m_lblSurrender->draw();
            }
        }
    }

    if (m_inChat) m_inChat->draw();
    for (auto* t : m_chatTexts) {
        if (t) t->draw();
    }
}

void SceneGame::renderHealthBar(const NetPlayerState& player, const std::string& nameLabel) {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    if (!renderer) return;

    const int currentHealth = player.hp < 0 ? 0 : player.hp;
    const int maxHealth = 100;

    const int barWidth = 40;
    const int barHeight = 6;
    const int barX = (int)player.x + (32 - barWidth) / 2;
    const int barY = (int)player.y - 10;

    SDL_Rect healthBg = {barX, barY, barWidth, barHeight};
    SDL_SetRenderDrawColor(renderer, 100, 20, 20, 255);
    SDL_RenderFillRect(renderer, &healthBg);

    const float ratio = (maxHealth > 0) ? (currentHealth / (float)maxHealth) : 0.0f;
    int healthWidth = (int)(ratio * barWidth);
    if (healthWidth < 0) healthWidth = 0;
    if (healthWidth > barWidth) healthWidth = barWidth;

    SDL_Rect healthFill = {barX, barY, healthWidth, barHeight};
    if (currentHealth > 66) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    } else if (currentHealth > 33) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    }
    SDL_RenderFillRect(renderer, &healthFill);

    SDL_Rect healthBox = {barX, barY, barWidth, barHeight};
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &healthBox);

    // Render Stamina Bar (only if it's their turn)
    if (player.isMyTurn) {
        const int stamBarH = 4;
        const int stamBarY = barY - stamBarH - 2;
        
        const float maxStamina = 400.0f; // Must match server constant
        float sRatio = player.stamina / maxStamina;
        if (sRatio > 1.0f) sRatio = 1.0f;
        if (sRatio < 0.0f) sRatio = 0.0f;

        SDL_Rect sBg{barX, stamBarY, barWidth, stamBarH};
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &sBg);

        int sW = (int)(sRatio * barWidth);
        SDL_Rect sFill{barX, stamBarY, sW, stamBarH};
        SDL_SetRenderDrawColor(renderer, 50, 200, 255, 255); // Cyan/Blue
        SDL_RenderFillRect(renderer, &sFill);

        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &sBg);
    }

    // Numeric HP (centered over the health bar)
    int hpTextX = barX + barWidth / 2;
    int hpTextY = barY - 10; // will be corrected if font loads
    int hpTextW = 0;
    int hpTextH = 0;

    if (m_font) {
        std::string hpText = std::to_string(currentHealth);
        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Surface* hpSurface = TTF_RenderText_Solid(m_font, hpText.c_str(), textColor);
        if (hpSurface) {
            hpTextW = hpSurface->w;
            hpTextH = hpSurface->h;
            hpTextX = barX + (barWidth - hpTextW) / 2;
            hpTextY = barY - hpTextH - 2;

            SDL_Texture* hpTexture = SDL_CreateTextureFromSurface(renderer, hpSurface);
            if (hpTexture) {
                SDL_Rect hpRect = {hpTextX, hpTextY, hpTextW, hpTextH};
                SDL_RenderCopy(renderer, hpTexture, NULL, &hpRect);
                SDL_DestroyTexture(hpTexture);
            }
            SDL_FreeSurface(hpSurface);
        }
    }


    // Turn indicator: filled red upside-down triangle with white stroke,
    // positioned above the HP value with a small padding.
    const int triPadding = 3;
    const int triHalfW = 6;
    const int triH = 8;

    const int centerX = barX + barWidth / 2;
    const int hpTopY = (hpTextH > 0) ? hpTextY : (barY - 2);
    int topY = hpTopY;

    if (player.isMyTurn != 0) {
        topY = hpTopY - triPadding - triH;

        // Filled triangle (scanline fill)
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        for (int y = 0; y <= triH; y++) {
            const int half = (triHalfW * (triH - y)) / triH;
            SDL_RenderDrawLine(renderer, centerX - half, topY + y, centerX + half, topY + y);
        }

        // White outline
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, centerX - triHalfW, topY, centerX + triHalfW, topY);
        SDL_RenderDrawLine(renderer, centerX - triHalfW, topY, centerX, topY + triH);
        SDL_RenderDrawLine(renderer, centerX + triHalfW, topY, centerX, topY + triH);
    }

    // Player name label (centered above the HP/turn indicator stack).
    if (!nameLabel.empty() && m_font) {
        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Surface* nameSurface = TTF_RenderText_Solid(m_font, nameLabel.c_str(), textColor);
        if (nameSurface) {
            const int nameW = nameSurface->w;
            const int nameH = nameSurface->h;

            int nameX = centerX - (nameW / 2);
            int nameY = topY - nameH - 2;

            // Keep it on-screen.
            if (nameX < 0) nameX = 0;
            if (nameX + nameW > 1280) nameX = 1280 - nameW;
            if (nameY < 0) nameY = 0;

            SDL_Texture* nameTexture = SDL_CreateTextureFromSurface(renderer, nameSurface);
            if (nameTexture) {
                SDL_Rect nameRect = {nameX, nameY, nameW, nameH};
                SDL_RenderCopy(renderer, nameTexture, NULL, &nameRect);
                SDL_DestroyTexture(nameTexture);
            }
            SDL_FreeSurface(nameSurface);
        }
    }
}

void SceneGame::EnsureChatUI() {
    if (m_inChat) return;
    
    // Bottom left
    int y = 720 - 40 - 10;
    m_inChat = new TextInput(20, (float)y, 400, 30, "assets/font.ttf", 18);
    m_inChat->setFocus(false);
}

void SceneGame::DestroyChatUI() {
    if (m_inChat) { m_inChat->clean(); delete m_inChat; m_inChat = nullptr; }
    for (auto* t : m_chatTexts) {
        if (t) { t->clean(); delete t; }
    }
    m_chatTexts.clear();
}

bool SceneGame::HandleChatInput() {
    if (!m_inChat) return false;

    // Toggle interaction handled by update

    // Check click-to-focus logic (handled inside TextInput::update, but we can verify)
    // If user clicked box, m_inChat->hasFocus() will be true.

    static bool tabPressed = false;
    bool isTabDown = InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_TAB);
    if (isTabDown && !tabPressed) {
        // Toggle focus
        m_inChat->setFocus(!m_inChat->hasFocus());
        // If unfocussing, maybe clear partial text? Or keep it. Keep it.
    }
    tabPressed = isTabDown;

    bool consumed = false;

    static bool enterPressed = false;
    bool isEnterDown = InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_RETURN);

    if (isEnterDown && !enterPressed) {
        if (m_inChat->hasFocus()) {
            // Send
            std::string msg = m_inChat->getString();
            if (!msg.empty()) {
                if (m_socket && m_socket->IsValid()) {
                    ReqIngameChat req{};
                    req.matchId = m_matchId;
                    std::snprintf(req.message, sizeof(req.message), "%s", msg.c_str());
                    PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_CHAT, req);
                }
                m_inChat->setString(""); 
            }
            m_inChat->setFocus(false);
            consumed = true; // Block this Enter from firing gun
        }
    }
    enterPressed = isEnterDown;

    return consumed;
}

void SceneGame::UpdateChatDisplay() {
    bool update = false;
    std::vector<std::string> logCopy;
    {
        std::lock_guard<std::mutex> lock(m_chatMutex);
        if (m_chatLogUpdated) {
            logCopy = m_chatLog;
            m_chatLogUpdated = false;
            update = true;
        }
    }

    if (update) {
        for (auto* t : m_chatTexts) {
            if (t) { t->clean(); delete t; }
        }
        m_chatTexts.clear();

        int lineH = 22;
        int startY = 720 - 40 - 20 - (int)(logCopy.size() * lineH);
        for (const auto& line : logCopy) {
            Text* t = new Text(20, (float)startY, "assets/font.ttf", 18, line, {255, 255, 255, 255});
            m_chatTexts.push_back(t);
            startY += lineH;
        }
    }
}
