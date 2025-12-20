#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <iostream>

// Simple Vector2D struct for Mouse Position
struct Vector2D {
    float x;
    float y;
};

class InputHandler {
public:
    static InputHandler* getInstance() {
        static InputHandler instance;
        return &instance;
    }

    // Called every frame to poll events
    void update(); //Keyboard
    void updateEvent(SDL_Event& event); //Mouse

    // Check if a specific key is currently held down
    bool isKeyDown(SDL_Scancode key);

    // Get Mouse position
    Vector2D* getMousePosition() { return mousePosition; }

    // Mouse button states
    bool getMouseButtonState(int buttonNumber); 

    // Reset states (if needed)
    void clean();

private:
    InputHandler();
    ~InputHandler();

    const Uint8* keystates;
    std::vector<bool> mouseButtonStates;
    Vector2D* mousePosition;
};