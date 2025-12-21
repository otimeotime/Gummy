#include "../client/core/Game.hpp"
#include "../client/scenes/SceneGame.hpp" // Change this import

const int FPS = 60;
const int DELAY_TIME = 1000.0f / FPS;

int main(int argc, char* argv[]) {
    if (!Game::getInstance()->init("Gummy Game Client", 1280, 720)) {
        return -1;
    }

    Game::getInstance()->getStateMachine()->pushState(new SceneGame()); 

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