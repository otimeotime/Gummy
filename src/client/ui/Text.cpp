#include "Text.hpp"
#include "../core/Game.hpp"
#include <iostream>

Text::Text(float x, float y, std::string fontPath, int fontSize, std::string content, SDL_Color color)
    : UIObject(x, y, 0, 0),
      m_content(content), m_font(nullptr), m_texture(nullptr), m_color(color)
{
    // Load Font
    m_font = TTF_OpenFont(fontPath.c_str(), fontSize);
    if (!m_font) {
        std::cout << "[Text] Failed to load font: " << fontPath << std::endl;
    }

    // Create Texture
    createTexture();
}

void Text::clean() {
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
}

void Text::createTexture() {
    // Delete previous texture before create new one
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }

    if (!m_font || m_content.empty()) return;

    SDL_Surface* surface = TTF_RenderText_Blended(m_font, m_content.c_str(), m_color);
    if (surface) {
        m_texture = SDL_CreateTextureFromSurface(Game::getInstance()->getRenderer(), surface);
        
        // Update UIObject size to fit with string
        m_width = surface->w;
        m_height = surface->h;

        SDL_FreeSurface(surface);
    }
}

void Text::draw() {
    if (m_texture) {
        // Rewrite m_position của UIObject
        SDL_Rect destRect = { 
            (int)m_position.x, 
            (int)m_position.y, 
            m_width, 
            m_height 
        };
        SDL_RenderCopy(Game::getInstance()->getRenderer(), m_texture, NULL, &destRect);
    }
}

void Text::setText(std::string newContent) {
    if (m_content != newContent) {
        m_content = newContent;
        createTexture();
    }
}

void Text::setColor(SDL_Color newColor) {
    m_color = newColor;
    createTexture();
}