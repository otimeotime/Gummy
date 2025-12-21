#pragma once
#include "../core/GameState.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../core/InputHandler.hpp"
#include "../../ingame_server/logic/MapLoader.hpp"
#include <string>

class SceneGame : public GameState {
public:
    virtual bool onEnter() override;
    virtual bool onExit() override;
    virtual void update() override;
    virtual void render() override;

    virtual std::string getStateID() const override { return "SCENE_GAME"; }

private:
    std::string m_textureID;
    int m_playerX;
    int m_playerY;
    void createMapTexture();
    MapLoader* m_mapLoader;
    SDL_Texture* m_mapTexture;
    std::string m_bgTextureID;
};