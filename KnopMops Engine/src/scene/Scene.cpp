#include "scene/Scene.h"
#include "core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace knopmops::scene {

using json = nlohmann::json;

Scene::Scene(const std::string& sceneName) : m_name(sceneName) {}
Scene::~Scene() = default;

GameObject* Scene::CreateGameObject(const std::string& name) {
    auto obj = std::make_unique<GameObject>(m_nextId++, name);
    GameObject* ptr = obj.get();
    m_gameObjects.push_back(std::move(obj));
    m_modified = true;
    return ptr;
}

void Scene::DestroyGameObject(uint32_t id) {
    auto it = std::remove_if(m_gameObjects.begin(), m_gameObjects.end(),
        [id](const std::unique_ptr<GameObject>& obj) { return obj->id == id; });

    if (it != m_gameObjects.end()) {
        m_gameObjects.erase(it, m_gameObjects.end());
        if (m_selectedId == static_cast<int>(id)) m_selectedId = -1;
        m_modified = true;
    }
}

GameObject* Scene::FindGameObject(uint32_t id) {
    for (const auto& obj : m_gameObjects) {
        if (obj->id == id) return obj.get();
    }
    return nullptr;
}

void Scene::Select(uint32_t id) {
    m_selectedId = static_cast<int>(id);
}

void Scene::Deselect() {
    m_selectedId = -1;
}

GameObject* Scene::GetSelected() {
    return (m_selectedId > 0) ? FindGameObject(static_cast<uint32_t>(m_selectedId)) : nullptr;
}

void Scene::Clear() {
    m_gameObjects.clear();
    m_selectedId = -1;
    m_nextId = 1;
    m_modified = false;
}

void Scene::SetupDefaultChapter4Scene() {
    Clear();
    m_name = "Chapter4_Bunker_Escape";

    auto* player = CreateGameObject("Player");
    player->transform.position = Vector3{0.0f, 1.65f, 2.0f};
    player->camera.enabled = true;
    player->camera.isMainPlayerCamera = true;
    player->camera.fov = 68.0f;
    player->collider.enabled = true;
    player->collider.size = Vector3{0.7f, 1.8f, 0.7f};
    player->script.enabled = true;
    player->script.scriptFile = "PlayerController.cpp";

    auto* enemy = CreateGameObject("Enemy_Prowler");
    enemy->transform.position = Vector3{-4.0f, 0.0f, -4.0f};
    enemy->aiSpawn.enabled = true;
    enemy->aiSpawn.monsterType = "Prowler";
    enemy->aiSpawn.hearingRadius = 9.0f;
    enemy->aiSpawn.visionAngle = 110.0f;
    enemy->aiSpawn.visionDistance = 14.0f;
    enemy->aiSpawn.patrolWaypoints = {
        Vector3{-4.0f, 0.0f, -4.0f},
        Vector3{4.0f, 0.0f, -4.0f},
        Vector3{4.0f, 0.0f, 4.0f},
        Vector3{-4.0f, 0.0f, 4.0f}
    };
    enemy->script.enabled = true;
    enemy->script.scriptFile = "ProwlerMonsterAI.cpp";

    auto* lamp = CreateGameObject("Ceiling_Lamp");
    lamp->transform.position = Vector3{0.0f, 3.0f, 0.0f};
    lamp->light.enabled = true;
    lamp->light.color = Color{255, 235, 190, 255};
    lamp->light.intensity = 280.0f;
    lamp->light.radius = 8.5f;

    auto* crate = CreateGameObject("SearchCrate_Attic");
    crate->transform.position = Vector3{3.0f, 0.4f, 3.0f};
    crate->searchSlot.enabled = true;
    crate->searchSlot.slotId = "slot_bunker_crate1";
    crate->searchSlot.displayName = "Ящик с припасами";
    crate->searchSlot.containedItem = "item_crowbar";
    crate->searchSlot.isLocked = false;
    crate->collider.enabled = true;
    crate->collider.size = Vector3{0.8f, 0.8f, 0.8f};

    auto* wardrobe = CreateGameObject("Hiding_Wardrobe");
    wardrobe->transform.position = Vector3{-3.5f, 0.0f, 2.5f};
    wardrobe->hidingSpot.enabled = true;
    wardrobe->hidingSpot.spotType = "Wardrobe";
    wardrobe->hidingSpot.eyeOffset = Vector3{0.0f, 1.55f, 0.0f};
    wardrobe->hidingSpot.exitYaw = 90.0f;
    wardrobe->collider.enabled = true;
    wardrobe->collider.size = Vector3{1.0f, 2.1f, 0.8f};

    auto* door = CreateGameObject("Bunker_SteelDoor");
    door->transform.position = Vector3{0.0f, 0.0f, -6.0f};
    door->door.enabled = true;
    door->door.openAngle = 90.0f;
    door->door.isLocked = true;
    door->door.requiredKey = "item_master_key";
    door->door.openSound = "audio/sfx/sfx_door_creak_open.wav";
    door->collider.enabled = true;
    door->collider.size = Vector3{1.4f, 2.2f, 0.25f};

    m_modified = false;
    core::Log::Success("Default Chapter 4 scene generated: Player, Prowler AI, Loot Slots, Door and Hiding Wardrobe.");
}

bool Scene::SaveToFile(const std::string& path) {
    try {
        json j;
        j["name"] = m_name;
        j["nextId"] = m_nextId;
        j["objects"] = json::array();

        for (const auto& obj : m_gameObjects) {
            json o;
            o["id"] = obj->id;
            o["name"] = obj->name;
            o["active"] = obj->active;

            o["transform"] = {
                {"pos", {obj->transform.position.x, obj->transform.position.y, obj->transform.position.z}},
                {"rot", {obj->transform.rotation.x, obj->transform.rotation.y, obj->transform.rotation.z}},
                {"scale", {obj->transform.scale.x, obj->transform.scale.y, obj->transform.scale.z}}
            };

            if (obj->meshRenderer.enabled) {
                o["meshRenderer"] = {
                    {"enabled", true},
                    {"mesh", obj->meshRenderer.meshName},
                    {"texture", obj->meshRenderer.textureName}
                };
            }

            if (obj->camera.enabled) {
                o["camera"] = {
                    {"enabled", true},
                    {"fov", obj->camera.fov},
                    {"isMain", obj->camera.isMainPlayerCamera}
                };
            }

            if (obj->light.enabled) {
                o["light"] = {
                    {"enabled", true},
                    {"intensity", obj->light.intensity},
                    {"radius", obj->light.radius},
                    {"color", {obj->light.color.r, obj->light.color.g, obj->light.color.b}}
                };
            }

            if (obj->searchSlot.enabled) {
                o["searchSlot"] = {
                    {"enabled", true},
                    {"slotId", obj->searchSlot.slotId},
                    {"name", obj->searchSlot.displayName},
                    {"item", obj->searchSlot.containedItem},
                    {"locked", obj->searchSlot.isLocked},
                    {"tool", obj->searchSlot.requiredTool}
                };
            }

            if (obj->hidingSpot.enabled) {
                o["hidingSpot"] = {
                    {"enabled", true},
                    {"type", obj->hidingSpot.spotType},
                    {"exitYaw", obj->hidingSpot.exitYaw}
                };
            }

            if (obj->door.enabled) {
                o["door"] = {
                    {"enabled", true},
                    {"locked", obj->door.isLocked},
                    {"key", obj->door.requiredKey},
                    {"sound", obj->door.openSound}
                };
            }

            if (obj->aiSpawn.enabled) {
                o["aiSpawn"] = {
                    {"enabled", true},
                    {"type", obj->aiSpawn.monsterType},
                    {"hearing", obj->aiSpawn.hearingRadius},
                    {"visionAngle", obj->aiSpawn.visionAngle},
                    {"visionDist", obj->aiSpawn.visionDistance}
                };
            }

            if (obj->script.enabled) {
                o["script"] = {
                    {"enabled", true},
                    {"file", obj->script.scriptFile}
                };
            }

            j["objects"].push_back(o);
        }

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        m_filePath = path;
        m_modified = false;
        core::Log::Success("Saved Scene to: " + path);
        return true;
    } catch (...) {
        core::Log::Error("Failed to serialize Scene to: " + path);
        return false;
    }
}

bool Scene::LoadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        json j;
        file >> j;

        Clear();
        m_name = j.value("name", "UntitledScene");
        m_nextId = j.value("nextId", 1);

        if (j.contains("objects") && j["objects"].is_array()) {
            for (const auto& o : j["objects"]) {
                uint32_t id = o.value("id", 0);
                std::string name = o.value("name", "Object");
                auto* obj = CreateGameObject(name);
                obj->id = id;
                obj->active = o.value("active", true);

                if (o.contains("transform")) {
                    auto pos = o["transform"].value("pos", std::vector<float>{0, 0, 0});
                    auto rot = o["transform"].value("rot", std::vector<float>{0, 0, 0});
                    auto sc = o["transform"].value("scale", std::vector<float>{1, 1, 1});
                    if (pos.size() >= 3) obj->transform.position = {pos[0], pos[1], pos[2]};
                    if (rot.size() >= 3) obj->transform.rotation = {rot[0], rot[1], rot[2]};
                    if (sc.size() >= 3) obj->transform.scale = {sc[0], sc[1], sc[2]};
                }

                if (o.contains("camera")) {
                    obj->camera.enabled = true;
                    obj->camera.fov = o["camera"].value("fov", 65.0f);
                    obj->camera.isMainPlayerCamera = o["camera"].value("isMain", false);
                }

                if (o.contains("light")) {
                    obj->light.enabled = true;
                    obj->light.intensity = o["light"].value("intensity", 250.0f);
                    obj->light.radius = o["light"].value("radius", 8.0f);
                }

                if (o.contains("searchSlot")) {
                    obj->searchSlot.enabled = true;
                    obj->searchSlot.slotId = o["searchSlot"].value("slotId", "slot_01");
                    obj->searchSlot.displayName = o["searchSlot"].value("name", "Ящик");
                    obj->searchSlot.containedItem = o["searchSlot"].value("item", "");
                    obj->searchSlot.isLocked = o["searchSlot"].value("locked", false);
                    obj->searchSlot.requiredTool = o["searchSlot"].value("tool", "");
                }

                if (o.contains("door")) {
                    obj->door.enabled = true;
                    obj->door.isLocked = o["door"].value("locked", false);
                    obj->door.requiredKey = o["door"].value("key", "");
                }

                if (o.contains("aiSpawn")) {
                    obj->aiSpawn.enabled = true;
                    obj->aiSpawn.monsterType = o["aiSpawn"].value("type", "Prowler");
                }

                if (o.contains("script")) {
                    obj->script.enabled = true;
                    obj->script.scriptFile = o["script"].value("file", "");
                }
            }
        }

        m_filePath = path;
        m_modified = false;
        core::Log::Success("Loaded Scene from: " + path);
        return true;
    } catch (...) {
        core::Log::Error("Failed to parse Scene file: " + path);
        return false;
    }
}

}

