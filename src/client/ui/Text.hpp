#pragma once
#include "UIObject.hpp"
#include <SDL2/SDL_ttf.h>
#include <string>

class Text : public UIObject {
public:
    // Constructor
    Text(float x, float y, std::string fontPath, int fontSize, std::string content, SDL_Color color = {255, 255, 255, 255});
    
    virtual ~Text() {}

    // Override methods from UIObject
    virtual void load() override {};
    virtual void draw() override;
    virtual void update() override {};
    virtual void clean() override;

    void setText(std::string newContent);
    void setColor(SDL_Color newColor);

private:
    std::string m_content;
    TTF_Font* m_font;
    SDL_Texture* m_texture;
    SDL_Color m_color;
    
    void createTexture();
};