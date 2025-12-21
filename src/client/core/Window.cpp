#include "Window.hpp"
#include "InputHandler.hpp"

SDL_Renderer* Window::renderer = nullptr;

Window::Window() : window(nullptr), running(false) {}

Window::~Window() {}

bool Window::init(const char* title, int xpos, int ypos, int width, int height, bool fullscreen) {
    int flags = 0;
    if (fullscreen) {
        flags = SDL_WINDOW_FULLSCREEN;
    }

    if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
        std::cout << "Subsystems Initialized!..." << std::endl;

        window = SDL_CreateWindow(title, xpos, ypos, width, height, flags);
        if (window) {
            std::cout << "Window created!" << std::endl;
        } else {
            return false;
        }

        renderer = SDL_CreateRenderer(window, -1, 0);
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White background
            std::cout << "Renderer created!" << std::endl;
        } else {
            return false;
        }

        int imgFlags = IMG_INIT_JPG | IMG_INIT_PNG;
        if (!(IMG_Init(imgFlags) & imgFlags)) {
            std::cerr << "SDL_image could not initialize! SDL_image Error: " << IMG_GetError() << std::endl;
            return false;
        }

        running = true;
    } else {
        return false;
    }

    return true;
}

void Window::handleEvents() {
    // Let InputHandler update its internal state
    InputHandler::getInstance()->update();

    // Handle window-specific events (like closing the window)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
         // Pass event to InputHandler so it knows about key presses
        InputHandler::getInstance()->updateEvent(event);

        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            default:
                break;
        }
    }
}

void Window::clear() {
    SDL_RenderClear(renderer);
}

void Window::display() {
    SDL_RenderPresent(renderer);
}

void Window::clean() {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    std::cout << "Game Cleaned" << std::endl;
}