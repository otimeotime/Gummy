#include "SceneDashboard.hpp"
#include "SceneLogin.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../core/InputHandler.hpp"
#include <iostream>

SceneDashboard::SceneDashboard(std::string username) 
    : m_username(username), m_showPlayerMenu(false), 
      m_btnChallenge(nullptr), m_btnProfile(nullptr),
      m_lblChallenge(nullptr), m_lblProfile(nullptr),
      m_lblWelcome(nullptr),
      m_isSearching(false), m_searchStartTime(0),
      m_btnFindMatch(nullptr), m_lblFindMatch(nullptr),
      m_btnCancelSearch(nullptr), m_lblSearchingTimer(nullptr),
      m_showMatchPopup(false), m_hasMatchDecision(false), 
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
    int screenW = 1280;
    int screenH = 720;
    int sidebarW = 300;

    // --- PLAYER LIST ---
    std::cout << "[SceneDashboard] Fetching user list..." << std::endl;
    m_allPlayers = Game::getInstance()->getClientSocket()->GetUserList();

    int sidebarX = 1280 - 300 + 20;
    
    // Sidebar Title
    Text* lblOnlineTitle = new Text(sidebarX, 50, "assets/Arial.ttf", 24, "PLAYERS", {255, 255, 0, 255});
    m_uiObjects.push_back(lblOnlineTitle);

    // --- HEADER ---
    // Initialize Welcome Label (Dynamic update later)
    m_lblWelcome = new Text(30, 30, "assets/Arial.ttf", 24, "Welcome, " + m_username + "!", {255, 255, 255, 255});
    m_uiObjects.push_back(m_lblWelcome);

    // Logout Button
    int logoutW = 100;
    int logoutH = 40;
    Button* btnLogout = new Button(screenW - logoutW - 30, 30, logoutW, logoutH, "btn_generic", []() {
        Game::getInstance()->getClientSocket()->Logout(); // Send Logout Packet
        Game::getInstance()->getStateMachine()->changeState(new SceneLogin());
    }, 181, 73);
    m_uiObjects.push_back(btnLogout);

    Text* lblLogout = new Text(screenW - logoutW - 30 + 20, 30 + 10, "assets/Arial.ttf", 16, "Logout", {0, 0, 0, 255});
    m_uiObjects.push_back(lblLogout);

    // --- MAIN CONTENT (Left Side) ---
    int mainAreaW = screenW - sidebarW;
    int centerX = mainAreaW / 2;
    int centerY = screenH / 2;

    // "FIND MATCH" Button
    int playBtnW = 250;
    int playBtnH = 80;
    m_btnFindMatch = new Button(centerX - (playBtnW / 2), centerY - 40, playBtnW, playBtnH, "btn_generic", [this]() {
        std::cout << "[SceneDashboard] Sending Find Match Request..." << std::endl;
        if(Game::getInstance()->getClientSocket()->SendFindMatch()) {
             std::cout << " > Request Sent. Waiting for server confirmation..." << std::endl;
             // Do NOT start timer yet. Wait for RES_MATCH_FIND.
        } else {
             std::cout << " > Request Failed (Send Error)." << std::endl;
        }
    }, 181, 73);
    // m_uiObjects.push_back(m_btnFindMatch); // Managed manually

    m_lblFindMatch = new Text(centerX - 80, centerY - 15, "assets/Arial.ttf", 28, "FIND MATCH", {0, 0, 0, 255});
    // m_uiObjects.push_back(m_lblFindMatch); // Managed manually

    // Cancel Search Button (Same position as Find Match)
    m_btnCancelSearch = new Button(centerX - (playBtnW / 2), centerY + 60, playBtnW, 50, "btn_generic", [this]() {
         std::cout << "[SceneDashboard] Cancelling Search..." << std::endl;
         Game::getInstance()->getClientSocket()->SendCancelMatch();
         m_isSearching = false;
    }, 181, 73);

    m_lblSearchingTimer = new Text(centerX - 100, centerY - 20, "assets/Arial.ttf", 24, "Searching... 00:00", {255, 255, 0, 255});

    // --- POPUP MENU BUTTONS (Hidden initially) ---
    // ... (Existing code) ...

    // --- MATCHMAKING POPUP ---
    m_lblMatchFound = new Text( centerX - 100, centerY - 80, "assets/Arial.ttf", 32, "MATCH FOUND!", {255, 0, 0, 255});
    m_lblMatchStatus = new Text( centerX - 100, centerY, "assets/Arial.ttf", 20, "", {0, 255, 0, 255});

    m_lblAccept = new Text(0, 0, "assets/Arial.ttf", 20, "Accept", {0,0,0,255});
    m_lblDecline = new Text(0, 0, "assets/Arial.ttf", 20, "Decline", {0,0,0,255});
    
    m_btnAccept = new Button(centerX - 110, centerY, 100, 40, "btn_generic", [this]() {
         std::cout << "Accepted Match!" << std::endl;
         Game::getInstance()->getClientSocket()->SendMatchDecision(true);
         m_hasMatchDecision = true;
         m_lblMatchStatus->setText("Waiting for opponent...");
         m_lblMatchStatus->setColor({255, 255, 0, 255});
    }, 181, 73);
    
    m_btnDecline = new Button(centerX + 10, centerY, 100, 40, "btn_generic", [this]() {
         std::cout << "Declined Match!" << std::endl;
         Game::getInstance()->getClientSocket()->SendMatchDecision(false);
         // m_showMatchPopup = false; // Maybe wait for server? Or just close?
         // If declined, server cancels match for both.
         m_hasMatchDecision = true;
         m_lblMatchStatus->setText("You declined.");
         m_lblMatchStatus->setColor({255, 0, 0, 255});
         // Close after short delay? Or immediately?
         m_showMatchPopup = false;
    }, 181, 73);

    m_btnChallenge = new Button(0, 0, 140, 40, "btn_generic", [this]() {
        std::cout << "[SceneDashboard] Challenge sent to " << m_selectedPlayer << std::endl;
        m_showPlayerMenu = false;
    }, 181, 73);

    m_lblChallenge = new Text(0, 0, "assets/Arial.ttf", 16, "Challenge", {0,0,0,255});

    m_btnProfile = new Button(0, 0, 140, 40, "btn_generic", [this]() {
        std::cout << "[SceneDashboard] View Profile of " << m_selectedPlayer << std::endl;
        m_showPlayerMenu = false;
    }, 181, 73);

    m_lblProfile = new Text(0, 0, "assets/Arial.ttf", 16, "View Profile", {0,0,0,255});

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
    int sidebarX = 1280 - 300 + 20;
    int startY = 100;
    int gap = 50;

    int drawCount = 0;
    std::string myEloPart = "";

    for (size_t i = 0; i < m_allPlayers.size(); i++) {
        // Filter Self from the list
        if (std::string(m_allPlayers[i].username) == m_username) {
            myEloPart = " (ELO: " + std::to_string(m_allPlayers[i].elo) + ")";
            continue;
        }

        SDL_Color color;
        if (m_allPlayers[i].isOnline) {
            color = {0, 255, 0, 255}; // Green
        } else {
            color = {128, 128, 128, 255}; // Gray
        }
        
        std::string entry = std::string(m_allPlayers[i].username) + " (" + std::to_string(m_allPlayers[i].elo) + ")";
        
        Text* t = new Text(sidebarX, startY + (drawCount * gap), "assets/Arial.ttf", 20, entry, color);
        m_playerListTexts.push_back(t);
        drawCount++;
    }

    // Update Welcome Header safely
    if (m_lblWelcome) {
        m_lblWelcome->setText("Welcome, " + m_username + "!" + myEloPart);
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
            m_isSearching = false;
            m_showMatchPopup = true;
            m_hasMatchDecision = false;
        }
        else if (packet.header.type == PacketType::RES_MATCH_DECIDE_2) {
             std::cout << "[Dashboard] Match Confirmed! Starting Game..." << std::endl;
             // TODO: Transition to SceneGame
             // ResMatchDecide2 has playerOrder info
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

    // Detect "Just Pressed"
    bool justPressed = InputHandler::getInstance()->getMouseButtonClicked(0);

    // Update Menu Buttons if visible
    if (m_showPlayerMenu) {
        m_btnChallenge->update();
        m_btnProfile->update();
        
        // Close menu if clicked outside
        if (justPressed) {
            Vector2D* mouse = InputHandler::getInstance()->getMousePosition();
            if (!isMouseInsideMenu(mouse)) {
                m_showPlayerMenu = false;
            }
        }
    } else {
        if (justPressed) {
            handlePlayerListClick();
        }
    }
}

bool SceneDashboard::isMouseInsideMenu(Vector2D* mousePos) {
    // Menu Rect defined in drawPlayerMenu: x-160, y-10, w=160, h=110
    int menuX = (int)m_menuPosition.x - 160;
    int menuY = (int)m_menuPosition.y - 10;
    int menuW = 160;
    int menuH = 110;

    return (mousePos->x >= menuX && mousePos->x <= menuX + menuW &&
            mousePos->y >= menuY && mousePos->y <= menuY + menuH);
}

void SceneDashboard::handlePlayerListClick() {
    // Simple manual hit detection for the list
    // List starts at x = 1280 - 300 = 980
    // y starts at 100
    // item height = 50
    
    // Note: We already checked justPressed in update(), so we assume this is a valid click event.

    Vector2D* mouse = InputHandler::getInstance()->getMousePosition();
    int sidebarX = 1280 - 300 + 20;
    int startY = 100;
    int itemH = 50;

    if (mouse->x > sidebarX && mouse->y > startY) {
        int index = (mouse->y - startY) / itemH;
        
        // We need to map this index to the FILTERED list (m_playerListTexts corresponds to it visually)
        // m_playerListTexts size is the count of displayed users.
        if (index >= 0 && index < (int)m_playerListTexts.size()) {
             // Retrieve the actual username from the Text object string? No, Text object has "Name (ELO)".
             // We need to know which user it is.
             // Best way: Reconstruct the filtering logic or store a parallel vector of "DisplayedUsers".
             
             // Quick fix: Iterate m_allPlayers just like we did in refreshPlayerList
             int currentVisIndex = 0;
             std::string selectedUser = "";
             
             for (const auto& p : m_allPlayers) {
                 if (std::string(p.username) == m_username) continue;
                 
                 if (currentVisIndex == index) {
                     selectedUser = p.username;
                     break;
                 }
                 currentVisIndex++;
             }

             if (!selectedUser.empty()) {
                m_selectedPlayer = selectedUser;
                m_showPlayerMenu = true;
                m_menuPosition = *mouse;
                
                // Position buttons near mouse
                m_btnChallenge->setPosition(m_menuPosition.x - 150, m_menuPosition.y);
                m_lblChallenge->setPosition(m_menuPosition.x - 150 + 30, m_menuPosition.y + 10); 
                
                m_btnProfile->setPosition(m_menuPosition.x - 150, m_menuPosition.y + 50);
                m_lblProfile->setPosition(m_menuPosition.x - 150 + 25, m_menuPosition.y + 60);
                
                std::cout << "Selected: " << m_selectedPlayer << std::endl;
             }
        }
    }
}

void SceneDashboard::drawPlayerMenu() {
    if (!m_showPlayerMenu) return;

    // Draw Menu Background
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    
    // Use TextureManager for drawing primitives too
    TextureManager::getInstance()->drawFillRect(
        (int)m_menuPosition.x - 160, 
        (int)m_menuPosition.y - 10, 
        160, 110, 
        50, 50, 50, 255, 
        renderer
    );

    // Draw Buttons
    m_btnChallenge->draw();
    m_btnProfile->draw();
    
    // Draw Labels
    m_lblChallenge->setPosition(m_btnChallenge->getPosition().x + 30, m_btnChallenge->getPosition().y + 10);
    m_lblChallenge->draw();
    
    m_lblProfile->setPosition(m_btnProfile->getPosition().x + 25, m_btnProfile->getPosition().y + 10);
    m_lblProfile->draw();
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
        int acceptX = (int)m_btnAccept->getPosition().x + 20;
        int acceptY = (int)m_btnAccept->getPosition().y + 10;
        m_lblAccept->setPosition(acceptX, acceptY);
        m_lblAccept->draw();

        int declineX = (int)m_btnDecline->getPosition().x + 15;
        int declineY = (int)m_btnDecline->getPosition().y + 10;
        m_lblDecline->setPosition(declineX, declineY);
        m_lblDecline->draw();
     } else {
        m_lblMatchStatus->draw();
     }
}

void SceneDashboard::drawSidebar() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    int screenW = 1280;
    int screenH = 720;
    int sidebarW = 300;
    int sidebarX = screenW - sidebarW;

    // Draw Sidebar Background (Semi-transparent Black) via TextureManager
    TextureManager::getInstance()->drawFillRect(sidebarX, 0, sidebarW, screenH, 0, 0, 0, 150, renderer);
    
    // Draw Player List (Dynamic)
    for (auto t : m_playerListTexts) {
        t->draw();
    }
    
    // 4. Popup Menu
    drawPlayerMenu();
}

void SceneDashboard::render() {
    // 1. Background
    TextureManager::getInstance()->drawStatic(m_bgTextureID, 0, 0, 1280, 720, Game::getInstance()->getRenderer());

    // 2. Main UI
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
         Text lblCancel(m_btnCancelSearch->getPosition().x + 90, m_btnCancelSearch->getPosition().y + 15, "assets/Arial.ttf", 20, "CANCEL", {255,0,0,255});
         lblCancel.draw();
    } else {
         m_btnFindMatch->draw();
         m_lblFindMatch->draw();
    }

    // 3. Sidebar (which calls drawPlayerMenu)
    drawSidebar();
    
    // 4. Match Popup
    drawMatchPopup();
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

    if (m_btnChallenge) { m_btnChallenge->clean(); delete m_btnChallenge; }
    if (m_btnProfile) { m_btnProfile->clean(); delete m_btnProfile; }
    if (m_lblChallenge) { m_lblChallenge->clean(); delete m_lblChallenge; }
    if (m_lblProfile) { m_lblProfile->clean(); delete m_lblProfile; }
    
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
    
    return true;
}
