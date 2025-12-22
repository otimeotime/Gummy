#include "Button.hpp"
#include "../core/InputHandler.hpp"  
#include "../core/TextureManager.hpp"
#include "../core/Game.hpp"          

Button::Button(float x, float y, int width, int height, std::string textureID, Callback callback)
    : UIObject(x, y, width, height), m_textureID(textureID), m_callback(callback) 
{
    m_currentFrame = MOUSE_OUT; // Default state
    m_bReleased = true;
}

void Button::load() {
    // Scene's task
}

void Button::draw() {
    // Sprite Sheet Size: 1x3
    TextureManager::getInstance()->drawFrame(
        m_textureID, 
        (int)m_position.x, 
        (int)m_position.y, 
        m_width, 
        m_height, 
        1,              // Row 1
        m_currentFrame, // Frame index (0, 1, or 2)
        Game::getInstance()->getRenderer(),
        0,              // Angle
        255,            // Alpha
        SDL_FLIP_NONE
    );
}

void Button::update() {
    Vector2D* mousePos = InputHandler::getInstance()->getMousePosition();

    // Check for AABB Collision (Mouse is INSIDE the button rectangle or not)
    if (mousePos->x < (m_position.x + m_width) && mousePos->x > m_position.x &&
        mousePos->y < (m_position.y + m_height) && mousePos->y > m_position.y) 
    {
        // INSIDE
        // Check if Left Mouse Button (0) is pressed and was previously released
        if (InputHandler::getInstance()->getMouseButtonState(0) && m_bReleased) {
            m_currentFrame = CLICKED;
            
            // Execute the callback function
            m_callback();
            
            m_bReleased = false; // Mark as held down
        }
        else if (!InputHandler::getInstance()->getMouseButtonState(0)) {
            m_bReleased = true; // Reset release state
            m_currentFrame = MOUSE_OVER; // Switch to Hover frame
        }
    } 
    // OUTSIDE
    else {
        m_currentFrame = MOUSE_OUT;
        m_bReleased = true; 
    }
}

void Button::clean() {
    // Scene's Task
}