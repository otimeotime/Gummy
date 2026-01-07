#include "Button.hpp"
#include "../core/InputHandler.hpp"  
#include "../core/Game.hpp"          

Button::Button(
    float x,
    float y,
    int width,
    int height,
    std::string textureID,
    Callback callback,
    int srcWidth,
    int srcHeight,
    SDL_Color strokeColor,
    int strokeThickness)
    : UIObject(x, y, width, height),
      m_textureID(std::move(textureID)),
      m_callback(std::move(callback)),
      m_strokeColor(strokeColor),
      m_strokeThickness(strokeThickness)
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
    // Keep supporting truly-invisible buttons used for text links.
    if (m_textureID.empty() || m_textureID == "invisible_btn") return;

    SDL_Renderer* renderer = Game::getInstance()->getRenderer();
    if (!renderer) return;

    // Transparent rectangle with white stroke, no radius.
    SDL_Rect rect{
        (int)m_position.x,
        (int)m_position.y,
        m_width,
        m_height
    };

    Uint8 prevR = 0, prevG = 0, prevB = 0, prevA = 0;
    SDL_GetRenderDrawColor(renderer, &prevR, &prevG, &prevB, &prevA);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const Uint8 drawA = (Uint8)((int)m_strokeColor.a * (int)m_alpha / 255);
    SDL_SetRenderDrawColor(renderer, m_strokeColor.r, m_strokeColor.g, m_strokeColor.b, drawA);

    const int thickness = (m_strokeThickness <= 0) ? 1 : m_strokeThickness;
    for (int i = 0; i < thickness; i++) {
        SDL_Rect r{rect.x + i, rect.y + i, rect.w - (2 * i), rect.h - (2 * i)};
        if (r.w <= 0 || r.h <= 0) break;
        SDL_RenderDrawRect(renderer, &r);
    }

    SDL_SetRenderDrawColor(renderer, prevR, prevG, prevB, prevA);
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

void Button::centerObject(UIObject* obj, float offsetX, float offsetY) const {
    if (!obj) return;
    const Vector2D btnPos = getPosition();
    const int objW = obj->getWidth();
    const int objH = obj->getHeight();

    const float x = btnPos.x + (float)(getWidth() - objW) / 2.0f + offsetX;
    const float y = btnPos.y + (float)(getHeight() - objH) / 2.0f + offsetY;
    obj->setPosition(x, y);
}