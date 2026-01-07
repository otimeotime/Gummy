#pragma once

#include "../core/GameState.hpp"
#include "../ui/UIObject.hpp"
#include "../ui/Text.hpp"
#include "../ui/Button.hpp"
#include <vector>
#include <string>

#include "../../common/network/PacketStructs.hpp"

class SceneViewProfile : public GameState {
public:
    SceneViewProfile(const std::string& username);

    bool onEnter() override;
    bool onExit() override;
    void update() override;
    void render() override;
    std::string getStateID() const override { return "SCENE_VIEW_PROFILE"; }

private:
    std::string m_username;

    std::vector<UIObject*> m_uiObjects;

    Button* m_btnBack = nullptr;
    Text* m_lblBack = nullptr;

    Text* m_lblUsername = nullptr;
    Text* m_lblCreatedAt = nullptr;
    Text* m_lblElo = nullptr;

    // Loading/error
    bool m_isLoading = true;
    Text* m_lblStatus = nullptr;

    // Profile data
    ResGetProfile m_profile{};
    bool m_hasProfile = false;

    // History rendering
    int m_scrollY = 0;
    int m_scrollMax = 0;

    // Pre-rendered history rows (rebuilt when profile arrives)
    std::vector<Text*> m_historyTexts;

    // Per-row replay buttons (same ordering as m_historyTexts)
    std::vector<Button*> m_historyReplayButtons;
    std::vector<Text*> m_historyReplayLabels;

    void rebuildHistoryTexts();
    void clearHistoryTexts();

    void drawHistoryPanel();
};
