#pragma once

#include "raylib.h"
#include <nlohmann/json_fwd.hpp>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace knopmops::scene {

// Позиция объекта (pivot): для Player/Monster — точка на полу ("ноги"),
// для остальных объектов — центр меша (как в Unity).
struct TransformComponent {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotation{0.0f, 0.0f, 0.0f}; // Pitch, Yaw, Roll (градусы). В рантайме используется Yaw.
    Vector3 scale{1.0f, 1.0f, 1.0f};
};

// meshName: Auto (по компонентам) | None | Cube | Sphere | Cylinder
struct MeshRendererComponent {
    bool enabled = true;
    std::string meshName = "Auto";
    std::string textureName = "";
    Color tint{255, 255, 255, 255};
    bool emissive = false;
};

struct CameraComponent {
    bool enabled = false;
    float fov = 70.0f;
    float nearPlane = 0.05f;
    float farPlane = 120.0f;
    bool isMainPlayerCamera = false;
};

// PlayerController: управление от первого лица (ходьба, бег, приседание, прыжок, гравитация).
struct PlayerComponent {
    bool enabled = false;
    float walkSpeed = 2.6f;
    float sprintSpeed = 4.6f;
    float crouchSpeed = 1.3f;
    float eyeHeight = 1.65f;
    bool startWithFlashlight = true;
    float radius = 0.3f;
    float height = 1.8f;
    float jumpSpeed = 4.0f;   // 0 = прыжок выключен
    float gravity = 18.0f;
    float mouseSens = 0.11f;
};

struct LightComponent {
    bool enabled = false;
    int type = 0; // 0: Point, 1: Spot (не реализован отдельно), 2: Directional (не реализован)
    Color color{255, 235, 190, 255};
    float intensity = 1.0f;
    float radius = 8.0f;
    bool flicker = false;
};

// Коллайдер: форма Box / Sphere / Capsule. Размеры в метрах (не зависят от scale).
struct BoxColliderComponent {
    bool enabled = false;
    int shape = 0;            // 0 Box, 1 Sphere (круг), 2 Capsule
    Vector3 size{1.0f, 1.0f, 1.0f}; // Box
    float radius = 0.5f;      // Sphere / Capsule
    float height = 1.8f;      // Capsule (полная высота)
    Vector3 offset{0.0f, 0.0f, 0.0f};
    bool isTrigger = false;
};

struct RigidbodyComponent {
    bool enabled = false;
    float mass = 1.0f;
    bool useGravity = true;
    bool isKinematic = false; // не двигается физикой (но толкает других)
    float drag = 0.2f;
    float bounciness = 0.2f;  // 0..1 отскок
    float friction = 0.8f;
};

struct SearchSlotComponent {
    bool enabled = false;
    std::string slotId = "slot_01";
    std::string displayName = "Ящик";
    std::string containedItem = "";
    bool isLocked = false;
    std::string requiredTool = "item_crowbar";
    bool consumeTool = false;
    float searchDuration = 1.2f;
    float noiseRadius = 0.0f; // шум для ИИ при вскрытии (0 = нет)
};

struct HidingSpotComponent {
    bool enabled = false;
    std::string spotType = "Wardrobe"; // Wardrobe, UnderBed, Locker
    Vector3 eyeOffset{0.0f, 0.45f, 0.0f}; // от центра объекта
    float exitYaw = 0.0f; // направление выхода (градусы, 0 = +Z)
};

struct DoorComponent {
    bool enabled = false;
    float openAngle = 90.0f;
    float speed = 3.0f;
    bool isLocked = false;
    bool startOpen = false;
    std::string requiredKey = "";
    bool consumeKey = false;
    std::string openSound = "audio/sfx/sfx_door_open.wav";
};

struct AISpawnPointComponent {
    bool enabled = false;
    std::string monsterType = "Prowler";
    float hearingRadius = 8.5f;
    float visionAngle = 110.0f;
    float visionDistance = 14.0f;
    float walkSpeed = 1.4f;
    float chaseSpeed = 3.3f;
    float waitAtWaypoint = 2.0f;
    std::vector<Vector3> patrolWaypoints;
};

struct ItemPickupComponent {
    bool enabled = false;
    std::string itemId = "item_key";
};

struct ScriptComponent {
    bool enabled = false;
    std::string scriptFile = "";
    bool interactable = false; // показывать подсказку [E] и вызывать on_interact
    std::string interactPrompt = "Использовать";
    std::map<std::string, std::string> parameters; // значения "exports" из скрипта
};

class GameObject {
public:
    uint32_t id = 0;
    uint32_t parent = 0; // 0 = корень
    std::string name = "GameObject";
    std::string tag = "";
    bool active = true;
    std::map<std::string, std::string> props; // пользовательские переменные (self:get_prop/set_prop)

    TransformComponent transform;
    MeshRendererComponent meshRenderer;
    CameraComponent camera;
    PlayerComponent player;
    LightComponent light;
    BoxColliderComponent collider;
    RigidbodyComponent rigidbody;
    SearchSlotComponent searchSlot;
    HidingSpotComponent hidingSpot;
    DoorComponent door;
    AISpawnPointComponent aiSpawn;
    ItemPickupComponent itemPickup;
    ScriptComponent script;

    GameObject() = default;
    GameObject(uint32_t objId, const std::string& objName) : id(objId), name(objName) {}
};

struct SceneEnvironment {
    float ambient = 0.10f;      // базовая освещённость 0..1
    float fogEnd = 24.0f;       // дальность полной темноты
    Color background{6, 6, 9, 255};
};

class Scene {
public:
    Scene(const std::string& sceneName = "Level");
    ~Scene();

    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    const std::string& GetPath() const { return m_filePath; }
    void SetPath(const std::string& path) { m_filePath = path; }

    bool IsModified() const { return m_modified; }
    void SetModified(bool mod = true) { m_modified = mod; }

    GameObject* CreateGameObject(const std::string& name = "GameObject");
    // Удаляет объект вместе с потомками.
    void DestroyGameObject(uint32_t id);
    GameObject* FindGameObject(uint32_t id);
    const GameObject* FindGameObject(uint32_t id) const;
    GameObject* FindByName(const std::string& name);
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return m_gameObjects; }

    // Иерархия (родитель смещает потомка на свою позицию; вращение/масштаб не наследуются).
    bool SetParent(uint32_t id, uint32_t parentId);
    std::vector<GameObject*> GetChildren(uint32_t id);
    Vector3 WorldPosition(const GameObject& obj) const;
    void SetWorldPosition(GameObject& obj, Vector3 world);

    void Select(uint32_t id);
    void Deselect();
    GameObject* GetSelected();
    int GetSelectedId() const { return m_selectedId; }

    SceneEnvironment& Env() { return m_env; }
    const SceneEnvironment& Env() const { return m_env; }

    void Clear();
    void CopyFrom(const Scene& other);
    GameObject* Duplicate(uint32_t id);

    bool SaveToFile(const std::string& path);
    bool LoadFromFile(const std::string& path);

    // Префабы: один объект + его потомки.
    bool SavePrefab(uint32_t id, const std::string& path);
    GameObject* InstantiatePrefab(const std::string& path, Vector3 at);

    static nlohmann::json ObjectToJson(const GameObject& obj);
    static void ObjectFromJson(const nlohmann::json& j, GameObject& obj);

private:
    std::string m_name;
    std::string m_filePath;
    bool m_modified = false;
    uint32_t m_nextId = 1;
    int m_selectedId = -1;
    SceneEnvironment m_env;
    std::vector<std::unique_ptr<GameObject>> m_gameObjects;
};

}
