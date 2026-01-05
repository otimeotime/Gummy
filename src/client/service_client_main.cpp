#include "core/Game.hpp"
#include "scenes/SceneLogin.hpp"
#include "network/ClientSocket.hpp"
#include <cstdlib>
#include <string>
#include <iostream>

const int FPS = 60;
const int DELAY_TIME = 1000.0f / FPS;

int main(int argc, char* argv[]) {
    // Initialize the game window
    if (!Game::getInstance()->init("Gummy Service Client", 1280, 720)) {
        return -1;
    }

    // Connect to Service Server
    if (!Game::getInstance()->getClientSocket()->Connect("127.0.0.1", 8080)) {
        std::cerr << "[Error] Failed to connect to Service Server (127.0.0.1:8080)" << std::endl;
    } else {
        std::cout << "[Info] Connected to Service Server." << std::endl;
    }

    // Start with the Login Scene
    Game::getInstance()->getStateMachine()->pushState(new SceneLogin());

    Uint32 frameStart, frameTime;

    // Main Game Loop
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
