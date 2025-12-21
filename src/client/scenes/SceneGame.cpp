#include <string>
#include "SceneGame.hpp"

#define delta_time 0.5f

SceneGame::SceneGame(std::string mapToLoad) 
    : m_mapToLoad(mapToLoad), m_playerX(0), m_playerY(0), m_startOrient(0),
      m_mapLoader(nullptr), m_mapTexture(nullptr), m_physics(new PhysicsEngine()), m_player(nullptr) {
}

bool SceneGame::onEnter() {
    m_bgTextureID = "background";
    m_bulletID = "bullet";
    m_playerID = "player";
    m_mapModified = true;  // Create texture on first render

    std::cout << "Entering Game Scene..." << std::endl;
    if (!TextureManager::getInstance()->load("assets/gameplay_background.png", m_bgTextureID, Game::getInstance()->getRenderer())) {
        std::cout << "Failed to load image!" << std::endl;
        return false;
    }
    m_mapLoader = new MapLoader();
    if (!m_mapLoader->loadMap(m_mapToLoad)) {
        std::cout << "Failed to load map!" << std::endl;
        return false;
    }
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
    
    float startX = 100.0f;
    float startY = 50.0f;
    float startOrient = 0;

    if (!spawns.empty()) {
        startX = spawns[0].x;
        startY = spawns[0].y;
    }

    m_player = new Player(1, "Player1", startX, startY, startOrient);
    m_players.push_back(m_player);

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
    m_players.clear();

    return true;
}

void SceneGame::update() {
    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_ESCAPE)) {
        Game::getInstance()->quit();
    }

    if (m_player && m_player->isAlive()) {
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

        // --- 3. FIRING (Spacebar) ---
        // Hold space to charge power, release to fire
        static bool wasSpacePressed = false;
        bool isSpacePressed = InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_SPACE);

        if (isSpacePressed) {
            // Increase power while space is held
            m_player->m_power += 1.0f;
            if (m_player->m_power > 100) m_player->m_power = 100;
        } else if (wasSpacePressed && m_player->m_power > 0) {
            // Fire when space is released
            std::cout << "Firing! Angle: " << m_player->m_angle << " Power: " << m_player->m_power << std::endl;
            m_physics->fireProjectile(m_player, m_projectiles);
            m_player->m_power = 0;  // Reset power after firing
        }
        
        wasSpacePressed = isSpacePressed;
    }

    m_physics->update(delta_time, m_players, m_projectiles, m_mapLoader);
    
    // Check if terrain was modified by an explosion
    if (m_physics->hasTerrainBeenModified()) {
        m_mapModified = true;
        m_physics->resetTerrainModifiedFlag();
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
            
            TextureManager::getInstance()->drawScaled(
                m_playerID, 
                (int)pos.x, (int)pos.y, 
                32, 32,
                Game::getInstance()->getRenderer()
            );
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
    if (m_player) {
        SDL_Renderer* renderer = Game::getInstance()->getRenderer();
        
        // Simple numeric display - just colored boxes to show values
        // Note: For proper text rendering, SDL_ttf would be needed
        
        // Angle display - fill width based on angle (0-90 degrees)
        SDL_Rect angleLabel = {10, 10, 50, 25};
        SDL_SetRenderDrawColor(renderer, 100, 100, 150, 255);
        SDL_RenderFillRect(renderer, &angleLabel);
        
        int angleWidth = (int)(m_player->m_angle / 90.0f * 80);  // Scale to max 80 pixels
        SDL_Rect angleValue = {65, 10, angleWidth, 25};
        SDL_SetRenderDrawColor(renderer, 100, 200, 255, 255);  // Light blue
        SDL_RenderFillRect(renderer, &angleValue);
        
        SDL_Rect angleBox = {10, 10, 145, 25};
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &angleBox);
        
        // Power display - fill width based on power (0-100)
        SDL_Rect powerLabel = {10, 45, 50, 25};
        SDL_SetRenderDrawColor(renderer, 150, 100, 100, 255);
        SDL_RenderFillRect(renderer, &powerLabel);
        
        int powerWidth = (int)(m_player->m_power / 100.0f * 80);  // Scale to max 80 pixels
        SDL_Rect powerValue = {65, 45, powerWidth, 25};
        
        // Color changes based on power level
        if (m_player->m_power < 33) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // Green
        } else if (m_player->m_power < 66) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);  // Yellow
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);  // Orange
        }
        SDL_RenderFillRect(renderer, &powerValue);
        
        SDL_Rect powerBox = {10, 45, 145, 25};
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &powerBox);
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

    // --- NEW: ENABLE BLENDING ---
    // This tells SDL to treat the Alpha channel (00) as transparent
    SDL_SetTextureBlendMode(m_mapTexture, SDL_BLENDMODE_BLEND); 
    // -----------------------------

    // 2. Lock and Write Pixels
    void* pixels;
    int pitch;
    SDL_LockTexture(m_mapTexture, NULL, &pixels, &pitch);

    Uint32* pixelBuffer = (Uint32*)pixels;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            
            if (m_mapLoader->isSolid(x, y)) {
                pixelBuffer[index] = 0x3C280DFF; // Brown color for Solid
            } else {
                // Draw Air (Transparent)
                pixelBuffer[index] = 0x00000000; 
            }
        }
    }

    SDL_UnlockTexture(m_mapTexture);
}

void SceneGame::updateMapTexture() {
    if (!m_mapTexture || !m_mapLoader) return;
    
    int width = m_mapLoader->getWidth();
    int height = m_mapLoader->getHeight();

    // Lock and update pixels
    void* pixels;
    int pitch;
    SDL_LockTexture(m_mapTexture, NULL, &pixels, &pitch);

    Uint32* pixelBuffer = (Uint32*)pixels;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            
            if (m_mapLoader->isSolid(x, y)) {
                pixelBuffer[index] = 0x3C280DFF; // Brown color for Solid
            } else {
                // Draw Air (Transparent)
                pixelBuffer[index] = 0x00000000; 
            }
        }
    }

    SDL_UnlockTexture(m_mapTexture);
}