#pragma once

#include "raylib.h"
#include "core/Math3D.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace knopmops::map {

struct FacePolygon {
    std::vector<Vector3> vertices;
    Vector3 normal{};
    Vector2 uvMin{0.0f, 0.0f};
    Vector2 uvMax{1.0f, 1.0f};
};

struct MapFace {
    Vector3 planePoints[3]{};
    std::string texture = "motel_wall";

    double uAxis[3]{1.0, 0.0, 0.0};
    double vAxis[3]{0.0, -1.0, 0.0};
    double uOffset = 0.0;
    double vOffset = 0.0;
    double rotation = 0.0;
    double uScale = 1.0;
    double vScale = 1.0;

    FacePolygon builtPoly;

    Vector2 CalculateUV(Vector3 worldPos) const;
};

struct MapBrush {
    std::string name = "Brush";
    std::vector<MapFace> faces;
    BoundingBox bounds{};
    Vector3 center{};
    bool selected = false;
    bool visible = true;

    void RecalculateBounds();
    void BuildPolygons();
};

struct MapEntity {
    std::string classname = "info_player_start";
    std::unordered_map<std::string, std::string> properties;
    std::vector<MapBrush> brushes;
    Vector3 origin{};
    float yaw = 0.0f;
    bool selected = false;

    std::string Get(const std::string& key, const std::string& fallback = "") const;
    void Set(const std::string& key, const std::string& value);
    float GetFloat(const std::string& key, float fallback = 0.0f) const;
    int GetInt(const std::string& key, int fallback = 0) const;
    Vector3 GetVector(const std::string& key, Vector3 fallback = {0.0f, 0.0f, 0.0f}) const;
    void SyncOriginFromProperties();
    void SyncPropertiesFromOrigin();
};

class MapDocument {
public:
    MapDocument();

    void Clear();
    void CreateDefaultScene();

    const std::string& GetFilePath() const { return m_filePath; }
    void SetFilePath(const std::string& path) { m_filePath = path; }

    bool IsModified() const { return m_isModified; }
    void SetModified(bool mod = true) { m_isModified = mod; }

    MapEntity& GetWorldspawn() { return m_worldspawn; }
    const MapEntity& GetWorldspawn() const { return m_worldspawn; }

    std::vector<MapBrush>& GetWorldBrushes() { return m_worldspawn.brushes; }
    const std::vector<MapBrush>& GetWorldBrushes() const { return m_worldspawn.brushes; }

    std::vector<MapEntity>& GetEntities() { return m_entities; }
    const std::vector<MapEntity>& GetEntities() const { return m_entities; }

    void AddBrush(const MapBrush& brush);
    void RemoveBrush(size_t index);

    void AddEntity(const MapEntity& ent);
    void RemoveEntity(size_t index);

    void DeselectAll();
    void SelectBrush(size_t index, bool addToSelection = false);
    void SelectEntity(size_t index, bool addToSelection = false);

    int GetSelectedBrushIndex() const { return m_selectedBrush; }
    int GetSelectedEntityIndex() const { return m_selectedEntity; }

    void RebuildAllGeometry();

private:
    std::string m_filePath;
    bool m_isModified = false;

    MapEntity m_worldspawn;
    std::vector<MapEntity> m_entities;

    int m_selectedBrush = -1;
    int m_selectedEntity = -1;
};

}

