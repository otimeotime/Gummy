#pragma once
#include "UIObject.hpp"
#include <functional>
#include <string>

using Callback = std::function<void()>;

class Button : public UIObject {
public:
    // Constructor
    Button(float x, float y, int width, int height, std::string textureID, Callback callback);

    // Override methods from UIObject
    virtual void load() override;
    virtual void draw() override;
    virtual void update() override;
    virtual void clean() override;

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
};