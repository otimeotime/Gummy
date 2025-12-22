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

        m_physics->setWind(0);
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
                : m_players(players),
                    m_projectiles(),
                    m_physics(new PhysicsEngine()),
                    m_state(WAITING_FOR_PLAYERS),
                    m_currentTurnIndex(0),
                    m_turnTimer(0.0f),
                    m_mapLoader(nullptr),
                    m_pendingShooter(nullptr),
                    m_waitingForShot(false) {}

    ~GameRoom() { delete m_physics; }

    void setMapLoader(MapLoader* mapLoader) { m_mapLoader = mapLoader; }

    const std::vector<Player*>& getPlayers() const { return m_players; }
    const std::vector<Projectile>& getProjectiles() const { return m_projectiles; }

    Player* getPlayerById(int id) const {
        for (auto* p : m_players) {
            if (p && p->getId() == id) return p;
        }
        return nullptr;
    }

    bool consumeTerrainModified() {
        if (!m_physics) return false;
        if (!m_physics->hasTerrainBeenModified()) return false;
        m_physics->resetTerrainModifiedFlag();
        return true;
    }

    bool consumeLastExplosion(float& outX, float& outY, float& outRadius) {
        if (!m_physics) return false;
        return m_physics->consumeLastExplosion(outX, outY, outRadius);
    }

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
        if (!m_physics || !m_mapLoader) return false;

        // Fire immediately; GameRoom owns the projectiles/physics.
        m_physics->fireProjectile(shooter, m_projectiles);
        shooter->m_power = 0.0f;

        m_state = FIRING_PHASE;
        m_pendingShooter = nullptr;
        m_waitingForShot = false;
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
        // Use real delta for physics on the server; clamp to keep stability.
        float physicsDt = deltaTime;
        if (physicsDt < 0.0f) physicsDt = 0.0f;
        if (physicsDt > 0.033f) physicsDt = 0.033f; // ~30 FPS worst-case step

        if (m_physics && m_mapLoader) {
            m_physics->update(physicsDt, m_players, m_projectiles, m_mapLoader);
        }

        if (m_state == PLAYING_TURN) {
            m_turnTimer -= deltaTime;
            if (m_turnTimer <= 0.0f) {
                commitShot();
            }
            return;
        }

        if (m_state == FIRING_PHASE) {
            if (!hasActiveProjectile(m_projectiles)) {
                checkWinCondition();
                if (m_state != GAME_OVER) {
                    switchTurn();
                }
            }
        }
    }

    void handleInput(int playerId, std::string command, float value = 0.0f) {
        if (m_state != PLAYING_TURN) return;

        Player* currentPlayer = m_players[m_currentTurnIndex];
        if (currentPlayer->getId() != playerId) return;
        if (command == "MOVE_LEFT") {
            currentPlayer->moveLeft();
            currentPlayer->setOrient(0);
        } else if (command == "MOVE_RIGHT") {
            currentPlayer->moveRight();
            currentPlayer->setOrient(1);
        } else if (command == "STOP") {
            currentPlayer->stopMoving();
        } else if (command == "ADJUST_ANGLE") {
            currentPlayer->adjustAngle(value);
            if (currentPlayer->m_angle < 0.0f) currentPlayer->m_angle = 0.0f;
            if (currentPlayer->m_angle > 180.0f) currentPlayer->m_angle = 180.0f;
        } else if (command == "ADJUST_POWER") {
            currentPlayer->adjustPower(value);
            if (currentPlayer->m_power < 0.0f) currentPlayer->m_power = 0.0f;
            if (currentPlayer->m_power > 100.0f) currentPlayer->m_power = 100.0f;
        } else if (command == "FIRE") {
            commitShot();
        }
    }
};