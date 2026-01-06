#include "SceneLogin.hpp"
#include "SceneRegister.hpp"
#include "SceneDashboard.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../network/ClientSocket.hpp"
#include <iostream>

// ========================================================
// HELPER CLASSES (LOCAL)
// ========================================================

// A button that has no visual (transparent), used for hit-testing over text
class InvisibleButton : public Button {
public:
    InvisibleButton(float x, float y, int width, int height, Callback callback)
        : Button(x, y, width, height, "", callback) {} // Empty texture ID

    virtual void draw() override {
        // Do nothing (Invisible)
        // Optional: Draw a rect for debug
        // SDL_SetRenderDrawColor(Game::getInstance()->getRenderer(), 255, 0, 0, 255);
        // SDL_Rect r = {(int)m_position.x, (int)m_position.y, m_width, m_height};
        // SDL_RenderDrawRect(Game::getInstance()->getRenderer(), &r);
    }
};

// A composite UI Object that displays Underlined Text and handles Clicks
class RegisterLink : public UIObject {
public:
    RegisterLink(float x, float y, std::string fontPath, int fontSize, std::string textStr, Callback callback)
        : UIObject(x, y, 0, 0) 
    {
        // 1. Create Text (Blue color)
        m_text = new Text(x, y, fontPath, fontSize, textStr, {0, 0, 255, 255});
        
        // 2. Get Size from Text
        m_width = m_text->getWidth();
        m_height = m_text->getHeight();
        
        // 3. Create Invisible Button on top
        m_button = new InvisibleButton(x, y, m_width, m_height, callback);
    }

    virtual void load() override {} 

    virtual void draw() override {
        // Draw Text
        m_text->draw();

        // Draw Underline
        int lineY = (int)m_position.y + m_height; 
        TextureManager::getInstance()->drawLine(
            (int)m_position.x, lineY, 
            (int)m_position.x + m_width, lineY, 
            0, 0, 255, 255, 
            Game::getInstance()->getRenderer()
        );
        
        // Draw Button (Invisible)
        m_button->draw();
    }

    virtual void update() override {
        m_button->update();
    }

    virtual void clean() override {
        m_text->clean();
        delete m_text;
        m_button->clean();
        delete m_button;
    }

private:
    Text* m_text;
    InvisibleButton* m_button;
};

SceneLogin::SceneLogin(std::string message) : m_initMessage(message) {}

bool SceneLogin::onEnter() {
    std::cout << "[SceneLogin] Entering..." << std::endl;

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    // ---------------------------------------------------------
    // 1. SETUP RESOURCES
    // ---------------------------------------------------------
    m_bannerTextureID = "Mario";
    if (!TextureManager::getInstance()->load("assets/Mario.png", m_bannerTextureID, Game::getInstance()->getRenderer())) {
        std::cout << "[SceneLogin] Failed to load banner image!" << std::endl;
    }

    if (!TextureManager::getInstance()->load("assets/button.png", "btn_login", renderer)) {
         std::cout << "[SceneLogin] Failed to load Login Button!" << std::endl;
    }

    // ---------------------------------------------------------
    // 2. LAYOUT CONSTANTS (1280x720)
    // ---------------------------------------------------------
    // Left half center X = 640 / 2 = 320
    int centerX = 320; 
    int startY = 150;  // Starting Y position
    int gapY = 80;    // Vertical gap between elements
    int inputW = 300;  // Width of input boxes
    int inputH = 40;   // Height of input boxes
    int inputX = centerX - (inputW / 2); // Center inputs horizontally

    // ---------------------------------------------------------
    // 3. CREATE UI ELEMENTS (LEFT SIDE)
    // ---------------------------------------------------------

    // A. Title Text: "LOGIN"
    Text* lblTitle = new Text(centerX - 60, startY, "assets/Arial.ttf", 40, "LOGIN", {0, 0, 0, 255});
    m_uiObjects.push_back(lblTitle);

    // B. Username Input
    Text* lblUserHint = new Text(inputX, startY + gapY - 25, "assets/Arial.ttf", 18, "Username:", {100, 100, 100, 255});
    m_uiObjects.push_back(lblUserHint);

    m_inputUsername = new TextInput(inputX, startY + gapY, inputW, inputH, "assets/Arial.ttf", 20);
    m_uiObjects.push_back(m_inputUsername);

    // C. Password Input
    Text* lblPassHint = new Text(inputX, startY + (gapY * 2) - 25, "assets/Arial.ttf", 18, "Password:", {100, 100, 100, 255});
    m_uiObjects.push_back(lblPassHint);

    m_inputPassword = new TextInput(inputX, startY + (gapY * 2), inputW, inputH, "assets/Arial.ttf", 20);
    m_uiObjects.push_back(m_inputPassword);

    // Error/Status Label (Below Password, Above Login Button)
    int errorY = startY + (gapY * 2) + inputH + 10;
    m_lblError = new Text(centerX - 150, errorY, "assets/Arial.ttf", 16, "", {255, 0, 0, 255});
    if (!m_initMessage.empty()) {
        m_lblError->setText(m_initMessage);
        m_lblError->setColor({0, 128, 0, 255}); // Green for success
    }
    m_uiObjects.push_back(m_lblError);

    // D. Login Button (Centered)
    int btnY = startY + (gapY * 2) + inputH + 30;
    int btnWidth = 120;
    int btnHeight = 50;
    int btnX = centerX - (btnWidth / 2);

    // Source size: 543x73 total => 181x73 per frame
    int srcBtnW = 181;
    int srcBtnH = 73;

    Button* btnLogin = new Button(btnX, btnY, btnWidth, btnHeight, "btn_login", [this]() {
        std::cout << "[SceneLogin] Attempting Login..." << std::endl;
        std::string user = m_inputUsername->getString();
        std::string pass = m_inputPassword->getString();
        
        if (user.empty() || pass.empty()) {
            m_lblError->setText("Username/Password cannot be empty!");
            m_lblError->setColor({255, 0, 0, 255});
            return;
        }

        std::string result = Game::getInstance()->getClientSocket()->Login(user, pass);
        if (result == "Success") {
            std::cout << " > Login Successful!" << std::endl;
            Game::getInstance()->getStateMachine()->changeState(new SceneDashboard(user));
        } else {
            std::cout << " > Login Failed: " << result << std::endl;
            m_lblError->setText(result);
            m_lblError->setColor({255, 0, 0, 255});
        }

    }, srcBtnW, srcBtnH);
    m_uiObjects.push_back(btnLogin);

    // Login Text Label (Centered on button)
    Text* lblLoginBtn = new Text(btnX + 35, btnY + 12, "assets/Arial.ttf", 18, "Login", {0, 0, 0, 255});
    m_uiObjects.push_back(lblLoginBtn);

    // E. Register Link (Below Login Button)
    int regLinkX = centerX - 35; 
    int regLinkY = btnY + btnHeight + 20;

    RegisterLink* lnkRegister = new RegisterLink(regLinkX, regLinkY, "assets/Arial.ttf", 18, "Register", []() {
        std::cout << "[SceneLogin] Switch to Register Scene" << std::endl;
        Game::getInstance()->getStateMachine()->changeState(new SceneRegister());
    });
    m_uiObjects.push_back(lnkRegister);

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
    // The screen is 1280 wide. Right side starts at x=640.
    // We draw the banner filling the right half (640x720).
    TextureManager::getInstance()->drawStatic(
        m_bannerTextureID,
        640, 0,         // X, Y (Start at middle of screen)
        640, 720,      // Width, Height (Fill right half)
        Game::getInstance()->getRenderer()
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