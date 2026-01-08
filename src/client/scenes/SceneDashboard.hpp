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
    
    struct PlayerEntry {
        Text* label;
        Button* clickArea;
    };
    std::vector<PlayerEntry> m_playerListUI;
    uint32_t m_lastRefreshTime;

    // Player Interaction Popup (Challenge/Profile)
    bool m_showInteractPopup = false;
    std::string m_targetPlayerName;
    Button* m_btnChallenge = nullptr;
    Text* m_lblChallenge = nullptr;
    Button* m_btnViewOtherProfile = nullptr;
    Text* m_lblViewOtherProfile = nullptr;
    Button* m_btnCloseInteract = nullptr;
    Text* m_lblCloseInteract = nullptr;

    void drawInteractionPopup();
    
    // Incoming Challenge Popup
    bool m_showIncomingChallenge = false;
    std::string m_incomingChallengerName;
    Text* m_lblIncomingTitle = nullptr;
    Text* m_lblIncomingMsg = nullptr;
    Button* m_btnIncomingAccept = nullptr;
    Text* m_lblIncomingAccept = nullptr;
    Button* m_btnIncomingDecline = nullptr;
    Text* m_lblIncomingDecline = nullptr;

    void drawIncomingChallengePopup();

    // Final Confirm Popup (for Challenger)
    bool m_showFinalConfirm = false;
    std::string m_finalOpponentName;
    Text* m_lblFinalTitle = nullptr;
    Text* m_lblFinalMsg = nullptr;
    Button* m_btnFinalYes = nullptr;
    Text* m_lblFinalYes = nullptr;
    Button* m_btnFinalNo = nullptr;
    Text* m_lblFinalNo = nullptr;

    void drawFinalConfirmPopup();

    // Waiting for Response Popup (After sending challenge)
    bool m_showWaitingResponse = false;
    Text* m_lblWaitingResponse = nullptr;
    Button* m_btnCancelWaiting = nullptr; // Or "Close"
    Text* m_lblCancelWaiting = nullptr;
    
    void drawWaitingResponsePopup();

    // Waiting for Final Confirm (Player B waiting for A)
    bool m_showWaitingFinal = false;
    bool m_waitingFinalCanClose = false; // Only true if error occurs
    Text* m_lblWaitingFinal = nullptr;
    Button* m_btnCloseWaitingFinal = nullptr;
    Text* m_lblCloseWaitingFinal = nullptr;
    uint32_t m_waitingFinalStartTime = 0; // Timer for timeout

    void drawWaitingFinalPopup();

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
