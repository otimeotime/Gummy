#include "SceneGameNet.hpp"

#include <chrono>
#include <cstring>
#include <iostream>

SceneGameNet::SceneGameNet(std::string serverIp, int serverPort)
    : m_serverIp(std::move(serverIp)),
      m_serverPort(serverPort),
      m_running(false),
      m_socket(nullptr),
      m_matchId(1),
      m_playerId(UINT32_MAX),
      m_seq(0),
      m_hasState(false),
      m_bgTextureID(""),
      m_playerID(""),
      m_bulletID(""),
            m_mapPath("assets/maps/flatmap.txt"),
            m_mapLoader(nullptr),
            m_mapTexture(nullptr),
            m_mapModified(true),
            m_font(nullptr),
      m_lastTick(0) {
    std::memset(&m_lastState, 0, sizeof(m_lastState));
}

bool SceneGameNet::onEnter() {
    m_lastTick = SDL_GetTicks();

    m_bgTextureID = "background";
    m_playerID = "player";
    m_bulletID = "bullet";

    if (!TextureManager::getInstance()->load("assets/gameplay_background.png", m_bgTextureID, Game::getInstance()->getRenderer())) {
        std::cerr << "SceneGameNet: failed to load background" << std::endl;
        return false;
    }
    if (!TextureManager::getInstance()->load("assets/player.png", m_playerID, Game::getInstance()->getRenderer())) {
        std::cerr << "SceneGameNet: failed to load player texture" << std::endl;
        return false;
    }
    if (!TextureManager::getInstance()->load("assets/bullet.png", m_bulletID, Game::getInstance()->getRenderer())) {
        std::cerr << "SceneGameNet: failed to load bullet texture" << std::endl;
        // Don't hard-fail: we can render projectiles as rectangles.
    }

    m_font = TTF_OpenFont("assets/font.ttf", 20);
    if (!m_font) {
        std::cout << "SceneGameNet: Warning: Failed to load font (assets/font.ttf)" << std::endl;
    }

    // Local terrain (visual only). Server-side terrain deformation is not replicated yet.
    m_mapLoader = new MapLoader();
    if (!m_mapLoader->loadMap(m_mapPath)) {
        std::cerr << "SceneGameNet: failed to load map (visual layer): " << m_mapPath << std::endl;
    } else {
        createMapTexture();
        m_mapModified = false;
    }

    try {
        m_socket = new TCPSocket();
        m_socket->Connect(m_serverIp, m_serverPort);
        std::cout << "SceneGameNet connected to " << m_serverIp << ":" << m_serverPort << std::endl;

        ReqIngameJoin join{};
        join.matchId = m_matchId;
        join.userId = 0;
        join.mapName[0] = '\0';

        if (!PacketUtils::SendPacket(m_socket, PacketType::REQ_INGAME_JOIN, join)) {
            std::cerr << "SceneGameNet: failed to send join" << std::endl;
            return false;
        }

        Packet resp;
        if (!PacketUtils::ReceivePacket(m_socket, resp) || resp.header.type != PacketType::RES_INGAME_JOIN) {
            std::cerr << "SceneGameNet: join failed (no response)" << std::endl;
            return false;
        }

        ResIngameJoin joined = resp.GetPayload<ResIngameJoin>();
        if (!joined.isSuccess) {
            std::cerr << "SceneGameNet: join rejected: " << joined.message << std::endl;
            return false;
        }

        m_matchId = joined.matchId;
        m_playerId = joined.playerId;
        std::cout << "SceneGameNet: joined match " << m_matchId << " as player " << m_playerId << std::endl;

        m_running = true;
        m_receiverThread = std::thread(&SceneGameNet::ReceiverLoop, this);
        return true;

    } catch (const std::exception& e) {
        std::cerr << "SceneGameNet: exception connecting: " << e.what() << std::endl;
        return false;
    }
}

bool SceneGameNet::onExit() {
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

    TextureManager::getInstance()->clearFromTextureMap(m_bgTextureID);
    TextureManager::getInstance()->clearFromTextureMap(m_playerID);
    TextureManager::getInstance()->clearFromTextureMap(m_bulletID);

    return true;
}

void SceneGameNet::createMapTexture() {
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
        std::cerr << "SceneGameNet: failed to create map texture: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_SetTextureBlendMode(m_mapTexture, SDL_BLENDMODE_BLEND);

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_mapTexture, NULL, &pixels, &pitch) != 0 || !pixels) {
        std::cerr << "SceneGameNet: failed to lock map texture: " << SDL_GetError() << std::endl;
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

void SceneGameNet::updateMapTexture() {
    if (!m_mapTexture || !m_mapLoader) return;

    const int width = m_mapLoader->getWidth();
    const int height = m_mapLoader->getHeight();
    if (width <= 0 || height <= 0) return;

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_mapTexture, NULL, &pixels, &pitch) != 0 || !pixels) {
        std::cerr << "SceneGameNet: failed to lock map texture: " << SDL_GetError() << std::endl;
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

void SceneGameNet::ReceiverLoop() {
    while (m_running && m_socket && m_socket->IsValid()) {
        Packet p;
        if (!PacketUtils::ReceivePacket(m_socket, p)) {
            break;
        }
        if (p.header.type != PacketType::RES_INGAME_STATE) {
            continue;
        }

        ResIngameState s = p.GetPayload<ResIngameState>();
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_lastState = s;
            m_hasState = true;
        }
    }

    m_running = false;
}

void SceneGameNet::SendInput(uint32_t command, float value) {
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

void SceneGameNet::update() {
    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_ESCAPE)) {
        Game::getInstance()->quit();
        return;
    }

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

    // Compute dt for power charging.
    Uint32 now = SDL_GetTicks();
    float dt = (m_lastTick == 0) ? 0.0f : (float)(now - m_lastTick) / 1000.0f;
    m_lastTick = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;

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

    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_SPACE)) {
        SendInput(INGAME_CMD_ADJUST_POWER, 60.0f * dt);
    }

    static bool wasEnterPressed = false;
    const bool isEnterPressed = InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_RETURN);
    if (isEnterPressed && !wasEnterPressed) {
        SendInput(INGAME_CMD_FIRE);
    }
    wasEnterPressed = isEnterPressed;
}

void SceneGameNet::render() {
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
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_hasState) {
            state = m_lastState;
            hasState = true;
        }
    }

    if (!hasState) return;

    // Apply explosion events from server to our local terrain.
    if (state.hasExplosion && m_mapLoader) {
        m_mapLoader->applyExplosion(state.explosionX, state.explosionY, state.explosionRadius);
        m_mapModified = true;
    }

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
    }

    // Draw projectiles.
    for (uint8_t i = 0; i < state.projectileCount && i < INGAME_MAX_PROJECTILES; i++) {
        const auto& pr = state.projectiles[i];
        if (!pr.isActive) continue;

        // Prefer sprite if loaded; otherwise (or additionally) draw a small rect.
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

    // HUD: show Angle/Power/Time for local player.
    if (m_font) {
        NetPlayerState me{};
        bool foundMe = false;
        for (uint8_t i = 0; i < state.playerCount && i < INGAME_MAX_PLAYERS; i++) {
            if (state.players[i].id == m_playerId) {
                me = state.players[i];
                foundMe = true;
                break;
            }
        }

        if (foundMe) {
            SDL_Renderer* renderer = Game::getInstance()->getRenderer();
            SDL_Color textColor = {255, 255, 255, 255};

            const int angleInt = (int)me.angle;
            const int powerInt = (int)me.power;
            float timeLeft = state.turnTimer;
            if (timeLeft < 0.0f) timeLeft = 0.0f;
            int secondsLeft = (int)(timeLeft + 0.999f);

            auto drawText = [&](const std::string& text, int x, int y) {
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

            drawText("Angle " + std::to_string(angleInt), 10, 10);
            drawText("Power " + std::to_string(powerInt), 10, 40);
            drawText("Time " + std::to_string(secondsLeft), 10, 70);
        }
    }
}
