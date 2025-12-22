#pragma once
#include "../core/GameState.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../ui/Button.hpp"
#include <iostream>

class SceneTest : public GameState {
public:
    // Pointer to our button
    Button* m_btnQuit = nullptr;
    std::string btnTextureID = "btn_quit";

    virtual bool onEnter() override {
        std::cout << "[SceneTest] Entering..." << std::endl;
        
        // 1. Load the Button Texture (Sprite Sheet)
        // IMPORTANT: The image must have 3 frames (Normal, Hover, Clicked)
        // Ensure you have "assets/button.png" in your project folder
        if (!TextureManager::getInstance()->load("assets/button.png", btnTextureID, Game::getInstance()->getRenderer())) {
            std::cout << "[SceneTest] Failed to load button texture!" << std::endl;
            return false;
        }

        // 2. Create the Button
        // Position: (300, 200), Size: 200x50
        // Callback: Using a Lambda function to Quit the game when clicked
        m_btnQuit = new Button(
            300, 200, 181, 73, 
            btnTextureID, 
            []() {
                std::cout << "[SceneTest] Button Clicked! Quitting Game..." << std::endl;
                Game::getInstance()->quit();
            }
        );

        return true;
    }

    virtual void update() override {
        // 3. Update Button Logic (Check for mouse hover/click)
        if (m_btnQuit) {
            m_btnQuit->update();
        }
    }

    virtual void render() override {
        // 4. Draw the Button
        if (m_btnQuit) {
            m_btnQuit->draw();
        }
    }

    virtual bool onExit() override {
        std::cout << "[SceneTest] Exiting..." << std::endl;

        // 5. CLEANUP (Very Important)
        if (m_btnQuit) {
            delete m_btnQuit;
            m_btnQuit = nullptr;
        }

        // Remove texture from RAM
        TextureManager::getInstance()->clearFromTextureMap(btnTextureID);
        return true;
    }

    virtual std::string getStateID() const override { return "TEST_SCENE"; }
};