#include "core/Game.hpp"
#include "scenes/SceneGameNet.hpp"

#include <cstdlib>
#include <string>

const int FPS = 60;
const int DELAY_TIME = 1000.0f / FPS;

int main(int argc, char* argv[]) {
    std::string ip = "127.0.0.1";
    int port = 9090;

    if (argc >= 2) ip = argv[1];
    if (argc >= 3) port = std::atoi(argv[2]);

    if (!Game::getInstance()->init("Gummy Network Client", 1280, 720)) {
        return -1;
    }

    Game::getInstance()->getStateMachine()->pushState(new SceneGameNet(ip, port));

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
