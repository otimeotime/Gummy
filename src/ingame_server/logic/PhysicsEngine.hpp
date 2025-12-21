#pragma once
#include "Player.hpp"
#include <vector>
#include <cmath>

#define PI 3.14159265f

struct Projectile {
    Position position;
    Velocity velocity;
    bool isActive;
    int ownerId;
};

class PhysicsEngine {
private:
    const float GRAVITY;
    float WIND;

    bool checkCollision(const Projectile& proj, const Player* p) {
        float dx = proj.position.x - p->getPosition().x;
        float dy = proj.position.y - p->getPosition().y;
        return (std::sqrt(dx*dx + dy*dy) < 20.0f);
    }

public:
    PhysicsEngine(): GRAVITY(0.98f), WIND(0.0f) {}

    void update(float deltaTime, std::vector<Player*>& players, std::vector<Projectile>& projectiles) {
        // Update players
        for (auto p : players) {
            // Skip if player is dead
            if(!p->isAlive()) continue;

            // Apply gravity
            p->m_velocity.vy += GRAVITY * deltaTime;

            // Apply velocity
            p->m_position.x += p->m_velocity.vx * deltaTime;
            p->m_position.y += p->m_velocity.vy * deltaTime;

            // Floor collision
            if (p->m_position.y >= 500.0f) {
                p->m_position.y = 500.0f;
                p->m_velocity.vy = 0;
            }

            // Boundary checks
            if (p->m_position.x < 0) p->m_position.x = 0;
            if (p->m_position.x > 1280) p->m_position.x = 1280;
        }

        // Update projectiles
        for (auto& proj : projectiles) {
            // Skip inactive projectiles
            if (!proj.isActive) continue;

            // Apply gravity and wind
            proj.velocity.vx += WIND * deltaTime;
            proj.velocity.vy += GRAVITY * deltaTime;

            // Apply velocity
            proj.position.x += proj.velocity.vx * deltaTime;
            proj.position.y += proj.velocity.vy * deltaTime;

            // Floor collision
            if (proj.position.y >= 500.0f) {
                proj.position.y = 500.0f;
                proj.isActive = false; // Deactivate projectile on hit
            }

            // Boundary checks
            if (proj.position.x < 0 || proj.position.x > 1280) {
                proj.isActive = false; // Deactivate projectile if out of bounds
            }

            // Check collision with players
            for (auto p : players) {
                // Don't affect the owner
                if (p->getId() != proj.ownerId && p->isAlive()) {
                    if (checkCollision(proj, p)) {
                        p->takeDamage(25);
                        proj.isActive = false;
                        break;
                    }
                }
            }
        }
    }
            
    // Calculate initial velocity of projectile
    void fireProjectile(Player* p, std::vector<Projectile>& projectiles) {
        float rad = p->m_angle * (PI / 180.0f);
        Projectile proj;
        proj.position.x = p->m_position.x;
        proj.position.y = p->m_position.y - 20;
        proj.velocity.vx = std::cos(rad) * p->m_power * 10.0f;
        proj.velocity.vy = -std::sin(rad) * p->m_power * 10.0f;
        proj.isActive = true;
        proj.ownerId = p->getId();
        projectiles.push_back(proj);
    }

    void setWind(float wind) {
        WIND = wind;
    }
};