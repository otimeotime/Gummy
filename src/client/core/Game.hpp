#pragma once
#include "Window.hpp"
#include "StateMachine.hpp"
#include "InputHandler.hpp"
#include "TextureManager.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Game {
public:
    // Singleton Accessor
    static Game* getInstance() {
        static Game instance;
        return &instance;
    }

    // Initialize Window, Renderer, and Subsystems
    bool init(const char* title, int width, int height);

    // Core Game Loop functions
    void render();
    void update();
    void handleEvents();
    void clean();

    // Check if the game loop should continue
    bool running() { return m_bRunning; }
    
    // Stop the game loop
    void quit() { m_bRunning = false; }

    // Accessors
    StateMachine* getStateMachine() { return m_pStateMachine; }
    SDL_Renderer* getRenderer() const { return Window::renderer; }

private:
    Game();
    ~Game() {}

    // Disable copy constructor
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    Window* m_pWindow;
    StateMachine* m_pStateMachine;
    bool m_bRunning;
};