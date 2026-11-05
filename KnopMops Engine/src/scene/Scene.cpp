#include "scene/Scene.h"
#include "core/Log.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace knopmops::scene {

using json = nlohmann::json;

namespace {

json V3(Vector3 v) { return json::array({v.x, v.y, v.z}); }
json Col(Color c) { return json::array({c.r, c.g, c.b, c.a}); }

Vector3 ReadV3(const json& j, const char* key, Vector3 def) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 3) return def;
    return Vector3{j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>()};
}

Color ReadCol(const json& j, const char* key, Color def) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 3) return def;
    Color c = def;
    c.r = static_cast<unsigned char>(j[key][0].get<int>());
    c.g = static_cast<unsigned char>(j[key][1].get<int>());
    c.b = static_cast<unsigned char>(j[key][2].get<int>());
    c.a = j[key].size() > 3 ? static_cast<unsigned char>(j[key][3].get<int>()) : 255;
    return c;
}

}

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
    std::vector<uint32_t> doomed{id};
    for (size_t i = 0; i < doomed.size(); ++i) {
        for (const auto& o : m_gameObjects) {
            if (o->parent == doomed[i]) doomed.push_back(o->id);
        }
    }
    auto it = std::remove_if(m_gameObjects.begin(), m_gameObjects.end(),
        [&](const std::unique_ptr<GameObject>& obj) {
            return std::find(doomed.begin(), doomed.end(), obj->id) != doomed.end();
        });
    if (it != m_gameObjects.end()) {
        m_gameObjects.erase(it, m_gameObjects.end());
        if (m_selectedId > 0 && !FindGameObject(static_cast<uint32_t>(m_selectedId))) m_selectedId = -1;
        m_modified = true;
    }
}

GameObject* Scene::FindGameObject(uint32_t id) {
    for (const auto& obj : m_gameObjects) {
        if (obj->id == id) return obj.get();
    }
    return nullptr;
}

const GameObject* Scene::FindGameObject(uint32_t id) const {
    for (const auto& obj : m_gameObjects) {
        if (obj->id == id) return obj.get();
    }
    return nullptr;
}

GameObject* Scene::FindByName(const std::string& name) {
    for (const auto& obj : m_gameObjects) {
        if (obj->name == name) return obj.get();
    }
    return nullptr;
}

bool Scene::SetParent(uint32_t id, uint32_t parentId) {
    GameObject* obj = FindGameObject(id);
    if (!obj || id == parentId) return false;
    if (parentId != 0) {
        // Запрет циклов: parentId не должен быть потомком id.
        for (const GameObject* p = FindGameObject(parentId); p; p = FindGameObject(p->parent)) {
            if (p->id == id) return false;
            if (p->parent == 0) break;
        }
        if (!FindGameObject(parentId)) return false;
    }
    Vector3 world = WorldPosition(*obj);
    obj->parent = parentId;
    SetWorldPosition(*obj, world);
    m_modified = true;
    return true;
}

std::vector<GameObject*> Scene::GetChildren(uint32_t id) {
    std::vector<GameObject*> out;
    for (const auto& o : m_gameObjects) {
        if (o->parent == id) out.push_back(o.get());
    }
    return out;
}

Vector3 Scene::WorldPosition(const GameObject& obj) const {
    Vector3 p = obj.transform.position;
    const GameObject* cur = &obj;
    int guard = 0;
    while (cur->parent != 0 && guard++ < 32) {
        cur = FindGameObject(cur->parent);
        if (!cur) break;
        p.x += cur->transform.position.x;
        p.y += cur->transform.position.y;
        p.z += cur->transform.position.z;
    }
    return p;
}

void Scene::SetWorldPosition(GameObject& obj, Vector3 world) {
    Vector3 offset{0.0f, 0.0f, 0.0f};
    const GameObject* cur = &obj;
    int guard = 0;
    while (cur->parent != 0 && guard++ < 32) {
        cur = FindGameObject(cur->parent);
        if (!cur) break;
        offset.x += cur->transform.position.x;
        offset.y += cur->transform.position.y;
        offset.z += cur->transform.position.z;
    }
    obj.transform.position = Vector3{world.x - offset.x, world.y - offset.y, world.z - offset.z};
}

void Scene::Select(uint32_t id) { m_selectedId = static_cast<int>(id); }
void Scene::Deselect() { m_selectedId = -1; }

GameObject* Scene::GetSelected() {
    return (m_selectedId > 0) ? FindGameObject(static_cast<uint32_t>(m_selectedId)) : nullptr;
}

void Scene::Clear() {
    m_gameObjects.clear();
    m_selectedId = -1;
    m_nextId = 1;
    m_modified = false;
    m_env = SceneEnvironment{};
}

void Scene::CopyFrom(const Scene& other) {
    m_name = other.m_name;
    m_filePath = other.m_filePath;
    m_nextId = other.m_nextId;
    m_selectedId = -1;
    m_env = other.m_env;
    m_gameObjects.clear();
    for (const auto& o : other.m_gameObjects) {
        m_gameObjects.push_back(std::make_unique<GameObject>(*o));
    }
    m_modified = false;
}

GameObject* Scene::Duplicate(uint32_t id) {
    const GameObject* src = FindGameObject(id);
    if (!src) return nullptr;

    std::unordered_map<uint32_t, uint32_t> remap;
    std::vector<uint32_t> queue{id};
    GameObject* rootCopy = nullptr;
    for (size_t i = 0; i < queue.size(); ++i) {
        const GameObject* s = FindGameObject(queue[i]);
        if (!s) continue;
        GameObject copy = *s;
        GameObject* c = CreateGameObject(s->name);
        uint32_t newId = c->id;
        *c = copy;
        c->id = newId;
        remap[s->id] = newId;
        if (i == 0) {
            c->name = s->name + "_Copy";
            c->transform.position.x += 1.0f;
            c->transform.position.z += 1.0f;
            rootCopy = c;
        } else {
            auto pit = remap.find(s->parent);
            if (pit != remap.end()) c->parent = pit->second;
        }
        for (const auto& o : m_gameObjects) {
            if (o->parent == s->id && remap.find(o->id) == remap.end() && o->id != newId) {
                if (std::find(queue.begin(), queue.end(), o->id) == queue.end()) queue.push_back(o->id);
            }
        }
    }
    return rootCopy;
}

json Scene::ObjectToJson(const GameObject& g) {
    json o;
    o["id"] = g.id;
    o["parent"] = g.parent;
    o["name"] = g.name;
    o["tag"] = g.tag;
    o["active"] = g.active;
    if (!g.props.empty()) o["props"] = g.props;

    o["transform"] = {{"pos", V3(g.transform.position)}, {"rot", V3(g.transform.rotation)}, {"scale", V3(g.transform.scale)}};

    o["mesh"] = {{"enabled", g.meshRenderer.enabled}, {"shape", g.meshRenderer.meshName}, {"texture", g.meshRenderer.textureName},
                 {"tint", Col(g.meshRenderer.tint)}, {"emissive", g.meshRenderer.emissive}};

    if (g.camera.enabled) {
        o["camera"] = {{"fov", g.camera.fov}, {"near", g.camera.nearPlane}, {"far", g.camera.farPlane}, {"main", g.camera.isMainPlayerCamera}};
    }
    if (g.player.enabled) {
        o["player"] = {{"walk", g.player.walkSpeed}, {"sprint", g.player.sprintSpeed}, {"crouch", g.player.crouchSpeed},
                       {"eye", g.player.eyeHeight}, {"flashlight", g.player.startWithFlashlight},
                       {"radius", g.player.radius}, {"height", g.player.height}, {"jump", g.player.jumpSpeed},
                       {"gravity", g.player.gravity}, {"sens", g.player.mouseSens}};
    }
    if (g.light.enabled) {
        o["light"] = {{"type", g.light.type}, {"color", Col(g.light.color)}, {"intensity", g.light.intensity},
                      {"radius", g.light.radius}, {"flicker", g.light.flicker}};
    }
    if (g.collider.enabled) {
        o["collider"] = {{"shape", g.collider.shape}, {"size", V3(g.collider.size)}, {"radius", g.collider.radius}, {"height", g.collider.height},
                           {"offset", V3(g.collider.offset)}, {"trigger", g.collider.isTrigger}};
    }
    if (g.rigidbody.enabled) {
        const auto& r = g.rigidbody;
        o["rigidbody"] = {{"mass", r.mass}, {"gravity", r.useGravity}, {"kinematic", r.isKinematic}, {"drag", r.drag}, {"bounce", r.bounciness}, {"friction", r.friction}};
    }
    if (g.searchSlot.enabled) {
        const auto& s = g.searchSlot;
        o["searchSlot"] = {{"slotId", s.slotId}, {"name", s.displayName}, {"item", s.containedItem}, {"locked", s.isLocked},
                           {"tool", s.requiredTool}, {"consumeTool", s.consumeTool}, {"duration", s.searchDuration}, {"noise", s.noiseRadius}};
    }
    if (g.hidingSpot.enabled) {
        o["hidingSpot"] = {{"type", g.hidingSpot.spotType}, {"eye", V3(g.hidingSpot.eyeOffset)}, {"exitYaw", g.hidingSpot.exitYaw}};
    }
    if (g.door.enabled) {
        const auto& d = g.door;
        o["door"] = {{"angle", d.openAngle}, {"speed", d.speed}, {"locked", d.isLocked}, {"startOpen", d.startOpen},
                     {"key", d.requiredKey}, {"consumeKey", d.consumeKey}, {"sound", d.openSound}};
    }
    if (g.aiSpawn.enabled) {
        const auto& a = g.aiSpawn;
        json wps = json::array();
        for (const auto& w : a.patrolWaypoints) wps.push_back(V3(w));
        o["aiSpawn"] = {{"type", a.monsterType}, {"hearing", a.hearingRadius}, {"visionAngle", a.visionAngle}, {"visionDist", a.visionDistance},
                        {"walk", a.walkSpeed}, {"chase", a.chaseSpeed}, {"wait", a.waitAtWaypoint}, {"waypoints", wps}};
    }
    if (g.itemPickup.enabled) {
        o["itemPickup"] = {{"item", g.itemPickup.itemId}};
    }
    if (g.script.enabled) {
        o["script"] = {{"file", g.script.scriptFile}, {"interactable", g.script.interactable},
                       {"prompt", g.script.interactPrompt}, {"params", g.script.parameters}};
    }
    return o;
}

void Scene::ObjectFromJson(const json& o, GameObject& g) {
    g.id = o.value("id", g.id);
    g.parent = o.value("parent", 0u);
    g.name = o.value("name", std::string("Object"));
    g.tag = o.value("tag", std::string());
    g.active = o.value("active", true);
    g.props.clear();
    if (o.contains("props") && o["props"].is_object()) {
        for (auto it = o["props"].begin(); it != o["props"].end(); ++it) g.props[it.key()] = it.value().get<std::string>();
    }

    if (o.contains("transform")) {
        const json& t = o["transform"];
        g.transform.position = ReadV3(t, "pos", {0, 0, 0});
        g.transform.rotation = ReadV3(t, "rot", {0, 0, 0});
        g.transform.scale = ReadV3(t, "scale", {1, 1, 1});
    }
    if (o.contains("mesh")) {
        const json& m = o["mesh"];
        g.meshRenderer.enabled = m.value("enabled", true);
        g.meshRenderer.meshName = m.value("shape", std::string("Auto"));
        g.meshRenderer.textureName = m.value("texture", std::string());
        g.meshRenderer.tint = ReadCol(m, "tint", Color{255, 255, 255, 255});
        g.meshRenderer.emissive = m.value("emissive", false);
    }
    if (o.contains("camera")) {
        const json& c = o["camera"];
        g.camera.enabled = true;
        g.camera.fov = c.value("fov", 70.0f);
        g.camera.nearPlane = c.value("near", 0.05f);
        g.camera.farPlane = c.value("far", 120.0f);
        g.camera.isMainPlayerCamera = c.value("main", false);
    }
    if (o.contains("player")) {
        const json& p = o["player"];
        g.player.enabled = true;
        g.player.walkSpeed = p.value("walk", 2.6f);
        g.player.sprintSpeed = p.value("sprint", 4.6f);
        g.player.crouchSpeed = p.value("crouch", 1.3f);
        g.player.eyeHeight = p.value("eye", 1.65f);
        g.player.startWithFlashlight = p.value("flashlight", true);
        g.player.radius = p.value("radius", 0.3f);
        g.player.height = p.value("height", 1.8f);
        g.player.jumpSpeed = p.value("jump", 4.0f);
        g.player.gravity = p.value("gravity", 18.0f);
        g.player.mouseSens = p.value("sens", 0.11f);
    }
    if (o.contains("light")) {
        const json& l = o["light"];
        g.light.enabled = true;
        g.light.type = l.value("type", 0);
        g.light.color = ReadCol(l, "color", Color{255, 235, 190, 255});
        g.light.intensity = l.value("intensity", 1.0f);
        g.light.radius = l.value("radius", 8.0f);
        g.light.flicker = l.value("flicker", false);
    }
    if (o.contains("collider")) {
        const json& c = o["collider"];
        g.collider.enabled = true;
        g.collider.shape = c.value("shape", 0);
        g.collider.size = ReadV3(c, "size", {1, 1, 1});
        g.collider.radius = c.value("radius", 0.5f);
        g.collider.height = c.value("height", 1.8f);
        g.collider.offset = ReadV3(c, "offset", {0, 0, 0});
        g.collider.isTrigger = c.value("trigger", false);
    }
    if (o.contains("rigidbody")) {
        const json& r = o["rigidbody"];
        g.rigidbody.enabled = true;
        g.rigidbody.mass = r.value("mass", 1.0f);
        g.rigidbody.useGravity = r.value("gravity", true);
        g.rigidbody.isKinematic = r.value("kinematic", false);
        g.rigidbody.drag = r.value("drag", 0.2f);
        g.rigidbody.bounciness = r.value("bounce", 0.2f);
        g.rigidbody.friction = r.value("friction", 0.8f);
    }
    if (o.contains("searchSlot")) {
        const json& s = o["searchSlot"];
        auto& d = g.searchSlot;
        d.enabled = true;
        d.slotId = s.value("slotId", std::string("slot_01"));
        d.displayName = s.value("name", std::string("Ящик"));
        d.containedItem = s.value("item", std::string());
        d.isLocked = s.value("locked", false);
        d.requiredTool = s.value("tool", std::string());
        d.consumeTool = s.value("consumeTool", false);
        d.searchDuration = s.value("duration", 1.2f);
        d.noiseRadius = s.value("noise", 0.0f);
    }
    if (o.contains("hidingSpot")) {
        const json& h = o["hidingSpot"];
        g.hidingSpot.enabled = true;
        g.hidingSpot.spotType = h.value("type", std::string("Wardrobe"));
        g.hidingSpot.eyeOffset = ReadV3(h, "eye", {0, 0.45f, 0});
        g.hidingSpot.exitYaw = h.value("exitYaw", 0.0f);
    }
    if (o.contains("door")) {
        const json& d = o["door"];
        auto& c = g.door;
        c.enabled = true;
        c.openAngle = d.value("angle", 90.0f);
        c.speed = d.value("speed", 3.0f);
        c.isLocked = d.value("locked", false);
        c.startOpen = d.value("startOpen", false);
        c.requiredKey = d.value("key", std::string());
        c.consumeKey = d.value("consumeKey", false);
        c.openSound = d.value("sound", std::string());
    }
    if (o.contains("aiSpawn")) {
        const json& a = o["aiSpawn"];
        auto& c = g.aiSpawn;
        c.enabled = true;
        c.monsterType = a.value("type", std::string("Prowler"));
        c.hearingRadius = a.value("hearing", 8.5f);
        c.visionAngle = a.value("visionAngle", 110.0f);
        c.visionDistance = a.value("visionDist", 14.0f);
        c.walkSpeed = a.value("walk", 1.4f);
        c.chaseSpeed = a.value("chase", 3.3f);
        c.waitAtWaypoint = a.value("wait", 2.0f);
        c.patrolWaypoints.clear();
        if (a.contains("waypoints") && a["waypoints"].is_array()) {
            for (const auto& w : a["waypoints"]) {
                if (w.is_array() && w.size() >= 3) c.patrolWaypoints.push_back({w[0].get<float>(), w[1].get<float>(), w[2].get<float>()});
            }
        }
    }
    if (o.contains("itemPickup")) {
        g.itemPickup.enabled = true;
        g.itemPickup.itemId = o["itemPickup"].value("item", std::string());
    }
    if (o.contains("script")) {
        const json& s = o["script"];
        g.script.enabled = true;
        g.script.scriptFile = s.value("file", std::string());
        g.script.interactable = s.value("interactable", false);
        g.script.interactPrompt = s.value("prompt", std::string("Использовать"));
        g.script.parameters.clear();
        if (s.contains("params") && s["params"].is_object()) {
            for (auto it = s["params"].begin(); it != s["params"].end(); ++it) g.script.parameters[it.key()] = it.value().get<std::string>();
        }
    }
}

bool Scene::SaveToFile(const std::string& path) {
    try {
        json j;
        j["format"] = 2;
        j["name"] = m_name;
        j["nextId"] = m_nextId;
        j["environment"] = {{"ambient", m_env.ambient}, {"fogEnd", m_env.fogEnd}, {"background", Col(m_env.background)}};
        j["objects"] = json::array();
        for (const auto& obj : m_gameObjects) j["objects"].push_back(ObjectToJson(*obj));

        std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        m_filePath = path;
        m_modified = false;
        return true;
    } catch (const std::exception& e) {
        core::Log::Error(std::string("Не удалось сохранить сцену: ") + e.what());
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
        m_name = j.value("name", std::string("Level"));
        if (j.contains("environment")) {
            const json& e = j["environment"];
            m_env.ambient = e.value("ambient", 0.10f);
            m_env.fogEnd = e.value("fogEnd", 24.0f);
            m_env.background = ReadCol(e, "background", Color{6, 6, 9, 255});
        }

        uint32_t maxId = 0;
        if (j.contains("objects") && j["objects"].is_array()) {
            for (const auto& o : j["objects"]) {
                auto obj = std::make_unique<GameObject>();
                ObjectFromJson(o, *obj);
                if (obj->id == 0) obj->id = ++maxId;
                maxId = std::max(maxId, obj->id);
                m_gameObjects.push_back(std::move(obj));
            }
        }
        m_nextId = std::max<uint32_t>(j.value("nextId", 1u), maxId + 1);
        m_filePath = path;
        m_modified = false;
        return true;
    } catch (const std::exception& e) {
        core::Log::Error(std::string("Ошибка чтения сцены ") + path + ": " + e.what());
        return false;
    }
}

bool Scene::SavePrefab(uint32_t id, const std::string& path) {
    const GameObject* root = FindGameObject(id);
    if (!root) return false;
    try {
        json j;
        j["format"] = 2;
        j["objects"] = json::array();
        std::vector<uint32_t> queue{id};
        for (size_t i = 0; i < queue.size(); ++i) {
            const GameObject* o = FindGameObject(queue[i]);
            if (!o) continue;
            json oj = ObjectToJson(*o);
            if (i == 0) {
                oj["parent"] = 0;
                oj["transform"]["pos"] = V3({0, o->transform.position.y, 0});
            }
            j["objects"].push_back(oj);
            for (const auto& c : m_gameObjects) {
                if (c->parent == o->id) queue.push_back(c->id);
            }
        }
        std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

GameObject* Scene::InstantiatePrefab(const std::string& path, Vector3 at) {
    try {
        std::ifstream f(path);
        if (!f.is_open()) return nullptr;
        json j;
        f >> j;
        if (!j.contains("objects") || !j["objects"].is_array()) return nullptr;

        std::unordered_map<uint32_t, uint32_t> remap;
        GameObject* root = nullptr;
        for (const auto& oj : j["objects"]) {
            GameObject tmp;
            ObjectFromJson(oj, tmp);
            uint32_t oldId = tmp.id;
            GameObject* c = CreateGameObject(tmp.name);
            uint32_t newId = c->id;
            *c = tmp;
            c->id = newId;
            remap[oldId] = newId;
            if (!root) {
                root = c;
                c->parent = 0;
                c->transform.position.x = at.x;
                c->transform.position.z = at.z;
            } else {
                auto pit = remap.find(c->parent);
                c->parent = pit != remap.end() ? pit->second : root->id;
            }
        }
        return root;
    } catch (...) {
        return nullptr;
    }
}

}
