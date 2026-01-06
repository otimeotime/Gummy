#include "TerminalScene.hpp"

#include "SceneGame.hpp"

#include <SDL2/SDL.h>

TerminalScene::TerminalScene(std::string serverIp, int serverPort)
    : m_serverIp(std::move(serverIp)),
      m_serverPort(serverPort),
      m_bgTextureID("terminal_bg"),
      m_restartBtnTextureID("btn_restart"),
      m_title(nullptr),
      m_restartLabel(nullptr) {}

bool TerminalScene::onEnter() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();

    TextureManager::getInstance()->load("assets/gameplay_background.png", m_bgTextureID, renderer);
    TextureManager::getInstance()->load("assets/button.png", m_restartBtnTextureID, renderer);

    const int screenW = 1280;
    const int screenH = 720;

    m_title = new Text(0, 0, "assets/Arial.ttf", 64, "GAME OVER", {255, 255, 255, 255});
    m_title->setPosition((screenW - m_title->getWidth()) / 2.0f, (screenH / 2.0f) - 170.0f);
    m_uiObjects.push_back(m_title);

    const int btnW = 181;
    const int btnH = 73;
    const int btnX = (screenW - btnW) / 2;
    const int btnY = (screenH - btnH) / 2;

    Button* restartBtn = new Button(btnX, btnY, btnW, btnH, m_restartBtnTextureID, [this]() {
        // Defer state replacement so we don't delete the current state during its update.
        Game::getInstance()->getStateMachine()->requestReplaceAll(new SceneGame(m_serverIp, m_serverPort));
    });
    m_uiObjects.push_back(restartBtn);

    m_restartLabel = new Text(0, 0, "assets/Arial.ttf", 28, "RESTART", {255, 255, 255, 255});
    m_restartLabel->setPosition((screenW - m_restartLabel->getWidth()) / 2.0f,
                                btnY + (btnH - m_restartLabel->getHeight()) / 2.0f);
    m_uiObjects.push_back(m_restartLabel);

    return true;
}

bool TerminalScene::onExit() {
    for (auto* obj : m_uiObjects) {
        if (!obj) continue;
        obj->clean();
        delete obj;
    }
    m_uiObjects.clear();

    m_title = nullptr;
    m_restartLabel = nullptr;

    TextureManager::getInstance()->clearFromTextureMap(m_bgTextureID);
    TextureManager::getInstance()->clearFromTextureMap(m_restartBtnTextureID);

    return true;
}

void TerminalScene::update() {
    for (auto* obj : m_uiObjects) {
        if (obj) obj->update();
    }
}

void TerminalScene::render() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    if (!renderer) return;

    TextureManager::getInstance()->drawScaled(m_bgTextureID, 0, 0, 1280, 720, renderer);

    // Dim overlay to simulate a popup.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_Rect full{0, 0, 1280, 720};
    SDL_RenderFillRect(renderer, &full);

    // Center panel
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 220);
    SDL_Rect panel{(1280 - 520) / 2, (720 - 280) / 2, 520, 280};
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &panel);

    for (auto* obj : m_uiObjects) {
        if (obj) obj->draw();
    }
}
