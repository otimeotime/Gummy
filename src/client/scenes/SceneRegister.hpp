#pragma once
#include "../core/GameState.hpp"
#include "../ui/UIObject.hpp"
#include "../ui/Text.hpp"
#include "../ui/TextInput.hpp"
#include "../ui/Button.hpp"
#include <vector>
#include <string>

class SceneRegister : public GameState {
public:
    virtual bool onEnter() override;
    virtual bool onExit() override;
    virtual void update() override;
    virtual void render() override;
    virtual std::string getStateID() const override { return "SCENE_REGISTER"; }

private:
    std::vector<UIObject*> m_uiObjects;
    
    TextInput* m_inputUsername;
    TextInput* m_inputPassword;
    TextInput* m_inputConfirmPassword;
    Text* m_lblError = nullptr;

    std::string m_bannerTextureID;
};
