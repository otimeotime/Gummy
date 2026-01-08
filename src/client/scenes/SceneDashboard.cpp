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
    // Will be populated by refreshPlayerList() at end of onEnter

    // --- LEFT TITLE ---
    m_lblTitle = new Text(80, 60, "assets/font.ttf", 72, "GUMMY", {255, 255, 255, 255});
    m_uiObjects.push_back(m_lblTitle);

    // --- RIGHT PANEL TITLE ---
    Text* lblOnlineTitle = new Text(0, 0, "assets/font.ttf", 22, "PLAYERS ONLINE", {255, 255, 255, 255});
    lblOnlineTitle->setPosition((float)(panelX + (panelW - lblOnlineTitle->getWidth()) / 2), (float)(panelY + 22));
    m_uiObjects.push_back(lblOnlineTitle);

    // --- MAIN CONTENT (Left Side) ---
    const int leftX = 80;
    const int actionBtnW = 280;
    const int actionBtnH = 64;
    const int actionGap = 16;
            
    // "FIND MATCH" Button
    const int playBtnY = 220;
    m_btnFindMatch = new Button(leftX, playBtnY, actionBtnW, actionBtnH, "btn_generic", [this]() {
        std::cout << "[SceneDashboard] Sending Find Match Request..." << std::endl;
        if(Game::getInstance()->getClientSocket()->SendFindMatch()) {
             std::cout << " > Request Sent. Waiting for server confirmation..." << std::endl;
        } else {
             std::cout << " > Request Failed (Send Error)." << std::endl;
        }
    }, 181, 73);

    m_lblFindMatch = new Text(0, 0, "assets/font.ttf", 26, "FIND MATCH", {255, 255, 255, 255});
    m_btnFindMatch->centerObject(m_lblFindMatch);

    // Cancel Search Button
    m_btnCancelSearch = new Button(leftX, playBtnY, actionBtnW, actionBtnH, "btn_generic", [this]() {
         std::cout << "[SceneDashboard] Cancelling Search..." << std::endl;
         Game::getInstance()->getClientSocket()->SendCancelMatch();
         m_isSearching = false;
    }, 181, 73);

    m_lblSearchingTimer = new Text(leftX, playBtnY - 46, "assets/font.ttf", 22, "Searching... 00:00", {255, 255, 255, 255});

    // VIEW PROFILE button
    const int viewProfileBtnY = playBtnY + actionBtnH + actionGap;
    m_btnViewProfile = new Button(leftX, viewProfileBtnY, actionBtnW, actionBtnH, "btn_generic", [this]() {
        Game::getInstance()->getClientSocket()->SendGetProfile();
        Game::getInstance()->getStateMachine()->pushState(new SceneViewProfile(m_username));
    }, 181, 73);
    m_lblViewProfile = new Text(0, 0, "assets/font.ttf", 22, "VIEW PROFILE", {255, 255, 255, 255});
    m_btnViewProfile->centerObject(m_lblViewProfile);
    m_uiObjects.push_back(m_btnViewProfile);
    m_uiObjects.push_back(m_lblViewProfile);

    // REPLAY button
    const int replayBtnY = viewProfileBtnY + actionBtnH + actionGap;
    m_btnReplay = new Button(leftX, replayBtnY, actionBtnW, actionBtnH, "btn_generic", [this]() {
        m_showReplayPopup = true;
    }, 181, 73);
    m_lblReplay = new Text(0, 0, "assets/font.ttf", 22, "REPLAY", {255, 255, 255, 255});
    m_btnReplay->centerObject(m_lblReplay);
    m_uiObjects.push_back(m_btnReplay);
    m_uiObjects.push_back(m_lblReplay);

    // LOGOUT button
    const int logoutBtnY = replayBtnY + actionBtnH + actionGap;
    Button* btnLogout = new Button(leftX, logoutBtnY, actionBtnW, actionBtnH, "btn_generic", []() {
        Game::getInstance()->getClientSocket()->Logout();
        Game::getInstance()->getStateMachine()->changeState(new SceneLogin());
    }, 181, 73);
    Text* lblLogout = new Text(0, 0, "assets/font.ttf", 22, "LOGOUT", {255, 255, 255, 255});
    btnLogout->centerObject(lblLogout);
    m_uiObjects.push_back(btnLogout);
    m_uiObjects.push_back(lblLogout);

    // QUIT button
    const int quitBtnY = logoutBtnY + actionBtnH + actionGap;
    Button* btnQuit = new Button(leftX, quitBtnY, actionBtnW, actionBtnH, "btn_generic", []() {
        Game::getInstance()->quit();
    }, 181, 73);
    Text* lblQuit = new Text(0, 0, "assets/font.ttf", 22, "QUIT", {255, 255, 255, 255});
    btnQuit->centerObject(lblQuit);
    m_uiObjects.push_back(btnQuit);
    m_uiObjects.push_back(lblQuit);

    // --- REPLAY WATCH POPUP ---
    m_lblReplayPopupTitle = new Text(0, 0, "assets/font.ttf", 28, "REPLAY", {255, 255, 255, 255});
    m_lblReplayPopupHint = new Text(0, 0, "assets/font.ttf", 16, "Directory / file", {200, 200, 200, 255});
    m_inReplayDir = new TextInput(0, 0, 560, 40, "assets/font.ttf", 18);
    m_btnReplayStart = new Button(0, 0, 140, 44, "btn_generic", [this]() {
        if (!m_inReplayDir) return;
        const std::string replayPath = m_inReplayDir->getString();
        if (replayPath.empty()) return;
        LaunchReplayViewerProcess(replayPath, m_username);
        m_showReplayPopup = false;
    }, 181, 73);
    m_lblReplayStart = new Text(0, 0, "assets/font.ttf", 18, "OPEN", {255, 255, 255, 255});
    m_btnReplayCancel = new Button(0, 0, 140, 44, "btn_generic", [this]() {
        m_showReplayPopup = false;
    }, 181, 73);
    m_lblReplayCancel = new Text(0, 0, "assets/font.ttf", 18, "CANCEL", {255, 255, 255, 255});

    // --- MATCHMAKING POPUP ---
    const int popupCX = (screenW - panelW) / 2;
    const int popupCY = screenH / 2;
    m_lblMatchFound = new Text( popupCX - 120, popupCY - 120, "assets/font.ttf", 32, "MATCH FOUND!", {255, 255, 255, 255});
    m_lblMatchStatus = new Text( popupCX - 160, popupCY - 40, "assets/font.ttf", 20, "", {255, 255, 255, 255});
    m_lblAccept = new Text(0, 0, "assets/font.ttf", 20, "Accept", {255, 255, 255, 255});
    m_lblDecline = new Text(0, 0, "assets/font.ttf", 20, "Decline", {255, 255, 255, 255});
    
    m_btnAccept = new Button(popupCX - 120, popupCY + 20, 140, 48, "btn_generic", [this]() {
        Game::getInstance()->getClientSocket()->SendMatchDecision(true, m_pendingMatchId);
        m_hasMatchDecision = true;
        m_lblMatchStatus->setText("Waiting for opponent...");
        m_lblMatchStatus->setColor({255, 255, 0, 255});
    }, 181, 73);
    
    m_btnDecline = new Button(popupCX + 10, popupCY + 20, 140, 48, "btn_generic", [this]() {
        Game::getInstance()->getClientSocket()->SendMatchDecision(false, m_pendingMatchId);
        m_hasMatchDecision = true;
        m_lblMatchStatus->setText("You declined.");
        m_lblMatchStatus->setColor({255, 0, 0, 255});
        m_showMatchPopup = false;
    }, 181, 73);

    // --- INTERACTION POPUP (Player Click) ---
    // m_lblChallenge Removed
    m_lblViewOtherProfile = new Text(0, 0, "assets/font.ttf", 20, "Profile", {255,255,255,255});
    m_lblCloseInteract = new Text(0, 0, "assets/font.ttf", 18, "Close", {200,200,200,255});
    
    // m_btnChallenge Removed
    
    m_btnViewOtherProfile = new Button(0, 0, 300, 60, "btn_generic", [this]() {
        std::cout << "[Dashboard] Viewing profile of " << m_targetPlayerName << std::endl;
        Game::getInstance()->getClientSocket()->SendGetProfile(m_targetPlayerName);
        Game::getInstance()->getStateMachine()->pushState(new SceneViewProfile(m_targetPlayerName));
        m_showInteractPopup = false;
    }, 181, 73);

    m_btnCloseInteract = new Button(0, 0, 140, 40, "btn_generic", [this]() {
        m_showInteractPopup = false;
    }, 181, 73);

    m_lblChallenge = new Text(0, 0, "assets/font.ttf", 20, "Challenge", {255,255,255,255});
    m_btnChallenge = new Button(0, 0, 300, 60, "btn_generic", [this]() {
         std::cout << "[Dashboard] Challenging " << m_targetPlayerName << std::endl;
         Game::getInstance()->getClientSocket()->SendChallengeUser(m_targetPlayerName);
         
         m_showInteractPopup = false;
         m_showWaitingResponse = true; // Show waiting popup
         m_lblWaitingResponse->setText("Waiting for " + m_targetPlayerName + "...");
         m_lblCancelWaiting->setText("Cancel");
    }, 181, 73);

    // --- INCOMING CHALLENGE POPUP ---
    m_lblIncomingTitle = new Text(0,0, "assets/font.ttf", 28, "CHALLENGE!", {255,50,50,255});
    m_lblIncomingMsg = new Text(0,0, "assets/font.ttf", 18, "Msg", {255,255,255,255});
    m_lblIncomingAccept = new Text(0,0, "assets/font.ttf", 20, "Accept", {255,255,255,255});
    m_lblIncomingDecline = new Text(0,0, "assets/font.ttf", 20, "Decline", {255,255,255,255});
    
    m_btnIncomingAccept = new Button(0,0, 140, 48, "btn_generic", [this]() {
         // Send Accept
         Game::getInstance()->getClientSocket()->SendChallengeResponse(true, m_incomingChallengerName);
         m_showIncomingChallenge = false;
         // Show waiting final popup
         m_showWaitingFinal = true;
         m_waitingFinalCanClose = false; 
         m_lblWaitingFinal->setText("Waiting for host to start...");
         m_waitingFinalStartTime = SDL_GetTicks(); // Start timer
    }, 181, 73);

    m_btnIncomingDecline = new Button(0,0, 140, 48, "btn_generic", [this]() {
         // Send Decline
         Game::getInstance()->getClientSocket()->SendChallengeResponse(false, m_incomingChallengerName);
         m_showIncomingChallenge = false;
    }, 181, 73);

    // --- FINAL CONFIRM POPUP ---
    m_lblFinalTitle = new Text(0,0, "assets/font.ttf", 28, "MATCH READY!", {50,255,50,255});
    m_lblFinalMsg = new Text(0,0, "assets/font.ttf", 18, "Msg", {255,255,255,255});
    m_lblFinalYes = new Text(0,0, "assets/font.ttf", 20, "Start!", {255,255,255,255});
    m_lblFinalNo = new Text(0,0, "assets/font.ttf", 20, "Cancel", {255,255,255,255});

    m_btnFinalYes = new Button(0,0, 140, 48, "btn_generic", [this]() {
         // Send Final Confirm = Yes
         Game::getInstance()->getClientSocket()->SendChallengeFinalConfirm(true, m_finalOpponentName);
         // Wait for INIT_GAME (will be handled in packet loop)
         m_lblFinalMsg->setText("Starting...");
    }, 181, 73);

    m_btnFinalNo = new Button(0,0, 140, 48, "btn_generic", [this]() {
         // Send Final Confirm = No
         Game::getInstance()->getClientSocket()->SendChallengeFinalConfirm(false, m_finalOpponentName);
         m_showFinalConfirm = false;
    }, 181, 73);

    // --- WAITING RESPONSE POPUP ---
    m_lblWaitingResponse = new Text(0,0, "assets/font.ttf", 20, "Waiting for response...", {255,255,255,255});
    m_lblCancelWaiting = new Text(0,0, "assets/font.ttf", 18, "Cancel", {255,255,255,255});
    m_btnCancelWaiting = new Button(0,0, 140, 48, "btn_generic", [this]() {            
        // Just close the popup. 
        // If response comes later, it will likely be ignored or show Final Popup which is fine.
        m_showWaitingResponse = false;
    }, 181, 73); 

    // --- WAITING FINAL POPUP (Player B) ---
    m_lblWaitingFinal = new Text(0,0, "assets/font.ttf", 20, "Waiting for host...", {255,255,255,255});
    m_lblCloseWaitingFinal = new Text(0,0,"assets/font.ttf", 18, "Close", {255,255,255,255});
    m_btnCloseWaitingFinal = new Button(0,0, 140, 48, "btn_generic", [this]() {
         m_showWaitingFinal = false;
    }, 181, 73);

    // Initial Refresh
    refreshPlayerList();
    m_lastRefreshTime = SDL_GetTicks();

    return true;
}

void SceneDashboard::refreshPlayerList() {
    // 1. Clear old UI
    for (auto& entry : m_playerListUI) {
        if (entry.label) { entry.label->clean(); delete entry.label; }
        if (entry.clickArea) { entry.clickArea->clean(); delete entry.clickArea; }
    }
    m_playerListUI.clear();

    // 2. Fetch new list
    m_allPlayers = Game::getInstance()->getClientSocket()->GetUserList();

    // 3. Rebuild texts
    const int screenW = 1280;
    const int outerMargin = 40;
    const int panelW = 360;
    const int panelX = screenW - outerMargin - panelW;
    const int startY = 110;
    const int gap = 44; // Increased gap for buttons

    int drawCount = 0;

    for (size_t i = 0; i < m_allPlayers.size(); i++) {
        // Only show other ONLINE users
        if (std::string(m_allPlayers[i].username) == m_username) continue;
        if (!m_allPlayers[i].isOnline) continue;

        SDL_Color color = {200, 200, 200, 255};
        
        std::string entryName = m_allPlayers[i].username;
        std::string displayStr = entryName + " (" + std::to_string(m_allPlayers[i].elo) + ")";
        
        // Create Button for the entire row
        int y = startY + (drawCount * gap);
        // Invisible button over the text Area
        Button* btn = new Button(panelX + 10, y, panelW - 20, 40, "btn_generic", [this, entryName]() {
            // Open Interaction Popup
            std::cout << "Clicked on player: " << entryName << std::endl;
            m_targetPlayerName = entryName;
            m_showInteractPopup = true;
            
        }, 181, 73); 
        // Note: using btn_generic but maybe we want a transparent one?  
        // For now, let's assume we render text on top.

        Text* t = new Text(0, 0, "assets/font.ttf", 18, displayStr, color);
        // Center text in button
        btn->centerObject(t);

        PlayerEntry pe;
        pe.label = t;
        pe.clickArea = btn;
        m_playerListUI.push_back(pe);
        
        drawCount++;
    }
}

void SceneDashboard::update() {
    // Check Matchmaking & Challenge Notifications
    Packet packet;
    if (Game::getInstance()->getClientSocket()->CheckNotifications(packet)) {
        if (packet.header.type == PacketType::RES_MATCH_FIND) {
             std::cout << "[Dashboard] Search Pending Confirmed by Server." << std::endl;
             m_isSearching = true;
             m_searchStartTime = SDL_GetTicks();
             InputHandler::getInstance()->reset();
        }
        else if (packet.header.type == PacketType::REQ_MATCH_DECIDE_1) {
            std::cout << "[Dashboard] Match Found! Displaying popup." << std::endl;
            ReqMatchDecide1 req = packet.GetPayload<ReqMatchDecide1>();
            m_pendingMatchId = req.matchId;
            m_isSearching = false;
            m_showMatchPopup = true;
            m_hasMatchDecision = false;
            InputHandler::getInstance()->reset();
        }
        else if (packet.header.type == PacketType::RES_MATCH_DECIDE_2) {
             std::cout << "[Dashboard] Match Confirmed! Waiting for INIT_GAME..." << std::endl;
        }
        // --- CHALLENGE SYSTEM HANDLERS ---
        else if (packet.header.type == PacketType::REQ_CHALLENGE_REQUEST) {
            ReqChallengeRequest req = packet.GetPayload<ReqChallengeRequest>();
            std::cout << "[Dashboard] Incoming Challenge from " << req.challengerUsername << std::endl;
            m_incomingChallengerName = req.challengerUsername;
            m_showIncomingChallenge = true;
            m_showInteractPopup = false; 
        }
        else if (packet.header.type == PacketType::REQ_CHALLENGE_FINAL_CONFIRM) {
            ReqChallengeFinalConfirm req = packet.GetPayload<ReqChallengeFinalConfirm>();
            std::cout << "[Dashboard] Final Confirm from " << req.opponentUsername << std::endl;
            
            // Fix Issue: Check if we are still waiting (User might have cancelled)
            if (!m_showWaitingResponse) {
                std::cout << "[Dashboard] Received Final Confirm but not waiting (Cancelled?). Auto-Declining." << std::endl;
                // Auto-decline to notify server/opponent
                Game::getInstance()->getClientSocket()->SendChallengeFinalConfirm(false, req.opponentUsername);
                return;
            }

            m_finalOpponentName = req.opponentUsername;
            m_showWaitingResponse = false; // Stop waiting
            m_showFinalConfirm = true;
            m_showInteractPopup = false;
        }
        else if (packet.header.type == PacketType::RES_CHALLENGE_DECLINED) {
             ResChallengeDeclined res = packet.GetPayload<ResChallengeDeclined>();
             std::cout << "[Dashboard] Challenge Declined: " << res.reason << std::endl;
             
             // Update waiting popup if visible (User A)
             if (m_showWaitingResponse) {
                m_lblWaitingResponse->setText(std::string("Declined: ") + res.reason);
                m_lblCancelWaiting->setText("Close");
             } 
             // Update waiting final popup if visible (User B)
             else if (m_showWaitingFinal) {
                 m_lblWaitingFinal->setText(std::string("Error: ") + res.reason);
                 m_waitingFinalCanClose = true;
             }
             else {
                 m_showFinalConfirm = false;
             }
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

                  // Run gameplay
                  // NOTE: allowSpectator=false to prevent accidental connection to Replay Server (zombie port)
                  Game::getInstance()->getStateMachine()->pushState(new SceneGame(host, port, mapPath, m_username, init.matchId, myUserId, false));
              
              // Close all popups & Reset Flags
              m_showMatchPopup = false;
              m_hasMatchDecision = false;
              m_showInteractPopup = false;
              m_showWaitingResponse = false;
              m_showIncomingChallenge = false;
              m_showFinalConfirm = false;
              m_showWaitingFinal = false; // Reset for B
              m_showReplayPopup = false;
              return;
        }
        else if (packet.header.type == PacketType::RES_MATCH_CANCEL) {
             m_isSearching = false;
             m_showMatchPopup = false; 
             m_hasMatchDecision = false;
             std::cout << "[Dashboard] Match cancelled." << std::endl;
        }
    }

    // Modal Popups Checking (Block interactions below if visible)

    if (m_showWaitingResponse) {
        if (m_btnCancelWaiting) m_btnCancelWaiting->update();
        return;
    }

    if (m_showWaitingFinal) {
        // Check timeout (10 seconds)
        if (!m_waitingFinalCanClose && SDL_GetTicks() - m_waitingFinalStartTime > 10000) {
             std::cout << "[Dashboard] Waiting for host timed out. Cancelling..." << std::endl;
             // Send cancellation to server.
             // We use SendChallengeResponse(false) to trigger Decline logic on server side
             // This notifies Challenger (A) that we declined/cancelled.
             Game::getInstance()->getClientSocket()->SendChallengeResponse(false, m_incomingChallengerName);
             
             m_lblWaitingFinal->setText("Error: Host timed out.");
             m_waitingFinalCanClose = true;
        }

        if (m_waitingFinalCanClose && m_btnCloseWaitingFinal) {
             m_btnCloseWaitingFinal->update();
        }
        return;
    }

    if (m_showInteractPopup) {
        if (m_btnViewOtherProfile) m_btnViewOtherProfile->update();
        if (m_btnChallenge) m_btnChallenge->update();
        if (m_btnCloseInteract) m_btnCloseInteract->update();
        return;
    }

    if (m_showIncomingChallenge) {
        if (m_btnIncomingAccept) m_btnIncomingAccept->update();
        if (m_btnIncomingDecline) m_btnIncomingDecline->update();
        return;
    }

    if (m_showFinalConfirm) {
        if (m_btnFinalYes) m_btnFinalYes->update();
        if (m_btnFinalNo) m_btnFinalNo->update();
        return;
    }

    if (m_showMatchPopup) {
         if (!m_hasMatchDecision) {
            m_btnAccept->update();
            m_btnDecline->update();
         }
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
    
    // Update dynamic player list (Buttons + Labels)
    for (auto& entry : m_playerListUI) {
        if (entry.clickArea) entry.clickArea->update();
        // Labels usually don't need update unless animating, but good practice
        // if (entry.label) entry.label->update(); 
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
    for (auto& entry : m_playerListUI) {
        if (entry.label) entry.label->draw();
        // Don't need to draw invisible buttons unless debugging
        // if (entry.clickArea) entry.clickArea->draw(); 
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
         Text lblCancel(0, 0, "assets/font.ttf", 20, "CANCEL", {255,0,0,255});
         m_btnCancelSearch->centerObject(&lblCancel);
         lblCancel.draw();
    } else {
         m_btnFindMatch->draw();
         m_btnFindMatch->centerObject(m_lblFindMatch);
         m_lblFindMatch->draw();
    }

    // 4. Modal Popups (Render order matters: latest on top)
    drawMatchPopup();
    drawInteractionPopup();
    drawIncomingChallengePopup();
    drawWaitingResponsePopup();
    drawFinalConfirmPopup();
    drawWaitingFinalPopup();
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

// showStatusMessage and drawStatusPopup Removed

void SceneDashboard::drawInteractionPopup() {
    if (!m_showInteractPopup) return;
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    
    // Dim background
    TextureManager::getInstance()->drawFillRect(0, 0, w, h, 0, 0, 0, 150, renderer);

    // Popup Box
    int boxW = 400;
    int boxH = 380; // Increased height
    int x = (w - boxW) / 2;
    int y = (h - boxH) / 2;

    TextureManager::getInstance()->drawFillRect(x, y, boxW, boxH, 30, 30, 30, 255, renderer);
    TextureManager::getInstance()->drawFillRect(x, y, boxW, 2, 100, 200, 255, 255, renderer);

    // Title: Player Name
    Text title(0, 0, "assets/font.ttf", 32, m_targetPlayerName, {255, 255, 255, 255});
    title.setPosition(x + (boxW - title.getWidth())/2, y + 30);
    title.draw();

    // Challenge Button
    if (m_btnChallenge) {
        m_btnChallenge->setPosition(x + 50, y + 100);
        m_btnChallenge->draw();
        m_btnChallenge->centerObject(m_lblChallenge);
        m_lblChallenge->draw();
    }

    // View Profile Button
    if (m_btnViewOtherProfile) {
        m_btnViewOtherProfile->setPosition(x + 50, y + 100 + 70);
        m_btnViewOtherProfile->draw();
        m_btnViewOtherProfile->centerObject(m_lblViewOtherProfile);
        m_lblViewOtherProfile->draw();
    }
    
    // Close Button (Small 'X' or button at bottom)
    if (m_btnCloseInteract) {
        m_btnCloseInteract->setPosition(x + 130, y + 100 + 70 + 80);
        m_btnCloseInteract->draw();
        m_btnCloseInteract->centerObject(m_lblCloseInteract);
        m_lblCloseInteract->draw();
    }
}

void SceneDashboard::drawIncomingChallengePopup() {
    if (!m_showIncomingChallenge) return;
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    TextureManager::getInstance()->drawFillRect(0, 0, w, h, 0, 0, 0, 180, renderer);

    int boxW = 500, boxH = 300;
    int x = (w - boxW)/2, y = (h - boxH)/2;
    TextureManager::getInstance()->drawFillRect(x, y, boxW, boxH, 40, 20, 20, 255, renderer);
    TextureManager::getInstance()->drawFillRect(x, y, boxW, 2, 255, 100, 100, 255, renderer);

    if (m_lblIncomingTitle) {
        m_lblIncomingTitle->setPosition((float)(x + (boxW - m_lblIncomingTitle->getWidth())/2), (float)(y + 30));
        m_lblIncomingTitle->draw();
    }

    if (m_lblIncomingMsg) {
        std::string msg = "Player " + m_incomingChallengerName + " challenges you!";
        m_lblIncomingMsg->setText(msg);
        m_lblIncomingMsg->setPosition((float)(x + (boxW - m_lblIncomingMsg->getWidth())/2), (float)(y + 80));
        m_lblIncomingMsg->draw();
    }

    if (m_btnIncomingAccept) {
        m_btnIncomingAccept->setPosition((float)(x + 60), (float)(y + 180));
        m_btnIncomingAccept->draw();
        m_btnIncomingAccept->centerObject(m_lblIncomingAccept);
        m_lblIncomingAccept->draw();
    }
    
    if (m_btnIncomingDecline) {
        m_btnIncomingDecline->setPosition((float)(x + 300), (float)(y + 180));
        m_btnIncomingDecline->draw();
        m_btnIncomingDecline->centerObject(m_lblIncomingDecline);
        m_lblIncomingDecline->draw();
    }
}

void SceneDashboard::drawWaitingResponsePopup() {
    if (!m_showWaitingResponse) return;
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    TextureManager::getInstance()->drawFillRect(0, 0, w, h, 0, 0, 0, 180, renderer);

    int boxW = 500, boxH = 200; // Smaller box
    int x = (w - boxW)/2, y = (h - boxH)/2;
    TextureManager::getInstance()->drawFillRect(x, y, boxW, boxH, 20, 20, 40, 255, renderer);
    TextureManager::getInstance()->drawFillRect(x, y, boxW, 2, 100, 100, 255, 255, renderer);

    if (m_lblWaitingResponse) {
        // Center text roughly
        m_lblWaitingResponse->setPosition((float)(x + (boxW - m_lblWaitingResponse->getWidth())/2), (float)(y + 50));
        m_lblWaitingResponse->draw();
    }

    if (m_btnCancelWaiting) {
        m_btnCancelWaiting->setPosition((float)(x + (boxW - 140)/2), (float)(y + 120));
        m_btnCancelWaiting->draw();
        m_btnCancelWaiting->centerObject(m_lblCancelWaiting);
        m_lblCancelWaiting->draw();
    }
}

void SceneDashboard::drawWaitingFinalPopup() {
    if (!m_showWaitingFinal) return;
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    TextureManager::getInstance()->drawFillRect(0, 0, w, h, 0, 0, 0, 180, renderer);

    int boxW = 500, boxH = 200;
    int x = (w - boxW)/2, y = (h - boxH)/2;
    TextureManager::getInstance()->drawFillRect(x, y, boxW, boxH, 20, 20, 40, 255, renderer);
    TextureManager::getInstance()->drawFillRect(x, y, boxW, 2, 100, 100, 255, 255, renderer);

    if (m_lblWaitingFinal) {
        // Center text roughly
        m_lblWaitingFinal->setPosition((float)(x + (boxW - m_lblWaitingFinal->getWidth())/2), (float)(y + 50));
        m_lblWaitingFinal->draw();
    }

    if (m_waitingFinalCanClose && m_btnCloseWaitingFinal) {
        m_btnCloseWaitingFinal->setPosition((float)(x + (boxW - 140)/2), (float)(y + 120));
        m_btnCloseWaitingFinal->draw();
        m_btnCloseWaitingFinal->centerObject(m_lblCloseWaitingFinal);
        m_lblCloseWaitingFinal->draw();
    }
}

void SceneDashboard::drawFinalConfirmPopup() {
    if (!m_showFinalConfirm) return;
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    TextureManager::getInstance()->drawFillRect(0, 0, w, h, 0, 0, 0, 180, renderer);

    int boxW = 500, boxH = 300;
    int x = (w - boxW)/2, y = (h - boxH)/2;
    TextureManager::getInstance()->drawFillRect(x, y, boxW, boxH, 20, 40, 20, 255, renderer);
    TextureManager::getInstance()->drawFillRect(x, y, boxW, 2, 100, 255, 100, 255, renderer);

    if (m_lblFinalTitle) {
        m_lblFinalTitle->setPosition((float)(x + (boxW - m_lblFinalTitle->getWidth())/2), (float)(y + 30));
        m_lblFinalTitle->draw();
    }

    if (m_lblFinalMsg) {
        std::string msg = m_finalOpponentName + " accepted! Start game?";
        m_lblFinalMsg->setText(msg);
        m_lblFinalMsg->setPosition((float)(x + (boxW - m_lblFinalMsg->getWidth())/2), (float)(y + 80));
        m_lblFinalMsg->draw();
    }

    if (m_btnFinalYes) {
        m_btnFinalYes->setPosition((float)(x + 60), (float)(y + 180));
        m_btnFinalYes->draw();
        m_btnFinalYes->centerObject(m_lblFinalYes);
        m_lblFinalYes->draw();
    }

    if (m_btnFinalNo) {
        m_btnFinalNo->setPosition((float)(x + 300), (float)(y + 180));
        m_btnFinalNo->draw();
        m_btnFinalNo->centerObject(m_lblFinalNo);
        m_lblFinalNo->draw();
    }
}

bool SceneDashboard::onExit() {
    // 1. Clean objects managed by m_uiObjects
    for (auto obj : m_uiObjects) {
        obj->clean();
        delete obj;
    }
    m_uiObjects.clear();
    
    // Nullify pointers that were in m_uiObjects to avoid accidental use
    m_lblTitle = nullptr;
    m_btnViewProfile = nullptr; 
    m_lblViewProfile = nullptr;
    m_btnReplay = nullptr; 
    m_lblReplay = nullptr;
    
    // 2. Clean dynamic player list
    for (auto& entry : m_playerListUI) {
        if (entry.label) { entry.label->clean(); delete entry.label; }
        if (entry.clickArea) { entry.clickArea->clean(); delete entry.clickArea; }
    }
    m_playerListUI.clear();

    // 3. Clean Manually Managed Objects (NOT in m_uiObjects)

    // Match Popup cleanup
    if (m_lblMatchFound) { m_lblMatchFound->clean(); delete m_lblMatchFound; m_lblMatchFound=nullptr; }
    if (m_lblMatchStatus) { m_lblMatchStatus->clean(); delete m_lblMatchStatus; m_lblMatchStatus=nullptr; }
    if (m_btnAccept) { m_btnAccept->clean(); delete m_btnAccept; m_btnAccept=nullptr; }
    if (m_btnDecline) { m_btnDecline->clean(); delete m_btnDecline; m_btnDecline=nullptr; }
    if (m_lblAccept) { m_lblAccept->clean(); delete m_lblAccept; m_lblAccept=nullptr; }
    if (m_lblDecline) { m_lblDecline->clean(); delete m_lblDecline; m_lblDecline=nullptr; }

    // Search UI cleanup
    if (m_btnFindMatch) { m_btnFindMatch->clean(); delete m_btnFindMatch; m_btnFindMatch=nullptr; }
    if (m_lblFindMatch) { m_lblFindMatch->clean(); delete m_lblFindMatch; m_lblFindMatch=nullptr; }
    if (m_btnCancelSearch) { m_btnCancelSearch->clean(); delete m_btnCancelSearch; m_btnCancelSearch=nullptr; }
    if (m_lblSearchingTimer) { m_lblSearchingTimer->clean(); delete m_lblSearchingTimer; m_lblSearchingTimer=nullptr; }

    // Replay Popup (These were NOT added to m_uiObjects in onEnter, check?)
    // In onEnter: m_lblReplayPopupTitle = new Text...
    // They are NOT pushed to m_uiObjects. So we must delete them here.
    if (m_lblReplayPopupTitle) { m_lblReplayPopupTitle->clean(); delete m_lblReplayPopupTitle; m_lblReplayPopupTitle = nullptr; }
    if (m_lblReplayPopupHint) { m_lblReplayPopupHint->clean(); delete m_lblReplayPopupHint; m_lblReplayPopupHint = nullptr; }
    if (m_inReplayDir) { m_inReplayDir->clean(); delete m_inReplayDir; m_inReplayDir = nullptr; }
    if (m_btnReplayStart) { m_btnReplayStart->clean(); delete m_btnReplayStart; m_btnReplayStart = nullptr; }
    if (m_lblReplayStart) { m_lblReplayStart->clean(); delete m_lblReplayStart; m_lblReplayStart = nullptr; }
    if (m_btnReplayCancel) { m_btnReplayCancel->clean(); delete m_btnReplayCancel; m_btnReplayCancel = nullptr; }
    if (m_lblReplayCancel) { m_lblReplayCancel->clean(); delete m_lblReplayCancel; m_lblReplayCancel = nullptr; }

    // Interaction Popup
    if (m_lblViewOtherProfile) { m_lblViewOtherProfile->clean(); delete m_lblViewOtherProfile; m_lblViewOtherProfile=nullptr; }
    if (m_btnViewOtherProfile) { m_btnViewOtherProfile->clean(); delete m_btnViewOtherProfile; m_btnViewOtherProfile=nullptr; }
    if (m_lblCloseInteract) { m_lblCloseInteract->clean(); delete m_lblCloseInteract; m_lblCloseInteract=nullptr; }
    if (m_btnCloseInteract) { m_btnCloseInteract->clean(); delete m_btnCloseInteract; m_btnCloseInteract=nullptr; }

    // Challenge Popups Cleanup
    if (m_lblChallenge) { m_lblChallenge->clean(); delete m_lblChallenge; m_lblChallenge=nullptr; }
    if (m_btnChallenge) { m_btnChallenge->clean(); delete m_btnChallenge; m_btnChallenge=nullptr; }
    
    // Incoming
    if (m_lblIncomingTitle) { m_lblIncomingTitle->clean(); delete m_lblIncomingTitle; m_lblIncomingTitle=nullptr; }
    if (m_lblIncomingMsg) { m_lblIncomingMsg->clean(); delete m_lblIncomingMsg; m_lblIncomingMsg=nullptr; }
    if (m_btnIncomingAccept) { m_btnIncomingAccept->clean(); delete m_btnIncomingAccept; m_btnIncomingAccept=nullptr; }
    if (m_lblIncomingAccept) { m_lblIncomingAccept->clean(); delete m_lblIncomingAccept; m_lblIncomingAccept=nullptr; }
    if (m_btnIncomingDecline) { m_btnIncomingDecline->clean(); delete m_btnIncomingDecline; m_btnIncomingDecline=nullptr; }
    if (m_lblIncomingDecline) { m_lblIncomingDecline->clean(); delete m_lblIncomingDecline; m_lblIncomingDecline=nullptr; }

    // Final
    if (m_lblFinalTitle) { m_lblFinalTitle->clean(); delete m_lblFinalTitle; m_lblFinalTitle=nullptr; }
    if (m_lblFinalMsg) { m_lblFinalMsg->clean(); delete m_lblFinalMsg; m_lblFinalMsg=nullptr; }
    if (m_btnFinalYes) { m_btnFinalYes->clean(); delete m_btnFinalYes; m_btnFinalYes=nullptr; }
    if (m_lblFinalYes) { m_lblFinalYes->clean(); delete m_lblFinalYes; m_lblFinalYes=nullptr; }
    if (m_btnFinalNo) { m_btnFinalNo->clean(); delete m_btnFinalNo; m_btnFinalNo=nullptr; }
    if (m_lblFinalNo) { m_lblFinalNo->clean(); delete m_lblFinalNo; m_lblFinalNo=nullptr; }

    // Waiting
    if (m_lblWaitingResponse) { m_lblWaitingResponse->clean(); delete m_lblWaitingResponse; m_lblWaitingResponse=nullptr; }
    if (m_btnCancelWaiting) { m_btnCancelWaiting->clean(); delete m_btnCancelWaiting; m_btnCancelWaiting=nullptr; }
    if (m_lblCancelWaiting) { m_lblCancelWaiting->clean(); delete m_lblCancelWaiting; m_lblCancelWaiting=nullptr; }

    // Waiting Final (B)
    if (m_lblWaitingFinal) { m_lblWaitingFinal->clean(); delete m_lblWaitingFinal; m_lblWaitingFinal=nullptr; }
    if (m_btnCloseWaitingFinal) { m_btnCloseWaitingFinal->clean(); delete m_btnCloseWaitingFinal; m_btnCloseWaitingFinal=nullptr; }
    if (m_lblCloseWaitingFinal) { m_lblCloseWaitingFinal->clean(); delete m_lblCloseWaitingFinal; m_lblCloseWaitingFinal=nullptr; }

    return true;
}