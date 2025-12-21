#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iostream>

class Window {
public:
    Window();
    ~Window();

    // Initialize SDL, create window and renderer
    bool init(const char* title, int xpos, int ypos, int width, int height, bool fullscreen);
    
    // Handle global events (like clicking the X button)
    void handleEvents();
    
    // Clear the screen (usually called at start of loop)
    void clear();
    
    // Present the buffer to screen (usually called at end of loop)
    void display();
    
    // Cleaning up memory
    void clean();

    // Check if the game is still running
    bool isRunning() { return running; }

    // Get the raw SDL renderer (needed for TextureManager)
    static SDL_Renderer* renderer;

private:
    SDL_Window* window;
    bool running;
};