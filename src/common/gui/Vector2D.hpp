#pragma once

class Vector2D {
public:
    float x;
    float y;

    // Constructors
    Vector2D(float _x = 0, float _y = 0) : x(_x), y(_y) {}

    // -------------------------------------------------------------------------
    // BASIC OPERATIONS
    // -------------------------------------------------------------------------

    // Addition: Result = this + v2
    Vector2D operator+(const Vector2D& v2) const {
        return Vector2D(x + v2.x, y + v2.y);
    }

    // Subtraction: Result = this - v2
    Vector2D operator-(const Vector2D& v2) const {
        return Vector2D(x - v2.x, y - v2.y);
    }

    // Multiplication: Result = this * scalar number
    Vector2D operator*(float scalar) const {
        return Vector2D(x * scalar, y * scalar);
    }

    // Compound Addition: this += v2
    void operator+=(const Vector2D& v2) {
        x += v2.x;
        y += v2.y;
    }
};