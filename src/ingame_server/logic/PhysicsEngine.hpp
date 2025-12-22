#pragma once
#include "Player.hpp"
#include "MapLoader.hpp"
#include <vector>
#include <cmath>

#define PI 3.14159265f
#define gameSpeed 40.0f

struct Projectile {
    Position position;
    Velocity velocity;
    bool isActive;
};

class PhysicsEngine {
private:
    const float GRAVITY;
    float WIND;
    bool m_terrainModified;
    bool m_hasLastExplosion;
    float m_lastExplosionX;
    float m_lastExplosionY;
    float m_lastExplosionRadius;

    bool checkCollision(const Projectile& proj, const Player* p) {
        float dx = proj.position.x - p->getPosition().x;
        float dy = proj.position.y - p->getPosition().y;
        return (std::sqrt(dx*dx + dy*dy) < 20.0f);
    }

public:
    PhysicsEngine()
                : GRAVITY(0.98f),
          WIND(0.0f),
          m_terrainModified(false),
          m_hasLastExplosion(false),
          m_lastExplosionX(0.0f),
          m_lastExplosionY(0.0f),
          m_lastExplosionRadius(0.0f) {}

    bool hasTerrainBeenModified() const { return m_terrainModified; }
    void resetTerrainModifiedFlag() { m_terrainModified = false; }

    bool consumeLastExplosion(float& outX, float& outY, float& outRadius) {
        if (!m_hasLastExplosion) return false;
        outX = m_lastExplosionX;
        outY = m_lastExplosionY;
        outRadius = m_lastExplosionRadius;
        m_hasLastExplosion = false;
        return true;
    }

    void update(float deltaTime, std::vector<Player*>& players, std::vector<Projectile>& projectiles, MapLoader* map) {
        // The game constants (SPEED, GRAVITY, power -> velocity) are tuned for a ~60 FPS fixed step.
        // The engine provides deltaTime in seconds, so convert to a 60 FPS-scaled step to avoid
        // slow-motion movement/projectiles.
        float simDt = deltaTime * gameSpeed;
        if (simDt < 0.0f) simDt = 0.0f;
        if (simDt > 3.0f) simDt = 3.0f; // clamp (prevents huge jumps on stalls)

        // Update players
        for (auto p : players) {
            if(!p->isAlive()) continue;

            // Apply gravity
            p->m_velocity.vy += GRAVITY * simDt;

            // Apply velocity
            p->m_position.x += p->m_velocity.vx * simDt;
            p->m_position.y += p->m_velocity.vy * simDt;

            // --- NEW: PIXEL-PERFECT COLLISION ---
            
            // 1. Calculate the position of the player's feet (bottom-center)
            // Assuming the sprite is approx 32x32 pixels. Adjust offsets if your sprite is different.
            float feetX = p->m_position.x + 16; 
            float feetY = p->m_position.y + 32;

            // 2. Check if the feet are inside a solid pixel
            if (map->isSolid(feetX, feetY)) {
                // Stop falling
                p->m_velocity.vy = 0;
                
                // Optional: Snap to top of the pixel to prevent sinking
                // A simple way is to move them up pixel by pixel until not solid, 
                // but strictly setting vy=0 on contact is a good start.
                // For smoother landing, we might nudge Y up slightly:
                while (map->isSolid(feetX, feetY) && feetY > 0) {
                    p->m_position.y -= 1.0f;
                    feetY -= 1.0f;
                }
            }

            // Boundary checks
            if (p->m_position.x < 0) p->m_position.x = 0;
            if (p->m_position.x > 1280) p->m_position.x = 1280;
            
            // Hard floor safety net (below screen)
            if (p->m_position.y > 720) { 
                p->m_position.y = 0; // Respawn at top if they fall out of world
            }
        }

        // Update Projectile
        for (auto& proj : projectiles) {
            if (!proj.isActive) continue;

            // Apply gravity
            proj.velocity.vy += GRAVITY * simDt;

            // Apply wind
            proj.velocity.vx += WIND * simDt;

            // Update position
            proj.position.x += proj.velocity.vx * simDt;
            proj.position.y += proj.velocity.vy * simDt;

            // Check collision with map
            if (map->isSolid(proj.position.x, proj.position.y)) {
                // Create an explosion effect with radius of 30 pixels
                map->applyExplosion(proj.position.x, proj.position.y, 30.0f);
                m_terrainModified = true;  // Mark terrain as modified
                m_hasLastExplosion = true;
                m_lastExplosionX = proj.position.x;
                m_lastExplosionY = proj.position.y;
                m_lastExplosionRadius = 30.0f;
                proj.isActive = false;
                continue;
            }

            // Check collision with players
            for (auto p : players) {
                if (p->isAlive()) {
                    if (checkCollision(proj, p)) {
                        std::cerr << "Player " << p->getId() << " took damage" << std::endl;
                        proj.isActive = false;
                        p->takeDamage(10); // Deal 10 damage on hit
                        break;
                    }
                }
            }

            // Boundary check - deactivate if out of bounds
            if (proj.position.x < 0 || proj.position.x > 1280 ||
                proj.position.y < 0 || proj.position.y > 720) {
                proj.isActive = false;
            }
        }
    }
            
    // Calculate initial velocity of projectile based on angle and player orientation
    void fireProjectile(Player* p, std::vector<Projectile>& projectiles) {
        float rad = p->m_angle * (PI / 180.0f);
        Projectile proj;
        proj.position.x = p->m_position.x;
        proj.position.y = p->m_position.y - 20;
        float directionMult = p->m_position.orient ? 1.0f : -1.0f;
        
        proj.velocity.vx = std::cos(rad) * p->m_power * 1.0f * directionMult;
        proj.velocity.vy = -std::sin(rad) * p->m_power * 1.0f;
        proj.isActive = true;
        projectiles.push_back(proj);
    }

    void setWind(float wind) {
        WIND = wind;
        std::cout << "Wind set to: " << WIND << std::endl;
    }
};