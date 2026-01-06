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
    TerminalScene(std::string serverIp,
                  int serverPort,
                  std::string mapPath = "assets/maps/flatmap.txt",
                  std::string username = "",
                  std::string resultText = "");

    bool onEnter() override;
    bool onExit() override;
    void update() override;
    void render() override;

    std::string getStateID() const override { return "TERMINAL_SCENE"; }

private:
    std::string m_serverIp;
    int m_serverPort;
    std::string m_mapPath;
    std::string m_username;
    std::string m_resultText;

    std::string m_bgTextureID;
    std::string m_restartBtnTextureID;
    std::string m_homeBtnTextureID;

    std::vector<UIObject*> m_uiObjects;

    Text* m_title;
    Text* m_resultLabel;
    Text* m_rematchStatusLabel;
    Text* m_rematchTimerLabel;
    Button* m_rematchBtn;
    Text* m_restartLabel;
    Text* m_homeLabel;

    bool m_rematchRequested = false;
    Uint32 m_rematchRequestTick = 0;

    // Tracks the start of the 60s rematch window (approx. client-side).
    Uint32 m_rematchWindowStartTick = 0;
    int m_lastShownRematchSeconds = -1;
};
