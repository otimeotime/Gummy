#pragma once
#include "../core/GameState.hpp"
#include "../ui/Text.hpp"
#include "../ui/TextInput.hpp"
#include "../ui/Button.hpp"
#include <vector>
#include <iostream>

class SceneTest : public GameState {
public:
    // Polymorphism: Manage all UI elements in one list
    std::vector<UIObject*> m_uiObjects; 

    virtual bool onEnter() override {
        std::cout << "[SceneTest] Entering..." << std::endl;

        // 1. Add a Static Label (Text)
        // Position: (300, 150), Font size: 24, Color: Red
        Text* lblName = new Text(300, 150, "assets/Arial.ttf", 24, "Enter Username:", {255, 0, 0, 255});
        m_uiObjects.push_back(lblName);

        // 2. Add an Input Field (TextInput)
        // Position: (300, 200), Width: 250, Height: 40
        TextInput* inputName = new TextInput(300, 200, 250, 40, "assets/Arial.ttf", 20);
        m_uiObjects.push_back(inputName);

        // 3. Add a Submit Button (Using your Button class)
        // Position: (300, 260)
        Button* btnSubmit = new Button(300, 260, 100, 50, "btn_submit", [inputName]() {
            // Callback: Print the text when button is clicked
            std::cout << "User typed: " << inputName->getString() << std::endl;
        });
        m_uiObjects.push_back(btnSubmit);

        return true;
    }

    virtual void update() override {
        // Update all objects in one loop
        for (auto obj : m_uiObjects) {
            obj->update();
        }
    }

    virtual void render() override {
        // Render all objects in one loop
        for (auto obj : m_uiObjects) {
            obj->draw();
        }
    }

    virtual bool onExit() override {
        std::cout << "[SceneTest] Exiting..." << std::endl;
        
        // Clean up memory
        for (auto obj : m_uiObjects) {
            obj->clean();
            delete obj;
        }
        m_uiObjects.clear();
        return true;
    }

    virtual std::string getStateID() const override { return "TEST_SCENE"; }
};