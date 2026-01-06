#pragma once
#include "../core/GameState.hpp"
#include "../ui/UIObject.hpp"
#include "../ui/Text.hpp"
#include "../ui/Button.hpp"
#include <vector>
#include <string>
#include "../network/ClientSocket.hpp" // For PlayerStatusInfo and PacketStructs if needed. Accessing PacketStructs via ClientSocket usually, but needed for PlayerStatusInfo type.

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

    // Specific UI pointers for updates
    Text* m_lblWelcome;

    // Players List
    std::vector<PlayerStatusInfo> m_allPlayers;
    std::vector<Text*> m_playerListTexts; // Separate list for dynamic UI
    uint32_t m_lastRefreshTime;
    
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
    void refreshPlayerList(); // New helper
};
