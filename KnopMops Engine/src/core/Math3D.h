#pragma once

#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace knopmops::core {

constexpr float QUAKE_UNITS_PER_METER = 32.0f;

inline Vector3 QuakeToWorld(double qx, double qy, double qz) {
    return Vector3{
        static_cast<float>(qx / QUAKE_UNITS_PER_METER),
        static_cast<float>(qz / QUAKE_UNITS_PER_METER),
        static_cast<float>(-qy / QUAKE_UNITS_PER_METER)
    };
}

inline Vector3 QuakeToWorld(Vector3 q) {
    return QuakeToWorld(q.x, q.y, q.z);
}

inline Vector3 WorldToQuake(Vector3 w) {
    return Vector3{
        w.x * QUAKE_UNITS_PER_METER,
        -w.z * QUAKE_UNITS_PER_METER,
        w.y * QUAKE_UNITS_PER_METER
    };
}

inline Vector3 Vec3Add(Vector3 a, Vector3 b) { return Vector3Add(a, b); }
inline Vector3 Vec3Sub(Vector3 a, Vector3 b) { return Vector3Subtract(a, b); }
inline Vector3 Vec3Scale(Vector3 v, float s) { return Vector3Scale(v, s); }
inline float Vec3Dot(Vector3 a, Vector3 b) { return Vector3DotProduct(a, b); }
inline Vector3 Vec3Cross(Vector3 a, Vector3 b) { return Vector3CrossProduct(a, b); }
inline float Vec3Length(Vector3 v) { return Vector3Length(v); }
inline Vector3 Vec3Normalize(Vector3 v) { return Vector3Normalize(v); }

struct Plane3D {
    Vector3 normal{};
    float dist = 0.0f;

    Plane3D() = default;
    Plane3D(Vector3 n, float d) : normal(Vec3Normalize(n)), dist(d) {}
    Plane3D(Vector3 p1, Vector3 p2, Vector3 p3) {
        Vector3 edge1 = Vec3Sub(p2, p1);
        Vector3 edge2 = Vec3Sub(p3, p1);
        normal = Vec3Normalize(Vec3Cross(edge1, edge2));
        dist = Vec3Dot(normal, p1);
    }

    float DistanceToPoint(Vector3 p) const {
        return Vec3Dot(normal, p) - dist;
    }
};

inline bool RayIntersectsBox(Ray ray, BoundingBox box, float* outDist = nullptr) {
    RayCollision col = GetRayCollisionBox(ray, box);
    if (col.hit) {
        if (outDist) *outDist = col.distance;
        return true;
    }
    return false;
}

inline float SnapValue(float value, float snapStep) {
    if (snapStep <= 0.001f) return value;
    return std::round(value / snapStep) * snapStep;
}

inline Vector3 SnapVector(Vector3 v, float snapStep) {
    return Vector3{
        SnapValue(v.x, snapStep),
        SnapValue(v.y, snapStep),
        SnapValue(v.z, snapStep)
    };
}

}

