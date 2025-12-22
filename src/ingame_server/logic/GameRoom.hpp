#pragma once
#include "Player.hpp"
#include "PhysicsEngine.hpp"
#include "MapLoader.hpp"
#include <vector>
#include <iostream>

#define TIMER 30.0f

enum RoomState {
    WAITING_FOR_PLAYERS,
    PLAYING_TURN,
    FIRING_PHASE,
    GAME_OVER
};

class GameRoom {
private:
    std::vector<Player*> m_players;
    std::vector<Projectile> m_projectiles;
    PhysicsEngine* m_physics;
    RoomState m_state;
    int m_currentTurnIndex;
    float m_turnTimer;
    MapLoader* m_mapLoader;
    void switchTurn() {
        m_players[m_currentTurnIndex]->setTurn(false);
        m_players[m_currentTurnIndex]->stopMoving();

        do {
            m_currentTurnIndex = (m_currentTurnIndex + 1) % m_players.size();
        } while (!m_players[m_currentTurnIndex]->isAlive());

        m_players[m_currentTurnIndex]->setTurn(true);
        m_turnTimer = TIMER;

        m_physics->setWind((rand() % 20) - 10.0f);
    }

    void checkWinCondition() {
        int aliveCount = 0;
        for (auto p : m_players) {
            if (p->isAlive()) aliveCount++;
        }
        if (aliveCount <= 1) {
            m_state = GAME_OVER;
            for (auto p : m_players) {
                if (p->isAlive()) {
                    std::cout << "Player " << p->getId() << " wins!" << std::endl;
                }
            }
        }
    }

public:
    GameRoom() : m_state(WAITING_FOR_PLAYERS), m_currentTurnIndex(0), m_turnTimer(0.0f) {
        m_physics = new PhysicsEngine();
    }

    ~GameRoom() { delete m_physics; }

    void addPlayer(int id, std::string name) {
        float startX = 100.0f + m_players.size() * 200.0f;
        m_players.push_back(new Player(id, name, startX, 0, 0));
    }

    void startGame() {
        if (m_players.size() >= 2) {
            m_state = PLAYING_TURN;
            m_currentTurnIndex = 0;
            m_players[0]->setTurn(true);
            std::cout << "Game Started! Player " << m_players[0]->getId() << "'s turn." << std::endl;
        }
    }

    void update(float deltaTime) {
        // Physics update
        m_physics->update(deltaTime, m_players, m_projectiles, m_mapLoader);

        // Game logic
        if (m_state == PLAYING_TURN) {
            m_turnTimer -= deltaTime;
            if (m_turnTimer <= 0) { // 30 seconds per turn
                switchTurn();
            }
        } else if (m_state = FIRING_PHASE) {
            // Wait for projectile to settle
            bool activeProjectiles = false;
            for (const auto& p : m_projectiles) {
                if (p.isActive) activeProjectiles = true;
            }

            if (!activeProjectiles) {
                checkWinCondition();
                switchTurn();
            }
        }
    }

    void handleInput(int playerId, std::string command, float value = 0.0f) {
        if (m_state != PLAYING_TURN) return;

        Player* currentPlayer = m_players[m_currentTurnIndex];
        if (currentPlayer->getId() != playerId) return;
        if (command == "MOVE_LEFT") {
            currentPlayer->moveLeft();
        } else if (command == "MOVE_RIGHT") {
            currentPlayer->moveRight();
        } else if (command == "STOP") {
            currentPlayer->stopMoving();
        } else if (command == "ADJUST_ANGLE") {
            currentPlayer->adjustAngle(value);
        } else if (command == "ADJUST_POWER") {
            currentPlayer->adjustPower(value);
        } else if (command == "FIRE") {
            m_physics->fireProjectile(currentPlayer, m_projectiles);
            m_state = FIRING_PHASE;
        }
    }
};