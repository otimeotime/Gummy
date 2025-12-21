#pragma once
#include "../../common/Vector2D.hpp"

class UIObject {
public:
    // Constructor: Sets position and size
    UIObject(float x, float y, int width, int height) 
        : m_position(x, y), m_width(width), m_height(height) {}

    virtual ~UIObject() {}

    // Pure virtual functions: Child classes MUST implement these
    virtual void load() = 0;
    virtual void draw() = 0;
    virtual void update() = 0;
    virtual void clean() = 0;

protected:
    Vector2D m_position;
    int m_width;
    int m_height;
};