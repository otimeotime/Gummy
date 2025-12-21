#pragma once
#include "SceneGame.hpp"

bool SceneGame::onEnter() {
    std::cout << "Entering Game Scene..." << std::endl;
    if (!TextureManager::getInstance()->load("assets/gameplay_background.png", SceneGame::m_textureID, Game::getInstance()->getRenderer())) {
        std::cout << "Failed to load image!" << std::endl;
        return false;
    }
    m_mapLoader = new MapLoader();
    if (!m_mapLoader->loadMap("/mnt/c/Users/TRINHQUYNH/LearningMaterial/NetworkProgramming/Gummy/assets/maps/valley_map.txt")) {
        std::cout << "Failed to load map!" << std::endl;
        return false;
    }
    createMapTexture();
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

    return true;
}

void SceneGame::update() {
    if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_ESCAPE)) {
        Game::getInstance()->quit();
    }
}

void SceneGame::render() {   
    TextureManager::getInstance()->drawStatic(
        SceneGame::m_textureID, 
        0, 0, 
        1280, 720,
        Game::getInstance()->getRenderer()
    );
    
    // Draw the map texture
    if (m_mapTexture) {
        SDL_RenderCopy(Game::getInstance()->getRenderer(), m_mapTexture, NULL, NULL);
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
                pixelBuffer[index] = 0xFF228B22; // Brown color for Solid
            } else {
                // Draw Air (Transparent)
                pixelBuffer[index] = 0x00000000; 
            }
        }
    }

    SDL_UnlockTexture(m_mapTexture);
}