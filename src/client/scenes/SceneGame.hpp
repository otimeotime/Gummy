#pragma once
#include "../core/GameState.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../core/InputHandler.hpp"
#include "../../ingame_server/logic/MapLoader.hpp"
#include "../../ingame_server/logic/Player.hpp"
#include "../../ingame_server/logic/PhysicsEngine.hpp"
#include <string>
#include <SDL2/SDL_ttf.h>

class SceneGame : public GameState {
public:
    SceneGame(std::string mapToLoad = "assets/maps/flatmap.txt");
    virtual bool onEnter() override;
    virtual bool onExit() override;
    virtual void update() override;
    virtual void render() override;

    virtual std::string getStateID() const override { return "SCENE_GAME"; }

private:
    std::string m_mapToLoad;
    std::string m_bgTextureID;
    std::string m_playerID;
    std::string m_bulletID;
    int m_playerX;
    int m_playerY;
    bool m_startOrient;
    bool m_mapModified;
    void createMapTexture();
    void updateMapTexture();
    void renderHealthBar(Position playerPos);
    MapLoader* m_mapLoader;
    SDL_Texture* m_mapTexture;
    PhysicsEngine* m_physics;
    Player* m_player;
    std::vector<Player*> m_players;
    std::vector<Projectile> m_projectiles;
    TTF_Font* m_font;
};