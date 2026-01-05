#include "SceneDashboard.hpp"
#include "SceneLogin.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../core/InputHandler.hpp"
#include <iostream>

SceneDashboard::SceneDashboard(std::string username) 
    : m_username(username), m_showPlayerMenu(false), 
      m_btnChallenge(nullptr), m_btnProfile(nullptr),
      m_lblChallenge(nullptr), m_lblProfile(nullptr)
{
    // Mock Data
    m_onlinePlayers = {"PlayerOne", "DragonSlayer", "GummyBear", "ProGamer99", "TestUser"};
    
    // Create Text objects for players
    int sidebarX = 1280 - 300 + 20;
    int startY = 100;
    int gap = 50;
    
    Text* lblOnlineTitle = new Text(sidebarX, 50, "assets/Arial.ttf", 24, "ONLINE PLAYERS", {0, 255, 0, 255});
    m_uiObjects.push_back(lblOnlineTitle);

    for (size_t i = 0; i < m_onlinePlayers.size(); i++) {
        Text* t = new Text(sidebarX, startY + (i * gap), "assets/Arial.ttf", 20, m_onlinePlayers[i], {200, 200, 200, 255});
        m_uiObjects.push_back(t);
    }
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

    // --- HEADER ---
    std::string welcomeMsg = "Welcome, " + m_username + "!";
    Text* lblWelcome = new Text(30, 30, "assets/Arial.ttf", 24, welcomeMsg, {255, 255, 255, 255});
    m_uiObjects.push_back(lblWelcome);

    // Logout Button
    int logoutW = 100;
    int logoutH = 40;
    Button* btnLogout = new Button(screenW - logoutW - 30, 30, logoutW, logoutH, "btn_generic", []() {
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
    Button* btnFindMatch = new Button(centerX - (playBtnW / 2), centerY - 40, playBtnW, playBtnH, "btn_generic", []() {
        std::cout << "[SceneDashboard] Finding Match..." << std::endl;
    }, 181, 73);
    m_uiObjects.push_back(btnFindMatch);

    Text* lblFindMatch = new Text(centerX - 80, centerY - 15, "assets/Arial.ttf", 28, "FIND MATCH", {0, 0, 0, 255});
    m_uiObjects.push_back(lblFindMatch);

    // --- POPUP MENU BUTTONS (Hidden initially, managed manually) ---
    // We create them but don't add to m_uiObjects to avoid auto-update/draw in the wrong order
    // We will update/draw them manually when menu is open
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

    return true;
}

void SceneDashboard::update() {
    // Update standard UI
    for (auto obj : m_uiObjects) {
        obj->update();
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
    int sidebarX = 1280 - 300;
    int startY = 100;
    int itemH = 50;

    if (mouse->x > sidebarX && mouse->y > startY) {
        int index = (mouse->y - startY) / itemH;
        if (index >= 0 && index < m_onlinePlayers.size()) {
            // Clicked on a player
            m_selectedPlayer = m_onlinePlayers[index];
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

void SceneDashboard::drawSidebar() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    int screenW = 1280;
    int screenH = 720;
    int sidebarW = 300;
    int sidebarX = screenW - sidebarW;

    // Draw Sidebar Background (Semi-transparent Black)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_Rect sidebarRect = {sidebarX, 0, sidebarW, screenH};
    SDL_RenderFillRect(renderer, &sidebarRect);

    // Draw Title
    // We can use a static Text object or create one on fly (inefficient)
    // For now, let's assume we added it to m_uiObjects or draw manually.
    // Let's just use a temporary Text for simplicity of this snippet, 
    // BUT creating textures every frame is BAD.
    // Ideally, these should be members. I'll skip the title text for now or use a member if I added it.
    
    // Draw Player List
    int startY = 100;
    int itemH = 50;
    
    // We need a font. We can reuse one or load one.
    // To avoid creating textures every frame, we should have created Text objects for each player in onEnter.
    // But the list is dynamic.
    // The Text class creates a texture in constructor.
    // For a dynamic list, we usually cache textures.
    // For this demo, I will iterate and draw text using a helper if available, 
    // OR I will just create Text objects in onEnter since the list is static mock data.
}

void SceneDashboard::drawPlayerMenu() {
    if (!m_showPlayerMenu) return;

    // Draw Menu Background
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect menuRect = {(int)m_menuPosition.x - 160, (int)m_menuPosition.y - 10, 160, 110};
    SDL_RenderFillRect(renderer, &menuRect);

    // Draw Buttons
    m_btnChallenge->draw();
    m_btnProfile->draw();
    
    // Draw Labels (We need to manually update their positions before drawing if they moved)
    // I updated positions in handlePlayerListClick.
    m_lblChallenge->setPosition(m_btnChallenge->getPosition().x + 30, m_btnChallenge->getPosition().y + 10);
    m_lblChallenge->draw();
    
    m_lblProfile->setPosition(m_btnProfile->getPosition().x + 25, m_btnProfile->getPosition().y + 10);
    m_lblProfile->draw();
}

void SceneDashboard::render() {
    // 1. Background
    TextureManager::getInstance()->drawStatic(m_bgTextureID, 0, 0, 1280, 720, Game::getInstance()->getRenderer());

    // 2. Main UI
    for (auto obj : m_uiObjects) {
        obj->draw();
    }

    // 3. Sidebar
    drawSidebar();
    
    // 3.1 Draw Player Names (Optimized: Create Text objects once, but here we hack for demo)
    // To do this properly without leaks in render loop:
    // I should have created a vector<Text*> m_playerTextObjects in onEnter.
    // Let's do a quick fix: I'll just draw them using a static helper if I had one.
    // Since I don't, I'll rely on the fact that I didn't implement the text drawing in drawSidebar yet.
    // Let's add the text objects to m_uiObjects in onEnter? 
    // No, they need to be in the sidebar.
    
    // REAL FIX: Render the text using a temporary surface/texture is slow but works for 5 items.
    // Better: Add them to a separate list in onEnter.
    
    int sidebarX = 1280 - 300 + 20;
    int startY = 100;
    // This is just a placeholder loop to show where they would be. 
    // In a real engine, we'd have a ListView component.
    // For this assignment, I will assume we added them to m_uiObjects but positioned them in the sidebar.
    
    // 4. Popup Menu
    drawPlayerMenu();
}

bool SceneDashboard::onExit() {
    for (auto obj : m_uiObjects) {
        obj->clean();
        delete obj;
    }
    m_uiObjects.clear();
    
    if (m_btnChallenge) { m_btnChallenge->clean(); delete m_btnChallenge; }
    if (m_btnProfile) { m_btnProfile->clean(); delete m_btnProfile; }
    if (m_lblChallenge) { m_lblChallenge->clean(); delete m_lblChallenge; }
    if (m_lblProfile) { m_lblProfile->clean(); delete m_lblProfile; }
    
    return true;
}
