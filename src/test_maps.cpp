#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace fs = std::filesystem;

void DisplayMapThumbnail(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << path << std::endl;
        return;
    }

    int width, height;
    file >> width >> height;

    // Read the newline after dimensions
    std::string line;
    std::getline(file, line); 

    std::vector<std::string> grid;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            grid.push_back(line);
        }
    }
    file.close();

    std::cout << "\n=== Map: " << path << " (" << width << "x" << height << ") ===" << std::endl;

    // Downsample for console display
    // Console parsed width ~ 1280 -> target 80 chars => step X = 16
    // Console parsed height ~ 720 -> target 20 chars => step Y = 36
    const int stepX = 16;
    const int stepY = 36;

    // Top border
    for (int x = 0; x < width; x += stepX) std::cout << "-";
    std::cout << std::endl;

    for (int y = 0; y < height && y < (int)grid.size(); y += stepY) {
        for (int x = 0; x < width && x < (int)grid[y].size(); x += stepX) {
            bool solid = (grid[y][x] == '1');
            // Check neighborhood for better visibility of thin lines
            if (!solid) {
                 for(int dy=0; dy<stepY && (y+dy)<grid.size(); ++dy) {
                     for(int dx=0; dx<stepX && (x+dx)<grid[y+dy].size(); ++dx) {
                         if (grid[y+dy][x+dx] == '1') {
                             solid = true;
                             break;
                         }
                     }
                     if(solid) break;
                 }
            }
            std::cout << (solid ? "#" : " ");
        }
        std::cout << "|" << std::endl;
    }

    // Bottom border
    for (int x = 0; x < width; x += stepX) std::cout << "-";
    std::cout << std::endl;
}

int main() {
    std::cout << "Scanning assets/maps/ for .txt files..." << std::endl;
    std::vector<std::string> maps;
    const std::string mapsDir = "assets/maps";

    try {
        if (fs::exists(mapsDir) && fs::is_directory(mapsDir)) {
            for (const auto& entry : fs::directory_iterator(mapsDir)) {
                 if (entry.path().extension() == ".txt") {
                     maps.push_back(entry.path().string());
                 }
            }
        } else {
            std::cerr << "Directory " << mapsDir << " does not exist or is not a directory." << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    if (maps.empty()) {
        std::cout << "No maps found." << std::endl;
    } else {
        std::cout << "Found " << maps.size() << " maps:" << std::endl;
        for (const auto& map : maps) {
            DisplayMapThumbnail(map);
        }
    }

    return 0;
}
