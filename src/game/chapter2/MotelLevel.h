#pragma once
#include "engine/3d/Lighting3D.h"
#include "engine/3d/Physics3D.h"
#include "engine/3d/QuakeMap.h"
#include "engine/core/Engine.h"
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace game::chapter2 {

using engine::physics3d::BodyHandle;
using engine::physics3d::INVALID_BODY;

struct RenderBatch {
    Mesh mesh{};
    Material material{};
    bool emissive = false;
};

struct DoorObject {
    std::string name;
    std::string label;
    std::string lockItem;
    std::string lockedEvent;
    std::string unlockEvent;
    BodyHandle body = INVALID_BODY;
    std::vector<RenderBatch> batches;
    Vector3 hinge{};
    Vector3 restCenter{};
    float openAngle = 100.0f * DEG2RAD;
    bool bothWays = true;
    bool locked = false;
    bool open = false;
};

struct PropObject {
    BodyHandle body = INVALID_BODY;
    std::vector<RenderBatch> batches;
    std::string label;
    float mass = 10.0f;
    Vector3 lastVelocity{};
    float impactCooldown = 0.0f;
};

struct InteractObject {
    std::string event;
    std::string label;
    BoundingBox bounds{};
    std::vector<BodyHandle> bodies;
    Vector3 center{};
};

struct TriggerObject {
    std::string event;
    BoundingBox bounds{};
    bool once = true;
    bool fired = false;
    bool inside = false;
};

struct MirrorObject {
    RenderBatch surface;
    Vector3 center{};
    Vector3 normal{};
    float planeDistance = 0.0f;
    std::string lookEvent;
    bool fired = false;
    float lookTime = 0.0f;
    bool stingPlayed = false;
};

struct ItemObject {
    std::string item;
    Vector3 position{};
    float yaw = 0.0f;
    std::string requireFlag;
    bool taken = false;
    Vector3 bakedLight{};
    Vector3 bakedPower{};
};

struct HideSpot {
    Vector3 eye{};
    float yaw = 0.0f;
    std::string label;
};

struct LightObject {
    Vector3 position{};
    Vector3 color{1.0f, 1.0f, 1.0f};
    float radius = 5.0f;
    float intensity = 1.0f;
    std::string group;
    std::string style;
    bool dynamic = false;
    float phase = 0.0f;
};

struct SoundEmitter {
    Vector3 position{};
    std::string sound;
    float interval = 10.0f;
    float volume = 0.6f;
    float radius = 12.0f;
    std::string flag;
    std::string hideFlag;
    float timer = 0.0f;
};

struct Phantom {
    Vector3 position{};
    float yaw = 0.0f;
    std::string flag;
    std::string hideFlag;
};

struct VehicleObject {
    std::string name;
    std::string label;
    std::string event;
    BodyHandle body = INVALID_BODY;
    std::vector<RenderBatch> batches;
    Vector3 spawnOrigin{};
    float spawnHeading = 0.0f;
    Vector3 localMin{};
    Vector3 localMax{};
    bool present = false;
};

struct TreeObject {
    Vector3 position{};
    float yaw = 0.0f;
    float scale = 1.0f;
};

struct MarkerObject {
    std::string name;
    Vector3 position{};
    float yaw = 0.0f;
};

struct RoadNode {
    Vector3 position{};
    float s = 0.0f;
};

struct LevelEnvironment {
    Vector3 ambientSky{0.075f, 0.085f, 0.12f};
    Vector3 ambientGround{0.03f, 0.03f, 0.04f};
    Vector3 fogColor{0.035f, 0.045f, 0.07f};
    Vector3 skyLight{0.11f, 0.13f, 0.19f};
    float fogStart = 5.0f;
    float fogEnd = 26.0f;
};

class MotelLevel {
public:
    bool Load(engine::core::Engine& eng, engine::physics3d::Physics3D& physics,
              const engine::render3d::Lighting3D& lighting, const std::string& mapPath);
    void Unload();

    const std::vector<RenderBatch>& StaticBatches() const { return m_static; }
    std::vector<DoorObject>& Doors() { return m_doors; }
    const std::vector<DoorObject>& Doors() const { return m_doors; }
    std::vector<PropObject>& Props() { return m_props; }
    const std::vector<PropObject>& Props() const { return m_props; }
    std::vector<InteractObject>& Interacts() { return m_interacts; }
    std::vector<TriggerObject>& Triggers() { return m_triggers; }
    std::vector<MirrorObject>& Mirrors() { return m_mirrors; }
    const std::vector<MirrorObject>& Mirrors() const { return m_mirrors; }
    std::vector<ItemObject>& Items() { return m_items; }
    const std::vector<ItemObject>& Items() const { return m_items; }
    const std::vector<HideSpot>& HideSpots() const { return m_hideSpots; }
    const std::vector<LightObject>& Lights() const { return m_lights; }
    std::vector<SoundEmitter>& Sounds() { return m_sounds; }
    const std::vector<Phantom>& Phantoms() const { return m_phantoms; }
    const LevelEnvironment& Environment() const { return m_env; }
    VehicleObject& Vehicle() { return m_vehicle; }
    const VehicleObject& Vehicle() const { return m_vehicle; }
    const std::vector<TreeObject>& Trees() const { return m_trees; }
    const std::vector<RoadNode>& Road() const { return m_road; }
    const MarkerObject* FindMarker(const std::string& name) const;

    Vector3 PlayerStart() const { return m_playerStart; }
    float PlayerStartYaw() const { return m_playerYaw; }
    const std::string& ScriptPath() const { return m_scriptPath; }
    std::string SurfaceForBody(BodyHandle body) const;
    int FindDoorByBody(BodyHandle body) const;
    int FindDoorByName(const std::string& name) const;
    int FindPropByBody(BodyHandle body) const;
    int FindInteractByBody(BodyHandle body) const;
    size_t VertexCount() const { return m_vertexCount; }

private:
    Texture2D ResolveTexture(engine::core::Engine& eng, const std::string& name);

    std::vector<RenderBatch> m_static;
    std::vector<DoorObject> m_doors;
    std::vector<PropObject> m_props;
    std::vector<InteractObject> m_interacts;
    std::vector<TriggerObject> m_triggers;
    std::vector<MirrorObject> m_mirrors;
    std::vector<ItemObject> m_items;
    std::vector<HideSpot> m_hideSpots;
    std::vector<LightObject> m_lights;
    std::vector<SoundEmitter> m_sounds;
    std::vector<Phantom> m_phantoms;
    VehicleObject m_vehicle;
    std::vector<TreeObject> m_trees;
    std::vector<MarkerObject> m_markers;
    std::vector<RoadNode> m_road;
    std::unordered_map<BodyHandle, std::string> m_surfaces;
    std::unordered_map<std::string, Texture2D> m_textureCache;
    LevelEnvironment m_env;
    Vector3 m_playerStart{};
    float m_playerYaw = 0.0f;
    std::string m_scriptPath;
    size_t m_vertexCount = 0;
};

}
