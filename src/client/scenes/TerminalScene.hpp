#pragma once

#include "../core/GameState.hpp"

#include "../core/Game.hpp"
#include "../core/TextureManager.hpp"

#include "../ui/Button.hpp"
#include "../ui/Text.hpp"

#include <string>
#include <vector>

class TerminalScene : public GameState {
public:
    TerminalScene(std::string serverIp, int serverPort);

    bool onEnter() override;
    bool onExit() override;
    void update() override;
    void render() override;

    std::string getStateID() const override { return "TERMINAL_SCENE"; }

private:
    std::string m_serverIp;
    int m_serverPort;

    std::string m_bgTextureID;
    std::string m_restartBtnTextureID;

    std::vector<UIObject*> m_uiObjects;

    Text* m_title;
    Text* m_restartLabel;
};
