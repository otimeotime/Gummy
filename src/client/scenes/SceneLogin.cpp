#include "SceneLogin.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include <iostream>

bool SceneLogin::onEnter() {
    std::cout << "[SceneLogin] Entering..." << std::endl;

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    // ---------------------------------------------------------
    // 1. SETUP RESOURCES
    // ---------------------------------------------------------
    // Load the banner image for the right side
    // Make sure you have "assets/Mario.png"
    m_bannerTextureID = "Mario";
    if (!TextureManager::getInstance()->load("assets/Mario.png", m_bannerTextureID, Game::getInstance()->getRenderer())) {
        std::cout << "[SceneLogin] Failed to load banner image!" << std::endl;
        // Proceeding anyway (it will just be blank on the right)
    }

    if (!TextureManager::getInstance()->load("assets/button.png", "btn_register", renderer)) {
        std::cout << "[SceneLogin] Failed to load Register Button!" << std::endl;
    }

    if (!TextureManager::getInstance()->load("assets/button.png", "btn_login", renderer)) {
         std::cout << "[SceneLogin] Failed to load Login Button!" << std::endl;
    }

    // ---------------------------------------------------------
    // 2. LAYOUT CONSTANTS (1920x1080)
    // ---------------------------------------------------------
    // Left half center X = 960 / 2 = 480
    int centerX = 480; 
    int startY = 200;  // Starting Y position
    int gapY = 100;    // Vertical gap between elements
    int inputW = 500;  // Width of input boxes
    int inputH = 60;   // Height of input boxes
    int inputX = centerX - (inputW / 2); // Center inputs horizontally

    // ---------------------------------------------------------
    // 3. CREATE UI ELEMENTS (LEFT SIDE)
    // ---------------------------------------------------------

    // A. Title Text: "DANG NHAP"
    // Centering text roughly involves guessing width or using font metrics. 
    // Here we place it slightly left of center to account for text width.
    Text* lblTitle = new Text(centerX - 100, startY, "assets/Arial.ttf", 48, "LOGIN", {0, 0, 0, 255});
    m_uiObjects.push_back(lblTitle);

    // B. Username Input
    Text* lblUserHint = new Text(inputX, startY + gapY - 30, "assets/Arial.ttf", 20, "Username:", {100, 100, 100, 255});
    m_uiObjects.push_back(lblUserHint);

    m_inputUsername = new TextInput(inputX, startY + gapY, inputW, inputH, "assets/Arial.ttf", 24);
    m_uiObjects.push_back(m_inputUsername);

    // C. Password Input
    Text* lblPassHint = new Text(inputX, startY + (gapY * 2) - 30, "assets/Arial.ttf", 20, "Password:", {100, 100, 100, 255});
    m_uiObjects.push_back(lblPassHint);

    m_inputPassword = new TextInput(inputX, startY + (gapY * 2), inputW, inputH, "assets/Arial.ttf", 24);
    m_uiObjects.push_back(m_inputPassword);

    // D. Buttons (Register & Login)
    // We will place them side-by-side below the password input
    int btnY = startY + (gapY * 3.5);
    int btnWidth = 181;
    int btnHeight = 73;
    int btnGap = 60; // Gap between buttons

    // Calculate X for buttons to be centered together
    int totalBtnWidth = (btnWidth * 2) + btnGap;
    int btnStartX = centerX - (totalBtnWidth / 2);

    // D1. Register Button (Left)
    Button* btnRegister = new Button(btnStartX, btnY, btnWidth, btnHeight, "btn_register", []() {
        std::cout << "[SceneLogin] Switch to Register Scene (TODO)" << std::endl;
        // Game::getInstance()->getStateMachine()->changeState(new SceneRegister());
    });
    m_uiObjects.push_back(btnRegister);

    // D2. Login Button (Right)
    Button* btnLogin = new Button(btnStartX + btnWidth + btnGap, btnY, btnWidth, btnHeight, "btn_login", [this]() {
        std::cout << "[SceneLogin] Attempting Login..." << std::endl;
        std::cout << " > User: " << m_inputUsername->getString() << std::endl;
        std::cout << " > Pass: " << m_inputPassword->getString() << std::endl;
        // Add validation logic here later
    });
    m_uiObjects.push_back(btnLogin);

    return true;
}

void SceneLogin::update() {
    // Update all UI objects (handle inputs, clicks, hover states)
    for (auto obj : m_uiObjects) {
        obj->update();
    }
}

void SceneLogin::render() {
    // ---------------------------------------------------------
    // 1. RENDER RIGHT SIDE IMAGE
    // ---------------------------------------------------------
    // The screen is 1920 wide. Right side starts at x=960.
    // We draw the banner filling the right half (960x1080).
    TextureManager::getInstance()->drawFrame(
        m_bannerTextureID,
        960, 0,         // X, Y (Start at middle of screen)
        960, 1080,      // Width, Height (Fill right half)
        1, 0,           // Row, Frame (Assuming it's a static image, use 1, 0)
        Game::getInstance()->getRenderer(),
        0, 255, SDL_FLIP_NONE
    );

    // ---------------------------------------------------------
    // 2. RENDER UI OBJECTS (LEFT SIDE)
    // ---------------------------------------------------------
    for (auto obj : m_uiObjects) {
        obj->draw();
    }
}

bool SceneLogin::onExit() {
    std::cout << "[SceneLogin] Exiting..." << std::endl;

    // Clean up all UI objects
    for (auto obj : m_uiObjects) {
        obj->clean();
        delete obj;
    }
    m_uiObjects.clear();
    
    // Reset pointers
    m_inputUsername = nullptr;
    m_inputPassword = nullptr;

    // Optional: Clear texture if you want to save RAM and won't use it soon
    TextureManager::getInstance()->clearFromTextureMap(m_bannerTextureID);

    return true;
}