#pragma once
#include "engine/3d/Physics3D.h"
#include "engine/core/Engine.h"
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace game::chapter3 {

struct HouseBox {
    Vector3 min{};
    Vector3 max{};
    std::string side;
    std::string top;
    std::string bottom;
    Color tint{255, 255, 255, 255};
    float uvScale = 1.5f;
    float cell = 0.8f;
    float emissive = 0.0f;
    bool solid = true;
    bool occluder = true;
    bool fitUV = false;
    std::string tag;
};

struct HouseBatch {
    Mesh mesh{};
    Material material{};
    float emissive = 0.0f;
    std::string texture;
    std::string tag;
};

struct HouseLamp {
    Vector3 position{};
    Vector3 color{1.0f, 0.8f, 0.55f};
    float radius = 6.0f;
    float intensity = 1.0f;
};

struct HouseTree {
    Vector3 position{};
    float scale = 1.0f;
};

class HouseLevel {
public:
    void Build(engine::core::Engine& eng, engine::physics3d::Physics3D& physics, Shader lit);
    void Unload();

    const std::vector<HouseBatch>& Batches() const { return m_batches; }
    const std::vector<HouseTree>& Trees() const { return m_trees; }
    const std::vector<HouseLamp>& Lamps() const { return m_lamps; }
    const Mesh& TrunkMesh() const { return m_trunk; }
    const Mesh& CrownMesh() const { return m_crown; }
    const Material& BarkMaterial() const { return m_barkMat; }
    const Material& PineMaterial() const { return m_pineMat; }

    Vector3 PlayerStart() const { return m_playerStart; }
    float PlayerStartYaw() const { return m_playerYaw; }
    bool IsConcreteFloor(Vector3 feet) const;

private:
    void Box(Vector3 mn, Vector3 mx, const std::string& side, const std::string& top, const std::string& bottom,
             Color tint = WHITE, float uv = 1.5f);
    void Solid(Vector3 mn, Vector3 mx, const std::string& tex, Color tint = WHITE, float uv = 1.5f);
    void Fitted(Vector3 mn, Vector3 mx, const std::string& face, const std::string& rest, Color tint = WHITE);
    void Deco(Vector3 mn, Vector3 mx, const std::string& tex, Color tint, float emissive, const std::string& tag = "");
    void WallX(float x, float z0, float z1, float y0, float y1, const std::string& texNeg, const std::string& texPos,
               const std::vector<Vector3>& openings, Color tintNeg = WHITE, Color tintPos = WHITE);
    void WallZ(float z, float x0, float x1, float y0, float y1, const std::string& texNeg, const std::string& texPos,
               const std::vector<Vector3>& openings, Color tintNeg = WHITE, Color tintPos = WHITE);
    void Slab(float x0, float x1, float z0, float z1, float yTop, float thick, const std::string& top,
              const std::string& bottom, Color tint = WHITE);
    void Stairs(float x0, float x1, float zStart, float dir, float yBottom, float yTop, const std::string& tex);
    void Railing(Vector3 mn, Vector3 mx);
    void Lamp(Vector3 pos, Vector3 color, float radius, float intensity);
    void Window(Vector3 mn, Vector3 mx);

    void BuildShell();
    void BuildBasement();
    void BuildFirstFloor();
    void BuildSecondFloor();
    void BuildAttic();
    void BuildGarage();
    void BuildExterior();

    void GenerateMeshes(engine::core::Engine& eng, Shader lit);
    Texture2D ResolveTexture(engine::core::Engine& eng, const std::string& name);
    bool SegmentBlocked(Vector3 a, Vector3 b, const std::vector<int>& candidates) const;

    std::vector<HouseBox> m_boxes;
    std::vector<HouseLamp> m_lamps;
    std::vector<HouseBatch> m_batches;
    std::vector<HouseTree> m_trees;
    std::unordered_map<std::string, Texture2D> m_textures;
    Mesh m_trunk{};
    Mesh m_crown{};
    Material m_barkMat{};
    Material m_pineMat{};
    bool m_treeMeshes = false;
    Vector3 m_playerStart{};
    float m_playerYaw = 0.0f;
};

}
