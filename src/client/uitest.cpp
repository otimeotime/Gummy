#include "core/Game.hpp"

#include "scenes/SceneTest.hpp" //Test purpose

const int FPS = 60;
const int DELAY_TIME = 1000.0f / FPS;

int main(int argc, char* argv[]) {

    // 1. Initialize the Game Engine
    if (!Game::getInstance()->init("Gummy Game Client", 800, 600)) {
        return -1;
    }

    // 2. Load the initial State
    Game::getInstance()->getStateMachine()->pushState(new SceneTest());

    // 3. The Main Game Loop
    Uint32 frameStart, frameTime;

    while (Game::getInstance()->running()) {
        frameStart = SDL_GetTicks();

        Game::getInstance()->handleEvents();
        Game::getInstance()->update();
        Game::getInstance()->render();

        // Frame rate capping
        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < DELAY_TIME) {
            SDL_Delay((int)(DELAY_TIME - frameTime));
        }
    }

    // 4. Cleanup
    Game::getInstance()->clean();

    return 0;
}