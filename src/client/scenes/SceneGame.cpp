#include <string>
#include "SceneGame.hpp"
#include <SDL2/SDL_ttf.h>

SceneGame::SceneGame(std::string mapToLoad) 
    : m_mapToLoad(mapToLoad), m_playerX(0), m_playerY(0), m_startOrient(0),
    m_mapLoader(nullptr), m_mapTexture(nullptr), m_physics(new PhysicsEngine()), m_player(nullptr), m_font(nullptr), m_gameRoom(nullptr), m_lastTick(0) {
}

bool SceneGame::onEnter() {
    m_lastTick = SDL_GetTicks();

    m_bgTextureID = "background";
    m_bulletID = "bullet";
    m_playerID = "player";
    m_mapModified = true;  // Create texture on first render

    // Load font for HUD text
    m_font = TTF_OpenFont("assets/font.ttf", 20);
    if (!m_font) {
        std::cout << "Warning: Failed to load font! Text rendering will not be available." << std::endl;
    }

    std::cout << "Entering Game Scene..." << std::endl;
    if (!TextureManager::getInstance()->load("assets/gameplay_background.png", m_bgTextureID, Game::getInstance()->getRenderer())) {
        std::cout << "Failed to load image!" << std::endl;
        return false;
    }
    std::cout << "Background loaded." << std::endl;
    m_mapLoader = new MapLoader();
    std::cout << "Loading map: " << m_mapToLoad << std::endl;
    if (!m_mapLoader->loadMap(m_mapToLoad)) {
        std::cout << "Failed to load map!" << std::endl;
        return false;
    }
    std::cout << "Creating map texture..." << std::endl;
    createMapTexture();

    if (!TextureManager::getInstance()->load("assets/player.png", m_playerID, Game::getInstance()->getRenderer())) {
        std::cout << "Failed to load player texture!" << std::endl;
        return false;
    }
    std::cout << "Successfully loaded player texture!" << std::endl;

    if (!TextureManager::getInstance()->load("assets/bullet.png", m_bulletID, Game::getInstance()->getRenderer())) {
        std::cout << "Failed to load bullet texture!" << std::endl;
        return false;
    }
    std::cout << "Successfully loaded bullet texture!" << std::endl;
    
    const std::vector<SpawnPoint>& spawns = m_mapLoader->getSpawnPoints();

    for (int i = 0; i < 2; i++) {
        Player* newPlayer = new Player(i, "Player" + std::to_string(i + 1), spawns[i].x, spawns[i].y, (i % 2 == 0));
        m_players.push_back(newPlayer);
        if (i == 0) {
            m_player = newPlayer; // Local player is the first one
        }
    }

    if (m_gameRoom) {
        delete m_gameRoom;
        m_gameRoom = nullptr;
    }
    m_gameRoom = new GameRoom(m_players);
    m_gameRoom->startGame();

    return true;
}

bool SceneGame::onExit() {
    std::cout << "Exiting Game Scene..." << std::endl;

    if (m_mapLoader) {
        delete m_mapLoader;
        m_mapLoader = nullptr;
    }

    if (m_mapTexture) {
        SDL_DestroyTexture(m_mapTexture);
        m_mapTexture = nullptr;
    }

    if (m_player) {
        delete m_player;
        m_player = nullptr;
    }

    if (m_physics) {
        delete m_physics;
        m_physics = nullptr;
    }

    if (m_gameRoom) {
        delete m_gameRoom;
        m_gameRoom = nullptr;
    }
    
    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
    
    for (auto* p : m_players) {
        delete p;
    }
    m_players.clear();

    return true;
}

void SceneGame::update() {
    // Real-time delta (seconds)
    Uint32 now = SDL_GetTicks();
    float dt = (m_lastTick == 0) ? 0.0f : (float)(now - m_lastTick) / 1000.0f;
    m_lastTick = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f; // clamp to avoid huge jumps on pauses

    // The physics/gameplay code in this project was tuned with a fixed step.
    // Use real dt for timers, but keep physics step consistent to avoid slow-motion.
    const float physicsDt = 0.5f;

    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_ESCAPE)) {
        Game::getInstance()->quit();
    }

    const bool isPlayingTurn = (m_gameRoom && m_gameRoom->getState() == PLAYING_TURN);

    if (m_player && m_player->isAlive() && m_player->isMyTurn() && isPlayingTurn) {
        bool isMoving = false;

        // Move Left
        if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_A)) {
            m_player->moveLeft();
            isMoving = true;
            m_player->setOrient(0);
        }
        // Move Right
        else if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_D)) {
            m_player->moveRight();
            isMoving = true;
            m_player->setOrient(1);
        }

        // Stop if no keys are pressed
        if (!isMoving) {
            m_player->stopMoving();
        }

        // --- 2. ANGLE ADJUSTMENT (W/S) ---
        if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_W)) {
            m_player->adjustAngle(0.5f); // Increase angle
        }
        if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_S)) {
            m_player->adjustAngle(-0.5f); // Decrease angle
        }
        
        // Clamp angle between 0 and 180 (exclusive of 180)
        if (m_player->m_angle < 0.0f) {
            m_player->m_angle = 0.0f;
        } else if (m_player->m_angle > 180.0f) {
            m_player->m_angle = 180.0f;
        }

        // --- 3. POWER CHARGE (Spacebar) ---
        // Hold space to charge power; firing happens only on ENTER or timeout.
        bool isSpacePressed = InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_SPACE);
        if (isSpacePressed) {
            m_player->m_power += 60.0f * dt;
            if (m_player->m_power > 100) m_player->m_power = 100;
        }

        // --- 4. CONFIRM (Enter) ---
        static bool wasEnterPressed = false;
        bool isEnterPressed = InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_RETURN);
        if (isEnterPressed && !wasEnterPressed) {
            m_gameRoom->commitShot();
        }
        wasEnterPressed = isEnterPressed;
    }

    // If a shot was committed (ENTER or timeout), perform it exactly once.
    if (m_gameRoom) {
        if (Player* shooter = m_gameRoom->takePendingShooter()) {
            m_physics->fireProjectile(shooter, m_projectiles);
            shooter->m_power = 0.0f;
            m_gameRoom->notifyShotFired();
        }
    }

    m_physics->update(physicsDt, m_players, m_projectiles, m_mapLoader);
    
    // Check if terrain was modified by an explosion
    if (m_physics->hasTerrainBeenModified()) {
        m_mapModified = true;
        m_physics->resetTerrainModifiedFlag();
    }

    if (m_gameRoom) {
        m_gameRoom->update(dt, m_projectiles);

        // If the timer expired this frame, commitShot() happens inside GameRoom::update().
        // Fire it now (projectile will update starting next frame).
        if (Player* shooter = m_gameRoom->takePendingShooter()) {
            m_physics->fireProjectile(shooter, m_projectiles);
            shooter->m_power = 0.0f;
            m_gameRoom->notifyShotFired();
        }
    }
}

void SceneGame::render() {   
    // Only update map texture if terrain has changed
    if (m_mapModified) {
        updateMapTexture();
        m_mapModified = false;  // Reset flag after update
    }
    
    TextureManager::getInstance()->drawScaled(
        m_bgTextureID, 
        0, 0, 
        1280, 720,
        Game::getInstance()->getRenderer()
    );
    
    // Draw the map texture
    if (m_mapTexture) {
        SDL_RenderCopy(Game::getInstance()->getRenderer(), m_mapTexture, NULL, NULL);
    }

    // Render All Players
    for (const auto& player : m_players) {
        if (player && player->isAlive()) {
            Position pos = player->getPosition();

            // Player sprite faces right by default; flip when facing left.
            SDL_RendererFlip flip = (pos.orient == 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            
            TextureManager::getInstance()->drawScaled(
                m_playerID, 
                (int)pos.x, (int)pos.y, 
                32, 32,
                Game::getInstance()->getRenderer(),
                0.0,
                flip
            );
            
            renderHealthBar(pos, player);
        }
    }

    // Render All Projectiles
    for (const auto& proj : m_projectiles) {
        if (proj.isActive) {
            TextureManager::getInstance()->drawScaled(
                m_bulletID,
                (int)proj.position.x - 8, (int)proj.position.y - 8,
                16, 16, 
                Game::getInstance()->getRenderer()
            ); 
        }
    }

    // Display numeric HUD for Angle and Power
    if (m_player && m_font) {
        SDL_Renderer* renderer = Game::getInstance()->getRenderer();
        
        // Render Angle text
        std::string angleText = "Angle " + std::to_string((int)m_player->m_angle);
        SDL_Color textColor = {255, 255, 255, 255};  // White
        SDL_Surface* angleSurface = TTF_RenderText_Solid(m_font, angleText.c_str(), textColor);
        if (angleSurface) {
            SDL_Texture* angleTexture = SDL_CreateTextureFromSurface(renderer, angleSurface);
            if (angleTexture) {
                SDL_Rect angleRect = {10, 10, angleSurface->w, angleSurface->h};
                SDL_RenderCopy(renderer, angleTexture, NULL, &angleRect);
                SDL_DestroyTexture(angleTexture);
            }
            SDL_FreeSurface(angleSurface);
        }
        
        // Render Power text
        std::string powerText = "Power " + std::to_string((int)m_player->m_power);
        SDL_Surface* powerSurface = TTF_RenderText_Solid(m_font, powerText.c_str(), textColor);
        if (powerSurface) {
            SDL_Texture* powerTexture = SDL_CreateTextureFromSurface(renderer, powerSurface);
            if (powerTexture) {
                SDL_Rect powerRect = {10, 40, powerSurface->w, powerSurface->h};
                SDL_RenderCopy(renderer, powerTexture, NULL, &powerRect);
                SDL_DestroyTexture(powerTexture);
            }
            SDL_FreeSurface(powerSurface);
        }

        // Render Turn Timer
        if (m_gameRoom) {
            float timeLeft = m_gameRoom->getTurnTimer();
            if (timeLeft < 0.0f) timeLeft = 0.0f;
            int secondsLeft = (int)(timeLeft + 0.999f);

            std::string timerText = "Time " + std::to_string(secondsLeft);
            SDL_Surface* timerSurface = TTF_RenderText_Solid(m_font, timerText.c_str(), textColor);
            if (timerSurface) {
                SDL_Texture* timerTexture = SDL_CreateTextureFromSurface(renderer, timerSurface);
                if (timerTexture) {
                    SDL_Rect timerRect = {10, 70, timerSurface->w, timerSurface->h};
                    SDL_RenderCopy(renderer, timerTexture, NULL, &timerRect);
                    SDL_DestroyTexture(timerTexture);
                }
                SDL_FreeSurface(timerSurface);
            }
        }
    }
}

void SceneGame::createMapTexture() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    int width = m_mapLoader->getWidth();
    int height = m_mapLoader->getHeight();

    // 1. Create Texture
    m_mapTexture = SDL_CreateTexture(
        renderer, 
        SDL_PIXELFORMAT_RGBA8888, 
        SDL_TEXTUREACCESS_STREAMING, 
        width, height
    );

    if (!m_mapTexture) {
        std::cerr << "Failed to create map texture! W:" << width << " H:" << height << std::endl;
        return;
    }

    SDL_SetTextureBlendMode(m_mapTexture, SDL_BLENDMODE_BLEND); 

    // 2. Lock and Write Pixels
    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_mapTexture, NULL, &pixels, &pitch) != 0 || !pixels) {
        std::cerr << "Failed to lock map texture: " << SDL_GetError() << std::endl;
        return;
    }

    for (int y = 0; y < height; y++) {
        auto* row = reinterpret_cast<Uint32*>(reinterpret_cast<Uint8*>(pixels) + y * pitch);
        for (int x = 0; x < width; x++) {
            row[x] = m_mapLoader->isSolid((float)x, (float)y)
                ? 0x3C280DFF  // Solid (brown)
                : 0x00000000; // Air (transparent)
        }
    }

    SDL_UnlockTexture(m_mapTexture);
}

void SceneGame::updateMapTexture() {
    if (!m_mapTexture || !m_mapLoader) return;
    
    int width = m_mapLoader->getWidth();
    int height = m_mapLoader->getHeight();

    // Lock and update pixels
    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_mapTexture, NULL, &pixels, &pitch) != 0 || !pixels) {
        std::cerr << "Failed to lock map texture: " << SDL_GetError() << std::endl;
        return;
    }

    for (int y = 0; y < height; y++) {
        auto* row = reinterpret_cast<Uint32*>(reinterpret_cast<Uint8*>(pixels) + y * pitch);
        for (int x = 0; x < width; x++) {
            row[x] = m_mapLoader->isSolid((float)x, (float)y)
                ? 0x3C280DFF
                : 0x00000000;
        }
    }

    SDL_UnlockTexture(m_mapTexture);
}

void SceneGame::renderHealthBar(Position playerPos, Player* player) {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    
    // Poll current health from player
    int currentHealth = player->getHP();
    int maxHealth = 100;
    
    // Health bar dimensions and position (above player)
    int barWidth = 40;
    int barHeight = 6;
    int barX = (int)playerPos.x + (32 - barWidth) / 2;  // Center above player (player is 32x32)
    int barY = (int)playerPos.y - 10;  // 10 pixels above player
    
    // Background bar (dark red)
    SDL_Rect healthBg = {barX, barY, barWidth, barHeight};
    SDL_SetRenderDrawColor(renderer, 100, 20, 20, 255);
    SDL_RenderFillRect(renderer, &healthBg);
    
    // Health fill bar (red to green gradient based on health)
    int healthWidth = (int)((currentHealth / (float)maxHealth) * barWidth);
    SDL_Rect healthFill = {barX, barY, healthWidth, barHeight};
    
    // Color changes based on health level
    if (currentHealth > 66) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // Green
    } else if (currentHealth > 33) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);  // Yellow
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  // Red
    }
    SDL_RenderFillRect(renderer, &healthFill);
    
    // Border
    SDL_Rect healthBox = {barX, barY, barWidth, barHeight};
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &healthBox);
}