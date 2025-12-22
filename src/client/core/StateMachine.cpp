#include "StateMachine.hpp"
#include <iostream>

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
}

void StateMachine::render() {
    if (!m_gameStates.empty()) {
        m_gameStates.back()->render();
    }
}

void StateMachine::clean() {
    while (!m_gameStates.empty()) {
        m_gameStates.back()->onExit();
        delete m_gameStates.back();
        m_gameStates.pop_back();
    }
    std::cout << "StateMachine cleaned." << std::endl;
}