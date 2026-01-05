#pragma once
#include "../core/GameState.hpp"
#include "../ui/UIObject.hpp"
#include "../ui/Text.hpp"
#include "../ui/Button.hpp"
#include <vector>
#include <string>

class SceneDashboard : public GameState {
public:
    SceneDashboard(std::string username);

    virtual bool onEnter() override;
    virtual bool onExit() override;
    virtual void update() override;
    virtual void render() override;
    virtual std::string getStateID() const override { return "SCENE_DASHBOARD"; }

private:
    std::string m_username;
    std::vector<UIObject*> m_uiObjects;
    std::string m_bgTextureID;

    // Online Players List
    std::vector<std::string> m_onlinePlayers;
    
    // Player Menu Popup State
    bool m_showPlayerMenu;
    std::string m_selectedPlayer;
    Vector2D m_menuPosition;
    
    // UI Elements for the popup menu (created dynamically or hidden)
    Button* m_btnChallenge;
    Button* m_btnProfile;
    Text* m_lblChallenge;
    Text* m_lblProfile;

    void drawSidebar();
    void drawPlayerMenu();
    void handlePlayerListClick();
    bool isMouseInsideMenu(Vector2D* mousePos);
};
