#include "core/Game.hpp"
#include "scenes/SceneGame.hpp"

#include <cstdlib>
#include <string>

const int FPS = 60;
const int DELAY_TIME = 1000.0f / FPS;

int main(int argc, char* argv[]) {
    std::string ip = "127.0.0.1";
    int port = 9090;
    std::string mapPath = "assets/maps/flatmap.txt";

    if (argc >= 2) ip = argv[1];
    if (argc >= 3) port = std::atoi(argv[2]);
    if (argc >= 4) mapPath = argv[3];

    if (!Game::getInstance()->init("Gummy Network Client", 1280, 720)) {
        return -1;
    }

    Game::getInstance()->getStateMachine()->pushState(new SceneGame(ip, port, mapPath));

    Uint32 frameStart, frameTime;

    while (Game::getInstance()->running()) {
        frameStart = SDL_GetTicks();

        Game::getInstance()->handleEvents();
        Game::getInstance()->update();
        Game::getInstance()->render();

        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < DELAY_TIME) {
            SDL_Delay((int)(DELAY_TIME - frameTime));
        }
    }

    Game::getInstance()->clean();
    return 0;
}
