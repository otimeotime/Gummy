#include "StateMachine.hpp"
#include <iostream>

void StateMachine::requestPushState(GameState* pState) {
    if (m_pendingState && m_pendingState != pState) {
        delete m_pendingState;
    }
    m_pendingState = pState;
    m_pendingOp = PendingOp::Push;
}

void StateMachine::requestChangeState(GameState* pState) {
    if (m_pendingState && m_pendingState != pState) {
        delete m_pendingState;
    }
    m_pendingState = pState;
    m_pendingOp = PendingOp::Change;
}

void StateMachine::requestPopState() {
    if (m_pendingState) {
        delete m_pendingState;
        m_pendingState = nullptr;
    }
    m_pendingOp = PendingOp::Pop;
}

void StateMachine::requestReplaceAll(GameState* pState) {
    if (m_pendingState && m_pendingState != pState) {
        delete m_pendingState;
    }
    m_pendingState = pState;
    m_pendingOp = PendingOp::ReplaceAll;
}

void StateMachine::applyPending() {
    switch (m_pendingOp) {
        case PendingOp::None:
            return;
        case PendingOp::Pop:
            popState();
            break;
        case PendingOp::Push: {
            GameState* s = m_pendingState;
            m_pendingState = nullptr;
            pushState(s);
        } break;
        case PendingOp::Change: {
            GameState* s = m_pendingState;
            m_pendingState = nullptr;
            changeState(s);
        } break;
        case PendingOp::ReplaceAll: {
            GameState* s = m_pendingState;
            m_pendingState = nullptr;
            clean();
            pushState(s);
        } break;
    }
    m_pendingOp = PendingOp::None;
}

void StateMachine::pushState(GameState *pState) {
    m_gameStates.push_back(pState);
    m_gameStates.back()->onEnter();
}

void StateMachine::changeState(GameState *pState) {
    if (!m_gameStates.empty()) {
        // If the state is already the same, do nothing
        if (m_gameStates.back()->getStateID() == pState->getStateID()) {
            return;
        }

        // Exit and remove the current state
        if (m_gameStates.back()->onExit()) {
            delete m_gameStates.back();
            m_gameStates.pop_back();
        }
    }

    // Push the new state
    m_gameStates.push_back(pState);
    m_gameStates.back()->onEnter();
}

void StateMachine::popState() {
    if (!m_gameStates.empty()) {
        if (m_gameStates.back()->onExit()) {
            delete m_gameStates.back();
            m_gameStates.pop_back();
        }
    }
}

void StateMachine::update() {
    if (!m_gameStates.empty()) {
        m_gameStates.back()->update();
    }

    applyPending();
}

void StateMachine::render() {
    if (!m_gameStates.empty()) {
        // Render all states bottom -> top so pushed states behave like overlays.
        for (auto* s : m_gameStates) {
            if (s) s->render();
        }
    }
}

void StateMachine::clean() {
    while (!m_gameStates.empty()) {
        m_gameStates.back()->onExit();
        delete m_gameStates.back();
        m_gameStates.pop_back();
    }
    std::cout << "StateMachine cleaned." << std::endl;

    if (m_pendingState) {
        delete m_pendingState;
        m_pendingState = nullptr;
    }
    m_pendingOp = PendingOp::None;
}