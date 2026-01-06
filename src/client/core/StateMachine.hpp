#pragma once
#include "GameState.hpp"
#include <vector>

class StateMachine {
public:
    // Add a new state to the stack without removing the previous one (e.g., Pause Menu).
    void pushState(GameState* pState);

    // Remove the current state and switch to a new one (e.g., Login -> Home).
    void changeState(GameState* pState);

    // Remove the current state and go back to the previous one.
    void popState();

    // Update the current active state.
    void update();

    // Render the current active state.
    void render();

    // Clear all states.
    void clean();

    // Deferred transitions (safe to call from inside a state's update()).
    void requestPushState(GameState* pState);
    void requestChangeState(GameState* pState);
    void requestPopState();
    void requestReplaceAll(GameState* pState);

private:
    std::vector<GameState*> m_gameStates;
    enum class PendingOp {
        None,
        Push,
        Change,
        Pop,
        ReplaceAll
    };
    GameState* m_pendingState = nullptr;
    bool m_isChanging = false;
    bool m_isPopping = false;
    PendingOp m_pendingOp = PendingOp::None;
    GameState* getCurrentState();
    

    void applyPending();
};