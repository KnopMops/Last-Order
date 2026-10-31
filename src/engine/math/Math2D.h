#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>

namespace engine::math {

constexpr float PI_F = 3.14159265358979323846f;
constexpr float DEG2RAD_F = PI_F / 180.0f;
constexpr float RAD2DEG_F = 180.0f / PI_F;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}
    constexpr Vec2(Vector2 v) : x(v.x), y(v.y) {}

    constexpr operator Vector2() const { return Vector2{x, y}; }

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    float LengthSq() const { return x * x + y * y; }
    float Length() const { return std::sqrt(x * x + y * y); }

    Vec2 Normalized() const {
        float len = Length();
        return (len > 1e-5f) ? Vec2{x / len, y / len} : Vec2{0.0f, 0.0f};
    }

    Vec2 SnapToPixel() const {
        return {std::round(x), std::round(y)};
    }
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    constexpr float Left() const { return x; }
    constexpr float Right() const { return x + w; }
    constexpr float Top() const { return y; }
    constexpr float Bottom() const { return y + h; }
    constexpr Vec2 Center() const { return {x + w * 0.5f, y + h * 0.5f}; }

    constexpr operator Rectangle() const { return Rectangle{x, y, w, h}; }

    constexpr bool Contains(Vec2 p) const {
        return p.x >= x && p.x <= (x + w) && p.y >= y && p.y <= (y + h);
    }

    constexpr bool Intersects(const Rect& o) const {
        return !(Right() <= o.Left() || Left() >= o.Right() ||
                 Bottom() <= o.Top() || Top() >= o.Bottom());
    }
};

inline float Clamp(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * Clamp(t, 0.0f, 1.0f);
}

inline float Approach(float current, float target, float delta) {
    if (current < target) return std::min(current + delta, target);
    if (current > target) return std::max(current - delta, target);
    return target;
}

inline float Distance(Vec2 a, Vec2 b) {
    return (b - a).Length();
}

inline float Dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

inline float AngleDiffDeg(float a, float b) {
    float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
    return d;
}

inline bool PointInCone(Vec2 origin, float dirDeg, float fovDeg, float maxRange, Vec2 target) {
    Vec2 toTarget = target - origin;
    float distSq = toTarget.LengthSq();
    if (distSq > maxRange * maxRange) return false;
    if (distSq < 4.0f) return true;
    float targetDeg = std::atan2(toTarget.y, toTarget.x) * RAD2DEG_F;
    return std::abs(AngleDiffDeg(dirDeg, targetDeg)) <= (fovDeg * 0.5f);
}

}
