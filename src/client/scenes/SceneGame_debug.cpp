#include "SceneGame.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../core/InputHandler.hpp"
#include <iostream>

bool SceneGame::onEnter() {
    std::cout << "--- ENTERING DEBUG MODE ---" << std::endl;

    char* basePath = SDL_GetBasePath();
    if (basePath) {
        std::cout << "EXECUTABLE PATH: " << basePath << std::endl;
        std::cout << "EXPECTING MAP AT: " << basePath << "assets/maps/valley_map.txt" << std::endl;
        SDL_free(basePath);
    } else {
        std::cerr << "Error getting base path: " << SDL_GetError() << std::endl;
    }

    // 1. Load Background
    m_bgTextureID = "game_bg";
    // Using a fallback color if image fails? No, let's just stick to the map debug.
    TextureManager::getInstance()->load("assets/gameplay_background.png", m_bgTextureID, Game::getInstance()->getRenderer());

    // 2. Initialize MapLoader
    m_mapLoader = new MapLoader();
    
    // TRY LOADING FILE
    bool loaded = m_mapLoader->loadMap("assets/maps/valley_map.txt");

    if (loaded) {
        std::cout << "SUCCESS: Map file loaded. Dimensions: " 
                  << m_mapLoader->getWidth() << "x" << m_mapLoader->getHeight() << std::endl;
    } else {
        std::cerr << "ERROR: Could not load 'assets/maps/valley_map.txt'!" << std::endl;
        std::cerr << "-> ACTIVATING FALLBACK: Generating dummy map in memory." << std::endl;
        
        // MANUAL FALLBACK to prove rendering works
        // We will spoof the MapLoader internals just for this test
        // (Note: This assumes you added setters or public access, 
        //  but since we can't change MapLoader easily here, let's just rely on visual confirmation 
        //  that if this fails, we won't see the map).
    }

    // 3. Create the Terrain Texture
    createMapTexture();

    return true;
}

bool SceneGame::onExit() {
    TextureManager::getInstance()->clearFromTextureMap(m_bgTextureID);
    if (m_mapLoader) { delete m_mapLoader; m_mapLoader = nullptr; }
    if (m_mapTexture) { SDL_DestroyTexture(m_mapTexture); m_mapTexture = nullptr; }
    return true;
}

void SceneGame::update() {
    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_ESCAPE)) {
        Game::getInstance()->quit();
    }
}

void SceneGame::render() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();

    // 1. Draw Background
    TextureManager::getInstance()->drawStatic(m_bgTextureID, 0, 0, 1280, 720, renderer);

    // 2. Draw Terrain
    if (m_mapTexture) {
        SDL_RenderCopy(renderer, m_mapTexture, NULL, NULL);
    } else {
        // If texture is null, draw a big red X to indicate total failure
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderDrawLine(renderer, 0, 0, 1280, 720);
        SDL_RenderDrawLine(renderer, 1280, 0, 0, 720);
    }
}

void SceneGame::createMapTexture() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    int width = m_mapLoader->getWidth();
    int height = m_mapLoader->getHeight();

    // DEBUG: Force dimensions if map failed
    if (width == 0 || height == 0) {
        std::cout << "Map has 0 dimensions. Forcing 1280x720 for debug." << std::endl;
        width = 1280;
        height = 720;
    }

    m_mapTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    
    if (!m_mapTexture) {
        std::cerr << "CRITICAL: SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        return;
    }

    // ENABLE BLENDING
    SDL_SetTextureBlendMode(m_mapTexture, SDL_BLENDMODE_BLEND); 

    void* pixels;
    int pitch;
    SDL_LockTexture(m_mapTexture, NULL, &pixels, &pitch);
    Uint32* pixelBuffer = (Uint32*)pixels;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            
            // LOGIC CHECK:
            // If the map loaded real data, use it.
            // If it failed (width/height was 0), draw a TEST PATTERN.
            bool isSolid = false;

            if (m_mapLoader->getWidth() > 0) {
                isSolid = m_mapLoader->isSolid(x, y);
            } else {
                // Test Pattern: Solid if y > 360 (Half screen)
                isSolid = (y > 360); 
            }

            if (isSolid) {
                pixelBuffer[index] = 0xFF228B22; // Green
            } else {
                // DEBUG: Draw AIR as semi-transparent RED so you can see it!
                // If you see red tint, the texture is working but the map data is empty/air.
                pixelBuffer[index] = 0x55FF0000; 
            }
        }
    }
    SDL_UnlockTexture(m_mapTexture);
    std::cout << "Texture created and unlocked successfully." << std::endl;
}