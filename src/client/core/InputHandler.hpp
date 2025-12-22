#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <iostream>
#include "../../common/gui/Vector2D.hpp"
#include <string>

// // Simple Vector2D struct for Mouse Position
// struct Vector2D {
//     float x;
//     float y;
// };

class InputHandler {
public:
    static InputHandler* getInstance() {
        static InputHandler instance;
        return &instance;
    }

    // Called every frame to poll events
    void update();
    void updateEvent(SDL_Event& event);

    // Check if a specific key is currently held down
    bool isKeyDown(SDL_Scancode key);

    // Get Mouse position
    Vector2D* getMousePosition() { return mousePosition; }

    // Mouse button states
    bool getMouseButtonState(int buttonNumber); 

    // Reset states (if needed)
    void clean();

    // Get characters typed in the current frame
    std::string getInputText() { return m_inputText; }
    
    // Check if Backspace was pressed this frame
    bool isBackspaceDown() { return m_isBackspace; }

private:
    InputHandler();
    ~InputHandler();

    const Uint8* keystates;
    std::vector<bool> mouseButtonStates;
    Vector2D* mousePosition;

    std::string m_inputText = "";
    bool m_isBackspace = false;
};