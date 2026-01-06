#include "StateMachine.hpp"
#include "InputHandler.hpp"
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
    m_pendingState = pState;
    m_isChanging = true;
    // Clear input to prevent ghost clicks in new state
    InputHandler::getInstance()->reset(); 
}

void StateMachine::popState() {
    m_isPopping = true;
}

void StateMachine::update() {
    if (!m_gameStates.empty()) {
        m_gameStates.back()->update();
    }

    // Handle deferred state changes
    if (m_isChanging && m_pendingState != nullptr) {
        if (!m_gameStates.empty()) {
            if (m_gameStates.back()->getStateID() == m_pendingState->getStateID()) {
                m_isChanging = false;
                m_pendingState = nullptr;
                return;
            }
            if (m_gameStates.back()->onExit()) {
                delete m_gameStates.back();
                m_gameStates.pop_back();
            }
        }
        m_gameStates.push_back(m_pendingState);
        m_gameStates.back()->onEnter();
        
        m_isChanging = false;
        m_pendingState = nullptr;
    }
    else if (m_isPopping) {
        if (!m_gameStates.empty()) {
            if (m_gameStates.back()->onExit()) {
                delete m_gameStates.back();
                m_gameStates.pop_back();
            }
        }
        m_isPopping = false;
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