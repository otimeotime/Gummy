#include "SceneRegister.hpp"
#include "SceneLogin.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../network/ClientSocket.hpp"
#include <iostream>

// Helper class for Text Link (Duplicated from SceneLogin for now, should be refactored later)
class TextLink : public UIObject {
public:
    TextLink(float x, float y, std::string fontPath, int fontSize, std::string textStr, Callback callback)
        : UIObject(x, y, 0, 0) 
    {
        m_text = new Text(x, y, fontPath, fontSize, textStr, {0, 0, 255, 255});
        m_width = m_text->getWidth();
        m_height = m_text->getHeight();
        
        // Invisible button for click handling
        // Using a non-existent texture ID makes it invisible but clickable
        m_button = new Button(x, y, m_width, m_height, "invisible_btn", callback);
    }

    virtual void load() override {} 

    virtual void draw() override {
        m_text->draw();
        
        // Draw Underline
        int lineY = (int)m_position.y + m_height; 
        TextureManager::getInstance()->drawLine(
            (int)m_position.x, lineY, 
            (int)m_position.x + m_width, lineY, 
            0, 0, 255, 255, 
            Game::getInstance()->getRenderer()
        );
        
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
    Button* m_button;
};

bool SceneRegister::onEnter() {
    std::cout << "[SceneRegister] Entering..." << std::endl;

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    
    // 1. Resources
    m_bannerTextureID = "Mario"; 
    // Assuming Mario is already loaded or we load it again (TextureManager handles duplicates if implemented well, 
    // but here we just call load. If it's already there, it might reload or just work. 
    // TextureManager::load doesn't check existence, it overwrites. 
    // Ideally we check, but for now let's just load to be safe or assume SceneLogin loaded it.)
    if (!TextureManager::getInstance()->load("assets/Mario.png", m_bannerTextureID, renderer)) {
        std::cout << "[SceneRegister] Failed to load banner!" << std::endl;
    }
    
    if (!TextureManager::getInstance()->load("assets/button.png", "btn_register", renderer)) {
         std::cout << "[SceneRegister] Failed to load Register Button!" << std::endl;
    }

    // 2. Layout (1280x720)
    int centerX = 320; 
    int startY = 100;  // Higher start Y because we have 3 inputs
    int gapY = 80;    
    int inputW = 300;  
    int inputH = 40;   
    int inputX = centerX - (inputW / 2); 

    // 3. UI Elements

    // Title
    Text* lblTitle = new Text(centerX - 90, startY, "assets/font.ttf", 40, "REGISTER", {0, 0, 0, 255});
    m_uiObjects.push_back(lblTitle);

    // Username
    Text* lblUserHint = new Text(inputX, startY + gapY - 25, "assets/font.ttf", 18, "Username:", {100, 100, 100, 255});
    m_uiObjects.push_back(lblUserHint);

    m_inputUsername = new TextInput(inputX, startY + gapY, inputW, inputH, "assets/font.ttf", 20);
    m_uiObjects.push_back(m_inputUsername);

    // Password
    Text* lblPassHint = new Text(inputX, startY + (gapY * 2) - 25, "assets/font.ttf", 18, "Password:", {100, 100, 100, 255});
    m_uiObjects.push_back(lblPassHint);

    m_inputPassword = new TextInput(inputX, startY + (gapY * 2), inputW, inputH, "assets/font.ttf", 20);
    m_uiObjects.push_back(m_inputPassword);

    // Confirm Password
    Text* lblConfirmHint = new Text(inputX, startY + (gapY * 3) - 25, "assets/font.ttf", 18, "Confirm Password:", {100, 100, 100, 255});
    m_uiObjects.push_back(lblConfirmHint);

    m_inputConfirmPassword = new TextInput(inputX, startY + (gapY * 3), inputW, inputH, "assets/font.ttf", 20);
    m_uiObjects.push_back(m_inputConfirmPassword);

    // Error Label
    m_lblError = new Text(inputX, startY + (gapY * 3) + inputH + 5, "assets/font.ttf", 14, "", {255, 0, 0, 255});
    m_uiObjects.push_back(m_lblError);

    // Register Button
    int btnY = startY + (gapY * 3) + inputH + 30;
    int btnWidth = 120;
    int btnHeight = 50;
    int btnX = centerX - (btnWidth / 2);
    
    int srcBtnW = 181;
    int srcBtnH = 73;

    // Reusing "btn_register" texture if available, or "btn_login" if we want generic button.
    // SceneLogin loaded "btn_register" but didn't use it.
    Button* btnRegister = new Button(btnX, btnY, btnWidth, btnHeight, "btn_register", [this]() {
        std::cout << "[SceneRegister] Attempting Register..." << std::endl;
        std::string user = m_inputUsername->getString();
        std::string pass = m_inputPassword->getString();
        std::string confirm = m_inputConfirmPassword->getString();

        if (user.empty() || pass.empty()) {
            m_lblError->setText("Username/Password cannot be empty!");
            m_lblError->setColor({255, 0, 0, 255});
            return;
        }

        if (pass != confirm) {
            m_lblError->setText("Passwords do not match!");
            m_lblError->setColor({255, 0, 0, 255});
            return;
        }
        
        std::string result = Game::getInstance()->getClientSocket()->Register(user, pass);
        if (result == "Success") {
            std::cout << " > Registration Successful! Please Login." << std::endl;
            Game::getInstance()->getStateMachine()->changeState(new SceneLogin("Registration Successful! Please Login."));
        } else {
            std::cout << " > Registration Failed: " << result << std::endl;
            m_lblError->setText(result);
            m_lblError->setColor({255, 0, 0, 255});
        }
    }, srcBtnW, srcBtnH);
    m_uiObjects.push_back(btnRegister);

    // Register Text on Button
    Text* lblRegBtn = new Text(btnX + 25, btnY + 12, "assets/font.ttf", 18, "Register", {255, 255, 255, 255});
    btnRegister->centerObject(lblRegBtn);
    m_uiObjects.push_back(lblRegBtn);

    // Back to Login Link
    int linkX = centerX - 50;
    int linkY = btnY + btnHeight + 20;
    
    TextLink* lnkLogin = new TextLink(linkX, linkY, "assets/font.ttf", 18, "Back to Login", []() {
        Game::getInstance()->getStateMachine()->changeState(new SceneLogin());
    });
    m_uiObjects.push_back(lnkLogin);

    return true;
}

void SceneRegister::update() {
    for (auto obj : m_uiObjects) {
        obj->update();
    }
}

void SceneRegister::render() {
    // Right side banner
    TextureManager::getInstance()->drawStatic(
        m_bannerTextureID,
        640, 0,         
        640, 720,      
        Game::getInstance()->getRenderer()
    );

    for (auto obj : m_uiObjects) {
        obj->draw();
    }
}

bool SceneRegister::onExit() {
    std::cout << "[SceneRegister] Exiting..." << std::endl;
    for (auto obj : m_uiObjects) {
        obj->clean();
        delete obj;
    }
    m_uiObjects.clear();
    return true;
}
