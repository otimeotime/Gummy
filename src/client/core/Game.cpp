#include "Game.hpp"
#include <iostream>

Game::Game() : m_pWindow(nullptr), m_pStateMachine(nullptr), m_bRunning(false) {}

bool Game::init(const char* title, int width, int height) {
    // 1. Initialize SDL_ttf
    if (TTF_Init() == -1) {
        std::cerr << "Game Init: Failed to initialize SDL_ttf." << std::endl;
        return false;
    }

    // 2. Initialize Window (Using your existing Window class)
    m_pWindow = new Window();
    if (!m_pWindow->init(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, false)) {
        std::cerr << "Game Init: Failed to create window." << std::endl;
        return false;
    }

    // 3. Initialize State Machine
    m_pStateMachine = new StateMachine();

    // 4. Mark game as running
    m_bRunning = true;
    std::cout << "Game Engine Initialized Successfully." << std::endl;
    return true;
}

void Game::handleEvents() {
    // Delegate event handling to Window (which calls InputHandler)
    m_pWindow->handleEvents();
    // Check window turn off
    if (!m_pWindow->isRunning()) {
        m_bRunning = false;
    }
}

void Game::update() {
    // Delegate logic updates to the current State
    m_pStateMachine->update();
}

void Game::render() {
    // 1. Clear Screen
    m_pWindow->clear();

    // 2. Render the current State
    m_pStateMachine->render();

    // 3. Swap Buffers
    m_pWindow->display();
}

void Game::clean() {
    std::cout << "Cleaning Game Engine..." << std::endl;
    
    // Clean Input
    InputHandler::getInstance()->clean();

    // Clean States
    m_pStateMachine->clean();
    delete m_pStateMachine;

    // Clean Window
    m_pWindow->clean();
    delete m_pWindow;

    // Quit SDL_ttf
    TTF_Quit();
}