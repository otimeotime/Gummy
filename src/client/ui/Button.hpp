#pragma once
#include "UIObject.hpp"
#include <SDL2/SDL.h>
#include <functional>
#include <string>

using Callback = std::function<void()>;

class Button : public UIObject {
public:
    // Constructor
    // width, height: Destination size (on screen)
    // srcWidth, srcHeight: Source size (in sprite sheet). Defaults to 0 (same as dest)
    Button(
        float x,
        float y,
        int width,
        int height,
        std::string textureID,
        Callback callback,
        int srcWidth = 0,
        int srcHeight = 0,
        SDL_Color strokeColor = SDL_Color{255, 255, 255, 255},
        int strokeThickness = 3
    );

    // Override methods from UIObject
    virtual void load() override;
    virtual void draw() override;
    virtual void update() override;
    virtual void clean() override;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void setAlpha(Uint8 alpha) { m_alpha = alpha; }
    Uint8 getAlpha() const { return m_alpha; }

    void setStrokeColor(SDL_Color color) { m_strokeColor = color; }
    SDL_Color getStrokeColor() const { return m_strokeColor; }

    void setStrokeThickness(int thickness) { m_strokeThickness = thickness; }
    int getStrokeThickness() const { return m_strokeThickness; }

    void centerObject(UIObject* obj, float offsetX = 0.0f, float offsetY = 0.0f) const;

private:
    // Button visual states
    enum button_state {
        MOUSE_OUT = 0,  // Frame 0: Normal
        MOUSE_OVER = 1, // Frame 1: Hover
        CLICKED = 2     // Frame 2: Pressed
    };

    std::string m_textureID;
    Callback m_callback;
    
    int m_currentFrame; 
    bool m_bReleased; // To prevent rapid-fire clicking

    int m_srcWidth;
    int m_srcHeight;

    bool m_enabled = true;
    Uint8 m_alpha = 255;

    SDL_Color m_strokeColor{255, 255, 255, 255};
    int m_strokeThickness = 3;
};