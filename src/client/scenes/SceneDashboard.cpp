#include "SceneDashboard.hpp"
#include "SceneLogin.hpp"
#include "SceneGame.hpp"
#include "SceneViewProfile.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../core/InputHandler.hpp"
#include <iostream>
#include <cstring>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/types.h>
#endif

namespace {
void LaunchIngameClientProcess(const std::string& host, int port, const std::string& mapPath, const std::string& username) {
#if defined(__unix__) || defined(__APPLE__)
    pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork(net_game_client)");
        return;
    }
    if (pid == 0) {
        const std::string portStr = std::to_string(port);
        if (username.empty()) {
            execl("./net_game_client", "./net_game_client", host.c_str(), portStr.c_str(), mapPath.c_str(), (char*)nullptr);
        } else {
            execl("./net_game_client",
                  "./net_game_client",
                  host.c_str(),
                  portStr.c_str(),
                  mapPath.c_str(),
                  username.c_str(),
                  (char*)nullptr);
        }
        std::perror("execl(net_game_client)");
        _exit(127);
    }
#else
    // Fallback: best-effort
    std::string cmd = "./net_game_client \"" + host + "\" " + std::to_string(port) + " \"" + mapPath + "\"";
    if (!username.empty()) {
        cmd += " \"" + username + "\"";
    }
    cmd += " &";
    std::system(cmd.c_str());
#endif
}

void LaunchReplayViewerProcess(const std::string& replayPath, const std::string& username) {
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

SceneDashboard::SceneDashboard(std::string username) 
    : m_username(username),
      m_isSearching(false), m_searchStartTime(0),
      m_btnFindMatch(nullptr), m_lblFindMatch(nullptr),
      m_btnCancelSearch(nullptr), m_lblSearchingTimer(nullptr),
      m_showMatchPopup(false), m_hasMatchDecision(false), m_pendingMatchId(0),
      m_btnAccept(nullptr), m_btnDecline(nullptr), m_lblMatchFound(nullptr),
      m_lblAccept(nullptr), m_lblDecline(nullptr), m_lblMatchStatus(nullptr)
{
    // Implementation moved to onEnter to fetch real data
}


bool SceneDashboard::onEnter() {
    std::cout << "[SceneDashboard] Entering as " << m_username << "..." << std::endl;
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();

    // 1. Load Resources
    m_bgTextureID = "lobby_bg";
    if (!TextureManager::getInstance()->load("assets/gameplay_background.png", m_bgTextureID, renderer)) {
        // Fallback or just log
    }
    TextureManager::getInstance()->load("assets/button.png", "btn_generic", renderer);

    // 2. Layout Constants
    const int screenW = 1280;
    const int screenH = 720;
    const int outerMargin = 40;
    const int panelW = 360;
    const int panelX = screenW - outerMargin - panelW;
    const int panelY = 40;
    const int panelH = screenH - 2 * panelY;

    // --- PLAYER LIST ---
    std::cout << "[SceneDashboard] Fetching user list..." << std::endl;
    m_allPlayers = Game::getInstance()->getClientSocket()->GetUserList();

    // --- LEFT TITLE ---
    m_lblTitle = new Text(80, 60, "assets/font.ttf", 72, "GUMMY", {255, 255, 255, 255});
    m_uiObjects.push_back(m_lblTitle);

    // --- RIGHT PANEL TITLE ---
    Text* lblOnlineTitle = new Text(0, 0, "assets/font.ttf", 22, "PLAYERS ONLINE", {255, 255, 255, 255});
    lblOnlineTitle->setPosition((float)(panelX + (panelW - lblOnlineTitle->getWidth()) / 2), (float)(panelY + 22));
    m_uiObjects.push_back(lblOnlineTitle);

    // --- MAIN CONTENT (Left Side) ---
    const int leftX = 80;
    const int leftW = panelX - leftX - 40;
    (void)leftW;

    // 4 equal-size buttons stacked
    const int actionBtnW = 280;
    const int actionBtnH = 64;
    const int actionGap = 16;

                // "FIND MATCH" Button (primary action)
                const int playBtnY = 220;
                m_btnFindMatch = new Button(leftX, playBtnY, actionBtnW, actionBtnH, "btn_generic", [this]() {
        std::cout << "[SceneDashboard] Sending Find Match Request..." << std::endl;
        if(Game::getInstance()->getClientSocket()->SendFindMatch()) {
             std::cout << " > Request Sent. Waiting for server confirmation..." << std::endl;
             // Do NOT start timer yet. Wait for RES_MATCH_FIND.
        } else {
             std::cout << " > Request Failed (Send Error)." << std::endl;
        }
    }, 181, 73);
    // m_uiObjects.push_back(m_btnFindMatch); // Managed manually

    m_lblFindMatch = new Text(0, 0, "assets/font.ttf", 26, "FIND MATCH", {255, 255, 255, 255});
    m_btnFindMatch->centerObject(m_lblFindMatch);
    // m_uiObjects.push_back(m_lblFindMatch); // Managed manually

            // Cancel Search Button (same size/position as Find Match)
            m_btnCancelSearch = new Button(leftX, playBtnY, actionBtnW, actionBtnH, "btn_generic", [this]() {
         std::cout << "[SceneDashboard] Cancelling Search..." << std::endl;
         Game::getInstance()->getClientSocket()->SendCancelMatch();
         m_isSearching = false;
    }, 181, 73);

        m_lblSearchingTimer = new Text(leftX, playBtnY - 46, "assets/font.ttf", 22, "Searching... 00:00", {255, 255, 255, 255});

        // VIEW PROFILE button (between Find Match and Replay)
        const int viewProfileBtnY = playBtnY + actionBtnH + actionGap;
        m_btnViewProfile = new Button(leftX, viewProfileBtnY, actionBtnW, actionBtnH, "btn_generic", [this]() {
            // Send request then switch screen immediately.
            Game::getInstance()->getClientSocket()->SendGetProfile();
            Game::getInstance()->getStateMachine()->pushState(new SceneViewProfile(m_username));
        }, 181, 73);
        m_lblViewProfile = new Text(0, 0, "assets/font.ttf", 22, "VIEW PROFILE", {255, 255, 255, 255});
        m_btnViewProfile->centerObject(m_lblViewProfile);
        m_uiObjects.push_back(m_btnViewProfile);
        m_uiObjects.push_back(m_lblViewProfile);

        // REPLAY button (opens popup)
        const int replayBtnY = viewProfileBtnY + actionBtnH + actionGap;
        m_btnReplay = new Button(leftX, replayBtnY, actionBtnW, actionBtnH, "btn_generic", [this]() {
            m_showReplayPopup = true;
        }, 181, 73);
        m_lblReplay = new Text(0, 0, "assets/font.ttf", 22, "REPLAY", {255, 255, 255, 255});
        m_btnReplay->centerObject(m_lblReplay);
        m_uiObjects.push_back(m_btnReplay);
        m_uiObjects.push_back(m_lblReplay);

        // LOGOUT button (below replay)
        const int logoutBtnY = replayBtnY + actionBtnH + actionGap;
        Button* btnLogout = new Button(leftX, logoutBtnY, actionBtnW, actionBtnH, "btn_generic", []() {
            Game::getInstance()->getClientSocket()->Logout();
            Game::getInstance()->getStateMachine()->changeState(new SceneLogin());
        }, 181, 73);
        Text* lblLogout = new Text(0, 0, "assets/font.ttf", 22, "LOGOUT", {255, 255, 255, 255});
        btnLogout->centerObject(lblLogout);
        m_uiObjects.push_back(btnLogout);
        m_uiObjects.push_back(lblLogout);

        // QUIT button (below logout)
        const int quitBtnY = logoutBtnY + actionBtnH + actionGap;
        Button* btnQuit = new Button(leftX, quitBtnY, actionBtnW, actionBtnH, "btn_generic", []() {
            Game::getInstance()->quit();
        }, 181, 73);
        Text* lblQuit = new Text(0, 0, "assets/font.ttf", 22, "QUIT", {255, 255, 255, 255});
        btnQuit->centerObject(lblQuit);
        m_uiObjects.push_back(btnQuit);
        m_uiObjects.push_back(lblQuit);

    // --- REPLAY WATCH (single-user) ---
    // Start the replay viewer directly (it will start the replay server internally).
    // Replay popup UI (created once; drawn conditionally)
    m_lblReplayPopupTitle = new Text(0, 0, "assets/font.ttf", 28, "REPLAY", {255, 255, 255, 255});
    m_lblReplayPopupHint = new Text(0, 0, "assets/font.ttf", 16, "Directory / file", {200, 200, 200, 255});
    m_inReplayDir = new TextInput(0, 0, 560, 40, "assets/font.ttf", 18);
    m_btnReplayStart = new Button(0, 0, 140, 44, "btn_generic", [this]() {
        if (!m_inReplayDir) return;
        const std::string replayPath = m_inReplayDir->getString();
        if (replayPath.empty()) return;
        std::cout << "[SceneDashboard] Launching replay viewer for: " << replayPath << std::endl;
        LaunchReplayViewerProcess(replayPath, m_username);
        m_showReplayPopup = false;
    }, 181, 73);
    m_lblReplayStart = new Text(0, 0, "assets/font.ttf", 18, "OPEN", {255, 255, 255, 255});
    m_btnReplayCancel = new Button(0, 0, 140, 44, "btn_generic", [this]() {
        m_showReplayPopup = false;
    }, 181, 73);
    m_lblReplayCancel = new Text(0, 0, "assets/font.ttf", 18, "CANCEL", {255, 255, 255, 255});

    // --- POPUP MENU BUTTONS (Hidden initially) ---
    // ... (Existing code) ...

    // --- MATCHMAKING POPUP ---
    const int popupCX = (screenW - panelW) / 2;
    const int popupCY = screenH / 2;
    m_lblMatchFound = new Text( popupCX - 120, popupCY - 120, "assets/font.ttf", 32, "MATCH FOUND!", {255, 255, 255, 255});
    m_lblMatchStatus = new Text( popupCX - 160, popupCY - 40, "assets/font.ttf", 20, "", {255, 255, 255, 255});

    m_lblAccept = new Text(0, 0, "assets/font.ttf", 20, "Accept", {255, 255, 255, 255});
    m_lblDecline = new Text(0, 0, "assets/font.ttf", 20, "Decline", {255, 255, 255, 255});
    
     m_btnAccept = new Button(popupCX - 120, popupCY + 20, 140, 48, "btn_generic", [this]() {
         std::cout << "Accepted Match!" << std::endl;
            Game::getInstance()->getClientSocket()->SendMatchDecision(true, m_pendingMatchId);
         m_hasMatchDecision = true;
         m_lblMatchStatus->setText("Waiting for opponent...");
         m_lblMatchStatus->setColor({255, 255, 0, 255});
    }, 181, 73);
    
     m_btnDecline = new Button(popupCX + 10, popupCY + 20, 140, 48, "btn_generic", [this]() {
         std::cout << "Declined Match!" << std::endl;
            Game::getInstance()->getClientSocket()->SendMatchDecision(false, m_pendingMatchId);
         // m_showMatchPopup = false; // Maybe wait for server? Or just close?
         // If declined, server cancels match for both.
         m_hasMatchDecision = true;
         m_lblMatchStatus->setText("You declined.");
         m_lblMatchStatus->setColor({255, 0, 0, 255});
         // Close after short delay? Or immediately?
         m_showMatchPopup = false;
    }, 181, 73);

    // Initial Refresh
    refreshPlayerList();
    m_lastRefreshTime = SDL_GetTicks();

    return true;
}

void SceneDashboard::refreshPlayerList() {
    // 1. Clear old texts
    for (auto t : m_playerListTexts) {
        t->clean(); // If Text had resources
        delete t;
    }
    m_playerListTexts.clear();

    // 2. Fetch new list
    m_allPlayers = Game::getInstance()->getClientSocket()->GetUserList();

    // 3. Rebuild texts
    const int screenW = 1280;
    const int outerMargin = 40;
    const int panelW = 360;
    const int panelX = screenW - outerMargin - panelW;
    const int startY = 110;
    const int gap = 34;

    int drawCount = 0;

    for (size_t i = 0; i < m_allPlayers.size(); i++) {
        // Only show other ONLINE users
        if (std::string(m_allPlayers[i].username) == m_username) continue;
        if (!m_allPlayers[i].isOnline) continue;

        SDL_Color color = {200, 200, 200, 255};
        
        std::string entry = std::string(m_allPlayers[i].username) + " (" + std::to_string(m_allPlayers[i].elo) + ")";
        
        Text* t = new Text(0, 0, "assets/font.ttf", 18, entry, color);
        const int y = startY + (drawCount * gap);
        const int x = panelX + (panelW - t->getWidth()) / 2;
        t->setPosition((float)x, (float)y);
        m_playerListTexts.push_back(t);
        drawCount++;
    }
}

void SceneDashboard::update() {
    // Check Matchmaking Notifications
    Packet packet;
    if (Game::getInstance()->getClientSocket()->CheckNotifications(packet)) {
        if (packet.header.type == PacketType::RES_MATCH_FIND) {
             std::cout << "[Dashboard] Search Pending Confirmed by Server." << std::endl;
             m_isSearching = true;
             m_searchStartTime = SDL_GetTicks();
        }
        else if (packet.header.type == PacketType::REQ_MATCH_DECIDE_1) {
            std::cout << "[Dashboard] Match Found! Displaying popup." << std::endl;
            ReqMatchDecide1 req = packet.GetPayload<ReqMatchDecide1>();
            m_pendingMatchId = req.matchId;
            m_isSearching = false;
            m_showMatchPopup = true;
            m_hasMatchDecision = false;
        }
        else if (packet.header.type == PacketType::RES_MATCH_DECIDE_2) {
             std::cout << "[Dashboard] Match Confirmed! Starting Game..." << std::endl;
             // TODO: Transition to SceneGame
             // ResMatchDecide2 has playerOrder info
        }
           else if (packet.header.type == PacketType::INIT_GAME) {
              InitGame init = packet.GetPayload<InitGame>();

              std::string host = init.host;
              if (host.empty()) host = "127.0.0.1";
              if (init.port == 0) {
                  std::cout << "[Dashboard] INIT_GAME has port=0; ignoring (bad packet)" << std::endl;
                  m_showMatchPopup = false;
                  m_hasMatchDecision = false;
                  return;
              }
              int port = (int)init.port;
              std::string mapPath = init.mapPath;
              if (mapPath.empty()) mapPath = "assets/maps/flatmap.txt";

              std::cout << "[Dashboard] INIT_GAME received. Launching gameplay client: "
                      << host << ":" << port << " map=" << mapPath << std::endl;

                  const uint32_t myUserId = Game::getInstance()->getClientSocket()->GetUserId();

                  // Run gameplay inside this process so we can return to Dashboard after the match.
                  Game::getInstance()->getStateMachine()->pushState(new SceneGame(host, port, mapPath, m_username, init.matchId, myUserId));
              m_showMatchPopup = false;
              m_hasMatchDecision = false;
              return;
           }
        else if (packet.header.type == PacketType::RES_MATCH_CANCEL) {
             m_isSearching = false;
             m_showMatchPopup = false; 
             m_hasMatchDecision = false;
             std::cout << "[Dashboard] Match cancelled (Opponent declined or You cancelled)." << std::endl;
        }
    }

    if (m_showMatchPopup) {
         if (!m_hasMatchDecision) {
            m_btnAccept->update();
            m_btnDecline->update();
         }
         // Block other updates
         return;
    }

    if (m_showReplayPopup) {
        if (m_inReplayDir) m_inReplayDir->update();
        if (m_btnReplayStart) m_btnReplayStart->update();
        if (m_btnReplayCancel) m_btnReplayCancel->update();
        return;
    }
    
    if (m_isSearching) {
        uint32_t elapsed = (SDL_GetTicks() - m_searchStartTime) / 1000;
        int min = elapsed / 60;
        int sec = elapsed % 60;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Searching... %02d:%02d", min, sec);
        m_lblSearchingTimer->setText(buf);
        
        m_btnCancelSearch->update();
    } else {
        m_btnFindMatch->update();
    }

    // 1. Auto Refresh every 1 second
    if (SDL_GetTicks() - m_lastRefreshTime > 1000) {
        refreshPlayerList();
        m_lastRefreshTime = SDL_GetTicks();
    }

    // Update standard UI
    for (auto obj : m_uiObjects) {
        obj->update();
    }
    
    // Update dynamic player texts
    for (auto t : m_playerListTexts) {
        t->update();
    }

}

void SceneDashboard::drawMatchPopup() {
     if (!m_showMatchPopup) return;
     SDL_Renderer* renderer = Game::getInstance()->getRenderer();
     int w, h;
     SDL_GetRendererOutputSize(renderer, &w, &h);
     TextureManager::getInstance()->drawFillRect(0, 0, w, h, 0, 0, 0, 200, renderer);
     
     m_lblMatchFound->draw();
     
     if (!m_hasMatchDecision) {
        m_btnAccept->draw();
        m_btnDecline->draw();

          // Update label positions
          m_btnAccept->centerObject(m_lblAccept);
        m_lblAccept->draw();

          m_btnDecline->centerObject(m_lblDecline);
        m_lblDecline->draw();
     } else {
        m_lblMatchStatus->draw();
     }
}

void SceneDashboard::drawSidebar() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    const int screenW = 1280;
    const int screenH = 720;
    const int outerMargin = 40;
    const int sidebarW = 360;
    const int sidebarX = screenW - outerMargin - sidebarW;
    const int sidebarY = 40;
    const int sidebarH = screenH - 2 * sidebarY;

    // Draw Sidebar Background (Semi-transparent)
    TextureManager::getInstance()->drawFillRect(sidebarX, sidebarY, sidebarW, sidebarH, 0, 0, 0, 140, renderer);
    
    // Draw Player List (Dynamic)
    for (auto t : m_playerListTexts) {
        t->draw();
    }
}

void SceneDashboard::render() {
    // 1. Background
    TextureManager::getInstance()->drawStatic(m_bgTextureID, 0, 0, 1280, 720, Game::getInstance()->getRenderer());

    // 2. Right panel background + list
    drawSidebar();

    // 3. Main UI
    for (auto obj : m_uiObjects) {
        obj->draw();
    }
    
    // Draw Search UI
    if (m_isSearching) {
         m_lblSearchingTimer->draw();
         m_btnCancelSearch->draw();
         
         // Should add a "Cancel" text label on top of button if button texture is generic
         // For now, assume button texture has cancel? Or add label.
         // Let's add a quick text label for Cancel
            Text lblCancel(0, 0, "assets/font.ttf", 20, "CANCEL", {255,0,0,255});
            m_btnCancelSearch->centerObject(&lblCancel);
         lblCancel.draw();
    } else {
         m_btnFindMatch->draw();
            m_btnFindMatch->centerObject(m_lblFindMatch);
         m_lblFindMatch->draw();
    }

    // 4. Match Popup
    drawMatchPopup();

    // 5. Replay Popup
    drawReplayPopup();
}

void SceneDashboard::drawReplayPopup() {
    if (!m_showReplayPopup) return;
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    if (!renderer) return;

    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    TextureManager::getInstance()->drawFillRect(0, 0, w, h, 0, 0, 0, 180, renderer);

    const int panelW = 720;
    const int panelH = 260;
    const int x0 = (w - panelW) / 2;
    const int y0 = (h - panelH) / 2;

    TextureManager::getInstance()->drawFillRect(x0, y0, panelW, panelH, 0, 0, 0, 200, renderer);
    TextureManager::getInstance()->drawFillRect(x0, y0, panelW, 2, 255, 255, 255, 200, renderer);

    if (m_lblReplayPopupTitle) {
        m_lblReplayPopupTitle->setPosition((float)x0 + 24.0f, (float)y0 + 18.0f);
        m_lblReplayPopupTitle->draw();
    }
    if (m_lblReplayPopupHint) {
        m_lblReplayPopupHint->setPosition((float)x0 + 24.0f, (float)y0 + 70.0f);
        m_lblReplayPopupHint->draw();
    }
    if (m_inReplayDir) {
        m_inReplayDir->setPosition((float)x0 + 24.0f, (float)y0 + 98.0f);
        m_inReplayDir->draw();
    }

    const int btnY = y0 + panelH - 62;
    if (m_btnReplayCancel && m_lblReplayCancel) {
        m_btnReplayCancel->setPosition((float)(x0 + panelW - 24 - 140 - 12 - 140), (float)btnY);
        m_btnReplayCancel->draw();
        m_btnReplayCancel->centerObject(m_lblReplayCancel);
        m_lblReplayCancel->draw();
    }
    if (m_btnReplayStart && m_lblReplayStart) {
        m_btnReplayStart->setPosition((float)(x0 + panelW - 24 - 140), (float)btnY);
        m_btnReplayStart->draw();
        m_btnReplayStart->centerObject(m_lblReplayStart);
        m_lblReplayStart->draw();
    }
}

bool SceneDashboard::onExit() {
    for (auto obj : m_uiObjects) {
        obj->clean();
        delete obj;
    }
    m_uiObjects.clear();
    
    // Clean dynamic texts
    for (auto t : m_playerListTexts) {
        t->clean();
        delete t;
    }
    m_playerListTexts.clear();

    // Match Popup cleanup
    if (m_lblMatchFound) { m_lblMatchFound->clean(); delete m_lblMatchFound; }
    if (m_lblMatchStatus) { m_lblMatchStatus->clean(); delete m_lblMatchStatus; }
    if (m_btnAccept) { m_btnAccept->clean(); delete m_btnAccept; }
    if (m_btnDecline) { m_btnDecline->clean(); delete m_btnDecline; }
    if (m_lblAccept) { m_lblAccept->clean(); delete m_lblAccept; }
    if (m_lblDecline) { m_lblDecline->clean(); delete m_lblDecline; }

    // Search UI cleanup
    if (m_btnFindMatch) { m_btnFindMatch->clean(); delete m_btnFindMatch; }
    if (m_lblFindMatch) { m_lblFindMatch->clean(); delete m_lblFindMatch; }
    if (m_btnCancelSearch) { m_btnCancelSearch->clean(); delete m_btnCancelSearch; }
    if (m_lblSearchingTimer) { m_lblSearchingTimer->clean(); delete m_lblSearchingTimer; }

    // Header cleanup
    if (m_lblTitle) { m_lblTitle->clean(); delete m_lblTitle; m_lblTitle = nullptr; }

    // Replay button + popup cleanup
    if (m_btnViewProfile) { m_btnViewProfile->clean(); delete m_btnViewProfile; m_btnViewProfile = nullptr; }
    if (m_lblViewProfile) { m_lblViewProfile->clean(); delete m_lblViewProfile; m_lblViewProfile = nullptr; }
    if (m_btnReplay) { m_btnReplay->clean(); delete m_btnReplay; m_btnReplay = nullptr; }
    if (m_lblReplay) { m_lblReplay->clean(); delete m_lblReplay; m_lblReplay = nullptr; }
    if (m_lblReplayPopupTitle) { m_lblReplayPopupTitle->clean(); delete m_lblReplayPopupTitle; m_lblReplayPopupTitle = nullptr; }
    if (m_lblReplayPopupHint) { m_lblReplayPopupHint->clean(); delete m_lblReplayPopupHint; m_lblReplayPopupHint = nullptr; }
    if (m_inReplayDir) { m_inReplayDir->clean(); delete m_inReplayDir; m_inReplayDir = nullptr; }
    if (m_btnReplayStart) { m_btnReplayStart->clean(); delete m_btnReplayStart; m_btnReplayStart = nullptr; }
    if (m_lblReplayStart) { m_lblReplayStart->clean(); delete m_lblReplayStart; m_lblReplayStart = nullptr; }
    if (m_btnReplayCancel) { m_btnReplayCancel->clean(); delete m_btnReplayCancel; m_btnReplayCancel = nullptr; }
    if (m_lblReplayCancel) { m_lblReplayCancel->clean(); delete m_lblReplayCancel; m_lblReplayCancel = nullptr; }

    // Replay watch cleanup (if not already owned by m_uiObjects)
    // Note: these were added to m_uiObjects, so they are already deleted above.
    
    return true;
}
