#pragma once

#include "raylib.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace knopmops::scene {

enum class ComponentType {
    Transform,
    MeshRenderer,
    Camera,
    Light,
    BoxCollider,
    SearchSlot,
    HidingSpot,
    Door,
    AISpawnPoint,
    Script
};

struct TransformComponent {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotation{0.0f, 0.0f, 0.0f}; // Pitch, Yaw, Roll
    Vector3 scale{1.0f, 1.0f, 1.0f};
};

struct MeshRendererComponent {
    bool enabled = true;
    std::string meshName = "Cube";
    std::string textureName = "motel_wall";
    Color tint{255, 255, 255, 255};
    bool castShadows = true;
    bool wireframe = false;
};

struct CameraComponent {
    bool enabled = false;
    float fov = 65.0f;
    float nearPlane = 0.05f;
    float farPlane = 120.0f;
    bool isMainPlayerCamera = false;
};

struct LightComponent {
    bool enabled = false;
    int type = 0; // 0: Point, 1: Spot, 2: Directional
    Color color{255, 240, 200, 255};
    float intensity = 250.0f;
    float radius = 8.0f;
};

struct BoxColliderComponent {
    bool enabled = false;
    Vector3 size{1.0f, 1.0f, 1.0f};
    Vector3 offset{0.0f, 0.0f, 0.0f};
    bool isTrigger = false;
};

struct SearchSlotComponent {
    bool enabled = false;
    std::string slotId = "slot_01";
    std::string displayName = "Шкафчик";
    std::string containedItem = "";
    bool isLocked = false;
    std::string requiredTool = "item_crowbar";
    float searchDuration = 1.2f;
};

struct HidingSpotComponent {
    bool enabled = false;
    std::string spotType = "Wardrobe"; // Wardrobe, UnderBed, Locker
    Vector3 eyeOffset{0.0f, 1.6f, 0.0f};
    float exitYaw = 0.0f;
};

struct DoorComponent {
    bool enabled = false;
    float openAngle = 90.0f;
    float speed = 3.5f;
    bool isLocked = false;
    std::string requiredKey = "item_garage_key";
    std::string openSound = "audio/sfx/sfx_door_open.wav";
};

struct AISpawnPointComponent {
    bool enabled = false;
    std::string monsterType = "Prowler"; // Prowler, Stalker, Mimic
    float hearingRadius = 8.5f;
    float visionAngle = 110.0f;
    float visionDistance = 14.0f;
    std::vector<Vector3> patrolWaypoints;
};

struct ScriptComponent {
    bool enabled = false;
    std::string scriptFile = "PlayerController.cpp";
    std::unordered_map<std::string, std::string> parameters;
};

class GameObject {
public:
    uint32_t id = 0;
    std::string name = "GameObject";
    bool active = true;

    TransformComponent transform;
    MeshRendererComponent meshRenderer;
    CameraComponent camera;
    LightComponent light;
    BoxColliderComponent collider;
    SearchSlotComponent searchSlot;
    HidingSpotComponent hidingSpot;
    DoorComponent door;
    AISpawnPointComponent aiSpawn;
    ScriptComponent script;

    GameObject(uint32_t objId, const std::string& objName) : id(objId), name(objName) {}
};

class Scene {
public:
    Scene(const std::string& sceneName = "Chapter4_Bunker");
    ~Scene();

    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    const std::string& GetPath() const { return m_filePath; }
    void SetPath(const std::string& path) { m_filePath = path; }

    bool IsModified() const { return m_modified; }
    void SetModified(bool mod = true) { m_modified = mod; }

    GameObject* CreateGameObject(const std::string& name = "GameObject");
    void DestroyGameObject(uint32_t id);
    GameObject* FindGameObject(uint32_t id);
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return m_gameObjects; }

    void Select(uint32_t id);
    void Deselect();
    GameObject* GetSelected();
    int GetSelectedId() const { return m_selectedId; }

    void Clear();
    void SetupDefaultChapter4Scene();

    bool SaveToFile(const std::string& path);
    bool LoadFromFile(const std::string& path);

private:
    std::string m_name;
    std::string m_filePath;
    bool m_modified = false;
    uint32_t m_nextId = 1;
    int m_selectedId = -1;
    std::vector<std::unique_ptr<GameObject>> m_gameObjects;
};

}

