#pragma once
#include <string>

class GameState {
public:
    virtual ~GameState() {}

    // Called every frame. Handles game logic (input, movement, physics).
    virtual void update() = 0;

    // Called every frame. Handles drawing to the screen.
    virtual void render() = 0;

    // Called ONCE when the state is first loaded (e.g., load textures).
    virtual bool onEnter() = 0;

    // Called ONCE when leaving the state (e.g., free textures).
    virtual bool onExit() = 0;

    // Unique ID to identify the state (e.g., "LOGIN", "PLAY").
    virtual std::string getStateID() const = 0;
};