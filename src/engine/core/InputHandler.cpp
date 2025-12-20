#include "InputHandler.hpp"

InputHandler::InputHandler() {
    mousePosition = new Vector2D();
    // Initialize mouse buttons (Left, Middle, Right)
    for(int i = 0; i < 3; i++) {
        mouseButtonStates.push_back(false);
    }
}

InputHandler::~InputHandler() {
    delete mousePosition;
}

void InputHandler::update() {
    // Update Keyboard state array
    keystates = SDL_GetKeyboardState(0);
}

void InputHandler::updateEvent(SDL_Event& event) {
    // Update Mouse Events
    if(event.type == SDL_MOUSEBUTTONDOWN) {
        if(event.button.button == SDL_BUTTON_LEFT) {
            mouseButtonStates[0] = true;
        }
        if(event.button.button == SDL_BUTTON_MIDDLE) {
            mouseButtonStates[1] = true;
        }
        if(event.button.button == SDL_BUTTON_RIGHT) {
            mouseButtonStates[2] = true;
        }
    }

    if(event.type == SDL_MOUSEBUTTONUP) {
        if(event.button.button == SDL_BUTTON_LEFT) {
            mouseButtonStates[0] = false;
        }
        if(event.button.button == SDL_BUTTON_MIDDLE) {
            mouseButtonStates[1] = false;
        }
        if(event.button.button == SDL_BUTTON_RIGHT) {
            mouseButtonStates[2] = false;
        }
    }

    if(event.type == SDL_MOUSEMOTION) {
        mousePosition->x = (float)event.motion.x;
        mousePosition->y = (float)event.motion.y;
    }
}

bool InputHandler::isKeyDown(SDL_Scancode key) {
    if(keystates != 0) {
        if(keystates[key] == 1) {
            return true;
        }
    }
    return false;
}

bool InputHandler::getMouseButtonState(int buttonNumber) {
    return mouseButtonStates[buttonNumber];
}

void InputHandler::clean() {
    // Cleanup if necessary
}