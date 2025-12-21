#pragma once
#include "../core/GameState.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../core/InputHandler.hpp"

class SceneTest : public GameState {
public:
    std::string textureID = "bg_test";

    virtual bool onEnter() override {
        std::cout << "Entering Test Scene..." << std::endl;
        if (!TextureManager::getInstance()->load("assets/Mario.png", textureID, Game::getInstance()->getRenderer())) {
            std::cout << "Failed to load image!" << std::endl;
            return false;
        }
        return true;
    }

    virtual bool onExit() override {
        std::cout << "Exiting Test Scene..." << std::endl;
        TextureManager::getInstance()->clearFromTextureMap(textureID);
        return true;
    }

    virtual void update() override {
        if (InputHandler::getInstance()->isKeyDown(SDL_SCANCODE_ESCAPE)) {
            Game::getInstance()->quit();
        }
    }

    virtual void render() override {
        
        TextureManager::getInstance()->drawStatic(
            textureID, 
            0, 0, 
            800, 600,
            Game::getInstance()->getRenderer()
        );
    }

    virtual std::string getStateID() const override { return "TEST_SCENE"; }
};