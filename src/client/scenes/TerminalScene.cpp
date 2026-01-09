#include "TerminalScene.hpp"

#include "SceneGame.hpp"

#include "../network/ClientSocket.hpp"

#include <SDL2/SDL.h>

namespace {
constexpr int kScreenW = 1280;
constexpr int kScreenH = 720;

// End-screen panel dimensions (bigger to avoid text overlap)
constexpr int kPanelW = 640;
constexpr int kPanelH = 440;

inline int PanelX() { return (kScreenW - kPanelW) / 2; }
inline int PanelY() { return (kScreenH - kPanelH) / 2; }
} // namespace

TerminalScene::TerminalScene(std::string serverIp, int serverPort, std::string mapPath, std::string username, std::string resultText)
    : m_serverIp(std::move(serverIp)),
      m_serverPort(serverPort),
    m_mapPath(std::move(mapPath)),
    m_username(std::move(username)),
        m_resultText(std::move(resultText)),
      m_bgTextureID("terminal_bg"),
      m_restartBtnTextureID("btn_restart"),
    m_homeBtnTextureID("btn_home"),
      m_title(nullptr),
        m_resultLabel(nullptr),
        m_rematchStatusLabel(nullptr),
        m_rematchTimerLabel(nullptr),
        m_rematchBtn(nullptr),
    m_restartLabel(nullptr),
    m_homeLabel(nullptr) {}

bool TerminalScene::onEnter() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();

    TextureManager::getInstance()->load(TextureManager::spritePath("gameplay_background.png"), m_bgTextureID, renderer);
    TextureManager::getInstance()->load(TextureManager::spritePath("button.png"), m_restartBtnTextureID, renderer);
    TextureManager::getInstance()->load(TextureManager::spritePath("button.png"), m_homeBtnTextureID, renderer);

    m_title = new Text(0, 0, "assets/font.ttf", 64, "GAME OVER", {255, 255, 255, 255});
    m_title->setPosition((kScreenW - m_title->getWidth()) / 2.0f, (float)PanelY() + 30.0f);
    m_uiObjects.push_back(m_title);

    if (!m_resultText.empty()) {
        m_resultLabel = new Text(0, 0, "assets/font.ttf", 36, m_resultText, {255, 255, 255, 255});
        m_resultLabel->setPosition((kScreenW - m_resultLabel->getWidth()) / 2.0f, (float)PanelY() + 115.0f);
        m_uiObjects.push_back(m_resultLabel);
    }

    // Rematch status line (hidden until first rematch click)
    m_rematchStatusLabel = new Text(0, 0, "assets/font.ttf", 22, "", {255, 255, 255, 255});
    m_uiObjects.push_back(m_rematchStatusLabel);

    // Rematch timer line (hidden until rematch window is active)
    m_rematchTimerLabel = new Text(0, 0, "assets/font.ttf", 20, "", {255, 255, 255, 255});
    m_uiObjects.push_back(m_rematchTimerLabel);

    const int btnW = 181;
    const int btnH = 73;
    const int btnX = (kScreenW - btnW) / 2;
    const int btnY = PanelY() + 245;

    const bool homeEnabled = (Game::getInstance()->getClientSocket() && Game::getInstance()->getClientSocket()->IsConnected());

    m_rematchBtn = new Button(btnX, btnY, btnW, btnH, m_restartBtnTextureID, [this]() {
        if (m_rematchRequested) return;

        // TerminalScene overlays SceneGame. Ask the underlying SceneGame to send the rematch request
        // through the existing ingame connection.
        auto* sm = Game::getInstance()->getStateMachine();
        auto* under = sm ? dynamic_cast<SceneGame*>(sm->peekStateFromTop(1)) : nullptr;
        if (!under) return;

        if (!under->SendRematchRequest()) return;

        m_rematchRequested = true;
        m_rematchRequestTick = SDL_GetTicks();

        if (m_rematchStatusLabel) {
            m_rematchStatusLabel->setText("Wait for the response");
            m_rematchStatusLabel->setPosition((kScreenW - m_rematchStatusLabel->getWidth()) / 2.0f, (float)PanelY() + 170.0f);
        }
        if (m_rematchTimerLabel) {
            // Timer will be updated from update(); place it now to reserve consistent layout.
            m_rematchTimerLabel->setPosition((kScreenW - m_rematchTimerLabel->getWidth()) / 2.0f, (float)PanelY() + 200.0f);
        }
        if (m_rematchBtn) {
            m_rematchBtn->setEnabled(false);
            m_rematchBtn->setAlpha(140);
        }
    });
    m_uiObjects.push_back(m_rematchBtn);

    m_restartLabel = new Text(0, 0, "assets/font.ttf", 28, "REMATCH", {255, 255, 255, 255});
    m_restartLabel->setPosition((kScreenW - m_restartLabel->getWidth()) / 2.0f,
                                btnY + (btnH - m_restartLabel->getHeight()) / 2.0f);
    m_uiObjects.push_back(m_restartLabel);

    const int homeY = btnY + btnH + 18;
    Button* homeBtn = new Button(btnX, homeY, btnW, btnH, m_homeBtnTextureID, [this]() {
        // Go back to the "home" (Dashboard) by popping TerminalScene and SceneGame.
        Game::getInstance()->getStateMachine()->requestPopStates(2);
    });
    if (!homeEnabled) {
        homeBtn->setEnabled(false);
        homeBtn->setAlpha(110);
    }
    m_uiObjects.push_back(homeBtn);

    SDL_Color homeColor = homeEnabled ? SDL_Color{255, 255, 255, 255} : SDL_Color{255, 255, 255, 110};
    m_homeLabel = new Text(0, 0, "assets/font.ttf", 28, "DONE", homeColor);
    m_homeLabel->setPosition((kScreenW - m_homeLabel->getWidth()) / 2.0f,
                             homeY + (btnH - m_homeLabel->getHeight()) / 2.0f);
    m_uiObjects.push_back(m_homeLabel);

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
    m_resultLabel = nullptr;
    m_rematchStatusLabel = nullptr;
    m_rematchTimerLabel = nullptr;
    m_rematchBtn = nullptr;
    m_restartLabel = nullptr;
    m_homeLabel = nullptr;

    TextureManager::getInstance()->clearFromTextureMap(m_bgTextureID);
    TextureManager::getInstance()->clearFromTextureMap(m_restartBtnTextureID);
    TextureManager::getInstance()->clearFromTextureMap(m_homeBtnTextureID);

    return true;
}

void TerminalScene::update() {
    for (auto* obj : m_uiObjects) {
        if (obj) obj->update();
    }

    // Rematch overlay behavior (server-driven via SceneGame receiver thread)
    auto* sm = Game::getInstance()->getStateMachine();
    auto* under = sm ? dynamic_cast<SceneGame*>(sm->peekStateFromTop(1)) : nullptr;
    if (!under) return;

    // If the server signals start, immediately rematch (fresh SceneGame).
    if (under->ConsumeRematchStart()) {
        const bool hasHome = (Game::getInstance()->getClientSocket() && Game::getInstance()->getClientSocket()->IsConnected());
        if (hasHome) {
            // Request a RANDOM map for rematch
            Game::getInstance()->getStateMachine()->requestPopStatesAndPush(2, new SceneGame(m_serverIp, m_serverPort, "RANDOM", m_username));
        } else {
            Game::getInstance()->getStateMachine()->requestReplaceAll(new SceneGame(m_serverIp, m_serverPort, "RANDOM", m_username));
        }
        return;
    }

    const Uint32 now = SDL_GetTicks();
    uint8_t acceptedMask = 0;
    std::string msg;
    const uint8_t st = under->GetRematchStatus(&acceptedMask, &msg);

    // Opponent left / does not want rematch: show message and disable rematch permanently for this match.
    if (st == 2 && msg == "opponent_left") {
        if (m_rematchStatusLabel) {
            m_rematchStatusLabel->setText("Your opponent does not want to rematch");
            m_rematchStatusLabel->setPosition((kScreenW - m_rematchStatusLabel->getWidth()) / 2.0f, (float)PanelY() + 170.0f);
        }
        if (m_rematchTimerLabel) {
            m_rematchTimerLabel->setText("");
        }
        if (m_rematchBtn) {
            m_rematchBtn->setEnabled(false);
            m_rematchBtn->setAlpha(110);
        }
        m_rematchRequested = false;
        m_rematchRequestTick = 0;
        m_rematchWindowStartTick = 0;
        m_lastShownRematchSeconds = -1;
        return;
    }

    // If a rematch window is active (someone clicked), start tracking the 60s countdown.
    if (m_rematchWindowStartTick == 0 && st == 0 && (acceptedMask & 0x3u) != 0) {
        m_rematchWindowStartTick = now;
        m_lastShownRematchSeconds = -1;
    }

    // Update status text depending on whether *I* have accepted.
    if (st == 0 && (acceptedMask & 0x3u) != 0) {
        const uint32_t myId = under->GetPlayerId();
        const bool iAccepted = (myId < 2) ? ((acceptedMask & (uint8_t)(1u << myId)) != 0) : m_rematchRequested;
        const bool otherAccepted = (myId < 2) ? ((acceptedMask & (uint8_t)(1u << (1u - myId))) != 0) : false;

        if (!iAccepted && otherAccepted) {
            if (m_rematchStatusLabel) {
                m_rematchStatusLabel->setText("Your opponent want a rematch");
                m_rematchStatusLabel->setPosition((kScreenW - m_rematchStatusLabel->getWidth()) / 2.0f, (float)PanelY() + 170.0f);
            }
        } else if (iAccepted) {
            if (m_rematchStatusLabel) {
                m_rematchStatusLabel->setText("Wait for the response");
                m_rematchStatusLabel->setPosition((kScreenW - m_rematchStatusLabel->getWidth()) / 2.0f, (float)PanelY() + 170.0f);
            }
        }
    }

    // Timer display while the window is active.
    if (m_rematchWindowStartTick != 0) {
        const Uint32 elapsed = now - m_rematchWindowStartTick;
        int remainingSec = 60 - (int)(elapsed / 1000u);
        if (remainingSec < 0) remainingSec = 0;
        if (remainingSec != m_lastShownRematchSeconds) {
            m_lastShownRematchSeconds = remainingSec;
            if (m_rematchTimerLabel) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Time left: %ds", remainingSec);
                m_rematchTimerLabel->setText(buf);
                m_rematchTimerLabel->setPosition((kScreenW - m_rematchTimerLabel->getWidth()) / 2.0f, (float)PanelY() + 200.0f);
            }
        }
    }

    // Timeout handling: server timeout or local timeout cap.
    const bool localTimedOut = (m_rematchWindowStartTick != 0 && now - m_rematchWindowStartTick >= 60000u);
    if (st == 2 || localTimedOut) {
        if (m_rematchStatusLabel) {
            m_rematchStatusLabel->setText("No response");
            m_rematchStatusLabel->setPosition((kScreenW - m_rematchStatusLabel->getWidth()) / 2.0f, (float)PanelY() + 170.0f);
        }
        if (m_rematchTimerLabel) {
            m_rematchTimerLabel->setText("");
        }
        if (m_rematchBtn) {
            m_rematchBtn->setEnabled(true);
            m_rematchBtn->setAlpha(255);
        }
        m_rematchRequested = false;
        m_rematchRequestTick = 0;
        m_rematchWindowStartTick = 0;
        m_lastShownRematchSeconds = -1;
    }
}

void TerminalScene::render() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    if (!renderer) return;

    TextureManager::getInstance()->drawScaled(m_bgTextureID, 0, 0, kScreenW, kScreenH, renderer);

    // Dim overlay to simulate a popup.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_Rect full{0, 0, kScreenW, kScreenH};
    SDL_RenderFillRect(renderer, &full);

    // Center panel
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 220);
    SDL_Rect panel{PanelX(), PanelY(), kPanelW, kPanelH};
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &panel);

    for (auto* obj : m_uiObjects) {
        if (obj) obj->draw();
    }
}
