#pragma once

#include "../core/GameState.hpp"
#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"
#include "../core/InputHandler.hpp"

#include "../../common/network/TCPSocket.hpp"
#include "../../common/network/PacketUtils.hpp"
#include "../../common/network/PacketStructs.hpp"

#include "../../ingame_server/logic/MapLoader.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <SDL2/SDL_ttf.h>

class SceneGameNet : public GameState {
public:
    SceneGameNet(std::string serverIp = "127.0.0.1", int serverPort = 9090);

    bool onEnter() override;
    bool onExit() override;
    void update() override;
    void render() override;

    std::string getStateID() const override { return "SCENE_GAME_NET"; }

private:
    void ReceiverLoop();
    void SendInput(uint32_t command, float value = 0.0f);

    void createMapTexture();
    void updateMapTexture();

    std::string m_serverIp;
    int m_serverPort;

    std::atomic<bool> m_running;
    std::thread m_receiverThread;

    TCPSocket* m_socket;

    uint32_t m_matchId;
    uint32_t m_playerId;
    uint32_t m_seq;

    std::mutex m_stateMutex;
    ResIngameState m_lastState;
    bool m_hasState;

    std::string m_bgTextureID;
    std::string m_playerID;
    std::string m_bulletID;

    std::string m_mapPath;
    MapLoader* m_mapLoader;
    SDL_Texture* m_mapTexture;
    bool m_mapModified;

    TTF_Font* m_font;

    Uint32 m_lastTick;
};
