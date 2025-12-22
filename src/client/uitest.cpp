#include "core/Game.hpp"
#include "scenes/SceneLogin.hpp" // Include the test scene

const int FPS = 60;
const int DELAY_TIME = 1000.0f / FPS;

int main(int argc, char* argv[]) {
    // 1. Init
    if (!Game::getInstance()->init("Gummy UI Test", 1920, 1080)) {
        return -1;
    }

    // 2. Push the Test Scene
    Game::getInstance()->getStateMachine()->pushState(new SceneLogin());

    // 3. Game Loop
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