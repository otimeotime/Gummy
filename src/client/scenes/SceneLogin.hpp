#pragma once
#include "../core/GameState.hpp"
#include "../ui/UIObject.hpp"
#include "../ui/Text.hpp"
#include "../ui/TextInput.hpp"
#include "../ui/Button.hpp"
#include <vector>
#include <string>

class SceneLogin : public GameState {
public:
    virtual bool onEnter() override;
    virtual bool onExit() override;
    virtual void update() override;
    virtual void render() override;
    virtual std::string getStateID() const override { return "SCENE_LOGIN"; }

private:
    // List to manage all UI elements (Inputs, Buttons, Labels)
    std::vector<UIObject*> m_uiObjects;

    // Specific pointers to inputs if we need to access their text later (for validation)
    TextInput* m_inputUsername = nullptr;
    TextInput* m_inputPassword = nullptr;

    // ID for the background image on the right side
    std::string m_bannerTextureID;
};