#include "Button.hpp"
#include "../core/InputHandler.hpp"  
#include "../core/TextureManager.hpp"
#include "../core/Game.hpp"          

Button::Button(float x, float y, int width, int height, std::string textureID, Callback callback, int srcWidth, int srcHeight)
    : UIObject(x, y, width, height), m_textureID(textureID), m_callback(callback) 
{
    m_currentFrame = MOUSE_OUT; // Default state
    m_bReleased = true;
    
    // If src dimensions are not provided (0), assume they match destination
    m_srcWidth = (srcWidth > 0) ? srcWidth : width;
    m_srcHeight = (srcHeight > 0) ? srcHeight : height;
}

void Button::load() {
    // Scene's task
}

void Button::draw() {
    // Sprite Sheet Size: 1x3
    // Use drawFrameScaled to support resizing
    TextureManager::getInstance()->drawFrameScaled(
        m_textureID, 
        m_srcWidth,     // Source Width (e.g. 181)
        m_srcHeight,    // Source Height (e.g. 73)
        (int)m_position.x, 
        (int)m_position.y, 
        m_width,        // Dest Width (e.g. 120)
        m_height,       // Dest Height (e.g. 50)
        1,              // Row 1
        m_currentFrame, // Frame index (0, 1, or 2)
        Game::getInstance()->getRenderer(),
        0,              // Angle
        m_alpha,        // Alpha
        SDL_FLIP_NONE
    );
}

void Button::update() {
    if (!m_enabled) {
        m_currentFrame = MOUSE_OUT;
        m_bReleased = true;
        return;
    }
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