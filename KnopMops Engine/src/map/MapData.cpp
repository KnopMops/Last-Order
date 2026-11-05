#include "map/MapData.h"
#include "map/BrushBuilder.h"
#include <sstream>
#include <cmath>
#include <algorithm>

namespace knopmops::map {

Vector2 MapFace::CalculateUV(Vector3 worldPos) const {
    Vector3 q = core::WorldToQuake(worldPos);
    double u = (q.x * uAxis[0] + q.y * uAxis[1] + q.z * uAxis[2] + uOffset) / (uScale != 0.0 ? (uScale * 32.0) : 32.0);
    double v = (q.x * vAxis[0] + q.y * vAxis[1] + q.z * vAxis[2] + vOffset) / (vScale != 0.0 ? (vScale * 32.0) : 32.0);
    return Vector2{static_cast<float>(u), static_cast<float>(v)};
}

void MapBrush::RecalculateBounds() {
    if (faces.empty()) {
        bounds = BoundingBox{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
        center = {0.0f, 0.0f, 0.0f};
        return;
    }

    Vector3 minPt{1e9f, 1e9f, 1e9f};
    Vector3 maxPt{-1e9f, -1e9f, -1e9f};
    bool hasVerts = false;

    for (const auto& face : faces) {
        for (const auto& pt : face.builtPoly.vertices) {
            minPt.x = std::min(minPt.x, pt.x);
            minPt.y = std::min(minPt.y, pt.y);
            minPt.z = std::min(minPt.z, pt.z);
            maxPt.x = std::max(maxPt.x, pt.x);
            maxPt.y = std::max(maxPt.y, pt.y);
            maxPt.z = std::max(maxPt.z, pt.z);
            hasVerts = true;
        }
    }

    if (hasVerts) {
        bounds = BoundingBox{minPt, maxPt};
        center = Vector3{
            (minPt.x + maxPt.x) * 0.5f,
            (minPt.y + maxPt.y) * 0.5f,
            (minPt.z + maxPt.z) * 0.5f
        };
    }
}

namespace {

struct DVec {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

inline DVec Sub(DVec a, DVec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline DVec Add(DVec a, DVec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline DVec Scale(DVec a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double Dot(DVec a, DVec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline DVec Cross(DVec a, DVec b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline DVec Normalize(DVec a) {
    double len = std::sqrt(Dot(a, a));
    return len > 1e-12 ? Scale(a, 1.0 / len) : a;
}

struct DPlane {
    DVec normal;
    double dist = 0.0;
};

DPlane PlaneFromPoints(Vector3 p1, Vector3 p2, Vector3 p3) {
    DVec d1{p1.x, p1.y, p1.z};
    DVec d2{p2.x, p2.y, p2.z};
    DVec d3{p3.x, p3.y, p3.z};
    DVec norm = Normalize(Cross(Sub(d2, d1), Sub(d3, d1)));
    return {norm, Dot(norm, d1)};
}

std::vector<DVec> ClipPolygon(const std::vector<DVec>& poly, const DPlane& plane) {
    std::vector<DVec> out;
    if (poly.empty()) return out;

    for (size_t i = 0; i < poly.size(); ++i) {
        DVec curr = poly[i];
        DVec next = poly[(i + 1) % poly.size()];

        double dCurr = Dot(plane.normal, curr) - plane.dist;
        double dNext = Dot(plane.normal, next) - plane.dist;

        if (dCurr <= 1e-5) out.push_back(curr);

        if ((dCurr > 1e-5 && dNext < -1e-5) || (dCurr < -1e-5 && dNext > 1e-5)) {
            double t = dCurr / (dCurr - dNext);
            out.push_back(Add(curr, Scale(Sub(next, curr), t)));
        }
    }
    return out;
}

}

void MapBrush::BuildPolygons() {
    if (faces.size() < 4) return;

    std::vector<DPlane> planes;
    for (const auto& face : faces) {
        planes.push_back(PlaneFromPoints(face.planePoints[0], face.planePoints[1], face.planePoints[2]));
    }

    for (size_t i = 0; i < faces.size(); ++i) {
        const DPlane& p = planes[i];
        DVec up = std::abs(p.normal.z) < 0.9 ? DVec{0, 0, 1} : DVec{1, 0, 0};
        DVec u = Normalize(Cross(up, p.normal));
        DVec v = Cross(p.normal, u);
        DVec origin = Scale(p.normal, p.dist);

        const double HUGE_SZ = 16384.0;
        std::vector<DVec> poly = {
            Add(Add(origin, Scale(u, -HUGE_SZ)), Scale(v, -HUGE_SZ)),
            Add(Add(origin, Scale(u, HUGE_SZ)), Scale(v, -HUGE_SZ)),
            Add(Add(origin, Scale(u, HUGE_SZ)), Scale(v, HUGE_SZ)),
            Add(Add(origin, Scale(u, -HUGE_SZ)), Scale(v, HUGE_SZ))
        };

        for (size_t j = 0; j < planes.size(); ++j) {
            if (i == j) continue;
            poly = ClipPolygon(poly, planes[j]);
            if (poly.size() < 3) break;
        }

        faces[i].builtPoly.vertices.clear();
        if (poly.size() >= 3) {
            for (const auto& pt : poly) {
                faces[i].builtPoly.vertices.push_back(core::QuakeToWorld(pt.x, pt.y, pt.z));
            }
            Vector3 wNorm = core::QuakeToWorld(p.normal.x, p.normal.y, p.normal.z);
            faces[i].builtPoly.normal = core::Vec3Normalize(wNorm);
        }
    }

    RecalculateBounds();
}

std::string MapEntity::Get(const std::string& key, const std::string& fallback) const {
    auto it = properties.find(key);
    return it != properties.end() ? it->second : fallback;
}

void MapEntity::Set(const std::string& key, const std::string& value) {
    properties[key] = value;
    if (key == "origin") SyncOriginFromProperties();
}

float MapEntity::GetFloat(const std::string& key, float fallback) const {
    auto it = properties.find(key);
    if (it == properties.end()) return fallback;
    try {
        return std::stof(it->second);
    } catch (...) {
        return fallback;
    }
}

int MapEntity::GetInt(const std::string& key, int fallback) const {
    auto it = properties.find(key);
    if (it == properties.end()) return fallback;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return fallback;
    }
}

Vector3 MapEntity::GetVector(const std::string& key, Vector3 fallback) const {
    auto it = properties.find(key);
    if (it == properties.end()) return fallback;
    std::stringstream ss(it->second);
    float x = 0, y = 0, z = 0;
    if (ss >> x >> y >> z) {
        return core::QuakeToWorld(x, y, z);
    }
    return fallback;
}

void MapEntity::SyncOriginFromProperties() {
    origin = GetVector("origin", origin);
    yaw = GetFloat("angle", yaw);
}

void MapEntity::SyncPropertiesFromOrigin() {
    Vector3 q = core::WorldToQuake(origin);
    std::stringstream ss;
    ss << static_cast<int>(std::round(q.x)) << " "
       << static_cast<int>(std::round(q.y)) << " "
       << static_cast<int>(std::round(q.z));
    properties["origin"] = ss.str();
    properties["angle"] = std::to_string(static_cast<int>(std::round(yaw)));
}

MapDocument::MapDocument() {
    m_worldspawn.classname = "worldspawn";
    m_worldspawn.Set("fog_color", "20 22 30");
    m_worldspawn.Set("ambient_sky", "40 45 60");
    m_worldspawn.Set("ambient_ground", "20 22 28");
    m_worldspawn.Set("sky_light", "120 130 160");
    m_worldspawn.Set("fog_start", "4.0");
    m_worldspawn.Set("fog_end", "35.0");
}

void MapDocument::Clear() {
    m_filePath.clear();
    m_isModified = false;
    m_worldspawn.brushes.clear();
    m_entities.clear();
    m_selectedBrush = -1;
    m_selectedEntity = -1;
}

void MapDocument::CreateDefaultScene() {
    Clear();

    MapBrush floor = BrushBuilder::CreateBox(
        Vector3{-6.0f, -0.2f, -6.0f},
        Vector3{6.0f, 0.0f, 6.0f},
        "motel_carpet"
    );
    floor.name = "Room_Floor";
    AddBrush(floor);

    MapEntity playerStart;
    playerStart.classname = "info_player_start";
    playerStart.origin = Vector3{0.0f, 0.1f, 2.0f};
    playerStart.yaw = 0.0f;
    playerStart.SyncPropertiesFromOrigin();
    AddEntity(playerStart);

    MapEntity lampLight;
    lampLight.classname = "light";
    lampLight.origin = Vector3{0.0f, 2.5f, 0.0f};
    lampLight.Set("light", "250");
    lampLight.Set("_color", "255 240 200");
    lampLight.SyncPropertiesFromOrigin();
    AddEntity(lampLight);

    RebuildAllGeometry();
    m_isModified = false;
}

void MapDocument::AddBrush(const MapBrush& brush) {
    m_worldspawn.brushes.push_back(brush);
    m_worldspawn.brushes.back().BuildPolygons();
    m_isModified = true;
}

void MapDocument::RemoveBrush(size_t index) {
    if (index < m_worldspawn.brushes.size()) {
        m_worldspawn.brushes.erase(m_worldspawn.brushes.begin() + index);
        if (m_selectedBrush == static_cast<int>(index)) m_selectedBrush = -1;
        else if (m_selectedBrush > static_cast<int>(index)) --m_selectedBrush;
        m_isModified = true;
    }
}

void MapDocument::AddEntity(const MapEntity& ent) {
    m_entities.push_back(ent);
    m_isModified = true;
}

void MapDocument::RemoveEntity(size_t index) {
    if (index < m_entities.size()) {
        m_entities.erase(m_entities.begin() + index);
        if (m_selectedEntity == static_cast<int>(index)) m_selectedEntity = -1;
        else if (m_selectedEntity > static_cast<int>(index)) --m_selectedEntity;
        m_isModified = true;
    }
}

void MapDocument::DeselectAll() {
    for (auto& b : m_worldspawn.brushes) b.selected = false;
    for (auto& e : m_entities) e.selected = false;
    m_selectedBrush = -1;
    m_selectedEntity = -1;
}

void MapDocument::SelectBrush(size_t index, bool addToSelection) {
    if (!addToSelection) DeselectAll();
    if (index < m_worldspawn.brushes.size()) {
        m_worldspawn.brushes[index].selected = true;
        m_selectedBrush = static_cast<int>(index);
        m_selectedEntity = -1;
    }
}

void MapDocument::SelectEntity(size_t index, bool addToSelection) {
    if (!addToSelection) DeselectAll();
    if (index < m_entities.size()) {
        m_entities[index].selected = true;
        m_selectedEntity = static_cast<int>(index);
        m_selectedBrush = -1;
    }
}

void MapDocument::RebuildAllGeometry() {
    for (auto& b : m_worldspawn.brushes) {
        b.BuildPolygons();
    }
    for (auto& ent : m_entities) {
        for (auto& b : ent.brushes) {
            b.BuildPolygons();
        }
    }
}

}

