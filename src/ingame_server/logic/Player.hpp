#pragma once
#include <string>

#define SPEED 2.0f

typedef struct Position {
    float x;
    float y;
    bool orient;
} Position;

typedef struct Velocity {
    float vx;
    float vy;
} Velocity;

class Player {
public:
    Position m_position;
    Velocity m_velocity;
    float m_angle;
    float m_power;
private:
    int m_id;
    std::string m_name;
    int m_hp;
    bool m_isAlive;
    bool m_isMyTurn;

public:
    Player(int id, std::string name, float startX, float startY, bool startOrient)
        : m_id(id), m_name(name), m_hp(100), m_isAlive(true), m_isMyTurn(false) {
            m_position = {startX, startY, startOrient};
            m_velocity = {0.0f, 0.0f};
            m_angle = 45.0f;
            m_power = 0.0f;
        }
    int getId() const { return m_id; }
    Position getPosition() const { return m_position; }
    int getHP() const { return m_hp; }
    bool isAlive() const { return m_isAlive; }

    void setPosition(Position pos) { m_position = pos; }
    void setVelocity(Velocity vel) { m_velocity = vel; }
    void setOrient(bool orient) {m_position.orient = orient; }

    void moveLeft() { m_velocity.vx = -SPEED; }
    void moveRight() { m_velocity.vx = SPEED; }
    void stopMoving() { m_velocity.vx = 0.0f; }
    
    void adjustAngle(float delta) { m_angle += delta; }
    void adjustPower(float delta) { m_power += delta; }

    void takeDamage(int damage) {
        m_hp -= damage;
        if (m_hp <= 0) {
            m_hp = 0;
            m_isAlive = false;
        }
    }
    
    void setTurn(bool isMyTurn) { m_isMyTurn = isMyTurn; }
    bool isMyTurn() const { return m_isMyTurn; }
};