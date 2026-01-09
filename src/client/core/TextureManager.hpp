#pragma once
#include "Window.hpp"
#include <map>
#include <string>

class TextureManager {
public:
    static constexpr const char* kSpritesDir = "assets/sprites/";

    // Helper to build the canonical path for any sprite image.
    static std::string spritePath(const std::string& fileName) {
        return std::string(kSpritesDir) + fileName;
    }

    // Singleton access
    static TextureManager* getInstance() {
        static TextureManager instance;
        return &instance;
    }

    // Load an image from file path and store it with a string ID
    bool load(std::string fileName, std::string id, SDL_Renderer* renderer);

    // Draw a static image (backgrounds, UI)
    void drawStatic(std::string id, int x, int y, int width, int height, SDL_Renderer* renderer, SDL_RendererFlip flip = SDL_FLIP_NONE);

    // Draw a frame of an animation or a rotated object (Player, Cannon)
    // currentRow: row in spritesheet
    // currentFrame: column in spritesheet
    // angle: rotation angle (for aiming gunny)
    // center: pivot point (nullptr = center of image)
    void drawFrame(std::string id, int x, int y, int width, int height, int currentRow, int currentFrame, SDL_Renderer* renderer, double angle, int alpha = 255, SDL_RendererFlip flip = SDL_FLIP_NONE);

    // Draw a frame with scaling (Source Size != Dest Size)
    void drawFrameScaled(std::string id, int srcW, int srcH, int destX, int destY, int destW, int destH, int currentRow, int currentFrame, SDL_Renderer* renderer, double angle = 0, int alpha = 255, SDL_RendererFlip flip = SDL_FLIP_NONE);

    // Draw a filled rectangle (primitive)
    void drawFillRect(int x, int y, int width, int height, int r, int g, int b, int a, SDL_Renderer* renderer);

    // Draw a line
    void drawLine(int x1, int y1, int x2, int y2, int r, int g, int b, int a, SDL_Renderer* renderer);

    // Remove texture from memory
    void clearFromTextureMap(std::string id);

    void drawScaled(std::string id, int x, int y, int width, int height, SDL_Renderer* renderer, double angle = 0.0, SDL_RendererFlip flip = SDL_FLIP_NONE);

private:
    TextureManager() {}
    std::map<std::string, SDL_Texture*> textureMap;
};