#pragma once
#include "UIObject.hpp"
#include "Text.hpp" // TextInput involve Text
#include <string>

class TextInput : public UIObject {
public:
    // Constructor
    TextInput(float x, float y, int width, int height, std::string fontPath, int fontSize);
    
    virtual ~TextInput();

    // Override methods from UIObject
    virtual void load() override {};
    virtual void draw() override;
    virtual void update() override;
    virtual void clean() override;

    // Get string from user
    std::string getString() const { return m_rawString; }
    void setString(const std::string& s) { 
        m_rawString = s; 
        if (m_textComponent) m_textComponent->setText(s);
    }

    void setFocus(bool focus);
    bool hasFocus() const { return m_hasFocus; }

private:
    Text* m_textComponent; // Component
    std::string m_rawString;
    
    bool m_hasFocus;
    bool m_bReleased;
    int m_backspaceTimer;

    SDL_Color m_boxColor;
    SDL_Color m_borderColor;
};