#pragma once
#include "../core/GameState.hpp"
#include "../ui/UIObject.hpp"
#include "../ui/Text.hpp"
#include "../ui/Button.hpp"
#include "../ui/TextInput.hpp"
#include <vector>
#include <string>
#include "../network/ClientSocket.hpp" // For PlayerStatusInfo and PacketStructs if needed. Accessing PacketStructs via ClientSocket usually, but needed for PlayerStatusInfo type.

class SceneViewProfile;

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

    // Header
    Text* m_lblTitle = nullptr;

    // Searching State interaction (Searching...)
    bool m_isSearching;
    uint32_t m_searchStartTime;
    Button* m_btnCancelSearch;
    Text* m_lblSearchingTimer;
    Button* m_btnFindMatch; 
    Text* m_lblFindMatch;

    // Replay button + popup dialog
    Button* m_btnViewProfile = nullptr;
    Text* m_lblViewProfile = nullptr;

    Button* m_btnReplay = nullptr;
    Text* m_lblReplay = nullptr;
    bool m_showReplayPopup = false;
    Text* m_lblReplayPopupTitle = nullptr;
    Text* m_lblReplayPopupHint = nullptr;
    TextInput* m_inReplayDir = nullptr;
    Button* m_btnReplayStart = nullptr;
    Text* m_lblReplayStart = nullptr;
    Button* m_btnReplayCancel = nullptr;
    Text* m_lblReplayCancel = nullptr;

    // Players List
    std::vector<PlayerStatusInfo> m_allPlayers;
    std::vector<Text*> m_playerListTexts; // Separate list for dynamic UI
    uint32_t m_lastRefreshTime;

    // Matchmaking Popup
    bool m_showMatchPopup;
    bool m_hasMatchDecision; // If true, disable buttons
    uint32_t m_pendingMatchId;
    Button* m_btnAccept;
    Button* m_btnDecline;
    Text* m_lblMatchFound;
    Text* m_lblAccept;
    Text* m_lblDecline;
    Text* m_lblMatchStatus; // "Waiting for opponent..."

    void drawSidebar();
    void drawMatchPopup();
    void drawReplayPopup();
    void refreshPlayerList(); // New helper
};
