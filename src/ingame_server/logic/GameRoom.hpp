#pragma once
#include "Player.hpp"
#include "PhysicsEngine.hpp"
#include "MapLoader.hpp"
#include <vector>
#include <iostream>
#include <string>

#define TIMER 10.0f

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
    Player* m_pendingShooter;
    bool m_waitingForShot;
    void switchTurn() {
        m_players[m_currentTurnIndex]->setTurn(false);
        m_players[m_currentTurnIndex]->stopMoving();

        do {
            m_currentTurnIndex = (m_currentTurnIndex + 1) % m_players.size();
        } while (!m_players[m_currentTurnIndex]->isAlive());

        m_players[m_currentTurnIndex]->setTurn(true);
        m_turnTimer = TIMER;
        m_state = PLAYING_TURN;
        m_pendingShooter = nullptr;
        m_waitingForShot = false;

        m_physics->setWind((rand() % 20) - 10.0f);
    }

    bool hasActiveProjectile(const std::vector<Projectile>& projectiles) const {
        for (const auto& p : projectiles) {
            if (p.isActive) return true;
        }
        return false;
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
    GameRoom(const std::vector<Player*>& players)
        : m_state(WAITING_FOR_PLAYERS),
          m_currentTurnIndex(0),
          m_turnTimer(0.0f),
          m_mapLoader(nullptr),
          m_pendingShooter(nullptr),
          m_waitingForShot(false),
          m_players(players) {
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
            m_turnTimer = TIMER;
            m_pendingShooter = nullptr;
            m_waitingForShot = false;
            std::cout << "Game Started! Player " << m_players[0]->getId() << "'s turn." << std::endl;
        }
    }

    float getTurnTimer() const { return m_turnTimer; }

    RoomState getState() const { return m_state; }

    Player* getCurrentPlayer() const {
        if (m_players.empty()) return nullptr;
        return m_players[m_currentTurnIndex];
    }

    // Commit the shot for the current turn (ENTER or timeout). Returns true only once.
    bool commitShot() {
        if (m_state != PLAYING_TURN) return false;
        Player* shooter = getCurrentPlayer();
        if (!shooter || !shooter->isAlive()) return false;
        m_state = FIRING_PHASE;
        m_pendingShooter = shooter;
        m_waitingForShot = true;
        return true;
    }

    // Scene consumes this once to perform the actual PhysicsEngine::fireProjectile.
    Player* takePendingShooter() {
        Player* result = m_pendingShooter;
        m_pendingShooter = nullptr;
        return result;
    }

    // Scene calls this right after firing to allow the room to wait for projectile settle.
    void notifyShotFired() { m_waitingForShot = false; }

    // Update using Scene-owned projectile list (recommended for the current client architecture).
    void update(float deltaTime, const std::vector<Projectile>& externalProjectiles) {
        if (m_state == PLAYING_TURN) {
            m_turnTimer -= deltaTime;
            if (m_turnTimer <= 0.0f) {
                commitShot();
            }
            return;
        }

        if (m_state == FIRING_PHASE) {
            // Ensure the shot happens exactly once before we start waiting.
            if (m_waitingForShot) return;

            if (!hasActiveProjectile(externalProjectiles)) {
                checkWinCondition();
                switchTurn();
            }
        }
    }

    // Backward-compatible update for existing code paths.
    void update(float deltaTime) {
        update(deltaTime, m_projectiles);
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