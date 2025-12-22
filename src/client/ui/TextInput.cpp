#include "TextInput.hpp"
#include "../core/Game.hpp"
#include "../core/InputHandler.hpp"

TextInput::TextInput(float x, float y, int width, int height, std::string fontPath, int fontSize)
    : UIObject(x, y, width, height), m_hasFocus(false), m_bReleased(true), m_rawString(""), m_backspaceTimer(0)
{
    // Default Styling
    m_boxColor = {255, 255, 255, 255};      // White Background
    m_borderColor = {0, 0, 0, 255};         // Black Border

    // Initialize the Text Component.
    // We pass (0,0) initially because we will calculate its position dynamically in draw().
    // Color is set to Black {0,0,0,255} for the font.
    m_textComponent = new Text(0, 0, fontPath, fontSize, "", {0, 0, 0, 255});
}

TextInput::~TextInput() {
    clean();
}

void TextInput::clean() {
    // Clean up the composed Text object
    if (m_textComponent) {
        m_textComponent->clean();
        delete m_textComponent;
        m_textComponent = nullptr;
    }
}

void TextInput::update() {
    InputHandler* input = InputHandler::getInstance();
    Vector2D* mousePos = input->getMousePosition();
    
    // Check AABB Collision (Mouse is INSIDE the box)
    if (mousePos->x < (m_position.x + m_width) && mousePos->x > m_position.x &&
        mousePos->y < (m_position.y + m_height) && mousePos->y > m_position.y) 
    {
        // INSIDE
        if (input->getMouseButtonState(0) && m_bReleased) {
            // User clicked inside: Gain Focus
            m_hasFocus = true;
            m_boxColor = {230, 240, 255, 255}; // Light Blue when focused
            m_borderColor = {0, 0, 255, 255};  // Blue Border
            
            m_bReleased = false; // Mark as held
        }
        else if (!input->getMouseButtonState(0)) {
            m_bReleased = true; // Reset release state
        }
    } 
    else {
        // OUTSIDE
        if (input->getMouseButtonState(0) && m_bReleased) {
            // User clicked outside: Lose Focus
            m_hasFocus = false;
            m_boxColor = {255, 255, 255, 255}; // White
            m_borderColor = {0, 0, 0, 255};    // Black

            m_bReleased = false;
        }
        else if (!input->getMouseButtonState(0)) {
            m_bReleased = true;
        }
    }

    if (m_hasFocus) {
        bool textChanged = false;

        // Handle new character input
        std::string newChars = input->getInputText();
        if (!newChars.empty()) {
            m_rawString += newChars;
            textChanged = true;
        }

        // Handle Backspace (Delete character)
        // Using a simple timer to prevent deleting too fast
        if (input->isBackspaceDown()) {
            m_backspaceTimer++;
            // Delete on first frame (1) or every 5th frame after holding for 20 frames
            if (m_backspaceTimer == 1 || (m_backspaceTimer > 20 && m_backspaceTimer % 5 == 0)) {
                if (!m_rawString.empty()) {
                    m_rawString.pop_back();
                    textChanged = true;
                }
            }
        } else {
            m_backspaceTimer = 0; // Reset timer
        }

        // Update the Text Component if string changed
        if (textChanged) {
            m_textComponent->setText(m_rawString);
        }
    }
}

void TextInput::draw() {
    SDL_Renderer* renderer = Game::getInstance()->getRenderer();

    // DRAW BOX BACKGROUND
    SDL_Rect bgRect = { (int)m_position.x, (int)m_position.y, m_width, m_height };
    SDL_SetRenderDrawColor(renderer, m_boxColor.r, m_boxColor.g, m_boxColor.b, m_boxColor.a);
    SDL_RenderFillRect(renderer, &bgRect);

    // DRAW BOX BORDER
    SDL_SetRenderDrawColor(renderer, m_borderColor.r, m_borderColor.g, m_borderColor.b, m_borderColor.a);
    SDL_RenderDrawRect(renderer, &bgRect);

    // CALCULATE TEXT POSITION
    int padding = 10;
    int textW = m_textComponent->getWidth();
    int textH = m_textComponent->getHeight();
    
    // Default: Align text to the left with padding
    int textX = (int)m_position.x + padding;
    // Center text vertically
    int textY = (int)m_position.y + (m_height - textH) / 2;

    // If the text is wider than the box, shift it to the left 
    // so the end of the string is always visible.
    int maxTextWidth = m_width - (padding * 2);

    if (textW > maxTextWidth) {
        // Calculate the overflow amount and shift X backwards
        // Position = (Right Edge of Box) - (Text Width) - (Padding)
        textX = ((int)m_position.x + m_width - padding) - textW;
    }

    // APPLY CLIPPING
    SDL_Rect clipRect = { 
        (int)m_position.x + 2, 
        (int)m_position.y + 2, 
        m_width - 4, 
        m_height - 4 
    };

    // Activate Clipping
    SDL_RenderSetClipRect(renderer, &clipRect);

    // Update the position of the text component based on scrolling logic
    m_textComponent->setPosition(textX, textY);
    
    // Draw the text (The clipping rect will cut off the excess parts)
    m_textComponent->draw();

    // DISABLE CLIPPING
    SDL_RenderSetClipRect(renderer, NULL); 
}