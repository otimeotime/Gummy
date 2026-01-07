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

    // Mouse button states (Held Down)
    bool getMouseButtonState(int buttonNumber); 

    // Mouse button clicked (Just Pressed this frame)
    bool getMouseButtonClicked(int buttonNumber);

    // Reset states (if needed)
    void clean();
    void reset();

    // Get characters typed in the current frame
    std::string getInputText() { return m_inputText; }
    
    // Check if Backspace was pressed this frame
    bool isBackspaceDown() { return m_isBackspace; }

    // Mouse wheel delta for the current frame (positive = scroll up)
    int getMouseWheelY() const { return m_mouseWheelY; }

private:
    InputHandler();
    ~InputHandler();

    const Uint8* keystates;
    std::vector<bool> mouseButtonStates;
    std::vector<bool> mouseButtonJustPressed;
    Vector2D* mousePosition;

    std::string m_inputText = "";
    bool m_isBackspace = false;

    int m_mouseWheelY = 0;
};