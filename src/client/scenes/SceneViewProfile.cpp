#include "SceneViewProfile.hpp"

#include "../core/Game.hpp"
#include "../core/InputHandler.hpp"
#include "../core/TextureManager.hpp"
#include "../network/ClientSocket.hpp"

#include "../../common/network/Packet.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/types.h>
#endif

namespace {
void SafeCopy(char* dst, size_t dstSize, const std::string& s) {
    if (!dst || dstSize == 0) return;
    std::memset(dst, 0, dstSize);
    std::strncpy(dst, s.c_str(), dstSize - 1);
}

std::string ResultToString(uint8_t r) {
    if (r == 1) return "WIN";
    if (r == 2) return "DRAW";
    return "LOSS";
}

void LaunchReplayViewerProcess(const std::string& replayPath, const std::string& username) {
    if (replayPath.empty()) return;
#if defined(__unix__) || defined(__APPLE__)
    pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork(run_replay_viewer.sh)");
        return;
    }
    if (pid == 0) {
        if (username.empty()) {
            execl("./run_replay_viewer.sh", "./run_replay_viewer.sh", replayPath.c_str(), (char*)nullptr);
        } else {
            execl("./run_replay_viewer.sh",
                  "./run_replay_viewer.sh",
                  replayPath.c_str(),
                  "--username",
                  username.c_str(),
                  (char*)nullptr);
        }
        std::perror("execl(run_replay_viewer.sh)");
        _exit(127);
    }
#else
    std::string cmd = "./run_replay_viewer.sh \"" + replayPath + "\"";
    if (!username.empty()) {
        cmd += " --username \"" + username + "\"";
    }
    cmd += " &";
    std::system(cmd.c_str());
#endif
}
}

SceneViewProfile::SceneViewProfile(const std::string& username)
    : m_username(username) {
    std::memset(&m_profile, 0, sizeof(m_profile));
}

bool SceneViewProfile::onEnter() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();

    // Background is just the existing gameplay bg for now (consistent with dashboard)
    TextureManager::getInstance()->load(TextureManager::spritePath("gameplay_background.png"), "profile_bg", renderer);
    TextureManager::getInstance()->load(TextureManager::spritePath("button.png"), "btn_generic", renderer);

    // Back button top-left: "< BACK"
    m_btnBack = new Button(40, 30, 140, 44, "btn_generic", [this]() {
        Game::getInstance()->getStateMachine()->requestPopState();
    }, 181, 73);
    m_lblBack = new Text(0, 0, "assets/font.ttf", 18, "< BACK", {255, 255, 255, 255});
    m_btnBack->centerObject(m_lblBack);

    m_uiObjects.push_back(m_btnBack);
    m_uiObjects.push_back(m_lblBack);

    // Center header texts
    m_lblUsername = new Text(0, 0, "assets/font.ttf", 42, m_username, {255, 255, 255, 255});
    m_lblCreatedAt = new Text(0, 0, "assets/font.ttf", 18, "", {220, 220, 220, 255});
    m_lblElo = new Text(0, 0, "assets/font.ttf", 30, "", {255, 255, 255, 255});

    m_uiObjects.push_back(m_lblUsername);
    m_uiObjects.push_back(m_lblCreatedAt);
    m_uiObjects.push_back(m_lblElo);

    m_lblStatus = new Text(0, 0, "assets/font.ttf", 18, "Loading profile...", {255, 255, 0, 255});
    m_uiObjects.push_back(m_lblStatus);

    // Send request immediately
    std::cout << "[SceneViewProfile] Requesting profile..." << std::endl;
    if (!Game::getInstance()->getClientSocket()->SendGetProfile()) {
        m_isLoading = false;
        if (m_lblStatus) {
            m_lblStatus->setText("Failed to send request.");
            m_lblStatus->setColor({255, 80, 80, 255});
        }
    }

    return true;
}

void SceneViewProfile::clearHistoryTexts() {
    for (auto* t : m_historyTexts) {
        if (!t) continue;
        t->clean();
        delete t;
    }
    m_historyTexts.clear();

    for (auto* b : m_historyReplayButtons) {
        if (!b) continue;
        b->clean();
        delete b;
    }
    m_historyReplayButtons.clear();

    for (auto* t : m_historyReplayLabels) {
        if (!t) continue;
        t->clean();
        delete t;
    }
    m_historyReplayLabels.clear();
}

void SceneViewProfile::rebuildHistoryTexts() {
    clearHistoryTexts();

    if (!m_hasProfile || !m_profile.isSuccess) {
        m_scrollY = 0;
        m_scrollMax = 0;
        return;
    }

    const uint32_t count = std::min<uint32_t>(m_profile.gameCount, 20);
    m_historyTexts.reserve(count);
    m_historyReplayButtons.reserve(count);
    m_historyReplayLabels.reserve(count);

    // Build one line per game (most recent on top)
    for (int idx = (int)count - 1; idx >= 0; --idx) {
        const ProfileGameEntry& g = m_profile.games[idx];

        const std::string replayPath = g.replayPath;

        std::string opp = g.opponent;
        if (opp.empty()) opp = "Unknown";

        std::string when = g.endedAt;
        if (when.empty()) when = "(in progress)";

        const std::string result = ResultToString(g.result);
        const std::string line =
            std::string("[") + result + "] vs " + opp + " | " +
            std::to_string(g.myScore) + "-" + std::to_string(g.oppScore) +
            " | " + when;

        SDL_Color c = {220, 220, 220, 255};
        if (g.result == 1) c = {120, 255, 120, 255};
        else if (g.result == 0) c = {255, 120, 120, 255};
        else if (g.result == 2) c = {255, 255, 120, 255};

        Text* t = new Text(0, 0, "assets/font.ttf", 18, line, c);
        m_historyTexts.push_back(t);

        Button* btn = new Button(0, 0, 120, 40, "btn_generic", [this, replayPath]() {
            if (replayPath.empty()) {
                if (m_lblStatus) {
                    m_lblStatus->setText("Replay not available for this match.");
                    m_lblStatus->setColor({255, 80, 80, 255});
                }
                return;
            }
            LaunchReplayViewerProcess(replayPath, m_username);
        }, 181, 73);

        Text* lbl = new Text(0, 0, "assets/font.ttf", 16, "REPLAY", {255, 255, 255, 255});

        if (replayPath.empty()) {
            btn->setEnabled(false);
            btn->setStrokeColor({150, 150, 150, 255});
            lbl->setColor({150, 150, 150, 255});
        }

        m_historyReplayButtons.push_back(btn);
        m_historyReplayLabels.push_back(lbl);
    }

    m_scrollY = 0;
    m_scrollMax = 0; // computed in draw based on panel height
}

void SceneViewProfile::update() {
    // Back clickable
    if (m_btnBack) m_btnBack->update();

    // Poll for server response
    Packet packet;
    if (Game::getInstance()->getClientSocket()->CheckNotifications(packet)) {
        if (packet.header.type == PacketType::RES_GET_PROFILE) {
            m_profile = packet.GetPayload<ResGetProfile>();
            m_hasProfile = true;
            m_isLoading = false;

            if (m_profile.isSuccess) {
                if (m_lblUsername) m_lblUsername->setText(m_profile.username);
                if (m_lblCreatedAt) {
                    std::string created = m_profile.createdAt;
                    if (created.empty()) created = "";
                    m_lblCreatedAt->setText(std::string("Created: ") + created);
                }
                if (m_lblElo) {
                    m_lblElo->setText(std::string("ELO: ") + std::to_string((int)m_profile.elo));
                }
                if (m_lblStatus) {
                    m_lblStatus->setText(" ");
                }
                rebuildHistoryTexts();
            } else {
                if (m_lblStatus) {
                    m_lblStatus->setText(m_profile.message);
                    m_lblStatus->setColor({255, 80, 80, 255});
                }
                clearHistoryTexts();
            }
        }
    }

    // Scroll wheel for history
    {
        const int screenW = 1280;
        const int screenH = 720;
        const int x0 = 80;
        const int y0 = 250;
        const int w = screenW - 2 * x0;
        (void)w;
        const int h = screenH - y0 - 60;

        const int rowH = 78;
        const int padding = 18;
        const int totalRows = (int)m_historyTexts.size();
        const int contentH = padding + totalRows * rowH + padding;
        m_scrollMax = std::max(0, contentH - h);
        m_scrollY = std::max(0, std::min(m_scrollY, m_scrollMax));
    }

    const int wheel = InputHandler::getInstance()->getMouseWheelY();
    if (wheel != 0) {
        // wheel positive = up, negative = down
        const int step = 48;
        m_scrollY -= wheel * step;
        m_scrollY = std::max(0, std::min(m_scrollY, m_scrollMax));
    }

    for (auto* t : m_historyTexts) {
        if (t) t->update();
    }

    // Update replay buttons (only when visible in the panel)
    {
        const int screenW = 1280;
        const int screenH = 720;
        const int x0 = 80;
        const int y0 = 250;
        const int w = screenW - 2 * x0;
        const int h = screenH - y0 - 60;

        const int rowH = 78;
        const int padding = 18;
        const int btnW = 120;
        const int btnH = 40;

        const int totalRows = (int)m_historyReplayButtons.size();
        for (int i = 0; i < totalRows; ++i) {
            const int rowTop = y0 + padding + i * rowH - m_scrollY;
            const int rowBottom = rowTop + rowH;
            if (rowBottom < y0 || rowTop > y0 + h) continue;

            Button* btn = m_historyReplayButtons[i];
            Text* lbl = (i < (int)m_historyReplayLabels.size()) ? m_historyReplayLabels[i] : nullptr;
            if (!btn) continue;

            const int btnX = x0 + w - padding - btnW - 16;
            const int btnY = rowTop + (rowH - btnH) / 2;
            btn->setPosition((float)btnX, (float)btnY);
            btn->update();

            if (lbl) {
                btn->centerObject(lbl);
            }
        }
    }
}

void SceneViewProfile::drawHistoryPanel() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    if (!renderer) return;

    const int screenW = 1280;
    const int screenH = 720;

    // Panel area
    const int x0 = 80;
    const int y0 = 250;
    const int w = screenW - 2 * x0;
    const int h = screenH - y0 - 60;

    // Draw panel background
    TextureManager::getInstance()->drawFillRect(x0, y0, w, h, 0, 0, 0, 160, renderer);
    TextureManager::getInstance()->drawFillRect(x0, y0, w, 2, 255, 255, 255, 200, renderer);

    const int rowH = 78;
    const int padding = 18;

    const int totalRows = (int)m_historyTexts.size();
    const int contentH = padding + totalRows * rowH + padding;
    m_scrollMax = std::max(0, contentH - h);
    m_scrollY = std::max(0, std::min(m_scrollY, m_scrollMax));

    // Render each row (simple framed card)
    for (int i = 0; i < totalRows; ++i) {
        const int rowTop = y0 + padding + i * rowH - m_scrollY;
        const int rowBottom = rowTop + rowH;

        if (rowBottom < y0 || rowTop > y0 + h) continue;

        // Card background
        TextureManager::getInstance()->drawFillRect(x0 + padding, rowTop, w - 2 * padding, rowH - 12, 0, 0, 0, 120, renderer);
        TextureManager::getInstance()->drawFillRect(x0 + padding, rowTop, w - 2 * padding, 1, 255, 255, 255, 120, renderer);

        Text* t = m_historyTexts[i];
        if (!t) continue;
        t->setPosition((float)(x0 + padding + 16), (float)(rowTop + 22));
        t->draw();

        if (i < (int)m_historyReplayButtons.size() && m_historyReplayButtons[i]) {
            Button* btn = m_historyReplayButtons[i];
            Text* lbl = (i < (int)m_historyReplayLabels.size()) ? m_historyReplayLabels[i] : nullptr;

            const int btnW = 120;
            const int btnH = 40;
            const int btnX = x0 + w - padding - btnW - 16;
            const int btnY = rowTop + (rowH - btnH) / 2;
            btn->setPosition((float)btnX, (float)btnY);
            btn->draw();
            if (lbl) {
                btn->centerObject(lbl);
                lbl->draw();
            }
        }
    }
}

void SceneViewProfile::render() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();

    // Background
    TextureManager::getInstance()->drawStatic("profile_bg", 0, 0, 1280, 720, renderer);

    // Header layout (top-center)
    if (m_lblUsername) {
        m_lblUsername->setPosition((float)(640 - m_lblUsername->getWidth() / 2), 60.0f);
        m_lblUsername->draw();
    }
    if (m_lblCreatedAt) {
        m_lblCreatedAt->setPosition((float)(640 - m_lblCreatedAt->getWidth() / 2), 115.0f);
        m_lblCreatedAt->draw();
    }
    if (m_lblElo) {
        m_lblElo->setPosition((float)(640 - m_lblElo->getWidth() / 2), 145.0f);
        m_lblElo->draw();
    }

    // Back
    if (m_btnBack) m_btnBack->draw();
    if (m_lblBack) {
        m_btnBack->centerObject(m_lblBack);
        m_lblBack->draw();
    }

    // Status
    if (m_lblStatus) {
        m_lblStatus->setPosition((float)(640 - m_lblStatus->getWidth() / 2), 190.0f);
        m_lblStatus->draw();
    }

    // History
    drawHistoryPanel();
}

bool SceneViewProfile::onExit() {
    clearHistoryTexts();

    for (auto* obj : m_uiObjects) {
        if (!obj) continue;
        obj->clean();
        delete obj;
    }
    m_uiObjects.clear();

    m_btnBack = nullptr;
    m_lblBack = nullptr;
    m_lblUsername = nullptr;
    m_lblCreatedAt = nullptr;
    m_lblElo = nullptr;
    m_lblStatus = nullptr;

    return true;
}
