#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cmath>

struct SpawnPoint {
    float x, y;
};

class MapLoader {
public:
    MapLoader() : m_width(0), m_height(0) {}

    bool loadMap(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        file >> m_width >> m_height;

        m_collisionMask.resize(m_width * m_height);

        char input;
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                file >> input;
                int index = y * m_width + x;
                m_collisionMask[index] = (input == '1');
            }
        }

        file.close();

        // Generate 2 random spawn X coordinates with a minimum separation.
        static bool seeded = false;
        if (!seeded) {
            std::srand((unsigned)std::time(nullptr));
            seeded = true;
        }

        m_spawnPoints.clear();

        const float minSeparation = 600.0f;
        const float maxX = (m_width > 0) ? (float)(m_width - 1) : 0.0f;

        auto randomX = [&]() -> float {
            if (maxX <= 0.0f) return 0.0f;
            return (float)std::rand() / (float)RAND_MAX * maxX;
        };

        float spawnX_1 = randomX();
        float spawnX_2 = randomX();
        int attempts = 0;
        while (std::fabs(spawnX_1 - spawnX_2) < minSeparation && attempts++ < 2000) {
            spawnX_2 = randomX();
        }

        m_spawnPoints.push_back({spawnX_1, 100.0f});
        m_spawnPoints.push_back({spawnX_2, 100.0f});
        
        return true;
    }

    bool isSolid(float x, float y) const {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
            if (y >= m_height) return false;
            if (x < 0 || x >= m_width) return true;
            return false;
        }

        int index = (int)y * m_width + (int)x;
        return m_collisionMask[index];
    }

    void applyExplosion(float x, float y, float radius) {
        int minX = (int)(x - radius);
        int maxX = (int)(x + radius);
        int minY = (int)(y - radius);
        int maxY = (int)(y + radius);

        for (int cy = minY; cy <= maxY; ++cy) {
            for (int cx = minX; cx <= maxX; ++cx) {
                if (cx >= 0 && cx < m_width && cy >= 0 && cy < m_height) {
                    float dx = cx - x;
                    float dy = cy - y;
                    if ((dx * dx + dy * dy) <= (radius * radius)) {
                        m_collisionMask[cy * m_width + cx] = false;
                    }
                }
            }
        }
    }

    const std::vector<SpawnPoint>& getSpawnPoints() const {
        return m_spawnPoints;
    }

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    int m_width;
    int m_height;
    std::vector<bool> m_collisionMask;
    std::vector<SpawnPoint> m_spawnPoints;
};