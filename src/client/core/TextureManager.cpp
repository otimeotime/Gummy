#include "TextureManager.hpp"

bool TextureManager::load(std::string fileName, std::string id, SDL_Renderer* renderer) {
    SDL_Surface* tempSurface = IMG_Load(fileName.c_str());
    if (tempSurface == 0) {
        std::cout << "Failed to load image: " << fileName << std::endl;
        return false;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, tempSurface);
    SDL_FreeSurface(tempSurface);

    if (texture != 0) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        textureMap[id] = texture;
        return true;
    }

    return false;
}

void TextureManager::drawStatic(std::string id, int x, int y, int width, int height, SDL_Renderer* renderer, SDL_RendererFlip flip) {
    SDL_Rect srcRect;
    SDL_Rect destRect;

    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = destRect.w = width;
    srcRect.h = destRect.h = height;
    destRect.x = x;
    destRect.y = y;

    SDL_RenderCopyEx(renderer, textureMap[id], &srcRect, &destRect, 0, 0, flip);
}

void TextureManager::drawFrame(std::string id, int x, int y, int width, int height, int currentRow, int currentFrame, SDL_Renderer* renderer, double angle, int alpha, SDL_RendererFlip flip) {
    SDL_Rect srcRect;
    SDL_Rect destRect;

    // Calculate position in sprite sheet
    srcRect.x = width * currentFrame;
    srcRect.y = height * (currentRow - 1);
    srcRect.w = destRect.w = width;
    srcRect.h = destRect.h = height;
    destRect.x = x;
    destRect.y = y;

    // Set opacity if needed
    SDL_SetTextureAlphaMod(textureMap[id], alpha);

    // Render with rotation (angle) and center point (NULL = center)
    SDL_RenderCopyEx(renderer, textureMap[id], &srcRect, &destRect, angle, 0, flip);
}

void TextureManager::clearFromTextureMap(std::string id) {
    textureMap.erase(id);
}

void TextureManager::drawScaled(std::string id, int x, int y, int width, int height, SDL_Renderer* renderer, double angle, SDL_RendererFlip flip) {
    SDL_Rect srcRect;
    SDL_Rect destRect;

    // Check if texture exists to avoid crash
    if (textureMap.find(id) == textureMap.end() || textureMap[id] == nullptr) {
        // Texture missing, do nothing
        return;
    }

    // 1. Get the real size of the image (e.g., 500x500)
    SDL_QueryTexture(textureMap[id], NULL, NULL, &srcRect.w, &srcRect.h);
    srcRect.x = 0;
    srcRect.y = 0;

    // 2. Define where to draw it (e.g., 16x16)
    destRect.x = x;
    destRect.y = y;
    destRect.w = width;
    destRect.h = height;

    // 3. Render
    SDL_RenderCopyEx(renderer, textureMap[id], &srcRect, &destRect, angle, 0, flip);
}