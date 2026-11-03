#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::world3d {

struct MapFace {
    double planeNormal[3]{};
    double planeDist = 0.0;
    std::string texture;
    double uAxis[3]{1.0, 0.0, 0.0};
    double vAxis[3]{0.0, -1.0, 0.0};
    double uOffset = 0.0;
    double vOffset = 0.0;
    double uScale = 1.0;
    double vScale = 1.0;
};

struct MapBrush {
    std::vector<MapFace> faces;
};

struct BrushPolygon {
    std::vector<Vector3> points;
    Vector3 normal{};
    std::string texture;
    Vector3 uWorld{};
    Vector3 vWorld{};
    float uOffset = 0.0f;
    float vOffset = 0.0f;

    Vector2 TexelUV(Vector3 p) const;
};

struct MapEntity {
    std::unordered_map<std::string, std::string> props;
    std::vector<MapBrush> brushes;

    const std::string& ClassName() const;
    std::string Get(const std::string& key, const std::string& fallback = "") const;
    float GetFloat(const std::string& key, float fallback = 0.0f) const;
    int GetInt(const std::string& key, int fallback = 0) const;
    Vector3 GetVector(const std::string& key, Vector3 fallback = {0.0f, 0.0f, 0.0f}) const;
    Vector3 GetOrigin() const;
    float GetYawRadians() const;
};

class QuakeMap {
public:
    static constexpr float UNITS_PER_METER = 32.0f;

    bool Load(const std::string& path);
    const std::vector<MapEntity>& Entities() const { return m_entities; }
    const MapEntity* FindFirst(const std::string& className) const;
    std::vector<const MapEntity*> FindAll(const std::string& className) const;
    const std::string& GetError() const { return m_error; }

    static Vector3 ToWorld(double qx, double qy, double qz);
    static Vector3 ToWorldDirection(double qx, double qy, double qz);
    static std::vector<BrushPolygon> BuildPolygons(const MapBrush& brush);
    static std::vector<Vector3> BuildVertices(const MapBrush& brush);
    static BoundingBox BuildBounds(const MapBrush& brush);

private:
    std::vector<MapEntity> m_entities;
    std::string m_error;
};

}
