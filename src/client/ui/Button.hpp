#pragma once
#include "UIObject.hpp"
#include <functional>
#include <string>

using Callback = std::function<void()>;

class Button : public UIObject {
public:
    // Constructor
    // width, height: Destination size (on screen)
    // srcWidth, srcHeight: Source size (in sprite sheet). Defaults to 0 (same as dest)
    Button(float x, float y, int width, int height, std::string textureID, Callback callback, int srcWidth = 0, int srcHeight = 0);

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

    int m_srcWidth;
    int m_srcHeight;
};