#include "runtime/PlaySession.h"
#include "core/Log.h"
#include "core/Math3D.h"
#include "map/MapParser.h"
#include "render/TextureManager.h"
#include "runtime/ScriptHost.h"
#include "render/DrawUtil.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <queue>

namespace knopmops::runtime {

namespace fs = std::filesystem;

namespace {

constexpr float kStepHeight = 0.45f;
constexpr float kInteractDist = 2.4f;

float Len2D(Vector3 a, Vector3 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)); }
float Len3D(Vector3 a, Vector3 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }
Vector3 Forward(float yawDeg, float pitchDeg = 0.0f) {
    float y = yawDeg * DEG2RAD, p = pitchDeg * DEG2RAD;
    return Vector3{std::sin(y) * std::cos(p), std::sin(p), std::cos(y) * std::cos(p)};
}
float YawTo(Vector3 from, Vector3 to) { return std::atan2(to.x - from.x, to.z - from.z) * RAD2DEG; }
float AngleDiff(float a, float b) {
    float d = std::fmod(a - b + 540.0f, 360.0f) - 180.0f;
    return d;
}
float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
const char* ModeToString(MonsterMode m) {
    switch (m) {
    case MonsterMode::Idle: return "idle";
    case MonsterMode::Patrol: return "patrol";
    case MonsterMode::Investigate: return "investigate";
    case MonsterMode::Chase: return "chase";
    case MonsterMode::Search: return "search";
    }
    return "idle";
}

std::vector<std::string> WrapText(const Font& font, const std::string& text, float size, float maxW) {
    std::vector<std::string> lines;
    std::string cur, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string test = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && MeasureTextEx(font, test.c_str(), size, 1.0f).x > maxW) {
            lines.push_back(cur);
            cur = word;
        } else {
            cur = test;
        }
        word.clear();
    };
    for (char c : text) {
        if (c == '\n') { flushWord(); lines.push_back(cur); cur.clear(); }
        else if (c == ' ') flushWord();
        else word.push_back(c);
    }
    flushWord();
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

}

PlaySession::PlaySession() = default;

PlaySession::~PlaySession() {
    Shutdown();
}

// ---------------------------------------------------------------------------------------------
// Запуск / завершение
// ---------------------------------------------------------------------------------------------
bool PlaySession::Start(const ChapterProject& project, const std::string& level) {
    Shutdown();
    m_fromMemory = false;
    m_startLevel = level;
    return Begin(project, level);
}

bool PlaySession::StartFromMemory(const ChapterProject& project, const std::string& level, const scene::Scene& sc, const map::MapDocument& doc) {
    Shutdown();
    m_fromMemory = true;
    m_startLevel = level;
    m_origScene.CopyFrom(sc);
    m_origDoc = doc;
    return Begin(project, level);
}

bool PlaySession::LoadLevelData(const std::string& level) {
    m_scene.Clear();
    m_doc.Clear();
    if (m_fromMemory && level == m_startLevel) {
        m_scene.CopyFrom(m_origScene);
        m_doc = m_origDoc;
        m_doc.RebuildAllGeometry();
    } else {
        if (!m_scene.LoadFromFile(m_project.LevelScenePath(level))) {
            core::Log::Error("Не удалось загрузить уровень: " + level);
            return false;
        }
        std::string mapPath = m_project.LevelMapPath(level);
        if (fs::exists(mapPath)) map::MapParser::LoadFromFile(mapPath, m_doc);
    }
    m_level = level;
    return true;
}

bool PlaySession::Begin(const ChapterProject& project, const std::string& level) {
    m_project = project;
    m_end = EndState::Playing;
    m_endTitle.clear();
    m_endText.clear();
    m_wantsQuit = false;
    m_paused = false;
    m_time = 0.0;
    m_toasts.clear();
    m_subtitle = Toast{};
    m_vars = m_hooks.initialVars;
    m_inventory = project.Manifest().startItems;
    m_fade = 0.0f;
    m_fadeTarget = 0.0f;
    m_cutCam = false;
    m_codeOverlay = CodeOverlay{};

    if (fs::exists(project.TexturesDir())) render::TextureManager::Instance().AddDirectory(project.TexturesDir());

    if (!LoadLevelData(level)) return false;

    m_script = std::make_unique<ScriptHost>(this);
    m_running = true;

    m_player = PlayerState{};
    BuildGeometry();
    SpawnObjects();
    SetupPlayerFromScene("");
    m_levelStartInventory = m_inventory;
    m_levelStartVars = m_vars;

    m_script->LoadMain();
    m_script->BindAllObjects();
    m_script->Emit("level_loaded", {LuaArg(m_level)});
    if (!project.Manifest().intro.empty()) Subtitle(project.Manifest().intro, 5.0f);
    m_started = true;
    core::Log::Success("Play: уровень '" + level + "' запущен (" + std::to_string(m_scene.GetGameObjects().size()) + " объектов).");
    return true;
}

void PlaySession::Shutdown() {
    if (!m_running && !m_script) return;
    m_script.reset();
    StopAmbient();
    FreeGpu();
    if (m_shader.id != 0) { UnloadShader(m_shader); m_shader = Shader{}; m_shaderOk = false; MemFree(m_material.maps); m_material = Material{}; }
    for (auto& kv : m_sounds) UnloadSound(kv.second);
    m_sounds.clear();
    m_running = false;
    m_monsters.clear();
    m_rt.clear();
    m_batches.clear();
    m_tris.clear();
    m_staticBoxes.clear();
    m_boxCache.clear();
    m_scene.Clear();
    m_doc.Clear();
    m_origScene.Clear();
    m_origDoc.Clear();
    m_destroyQueue.clear();
    m_pendingLoad = false;
    m_restartPending = false;
}

void PlaySession::LoadLevel(const std::string& level, const std::string& spawnObject) {
    m_pendingLoad = true;
    m_pendingLevel = level;
    m_pendingSpawn = spawnObject;
}

void PlaySession::ApplyPendingLoad() {
    if (!m_pendingLoad && !m_restartPending) return;
    std::string level = m_pendingLoad ? m_pendingLevel : m_level;
    std::string spawn = m_pendingLoad ? m_pendingSpawn : "";
    bool restart = m_restartPending;
    m_pendingLoad = false;
    m_restartPending = false;

    if (m_script) m_script->UnloadLevelScripts();
    m_monsters.clear();
    m_rt.clear();
    m_destroyQueue.clear();
    m_cutCam = false;
    m_codeOverlay = CodeOverlay{};

    if (restart) {
        m_inventory = m_levelStartInventory;
        m_vars = m_levelStartVars;
        m_end = EndState::Playing;
    }
    if (!LoadLevelData(level)) {
        Lose("Не удалось загрузить уровень '" + level + "'");
        return;
    }
    BuildGeometry();
    SpawnObjects();
    SetupPlayerFromScene(spawn);
    if (!restart) {
        m_levelStartInventory = m_inventory;
        m_levelStartVars = m_vars;
    }
    m_player.hiddenIn = 0;
    m_player.locked = false;
    m_player.moveLocked = false;
    m_script->BindAllObjects();
    m_script->Emit("level_loaded", {LuaArg(m_level)});
    m_fade = 1.0f;
    Fade(0.0f, 0.8f);
}

void PlaySession::Restart() { m_restartPending = true; }

// ---------------------------------------------------------------------------------------------
// Геометрия и объекты
// ---------------------------------------------------------------------------------------------
namespace {

const char* kVS = R"GLSL(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec3 fragPos;
out vec3 fragNormal;
void main() {
    fragTexCoord = vertexTexCoord;
    fragPos = vertexPosition;
    fragNormal = vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

const char* kFS = R"GLSL(#version 330
in vec2 fragTexCoord;
in vec3 fragPos;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec3 viewPos;
uniform float ambient;
uniform float fogEnd;
uniform int lightCount;
uniform vec3 lightPos[24];
uniform vec3 lightCol[24];
uniform float lightRad[24];
uniform float flashOn;
uniform vec3 flashDir;
uniform float flashPower;
out vec4 finalColor;
void main() {
    vec4 tex = texture(texture0, fragTexCoord);
    vec3 n = normalize(fragNormal);
    vec3 c = vec3(ambient);
    for (int i = 0; i < lightCount; i++) {
        vec3 d = lightPos[i] - fragPos;
        float dist = length(d);
        if (dist < lightRad[i]) {
            float att = 1.0 - dist / lightRad[i];
            att *= att;
            float ndl = dot(n, d / max(dist, 0.001));
            c += lightCol[i] * (att * (0.4 + 0.6 * max(0.0, ndl)));
        }
    }
    float cd = length(fragPos - viewPos);
    if (flashOn > 0.5 && cd < 16.0) {
        vec3 dirTo = (fragPos - viewPos) / max(cd, 0.001);
        float spot = clamp((dot(dirTo, flashDir) - 0.68) / 0.26, 0.0, 1.0);
        float att = 1.0 - cd / 16.0;
        float ndl = max(0.0, -dot(n, dirTo));
        c += vec3(1.0, 0.95, 0.8) * (spot * att * att * (0.5 + 0.5 * ndl) * 1.5 * flashPower);
    }
    float fog = clamp(1.0 - cd / max(5.0, fogEnd), 0.0, 1.0);
    fog = 0.15 + 0.85 * fog;
    finalColor = vec4(tex.rgb * min(c, vec3(1.0)) * fog, 1.0);
}
)GLSL";

}

void PlaySession::FreeGpu() {
    for (auto& g : m_gpu) UnloadMesh(g.mesh);
    m_gpu.clear();
}

void PlaySession::UploadGeometry() {
    if (!m_shaderOk && m_shader.id == 0) {
        m_shader = LoadShaderFromMemory(kVS, kFS);
        m_shaderOk = m_shader.id != 0 && m_shader.id != rlGetShaderIdDefault();
        if (m_shaderOk) {
            m_locView = GetShaderLocation(m_shader, "viewPos");
            m_locAmbient = GetShaderLocation(m_shader, "ambient");
            m_locFogEnd = GetShaderLocation(m_shader, "fogEnd");
            m_locCount = GetShaderLocation(m_shader, "lightCount");
            m_locLPos = GetShaderLocation(m_shader, "lightPos");
            m_locLCol = GetShaderLocation(m_shader, "lightCol");
            m_locLRad = GetShaderLocation(m_shader, "lightRad");
            m_locFlashOn = GetShaderLocation(m_shader, "flashOn");
            m_locFlashDir = GetShaderLocation(m_shader, "flashDir");
            m_locFlashPow = GetShaderLocation(m_shader, "flashPower");
            m_material = LoadMaterialDefault();
            m_material.shader = m_shader;
        } else {
            core::Log::Warn("Шейдер освещения не скомпилировался — используется упрощённый рендер.");
        }
    }
    if (!m_shaderOk) return;

    for (auto& b : m_batches) {
        size_t n = b.verts.size();
        if (n < 3) continue;
        Mesh m{};
        m.vertexCount = (int)n;
        m.triangleCount = (int)(n / 3);
        m.vertices = (float*)MemAlloc((unsigned)(n * 3 * sizeof(float)));
        m.texcoords = (float*)MemAlloc((unsigned)(n * 2 * sizeof(float)));
        m.normals = (float*)MemAlloc((unsigned)(n * 3 * sizeof(float)));
        for (size_t i = 0; i < n; ++i) {
            const auto& v = b.verts[i];
            m.vertices[i * 3] = v.p.x; m.vertices[i * 3 + 1] = v.p.y; m.vertices[i * 3 + 2] = v.p.z;
            m.texcoords[i * 2] = v.uv.x; m.texcoords[i * 2 + 1] = v.uv.y;
            m.normals[i * 3] = v.n.x; m.normals[i * 3 + 1] = v.n.y; m.normals[i * 3 + 2] = v.n.z;
        }
        UploadMesh(&m, false);
        m_gpu.push_back(GpuMesh{m, b.texture});
    }
}

void PlaySession::BuildGeometry() {
    FreeGpu();
    ++m_geoRevision;
    m_tris.clear();
    m_batches.clear();
    m_staticBoxes.clear();

    auto batchFor = [&](const std::string& tex) -> Batch& {
        for (auto& b : m_batches) if (b.texture == tex) return b;
        m_batches.push_back(Batch{tex, {}});
        return m_batches.back();
    };

    std::function<void(Batch&, const map::MapFace&, Vector3, Vector3, Vector3, Vector3, int)> addTri =
        [&](Batch& batch, const map::MapFace& face, Vector3 a, Vector3 b, Vector3 c, Vector3 n, int depth) {
            float eab = Len3D(a, b), ebc = Len3D(b, c), eca = Len3D(c, a);
            float longest = std::max({eab, ebc, eca});
            if (longest > 1e9f && depth < 16) { // дробление не нужно: свет считается шейдером
                Vector3 m;
                auto mid = [](Vector3 p, Vector3 q) { return Vector3{(p.x + q.x) * 0.5f, (p.y + q.y) * 0.5f, (p.z + q.z) * 0.5f}; };
                if (longest == eab) { m = mid(a, b); addTri(batch, face, a, m, c, n, depth + 1); addTri(batch, face, m, b, c, n, depth + 1); }
                else if (longest == ebc) { m = mid(b, c); addTri(batch, face, a, b, m, n, depth + 1); addTri(batch, face, a, m, c, n, depth + 1); }
                else { m = mid(c, a); addTri(batch, face, a, b, m, n, depth + 1); addTri(batch, face, m, b, c, n, depth + 1); }
                return;
            }
            for (Vector3 p : {a, b, c}) batch.verts.push_back(LitVertex{p, face.CalculateUV(p), n});
        };

    auto addBrush = [&](const map::MapBrush& br, bool solid) {
        if (!br.visible) return;
        for (const auto& face : br.faces) {
            const auto& v = face.builtPoly.vertices;
            if (v.size() < 3) continue;
            Batch& batch = batchFor(face.texture);
            for (size_t k = 1; k + 1 < v.size(); ++k) {
                m_tris.push_back(Tri{v[0], v[k], v[k + 1]});
                addTri(batch, face, v[0], v[k], v[k + 1], face.builtPoly.normal, 0);
            }
        }
        if (solid) {
            Vector3 mn = br.bounds.min, mx = br.bounds.max;
            if (mx.x - mn.x > 0.001f && mx.y - mn.y > 0.001f && mx.z - mn.z > 0.001f) m_staticBoxes.push_back(Box{mn, mx, 0});
        }
    };

    for (const auto& br : m_doc.GetWorldBrushes()) addBrush(br, true);
    for (const auto& e : m_doc.GetEntities()) {
        bool trigger = e.classname.rfind("trigger", 0) == 0;
        for (const auto& br : e.brushes) addBrush(br, !trigger);
    }
    UploadGeometry();
    BuildNav();
}

void PlaySession::RegisterObject(scene::GameObject& o) {
    ObjRuntime& rt = m_rt[o.id];
    rt.lightOn = true;
    rt.flickerSeed = (float)(o.id * 7919 % 1000) / 100.0f;
    if (o.door.enabled && o.door.startOpen) { rt.doorOpen = true; rt.doorT = 1.0f; }

    if (o.aiSpawn.enabled) {
        MonsterState m;
        m.objId = o.id;
        m.pos = m_scene.WorldPosition(o);
        m.yaw = o.transform.rotation.y;
        m.mode = o.aiSpawn.patrolWaypoints.empty() ? MonsterMode::Idle : MonsterMode::Patrol;
        m_monsters.push_back(m);
    }
}

void PlaySession::SpawnObjects() {
    m_monsters.reserve(m_monsters.size() + 128);
    std::vector<uint32_t> ids;
    for (const auto& o : m_scene.GetGameObjects()) ids.push_back(o->id);
    for (uint32_t id : ids) {
        if (auto* o = m_scene.FindGameObject(id)) RegisterObject(*o);
    }
}

void PlaySession::SetupPlayerFromScene(const std::string& spawnObject) {
    scene::GameObject* po = nullptr;
    for (const auto& o : m_scene.GetGameObjects()) {
        if (o->player.enabled) { po = o.get(); break; }
    }
    if (!po) {
        for (const auto& o : m_scene.GetGameObjects()) {
            if (o->camera.enabled && o->camera.isMainPlayerCamera) { po = o.get(); break; }
        }
    }
    if (!po) {
        po = m_scene.CreateGameObject("Player");
        po->player.enabled = true;
        po->camera.enabled = true;
        po->camera.isMainPlayerCamera = true;
        Vector3 start{0, 0, 0};
        for (const auto& e : m_doc.GetEntities()) {
            if (e.classname == "info_player_start") { start = e.origin; break; }
        }
        po->transform.position = start;
        core::Log::Warn("В сцене нет Player — создан автоматически.");
    }
    m_playerObj = po->id;

    scene::GameObject* spawn = spawnObject.empty() ? nullptr : m_scene.FindByName(spawnObject);
    Vector3 pos = m_scene.WorldPosition(spawn ? *spawn : *po);
    float yaw = spawn ? spawn->transform.rotation.y : po->transform.rotation.y;

    m_player.pos = pos;
    m_player.yaw = yaw;
    m_player.pitch = 0.0f;
    m_player.velY = 0.0f;
    m_player.eyeHeight = po->player.eyeHeight;
    m_player.fov = po->camera.enabled ? po->camera.fov : 70.0f;
    m_player.radius = po->player.radius;
    m_player.height = po->player.height;
    m_player.jumpSpeed = po->player.jumpSpeed;
    m_player.gravity = po->player.gravity;
    m_player.sens = po->player.mouseSens;
    m_player.hasFlashlight = po->player.startWithFlashlight || HasItem("item_flashlight");
    m_player.flashlight = false;
    m_player.stamina = 1.0f;
    m_player.hiddenIn = 0;
    if (m_player.battery <= 0.0f) m_player.battery = 1.0f;
}

void PlaySession::TeleportPlayer(Vector3 pos, float yawDeg) {
    m_player.pos = pos;
    m_player.yaw = yawDeg;
    m_player.velY = 0.0f;
    m_player.hiddenIn = 0;
}

scene::GameObject* PlaySession::Spawn(const std::string& prefabFile, Vector3 pos) {
    std::string name = prefabFile;
    if (name.size() < 9 || name.substr(name.size() - 9) != ".kmprefab") name += ".kmprefab";
    uint32_t before = 0;
    for (const auto& o : m_scene.GetGameObjects()) before = std::max(before, o->id);
    scene::GameObject* root = m_scene.InstantiatePrefab(m_project.PrefabsDir() + "/" + name, pos);
    if (!root) {
        core::Log::Error("game.spawn: префаб не найден: " + name);
        return nullptr;
    }
    root->transform.position = pos;
    uint32_t rootId = root->id;
    std::vector<uint32_t> created;
    for (const auto& o : m_scene.GetGameObjects()) if (o->id > before) created.push_back(o->id);
    for (uint32_t id : created) if (auto* o = m_scene.FindGameObject(id)) RegisterObject(*o);
    for (uint32_t id : created) m_script->BindObject(id);
    for (uint32_t id : created) m_script->CallHook(id, "on_start");
    return m_scene.FindGameObject(rootId);
}

void PlaySession::DestroyObject(uint32_t id) {
    if (id == m_playerObj) return;
    if (auto* o = m_scene.FindGameObject(id)) {
        o->active = false;
        m_destroyQueue.push_back(id);
    }
}

void PlaySession::FlushDestroyed() {
    if (m_destroyQueue.empty()) return;
    std::vector<uint32_t> q;
    q.swap(m_destroyQueue);
    for (uint32_t id : q) {
        std::vector<uint32_t> doomed{id};
        for (size_t i = 0; i < doomed.size(); ++i)
            for (const auto& o : m_scene.GetGameObjects()) if (o->parent == doomed[i]) doomed.push_back(o->id);
        for (uint32_t d : doomed) {
            m_script->UnbindObject(d);
            m_rt.erase(d);
            m_monsters.erase(std::remove_if(m_monsters.begin(), m_monsters.end(), [d](const MonsterState& m) { return m.objId == d; }), m_monsters.end());
        }
        m_scene.DestroyGameObject(id);
    }
}

void PlaySession::SetObjectActive(uint32_t id, bool active) {
    if (auto* o = m_scene.FindGameObject(id)) o->active = active;
}

MonsterState* PlaySession::FindMonster(uint32_t objId) {
    for (auto& m : m_monsters) if (m.objId == objId) return &m;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// Инвентарь, сообщения, переменные
// ---------------------------------------------------------------------------------------------
const std::string& PlaySession::ItemName(const std::string& id) const {
    static std::string tmp;
    const ItemDef* d = m_project.FindItem(id);
    tmp = (d && !d->name.empty()) ? d->name : id;
    return tmp;
}

bool PlaySession::HasItem(const std::string& id) const {
    return std::find(m_inventory.begin(), m_inventory.end(), id) != m_inventory.end();
}

bool PlaySession::GiveItem(const std::string& id, bool silent) {
    if (id.empty()) return false;
    m_inventory.push_back(id);
    if (id == "item_flashlight") m_player.hasFlashlight = true;
    if (!silent) {
        Notify("Получено: " + ItemName(id), 3.0f);
        PlaySfx("audio/sfx/sfx_key_pickup.wav", 0.8f);
    }
    if (m_script) m_script->Emit("item_added", {LuaArg(id)});
    return true;
}

bool PlaySession::TakeItem(const std::string& id) {
    auto it = std::find(m_inventory.begin(), m_inventory.end(), id);
    if (it == m_inventory.end()) return false;
    m_inventory.erase(it);
    if (m_script) m_script->Emit("item_removed", {LuaArg(id)});
    return true;
}

void PlaySession::Notify(const std::string& text, float seconds) {
    m_toasts.push_back(Toast{text, 0.0f, seconds});
    if (m_toasts.size() > 5) m_toasts.erase(m_toasts.begin());
}

void PlaySession::Subtitle(const std::string& text, float seconds) {
    m_subtitle = Toast{text, 0.0f, seconds};
}

void PlaySession::SetVar(const std::string& key, const Var& v) {
    auto old = m_vars.find(key);
    bool changed = old == m_vars.end() || old->second.type != v.type || old->second.number != v.number || old->second.str != v.str || old->second.boolean != v.boolean;
    m_vars[key] = v;
    if (!changed) return;
    if (m_script) {
        LuaArg val;
        if (v.type == Var::Type::Bool) val = LuaArg(v.boolean);
        else if (v.type == Var::Type::Number) val = LuaArg(v.number);
        else if (v.type == Var::Type::String) val = LuaArg(v.str);
        m_script->Emit("var_changed", {LuaArg(key), val});
    }
}

Var PlaySession::GetVar(const std::string& key) const {
    auto it = m_vars.find(key);
    return it == m_vars.end() ? Var{} : it->second;
}

// ---------------------------------------------------------------------------------------------
// Звук
// ---------------------------------------------------------------------------------------------
std::string PlaySession::ResolveAudio(const std::string& path) const {
    if (path.empty()) return "";
    std::vector<std::string> candidates{m_project.AudioDir() + "/" + path, m_project.Root() + "/" + path};
    if (!m_hooks.gameAssetsDir.empty()) candidates.push_back(m_hooks.gameAssetsDir + "/" + path);
    candidates.push_back(path);
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) return c;
    }
    return "";
}

void PlaySession::PlaySfx(const std::string& path, float volume) {
    if (!IsAudioDeviceReady()) return;
    auto it = m_sounds.find(path);
    if (it == m_sounds.end()) {
        std::string real = ResolveAudio(path);
        Sound s{};
        if (!real.empty()) s = LoadSound(real.c_str());
        else core::Log::Warn("Звук не найден: " + path);
        it = m_sounds.emplace(path, s).first;
    }
    if (it->second.frameCount == 0) return;
    float master = m_hooks.sfxVolume ? m_hooks.sfxVolume() : 1.0f;
    SetSoundVolume(it->second, std::clamp(volume * master, 0.0f, 1.0f));
    PlaySound(it->second);
}

void PlaySession::PlayAmbient(const std::string& path, float volume) {
    if (!IsAudioDeviceReady()) return;
    StopAmbient();
    std::string real = ResolveAudio(path);
    if (real.empty()) { core::Log::Warn("Музыка не найдена: " + path); return; }
    m_ambient = LoadMusicStream(real.c_str());
    if (m_ambient.frameCount == 0) return;
    m_ambient.looping = true;
    float master = m_hooks.sfxVolume ? m_hooks.sfxVolume() : 1.0f;
    SetMusicVolume(m_ambient, std::clamp(volume * master, 0.0f, 1.0f));
    PlayMusicStream(m_ambient);
    m_hasAmbient = true;
}

void PlaySession::StopAmbient() {
    if (!m_hasAmbient) return;
    StopMusicStream(m_ambient);
    UnloadMusicStream(m_ambient);
    m_ambient = Music{};
    m_hasAmbient = false;
}

// ---------------------------------------------------------------------------------------------
// Исходы, эффекты
// ---------------------------------------------------------------------------------------------
void PlaySession::Win(const std::string& title, const std::string& text) {
    if (m_end != EndState::Playing) return;
    m_end = EndState::Won;
    m_endTitle = title.empty() ? m_project.Manifest().winTitle : title;
    m_endText = text;
    m_endTimer = 0.0f;
    m_script->Emit("chapter_won", {});
}

void PlaySession::Lose(const std::string& text) {
    if (m_end != EndState::Playing) return;
    m_end = EndState::Lost;
    m_endTitle = "ТЫ МЁРТВ";
    m_endText = text.empty() ? m_project.Manifest().loseText : text;
    m_endTimer = 0.0f;
}

void PlaySession::Fade(float targetAlpha, float seconds) {
    m_fadeTarget = Clamp01(targetAlpha);
    m_fadeSpeed = seconds <= 0.001f ? 1000.0f : std::fabs(m_fadeTarget - m_fade) / seconds;
}

void PlaySession::Shake(float intensity, float seconds) {
    m_shakeI = intensity;
    m_shakeT = seconds;
}

void PlaySession::SetCutsceneCamera(bool enabled, Vector3 pos, Vector3 lookAt) {
    m_cutCam = enabled;
    m_cutPos = pos;
    m_cutLook = lookAt;
}

void PlaySession::PlayTvVideo(int tape, float duration) {
    m_tvActive = true;
    m_tvTape = std::clamp(tape, 1, 3);
    m_tvTimer = 0.0f;
    m_tvDuration = std::max(2.0f, duration);
    m_tvFrame = 1;
    m_tvGlitch = 0.0f;
    PlaySfx("audio/sfx/sfx_radio_click.wav", 0.9f);
    for (auto& up : m_scene.GetGameObjects()) {
        if (!up) continue;
        if (up->name == "Экран_телевизора") {
            up->meshRenderer.textureName = "tape" + std::to_string(m_tvTape) + "_frame1";
            up->meshRenderer.emissive = true;
        } else if (up->name == "Свет_телевизора") {
            m_rt[up->id].lightOn = true;
            up->light.enabled = true;
            up->light.intensity = 0.7f;
        }
    }
}

void PlaySession::StopTvVideo() {
    if (!m_tvActive) return;
    m_tvActive = false;
    for (auto& up : m_scene.GetGameObjects()) {
        if (!up) continue;
        if (up->name == "Экран_телевизора") {
            up->meshRenderer.textureName = "ch35_tv_off";
            up->meshRenderer.emissive = false;
        } else if (up->name == "Свет_телевизора") {
            m_rt[up->id].lightOn = false;
            up->light.enabled = false;
            up->light.intensity = 0.0f;
        }
    }
}

void PlaySession::AskCode(const std::string& prompt, const std::string& expected, std::function<void(bool, const std::string&)> cb) {
    m_codeOverlay = CodeOverlay{};
    m_codeOverlay.active = true;
    m_codeOverlay.prompt = prompt;
    m_codeOverlay.expected = expected;
    m_codeOverlay.cb = std::move(cb);
}

// ---------------------------------------------------------------------------------------------
// Коллизии
// ---------------------------------------------------------------------------------------------
Vector3 PlaySession::ColliderSize(const scene::GameObject& o) const {
    if (!o.collider.enabled) return o.transform.scale;
    const auto& c = o.collider;
    if (c.shape == 1) return Vector3{c.radius * 2, c.radius * 2, c.radius * 2};
    if (c.shape == 2) return Vector3{c.radius * 2, std::max(c.height, c.radius * 2), c.radius * 2};
    return c.size;
}

BoundingBox PlaySession::ObjectBounds(const scene::GameObject& o) const {
    Vector3 c = m_scene.WorldPosition(o);
    Vector3 size = ColliderSize(o);
    Vector3 off = o.collider.enabled ? o.collider.offset : Vector3{0, 0, 0};
    float yaw = std::fmod(std::fabs(o.transform.rotation.y), 180.0f);
    if (yaw > 45.0f && yaw < 135.0f) std::swap(size.x, size.z);
    c = Vector3{c.x + off.x, c.y + off.y, c.z + off.z};
    return BoundingBox{Vector3{c.x - size.x * 0.5f, c.y - size.y * 0.5f, c.z - size.z * 0.5f},
                       Vector3{c.x + size.x * 0.5f, c.y + size.y * 0.5f, c.z + size.z * 0.5f}};
}

void PlaySession::BuildBoxCache() {
    m_boxCache = m_staticBoxes;
    for (const auto& up : m_scene.GetGameObjects()) {
        const auto& o = *up;
        if (!o.active || !o.collider.enabled || o.collider.isTrigger) continue;
        if (o.player.enabled || o.aiSpawn.enabled || o.id == m_playerObj) continue;
        if (o.door.enabled) {
            auto it = m_rt.find(o.id);
            if (it != m_rt.end() && it->second.doorT > 0.5f) continue;
        }
        if (o.itemPickup.enabled) continue;
        BoundingBox b = ObjectBounds(o);
        m_boxCache.push_back(Box{b.min, b.max, o.id});
    }
}

Vector3 PlaySession::MoveCollide(Vector3 from, Vector3 delta, float r, float h, uint32_t ignore, float step, bool* hitWall) const {
    auto blocked = [&](Vector3 p) {
        for (const auto& b : m_boxCache) {
            if (ignore != 0 && b.owner == ignore) continue;
            if (p.x + r <= b.min.x || p.x - r >= b.max.x) continue;
            if (p.z + r <= b.min.z || p.z - r >= b.max.z) continue;
            if (b.max.y <= p.y + step || b.min.y >= p.y + h) continue;
            return true;
        }
        return false;
    };
    Vector3 pos = from;
    if (hitWall) *hitWall = false;
    Vector3 nx = pos; nx.x += delta.x;
    if (!blocked(nx)) pos = nx; else if (hitWall) *hitWall = true;
    Vector3 nz = pos; nz.z += delta.z;
    if (!blocked(nz)) pos = nz; else if (hitWall) *hitWall = true;
    return pos;
}

float PlaySession::GroundHeight(Vector3 pos, float radius, float maxStepUp) const {
    float g = -1e9f;
    bool any = false;
    float r = radius * 0.8f;
    for (const auto& b : m_boxCache) {
        if (pos.x + r <= b.min.x || pos.x - r >= b.max.x) continue;
        if (pos.z + r <= b.min.z || pos.z - r >= b.max.z) continue;
        any = true;
        if (b.max.y <= pos.y + maxStepUp && b.max.y > g) g = b.max.y;
    }
    if (m_staticBoxes.empty() && !any) return 0.0f;
    return g;
}

bool PlaySession::RayWorld(Vector3 from, Vector3 dir, float maxDist, float* outDist) const {
    Ray ray{from, dir};
    float best = maxDist;
    bool hit = false;
    for (const auto& t : m_tris) {
        RayCollision c = GetRayCollisionTriangle(ray, t.a, t.b, t.c);
        if (c.hit && c.distance < best && c.distance > 0.001f) { best = c.distance; hit = true; }
    }
    for (const auto& up : m_scene.GetGameObjects()) {
        const auto& o = *up;
        if (!o.active || !o.door.enabled || !o.collider.enabled) continue;
        auto it = m_rt.find(o.id);
        if (it != m_rt.end() && it->second.doorT > 0.5f) continue;
        RayCollision c = GetRayCollisionBox(ray, ObjectBounds(o));
        if (c.hit && c.distance < best && c.distance > 0.001f) { best = c.distance; hit = true; }
    }
    if (outDist && hit) *outDist = best;
    return hit;
}

bool PlaySession::LineOfSightFree(Vector3 a, Vector3 b) const {
    Vector3 d{b.x - a.x, b.y - a.y, b.z - a.z};
    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len < 0.01f) return true;
    d = Vector3{d.x / len, d.y / len, d.z / len};
    return !RayWorld(a, d, len - 0.05f, nullptr);
}

// ---------------------------------------------------------------------------------------------
// Обновление
// ---------------------------------------------------------------------------------------------
void PlaySession::Update(float dt, bool inputActive) {
    if (!m_running) return;
    dt = std::min(dt, 0.1f);
    ApplyPendingLoad();

    if (m_hasAmbient) UpdateMusicStream(m_ambient);

    // Esc: пауза / закрытие оверлеев
    m_inputActive = inputActive;
    if (inputActive && IsKeyPressed(KEY_ESCAPE)) {
        if (m_tvActive) {
            StopTvVideo();
        } else if (m_codeOverlay.active) {
            auto cb = m_codeOverlay.cb;
            m_codeOverlay = CodeOverlay{};
            if (cb) cb(false, "");
        } else if (m_invOpen) {
            m_invOpen = false;
        } else if (m_pauseEnabled && m_end == EndState::Playing) {
            m_paused = !m_paused;
        } else if (m_end != EndState::Playing) {
            m_wantsQuit = true;
        }
    }

    if (m_end != EndState::Playing) {
        m_endTimer += dt;
        m_fade = std::min(1.0f, m_endTimer * 0.6f);
        if (inputActive && m_endTimer > 1.0f) {
            if (m_end == EndState::Lost && IsKeyPressed(KEY_R)) Restart();
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (m_end == EndState::Won) m_wantsQuit = true;
                else Restart();
            }
            if (IsKeyPressed(KEY_Q)) m_wantsQuit = true;
        }
        UpdateToasts(dt);
        return;
    }

    if (m_paused) {
        if (inputActive && IsKeyPressed(KEY_Q)) m_wantsQuit = true;
        return;
    }

    m_time += dt;
    m_flickerTime += dt;

    if (m_tvActive) {
        if (inputActive && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
            StopTvVideo();
        } else {
            m_tvTimer += dt;
            if (m_tvTimer >= m_tvDuration) {
                StopTvVideo();
            } else {
                float prog = m_tvTimer / m_tvDuration;
                m_tvFrame = std::clamp((int)(prog * 5.0f) + 1, 1, 5);
                m_tvGlitch += dt;
                std::string frameTex = "tape" + std::to_string(m_tvTape) + "_frame" + std::to_string(m_tvFrame);
                for (auto& up : m_scene.GetGameObjects()) {
                    if (!up) continue;
                    if (up->name == "Экран_телевизора") {
                        up->meshRenderer.textureName = frameTex;
                        up->meshRenderer.emissive = true;
                    } else if (up->name == "Свет_телевизора") {
                        m_rt[up->id].lightOn = true;
                        up->light.enabled = true;
                        float pulse = 0.55f + 0.20f * std::sin(m_tvTimer * 26.0f) + 0.15f * std::cos(m_tvTimer * 43.0f);
                        up->light.intensity = pulse;
                    }
                }
            }
        }
    }

    // затухание
    if (m_fade != m_fadeTarget) {
        float step = m_fadeSpeed * dt;
        if (std::fabs(m_fadeTarget - m_fade) <= step) m_fade = m_fadeTarget;
        else m_fade += (m_fadeTarget > m_fade ? step : -step);
    }
    if (m_shakeT > 0.0f) m_shakeT -= dt;

    BuildBoxCache();

    bool overlayBlocks = m_codeOverlay.active || m_invOpen;
    if (m_codeOverlay.active) UpdateCodeOverlay(inputActive);
    UpdatePlayer(dt, inputActive && !overlayBlocks);
    UpdateInteraction(dt, inputActive && !overlayBlocks);
    UpdateDoors(dt);
    UpdatePhysics(dt);
    UpdateMonsters(dt);
    UpdateTriggers();

    // инвентарь
    if (inputActive && !m_codeOverlay.active && IsKeyPressed(KEY_TAB)) {
        m_invOpen = !m_invOpen;
        m_invCursor = 0;
    }
    if (m_invOpen && inputActive) {
        int n = (int)m_inventory.size();
        if (n > 0) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) m_invCursor = (m_invCursor + 1) % n;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) m_invCursor = (m_invCursor + n - 1) % n;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E)) {
                std::string id = m_inventory[std::min(m_invCursor, n - 1)];
                if (id == "item_batteries") {
                    m_player.battery = 1.0f;
                    TakeItem(id);
                    Notify("Батарейки заменены.");
                    PlaySfx("audio/sfx/sfx_flashlight_click.wav", 0.8f);
                } else {
                    m_script->Emit("item_used", {LuaArg(id)});
                }
                m_invCursor = 0;
            }
        }
    }

    m_script->Update(dt);
    SyncTransforms();
    FlushDestroyed();
    UpdateToasts(dt);
    UpdateCamera(dt);
    CollectLights();
}

void PlaySession::UpdateToasts(float dt) {
    for (auto& t : m_toasts) t.t += dt;
    m_toasts.erase(std::remove_if(m_toasts.begin(), m_toasts.end(), [](const Toast& t) { return t.t > t.dur; }), m_toasts.end());
    if (!m_subtitle.text.empty()) {
        m_subtitle.t += dt;
        if (m_subtitle.t > m_subtitle.dur) m_subtitle = Toast{};
    }
}

void PlaySession::SyncTransforms() {
    if (auto* po = m_scene.FindGameObject(m_playerObj)) {
        m_scene.SetWorldPosition(*po, m_player.pos);
        po->transform.rotation.y = m_player.yaw;
    }
    for (const auto& m : m_monsters) {
        if (auto* o = m_scene.FindGameObject(m.objId)) {
            m_scene.SetWorldPosition(*o, m.pos);
            o->transform.rotation.y = m.yaw;
        }
    }
}

void PlaySession::UpdatePlayer(float dt, bool input) {
    PlayerState& p = m_player;
    const scene::GameObject* po = m_scene.FindGameObject(m_playerObj);
    float walk = po ? po->player.walkSpeed : 2.6f;
    float sprint = po ? po->player.sprintSpeed : 4.6f;
    float crouchSp = po ? po->player.crouchSpeed : 1.3f;

    bool canMove = input && !p.locked && !p.moveLocked && p.hiddenIn == 0;
    bool canLook = input && !p.locked && !m_cutCam;

    if (canLook) {
        Vector2 md = GetMouseDelta();
        p.yaw -= md.x * p.sens;
        p.pitch = std::clamp(p.pitch - md.y * p.sens, -85.0f, 85.0f);
        if (p.hiddenIn != 0) p.yaw = m_hideBaseYaw + std::clamp(AngleDiff(p.yaw, m_hideBaseYaw), -55.0f, 55.0f);
    }
    if (p.yaw > 360.0f) p.yaw -= 360.0f;
    if (p.yaw < -360.0f) p.yaw += 360.0f;

    Vector3 move{0, 0, 0};
    bool moving = false;
    p.sprinting = false;
    if (canMove) {
        Vector3 fwd = Forward(p.yaw);
        Vector3 right{-std::cos(p.yaw * DEG2RAD), 0.0f, std::sin(p.yaw * DEG2RAD)};
        if (IsKeyDown(KEY_W)) { move.x += fwd.x; move.z += fwd.z; }
        if (IsKeyDown(KEY_S)) { move.x -= fwd.x; move.z -= fwd.z; }
        if (IsKeyDown(KEY_D)) { move.x += right.x; move.z += right.z; }
        if (IsKeyDown(KEY_A)) { move.x -= right.x; move.z -= right.z; }
        float l = std::sqrt(move.x * move.x + move.z * move.z);
        moving = l > 0.01f;
        if (moving) { move.x /= l; move.z /= l; }

        bool wantCrouch = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C);
        if (wantCrouch != p.crouching) {
            if (wantCrouch) p.crouching = true;
            else {
                // встать можно, только если над головой свободно
                bool clear = true;
                for (const auto& b : m_boxCache) {
                    if (p.pos.x + m_player.radius <= b.min.x || p.pos.x - m_player.radius >= b.max.x) continue;
                    if (p.pos.z + m_player.radius <= b.min.z || p.pos.z - m_player.radius >= b.max.z) continue;
                    if (b.min.y > p.pos.y + (m_player.height * 0.61f) && b.min.y < p.pos.y + m_player.height) { clear = false; break; }
                }
                if (clear) p.crouching = false;
            }
        }
        bool wantSprint = IsKeyDown(KEY_LEFT_SHIFT) && moving && !p.crouching && p.stamina > 0.05f;
        p.sprinting = wantSprint;
        float speed = p.crouching ? crouchSp : (p.sprinting ? sprint : walk);

        if (p.sprinting) p.stamina = std::max(0.0f, p.stamina - dt * 0.22f);
        else p.stamina = std::min(1.0f, p.stamina + dt * (moving ? 0.10f : 0.2f));

        Vector3 delta{move.x * speed * dt, 0, move.z * speed * dt};
        float h = p.crouching ? (m_player.height * 0.61f) : m_player.height;
        p.pos = MoveCollide(p.pos, delta, m_player.radius, h, m_playerObj, kStepHeight);

        if (moving) {
            p.bob += dt * speed * 2.6f;
            p.stepTimer -= dt;
            if (p.stepTimer <= 0.0f) {
                p.stepTimer = p.sprinting ? 0.32f : (p.crouching ? 0.7f : 0.5f);
                int n = 1 + (int)(m_time * 13.0) % 4;
                PlaySfx("audio/sfx/sfx_footstep_concrete_0" + std::to_string(n) + ".wav", p.crouching ? 0.25f : (p.sprinting ? 0.8f : 0.5f));
                float noiseR = p.crouching ? 1.5f : (p.sprinting ? 8.5f : 3.5f);
                MakeNoise(p.pos, noiseR);
            }
        }
    } else {
        p.stamina = std::min(1.0f, p.stamina + dt * 0.2f);
    }

    if (canMove && p.jumpSpeed > 0.0f && p.onGround && !p.crouching && IsKeyPressed(KEY_SPACE)) {
        p.velY = p.jumpSpeed;
        p.onGround = false;
    }
    // вертикаль
    float ground = GroundHeight(p.pos, m_player.radius, kStepHeight);
    if (p.pos.y > ground + 0.02f || p.velY > 0.0f) {
        p.velY -= p.gravity * dt;
        p.pos.y += p.velY * dt;
        if (p.pos.y <= ground) { p.pos.y = ground; p.velY = 0.0f; p.onGround = true; }
        else p.onGround = false;
    } else {
        p.pos.y = ground;
        p.velY = 0.0f;
        p.onGround = true;
    }
    if (p.pos.y < -40.0f) {
        core::Log::Warn("Игрок упал за пределы карты — возврат на старт уровня.");
        SetupPlayerFromScene("");
    }

    // фонарь
    if (input && p.hasFlashlight && IsKeyPressed(KEY_F)) {
        if (p.battery > 0.0f) {
            p.flashlight = !p.flashlight;
            PlaySfx("audio/sfx/sfx_flashlight_click.wav", 0.7f);
        } else {
            Notify("Батарейки сели.");
        }
    }
    if (p.flashlight) {
        p.battery = std::max(0.0f, p.battery - m_project.Manifest().flashlightDrain * dt);
        if (p.battery <= 0.0f) { p.flashlight = false; Notify("Фонарь погас: села батарейка."); }
    }
}

void PlaySession::UpdateCamera(float dt) {
    (void)dt;
    PlayerState& p = m_player;
    float eye = p.crouching ? p.eyeHeight * 0.62f : p.eyeHeight;
    Vector3 pos{p.pos.x, p.pos.y + eye + std::sin(p.bob) * 0.025f * (p.sprinting ? 1.6f : 1.0f), p.pos.z};
    if (p.hiddenIn != 0) {
        if (auto* o = m_scene.FindGameObject((uint32_t)p.hiddenIn)) {
            Vector3 c = m_scene.WorldPosition(*o);
            pos = Vector3{c.x + o->hidingSpot.eyeOffset.x, c.y + o->hidingSpot.eyeOffset.y, c.z + o->hidingSpot.eyeOffset.z};
        }
    }
    if (m_shakeT > 0.0f) {
        float k = m_shakeI * std::min(1.0f, m_shakeT * 2.0f);
        pos.x += ((float)GetRandomValue(-100, 100) / 100.0f) * k * 0.1f;
        pos.y += ((float)GetRandomValue(-100, 100) / 100.0f) * k * 0.1f;
    }
    m_camera.up = Vector3{0, 1, 0};
    m_camera.fovy = m_fovOverride > 0.0f ? m_fovOverride : p.fov;
    m_camera.projection = CAMERA_PERSPECTIVE;
    if (m_cutCam) {
        m_camera.position = m_cutPos;
        m_camera.target = m_cutLook;
    } else {
        m_camera.position = pos;
        Vector3 f = Forward(p.yaw, p.pitch);
        m_camera.target = Vector3{pos.x + f.x, pos.y + f.y, pos.z + f.z};
    }
}

// ---------------------------------------------------------------------------------------------
// Взаимодействие
// ---------------------------------------------------------------------------------------------
scene::GameObject* PlaySession::PickInteractable(float* outDist) {
    Vector3 eye = m_camera.position;
    Vector3 dir = Forward(m_player.yaw, m_player.pitch);
    Ray ray{eye, dir};
    scene::GameObject* best = nullptr;
    float bestD = kInteractDist;

    for (const auto& up : m_scene.GetGameObjects()) {
        scene::GameObject& o = *up;
        if (!o.active) continue;
        bool interactive = o.searchSlot.enabled || o.door.enabled || o.hidingSpot.enabled || o.itemPickup.enabled || (o.script.enabled && o.script.interactable);
        if (!interactive) continue;
        if (m_player.hiddenIn != 0 && (uint32_t)m_player.hiddenIn != o.id) continue;
        auto rit = m_rt.find(o.id);
        if (rit != m_rt.end() && !rit->second.visible && !o.script.interactable) continue;

        BoundingBox b = ObjectBounds(o);
        const float pad = 0.12f;
        b.min = Vector3{b.min.x - pad, b.min.y - pad, b.min.z - pad};
        b.max = Vector3{b.max.x + pad, b.max.y + pad, b.max.z + pad};
        RayCollision c = GetRayCollisionBox(ray, b);
        if (!c.hit || c.distance > bestD) continue;
        float wd = 0.0f;
        if (RayWorld(eye, dir, c.distance, &wd) && wd < c.distance - 0.15f) continue;
        bestD = c.distance;
        best = &o;
    }
    if (outDist) *outDist = bestD;
    return best;
}

std::string PlaySession::PromptFor(const scene::GameObject& o) const {
    if (m_player.hiddenIn != 0) return "[E] Выйти из укрытия";
    auto it = m_rt.find(o.id);
    bool open = it != m_rt.end() && it->second.doorOpen;
    if (o.script.enabled && o.script.interactable && !o.door.enabled && !o.searchSlot.enabled && !o.hidingSpot.enabled && !o.itemPickup.enabled) return "[E] " + o.script.interactPrompt;
    if (o.itemPickup.enabled) return "[E] Взять: " + const_cast<PlaySession*>(this)->ItemName(o.itemPickup.itemId);
    if (o.hidingSpot.enabled) return "[E] Спрятаться";
    if (o.door.enabled) {
        if (o.door.isLocked) return "[E] Заперто";
        return open ? "[E] Закрыть дверь" : "[E] Открыть дверь";
    }
    if (o.searchSlot.enabled) {
        bool searched = it != m_rt.end() && it->second.searched;
        if (searched && o.searchSlot.containedItem.empty()) return "";
        if (o.searchSlot.isLocked) return "[E] " + o.searchSlot.displayName + " (заперто)";
        return "[E] Обыскать: " + o.searchSlot.displayName;
    }
    return "[E] Использовать";
}

void PlaySession::UpdateInteraction(float dt, bool input) {
    m_prompt.clear();
    uint32_t prevFocus = m_focusObj;
    m_focusObj = 0;
    if (m_player.locked || m_cutCam) { m_focusHold = 0.0f; return; }

    scene::GameObject* o = PickInteractable(nullptr);
    if (!o) { m_focusHold = 0.0f; return; }
    m_focusObj = o->id;
    m_prompt = PromptFor(*o);
    if (m_prompt.empty()) return;
    if (!input) return;

    if (prevFocus != m_focusObj) m_focusHold = 0.0f;
    uint32_t id = o->id;

    // обыск удерживанием E
    bool isSearch = o->searchSlot.enabled && m_player.hiddenIn == 0;
    if (isSearch) {
        ObjRuntime& rt = m_rt[id];
        bool empty = rt.searched && o->searchSlot.containedItem.empty();
        if (empty) return;
        if (IsKeyPressed(KEY_E) && o->searchSlot.isLocked) {
            TryUnlockSlot(*o);
            return;
        }
        if (IsKeyDown(KEY_E) && !o->searchSlot.isLocked) {
            rt.searchProgress += dt / std::max(0.05f, o->searchSlot.searchDuration);
            m_focusHold = rt.searchProgress;
            if (rt.searchProgress >= 1.0f) {
                rt.searchProgress = 0.0f;
                FinishSearch(*o);
            }
        } else {
            rt.searchProgress = 0.0f;
            m_focusHold = 0.0f;
        }
        return;
    }

    if (IsKeyPressed(KEY_E)) InteractWith(*o);
}

bool PlaySession::TryUnlockSlot(scene::GameObject& o) {
    const std::string tool = o.searchSlot.requiredTool;
    if (!tool.empty() && HasItem(tool)) {
        o.searchSlot.isLocked = false;
        if (o.searchSlot.consumeTool) TakeItem(tool);
        Notify("Вскрыто: " + o.searchSlot.displayName);
        PlaySfx("audio/sfx/sfx_ch3_crowbar_pry.wav", 0.9f);
        MakeNoise(m_scene.WorldPosition(o), std::max(o.searchSlot.noiseRadius, 6.0f));
        return true;
    }
    Notify(tool.empty() ? "Заперто." : "Заперто. Нужно: " + ItemName(tool));
    PlaySfx("audio/sfx/sfx_door_locked_rattle.wav", 0.7f);
    m_script->CallHook(o.id, "on_locked");
    return false;
}

bool PlaySession::DebugUse(const std::string& objectName) {
    scene::GameObject* o = m_scene.FindByName(objectName);
    if (!o) return false;
    if (o->searchSlot.enabled) {
        if (o->searchSlot.isLocked && !TryUnlockSlot(*o)) return false;
        FinishSearch(*o);
        return true;
    }
    return InteractWith(*o);
}

bool PlaySession::DebugAnswerCode(const std::string& code) {
    if (!m_codeOverlay.active) return false;
    m_codeOverlay.entered = code;
    auto cb = m_codeOverlay.cb;
    bool ok = code == m_codeOverlay.expected;
    m_codeOverlay = CodeOverlay{};
    if (cb) cb(ok, code);
    return ok;
}

void PlaySession::FinishSearch(scene::GameObject& o) {
    uint32_t id = o.id;
    ObjRuntime& rt = m_rt[id];
    std::string item = o.searchSlot.containedItem;
    rt.searched = true;
    PlaySfx("audio/sfx/sfx_ch3_creak_wood_01.wav", 0.6f);
    if (!item.empty()) {
        o.searchSlot.containedItem.clear();
        GiveItem(item);
    } else {
        Notify("Пусто.");
    }
    if (o.searchSlot.noiseRadius > 0.0f) MakeNoise(m_scene.WorldPosition(o), o.searchSlot.noiseRadius);
    m_script->CallHook(id, "on_search", {LuaArg(item)});
    m_script->Emit("slot_searched", {LuaArg::Object(id), LuaArg(item)});
}

bool PlaySession::InteractWith(scene::GameObject& o) {
    uint32_t id = o.id;

    if (m_player.hiddenIn != 0) { ExitHiding(); return true; }

    if (o.itemPickup.enabled) {
        std::string item = o.itemPickup.itemId;
        o.active = false;
        GiveItem(item);
        m_script->CallHook(id, "on_item_picked", {LuaArg(item)});
        return true;
    }
    if (o.hidingSpot.enabled) {
        EnterHiding(o);
        return true;
    }
    if (o.door.enabled) {
        if (o.door.isLocked) {
            if (!o.door.requiredKey.empty() && HasItem(o.door.requiredKey)) {
                o.door.isLocked = false;
                if (o.door.consumeKey) TakeItem(o.door.requiredKey);
                Notify("Отперто: " + ItemName(o.door.requiredKey));
                PlaySfx("audio/sfx/sfx_ch3_padlock_open.wav", 0.8f);
                m_script->CallHook(id, "on_unlocked");
                OpenDoor(id, true);
            } else {
                Notify(o.door.requiredKey.empty() ? "Заперто." : "Заперто. Нужно: " + ItemName(o.door.requiredKey));
                PlaySfx("audio/sfx/sfx_door_locked_rattle.wav", 0.8f);
                MakeNoise(m_scene.WorldPosition(o), 3.0f);
                m_script->CallHook(id, "on_locked");
            }
        } else {
            OpenDoor(id, !m_rt[id].doorOpen);
        }
        return true;
    }
    if (o.script.enabled && o.script.interactable) {
        m_script->CallHook(id, "on_interact");
        return true;
    }
    return false;
}

void PlaySession::OpenDoor(uint32_t id, bool open) {
    auto* o = m_scene.FindGameObject(id);
    if (!o || !o->door.enabled) return;
    ObjRuntime& rt = m_rt[id];
    if (rt.doorOpen == open) return;
    rt.doorOpen = open;
    if (!o->door.openSound.empty()) PlaySfx(open ? o->door.openSound : "audio/sfx/sfx_door_close_latch.wav", 0.8f);
    MakeNoise(m_scene.WorldPosition(*o), open ? 4.0f : 2.0f);
    m_script->CallHook(id, open ? "on_door_opened" : "on_door_closed");
}

void PlaySession::UpdateDoors(float dt) {
    for (auto& kv : m_rt) {
        auto* o = m_scene.FindGameObject(kv.first);
        if (!o || !o->door.enabled) continue;
        float target = kv.second.doorOpen ? 1.0f : 0.0f;
        float rate = std::max(0.2f, o->door.speed) * 0.5f;
        if (kv.second.doorT < target) kv.second.doorT = std::min(target, kv.second.doorT + dt * rate);
        else if (kv.second.doorT > target) kv.second.doorT = std::max(target, kv.second.doorT - dt * rate);
    }
}

// ---------------------------------------------------------------------------------------------
// Физика (Rigidbody): гравитация, сопротивление, столкновения с брашами и коллайдерами, отскок, толчок игроком.
// Все формы столкновений аппроксимируются осевой коробкой по размеру коллайдера.
// ---------------------------------------------------------------------------------------------
void PlaySession::UpdatePhysics(float dt) {
    constexpr float kBodyGravity = 9.81f;
    std::vector<uint32_t> ids;
    for (const auto& up : m_scene.GetGameObjects()) {
        if (up->active && up->rigidbody.enabled && !up->rigidbody.isKinematic) ids.push_back(up->id);
    }
    for (uint32_t id : ids) {
        auto* o = m_scene.FindGameObject(id);
        if (!o) continue;
        const auto& rb = o->rigidbody;
        ObjRuntime& rt = m_rt[id];
        Vector3 size = ColliderSize(*o);
        Vector3 off = o->collider.enabled ? o->collider.offset : Vector3{0, 0, 0};
        float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;
        Vector3 world = m_scene.WorldPosition(*o);
        Vector3 c{world.x + off.x, world.y + off.y, world.z + off.z};

        if (rb.useGravity) rt.vel.y -= kBodyGravity * dt;
        float damp = 1.0f / (1.0f + rb.drag * dt);
        rt.vel.x *= damp; rt.vel.z *= damp; rt.vel.y *= (1.0f / (1.0f + rb.drag * 0.2f * dt));

        // игрок толкает тело
        {
            float dx = c.x - m_player.pos.x, dz = c.z - m_player.pos.z;
            float d = std::sqrt(dx * dx + dz * dz);
            bool vert = c.y + hy > m_player.pos.y && c.y - hy < m_player.pos.y + m_player.height;
            if (vert && d < m_player.radius + std::max(hx, hz) + 0.15f && d > 0.001f) {
                float push = 2.2f / std::max(0.2f, rb.mass);
                rt.vel.x += dx / d * push * dt * 10.0f;
                rt.vel.z += dz / d * push * dt * 10.0f;
            }
        }

        auto overlap = [&](Vector3 p) -> const Box* {
            for (const auto& b : m_boxCache) {
                if (b.owner == id) continue;
                if (p.x + hx <= b.min.x || p.x - hx >= b.max.x) continue;
                if (p.y + hy <= b.min.y || p.y - hy >= b.max.y) continue;
                if (p.z + hz <= b.min.z || p.z - hz >= b.max.z) continue;
                return &b;
            }
            return nullptr;
        };
        float bounce = rb.bounciness;
        rt.grounded = false;

        Vector3 n = c;
        n.x += rt.vel.x * dt;
        if (rt.vel.x != 0.0f) if (const Box* b = overlap(n)) { n.x = rt.vel.x > 0 ? b->min.x - hx - 0.001f : b->max.x + hx + 0.001f; rt.vel.x = std::fabs(rt.vel.x) > 1.0f ? -rt.vel.x * bounce : 0.0f; }
        n.z += rt.vel.z * dt;
        if (rt.vel.z != 0.0f) if (const Box* b = overlap(n)) { n.z = rt.vel.z > 0 ? b->min.z - hz - 0.001f : b->max.z + hz + 0.001f; rt.vel.z = std::fabs(rt.vel.z) > 1.0f ? -rt.vel.z * bounce : 0.0f; }
        n.y += rt.vel.y * dt;
        if (const Box* b = overlap(n)) {
            if (rt.vel.y < 0) { n.y = b->max.y + hy + 0.001f; rt.grounded = true; }
            else n.y = b->min.y - hy - 0.001f;
            rt.vel.y = std::fabs(rt.vel.y) > 1.5f ? -rt.vel.y * bounce : 0.0f;
        }
        if (rt.grounded) {
            float f = std::max(0.0f, 1.0f - rb.friction * dt * 6.0f);
            rt.vel.x *= f; rt.vel.z *= f;
        }
        if (n.y < -60.0f) { n.y = 2.0f; rt.vel = Vector3{0, 0, 0}; } // упало за карту
        m_scene.SetWorldPosition(*o, Vector3{n.x - off.x, n.y - off.y, n.z - off.z});
    }
}

// ---------------------------------------------------------------------------------------------
// Навигация ИИ: сетка 0.5 м по статической геометрии, поиск пути A* со сглаживанием.
// ---------------------------------------------------------------------------------------------
void PlaySession::BuildNav() {
    m_nav = NavGrid{};
    if (m_staticBoxes.empty()) return;
    float minx = 1e9f, minz = 1e9f, maxx = -1e9f, maxz = -1e9f;
    for (const auto& b : m_staticBoxes) {
        minx = std::min(minx, b.min.x); minz = std::min(minz, b.min.z);
        maxx = std::max(maxx, b.max.x); maxz = std::max(maxz, b.max.z);
    }
    float cell = 0.5f;
    int w = (int)std::ceil((maxx - minx) / cell) + 2, h = (int)std::ceil((maxz - minz) / cell) + 2;
    if (w > 700 || h > 700 || w < 2 || h < 2) return;
    m_nav.ox = minx - cell; m_nav.oz = minz - cell; m_nav.cell = cell; m_nav.w = w; m_nav.h = h;
    m_nav.blocked.assign((size_t)w * h, 0);

    const float r = 0.3f; // запас под радиус монстра
    auto mark = [&](Vector3 mn, Vector3 mx) {
        int x0 = std::max(0, (int)std::floor((mn.x - r - m_nav.ox) / cell)), x1 = std::min(w - 1, (int)std::floor((mx.x + r - m_nav.ox) / cell));
        int z0 = std::max(0, (int)std::floor((mn.z - r - m_nav.oz) / cell)), z1 = std::min(h - 1, (int)std::floor((mx.z + r - m_nav.oz) / cell));
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x) m_nav.blocked[(size_t)z * w + x] = 1;
    };
    for (const auto& b : m_staticBoxes) {
        if (b.max.y <= 0.5f || b.min.y >= 1.9f) continue; // пол и потолок не мешают
        mark(b.min, b.max);
    }
    for (const auto& up : m_scene.GetGameObjects()) {
        const auto& o = *up;
        if (!o.collider.enabled || o.collider.isTrigger || o.door.enabled || o.rigidbody.enabled || o.player.enabled || o.aiSpawn.enabled || o.itemPickup.enabled) continue;
        BoundingBox bb = ObjectBounds(o);
        if (bb.max.y <= 0.5f || bb.min.y >= 1.9f) continue;
        mark(bb.min, bb.max);
    }
    // клетки вне основной геометрии (над пустотой) не проходимы: проверяем наличие пола
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            if (m_nav.blocked[(size_t)z * w + x]) continue;
            float wx = m_nav.ox + (x + 0.5f) * cell, wz = m_nav.oz + (z + 0.5f) * cell;
            bool floor = false;
            for (const auto& b : m_staticBoxes) {
                if (wx > b.min.x && wx < b.max.x && wz > b.min.z && wz < b.max.z && b.max.y <= 0.5f && b.max.y > -1.0f) { floor = true; break; }
            }
            if (!floor) m_nav.blocked[(size_t)z * w + x] = 1;
        }
    }
    m_nav.valid = true;
}

bool PlaySession::NavLine(int x0, int z0, int x1, int z1) const {
    int dx = std::abs(x1 - x0), dz = std::abs(z1 - z0);
    int sx = x0 < x1 ? 1 : -1, sz = z0 < z1 ? 1 : -1;
    int err = dx - dz;
    while (true) {
        if (!NavFree(x0, z0)) return false;
        if (x0 == x1 && z0 == z1) return true;
        int e2 = 2 * err;
        bool movedX = false, movedZ = false;
        if (e2 > -dz) { err -= dz; x0 += sx; movedX = true; }
        if (e2 < dx) { err += dx; z0 += sz; movedZ = true; }
        if (movedX && movedZ && (!NavFree(x0 - sx, z0) || !NavFree(x0, z0 - sz))) return false; // не режем углы
    }
}

std::vector<Vector3> PlaySession::FindPath(Vector3 a, Vector3 b) {
    std::vector<Vector3> out;
    if (!m_nav.valid) return out;
    auto cellOf = [&](Vector3 p, int& cx, int& cz) {
        cx = (int)std::floor((p.x - m_nav.ox) / m_nav.cell);
        cz = (int)std::floor((p.z - m_nav.oz) / m_nav.cell);
    };
    auto snap = [&](int& cx, int& cz) {
        if (NavFree(cx, cz)) return true;
        for (int r = 1; r <= 4; ++r)
            for (int dz = -r; dz <= r; ++dz)
                for (int dx = -r; dx <= r; ++dx)
                    if (NavFree(cx + dx, cz + dz)) { cx += dx; cz += dz; return true; }
        return false;
    };
    int sx, sz, gx, gz;
    cellOf(a, sx, sz); cellOf(b, gx, gz);
    if (!snap(sx, sz) || !snap(gx, gz)) return out;
    if (sx == gx && sz == gz) { out.push_back(b); return out; }

    const int W = m_nav.w;
    auto idx = [&](int x, int z) { return (size_t)z * W + x; };
    std::vector<float> g((size_t)W * m_nav.h, 1e30f);
    std::vector<int> parent((size_t)W * m_nav.h, -1);
    struct Node { float f; int x, z; bool operator<(const Node& o) const { return f > o.f; } };
    std::priority_queue<Node> open;
    auto hcost = [&](int x, int z) {
        float dx = (float)std::abs(x - gx), dz = (float)std::abs(z - gz);
        return (dx + dz) + (1.4142f - 2.0f) * std::min(dx, dz);
    };
    g[idx(sx, sz)] = 0.0f;
    open.push({hcost(sx, sz), sx, sz});
    int expanded = 0;
    bool found = false;
    while (!open.empty() && expanded < 25000) {
        Node n = open.top(); open.pop();
        if (n.x == gx && n.z == gz) { found = true; break; }
        if (n.f - hcost(n.x, n.z) > g[idx(n.x, n.z)] + 1e-3f) continue;
        ++expanded;
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dz) continue;
                int nx = n.x + dx, nz = n.z + dz;
                if (!NavFree(nx, nz)) continue;
                if (dx && dz && (!NavFree(n.x + dx, n.z) || !NavFree(n.x, n.z + dz))) continue;
                float ng = g[idx(n.x, n.z)] + ((dx && dz) ? 1.4142f : 1.0f);
                if (ng < g[idx(nx, nz)]) {
                    g[idx(nx, nz)] = ng;
                    parent[idx(nx, nz)] = (int)idx(n.x, n.z);
                    open.push({ng + hcost(nx, nz), nx, nz});
                }
            }
        }
    }
    if (!found) return out;

    std::vector<std::pair<int, int>> cells;
    for (int cur = (int)idx(gx, gz); cur != -1; cur = parent[cur]) cells.push_back({cur % W, cur / W});
    std::reverse(cells.begin(), cells.end());
    // сглаживание: идём к самой дальней клетке, видимой по прямой
    size_t i = 0;
    while (i + 1 < cells.size()) {
        size_t j = cells.size() - 1;
        while (j > i + 1 && !NavLine(cells[i].first, cells[i].second, cells[j].first, cells[j].second)) --j;
        out.push_back(Vector3{m_nav.ox + (cells[j].first + 0.5f) * m_nav.cell, a.y, m_nav.oz + (cells[j].second + 0.5f) * m_nav.cell});
        i = j;
    }
    if (!out.empty()) out.back() = Vector3{b.x, a.y, b.z};
    return out;
}

Vector3 PlaySession::NavNext(uint32_t id, Vector3 from, Vector3 goal) {
    ObjRuntime& rt = m_rt[id];
    float dg = Len2D(goal, rt.pathGoal);
    // пересчёт только по таймеру или при смене цели — иначе недостижимая цель считалась бы каждый кадр
    if (rt.repath <= 0.0f || dg > 1.0f) {
        rt.path = FindPath(from, goal);
        rt.pathIdx = 0;
        rt.pathGoal = goal;
        rt.repath = 0.8f;
    }
    while (rt.pathIdx < rt.path.size() && Len2D(from, rt.path[rt.pathIdx]) < 0.35f) ++rt.pathIdx;
    if (rt.pathIdx >= rt.path.size()) return goal;
    return rt.path[rt.pathIdx];
}

float PlaySession::NavMove(uint32_t id, Vector3 goal, float speed, float dt) {
    auto* o = m_scene.FindGameObject(id);
    if (!o) return 0.0f;
    ObjRuntime& rt = m_rt[id];
    rt.repath -= dt;
    Vector3 pos = m_scene.WorldPosition(*o);
    float remain = Len2D(pos, goal);
    if (remain < 0.4f) return remain;
    Vector3 next = NavNext(id, pos, goal);
    float dx = next.x - pos.x, dz = next.z - pos.z;
    float d = std::sqrt(dx * dx + dz * dz);
    if (d < 1e-4f) return remain;
    float step = std::min(speed * dt, d);
    float yaw = std::atan2(dx, dz) * RAD2DEG;
    o->transform.rotation.y = yaw;
    if (MonsterState* m = FindMonster(id)) m->yaw = yaw;
    bool hit = MoveObjectCollide(id, Vector3{dx / d * step, 0, dz / d * step}, 0.35f);
    if (hit) {
        // закрытая незапертая дверь рядом — открываем
        for (const auto& up : m_scene.GetGameObjects()) {
            const auto& dr = *up;
            if (!dr.active || !dr.door.enabled || dr.door.isLocked) continue;
            if (Len3D(m_scene.WorldPosition(dr), pos) < 1.8f && !m_rt[dr.id].doorOpen) { OpenDoor(dr.id, true); break; }
        }
        rt.repath = std::min(rt.repath, 0.2f);
    }
    return Len2D(m_scene.WorldPosition(*o), goal);
}

bool PlaySession::VisibleToPlayer(uint32_t id) {
    auto* o = m_scene.FindGameObject(id);
    if (!o) return false;
    Vector3 p = m_scene.WorldPosition(*o);
    Vector3 eye = m_camera.position;
    return LineOfSightFree(eye, Vector3{p.x, p.y + 1.2f, p.z}) || LineOfSightFree(eye, Vector3{p.x, p.y + 1.9f, p.z});
}

bool PlaySession::MoveObjectCollide(uint32_t id, Vector3 delta, float radius) {
    auto* o = m_scene.FindGameObject(id);
    if (!o) return true;
    BuildBoxCache();
    Vector3 p = m_scene.WorldPosition(*o);
    bool hit = false;
    Vector3 np = MoveCollide(p, delta, radius, 1.8f, id, kStepHeight, &hit);
    float g = GroundHeight(np, radius, kStepHeight);
    if (g > -1e8f && o->aiSpawn.enabled) np.y = g;
    if (MonsterState* m = FindMonster(id)) m->pos = np;
    m_scene.SetWorldPosition(*o, np);
    return hit;
}

bool PlaySession::Raycast(Vector3 origin, Vector3 dir, float maxDist, float* outDist, std::string* outName) {
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-5f) return false;
    dir = Vector3{dir.x / len, dir.y / len, dir.z / len};
    float best = maxDist;
    bool hit = false;
    std::string name;
    float wd = 0.0f;
    if (RayWorld(origin, dir, maxDist, &wd) && wd < best) { best = wd; hit = true; name = ""; }
    Ray ray{origin, dir};
    for (const auto& up : m_scene.GetGameObjects()) {
        const auto& o = *up;
        if (!o.active || !o.collider.enabled || o.collider.isTrigger || o.id == m_playerObj) continue;
        RayCollision c = GetRayCollisionBox(ray, ObjectBounds(o));
        if (c.hit && c.distance < best) { best = c.distance; hit = true; name = o.name; }
    }
    if (hit) {
        if (outDist) *outDist = best;
        if (outName) *outName = name;
    }
    return hit;
}

void PlaySession::EnterHiding(scene::GameObject& o) {
    m_player.hiddenIn = (int)o.id;
    m_hideBaseYaw = o.hidingSpot.exitYaw;
    m_player.yaw = m_hideBaseYaw;
    m_player.pitch = 0.0f;
    m_player.flashlight = false;
    PlaySfx("audio/sfx/sfx_wardrobe_hide.wav", 0.8f);
    m_script->CallHook(o.id, "on_hide");
    m_script->Emit("player_hidden", {LuaArg::Object(o.id)});
}

void PlaySession::ExitHiding() {
    auto* o = m_scene.FindGameObject((uint32_t)m_player.hiddenIn);
    uint32_t id = (uint32_t)m_player.hiddenIn;
    if (o) {
        Vector3 c = m_scene.WorldPosition(*o);
        Vector3 f = Forward(o->hidingSpot.exitYaw);
        m_player.pos = Vector3{c.x + f.x * 1.1f, c.y - o->transform.scale.y * 0.5f, c.z + f.z * 1.1f};
        float g = GroundHeight(m_player.pos, m_player.radius, 1.5f);
        if (g > -1e8f) m_player.pos.y = g;
    }
    m_player.hiddenIn = 0;
    PlaySfx("audio/sfx/sfx_wardrobe_hide.wav", 0.6f);
    if (o) m_script->CallHook(id, "on_unhide");
    m_script->Emit("player_unhidden", {});
}

void PlaySession::UpdateTriggers() {
    std::vector<uint32_t> ids;
    for (const auto& o : m_scene.GetGameObjects()) {
        if (o->active && o->collider.enabled && o->collider.isTrigger) ids.push_back(o->id);
    }
    Vector3 pc{m_player.pos.x, m_player.pos.y + 0.9f, m_player.pos.z};
    for (uint32_t id : ids) {
        auto* o = m_scene.FindGameObject(id);
        if (!o) continue;
        BoundingBox b = ObjectBounds(*o);
        bool inside = pc.x > b.min.x - 0.3f && pc.x < b.max.x + 0.3f && pc.y > b.min.y - 0.9f && pc.y < b.max.y + 0.9f && pc.z > b.min.z - 0.3f && pc.z < b.max.z + 0.3f;
        ObjRuntime& rt = m_rt[id];
        if (inside && !rt.triggerInside) {
            rt.triggerInside = true;
            m_script->CallHook(id, "on_trigger_enter", {LuaArg::Object(m_playerObj)});
            m_script->Emit("trigger_enter", {LuaArg::Object(id)});
        } else if (!inside && rt.triggerInside) {
            rt.triggerInside = false;
            m_script->CallHook(id, "on_trigger_exit", {LuaArg::Object(m_playerObj)});
        }
    }
}

void PlaySession::UpdateCodeOverlay(bool input) {
    CodeOverlay& c = m_codeOverlay;
    if (!input) return;
    c.shakeT = std::max(0.0f, c.shakeT - GetFrameTime());
    int ch;
    while ((ch = GetCharPressed()) != 0) {
        if (ch >= '0' && ch <= '9' && c.entered.size() < 12) c.entered.push_back((char)ch);
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !c.entered.empty()) c.entered.pop_back();
    bool submit = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || (!c.expected.empty() && c.entered.size() >= c.expected.size());
    if (submit && !c.entered.empty()) {
        bool ok = c.entered == c.expected;
        auto cb = c.cb;
        std::string entered = c.entered;
        m_codeOverlay = CodeOverlay{};
        PlaySfx(ok ? "audio/sfx/sfx_ch3_padlock_open.wav" : "audio/sfx/sfx_door_locked_rattle.wav", 0.8f);
        if (cb) cb(ok, entered);
    }
}

// ---------------------------------------------------------------------------------------------
// Шум и ИИ
// ---------------------------------------------------------------------------------------------
void PlaySession::MakeNoise(Vector3 pos, float radius) {
    if (radius <= 0.0f) return;
    for (auto& m : m_monsters) {
        if (m.frozen || m.mode == MonsterMode::Chase) continue;
        auto* o = m_scene.FindGameObject(m.objId);
        if (!o || !o->active) continue;
        float hear = std::max(0.1f, o->aiSpawn.hearingRadius / 8.5f);
        if (Len3D(m.pos, pos) <= radius * hear) {
            m.mode = MonsterMode::Investigate;
            m.target = Vector3{pos.x, m.pos.y, pos.z};
        }
    }
    if (m_script) m_script->Emit("noise", {LuaArg(pos.x), LuaArg(pos.y), LuaArg(pos.z), LuaArg(radius)});
}

void PlaySession::AlertMonsters(Vector3 pos) {
    for (auto& m : m_monsters) {
        if (m.mode == MonsterMode::Chase) continue;
        m.mode = MonsterMode::Investigate;
        m.target = Vector3{pos.x, m.pos.y, pos.z};
    }
}

void PlaySession::UpdateMonsters(float dt) {
    float threat = 0.0f;
    std::vector<uint32_t> ids;
    for (const auto& m : m_monsters) ids.push_back(m.objId);

    for (uint32_t id : ids) {
        MonsterState* mp = FindMonster(id);
        auto* o = m_scene.FindGameObject(id);
        if (!mp || !o || !o->active) continue;
        MonsterState& m = *mp;
        const auto& ai = o->aiSpawn;
        if (m.frozen) continue;

        Vector3 eyeM{m.pos.x, m.pos.y + 1.6f, m.pos.z};
        Vector3 eyeP{m_player.pos.x, m_player.pos.y + (m_player.crouching ? 0.8f : 1.5f), m_player.pos.z};
        float distP = Len3D(m.pos, m_player.pos);
        bool hidden = m_player.hiddenIn != 0;

        // зрение
        bool sees = false;
        float viewDist = ai.visionDistance * (m_player.crouching ? 0.65f : 1.0f);
        if (!hidden && distP <= viewDist) {
            float toYaw = YawTo(m.pos, m_player.pos);
            float diff = std::fabs(AngleDiff(toYaw, m.yaw));
            bool inCone = diff <= ai.visionAngle * 0.5f || distP < 1.5f;
            if (inCone && LineOfSightFree(eyeM, eyeP)) sees = true;
        }
        // нашли в укрытии
        if (hidden && m.mode == MonsterMode::Chase && distP < 2.0f) sees = true;

        if (sees) {
            m.lastSeen = m_player.pos;
            m.lostTimer = 0.0f;
            if (m.mode != MonsterMode::Chase) {
                m.mode = MonsterMode::Chase;
                m_script->Emit("monster_mode", {LuaArg::Object(id), LuaArg("chase")});
                PlaySfx("audio/sfx/sfx_sting_dread.wav", 0.8f);
            }
        } else if (m.mode == MonsterMode::Chase) {
            m.lostTimer += dt;
            if (m.lostTimer > 3.0f || hidden) {
                m.mode = MonsterMode::Search;
                m.target = m.lastSeen;
                m.searchTimer = 6.0f;
                m_script->Emit("monster_mode", {LuaArg::Object(id), LuaArg("search")});
            }
        }

        float speed = 0.0f;
        Vector3 goal = m.pos;
        bool hasGoal = false;
        switch (m.mode) {
        case MonsterMode::Idle:
            break;
        case MonsterMode::Patrol:
            if (!ai.patrolWaypoints.empty()) {
                if (m.waypoint >= (int)ai.patrolWaypoints.size()) m.waypoint = 0;
                Vector3 wp = ai.patrolWaypoints[m.waypoint];
                if (m.waitTimer > 0.0f) {
                    m.waitTimer -= dt;
                    m.yaw += 25.0f * dt;
                } else if (Len2D(m.pos, wp) < 0.4f) {
                    m.waitTimer = ai.waitAtWaypoint;
                    m.waypoint = (m.waypoint + 1) % (int)ai.patrolWaypoints.size();
                } else {
                    goal = wp; hasGoal = true; speed = ai.walkSpeed;
                }
            }
            break;
        case MonsterMode::Investigate:
            if (Len2D(m.pos, m.target) < 0.8f) {
                m.mode = MonsterMode::Search;
                m.searchTimer = 4.0f;
            } else {
                goal = m.target; hasGoal = true; speed = ai.walkSpeed * 1.5f;
            }
            break;
        case MonsterMode::Chase:
            goal = sees ? m_player.pos : m.lastSeen; hasGoal = true; speed = ai.chaseSpeed;
            break;
        case MonsterMode::Search:
            if (Len2D(m.pos, m.target) > 0.8f && m.searchTimer > 2.0f) {
                goal = m.target; hasGoal = true; speed = ai.walkSpeed * 1.3f;
            } else {
                m.searchTimer -= dt;
                m.yaw += 70.0f * dt;
                if (m.searchTimer <= 0.0f) {
                    m.mode = ai.patrolWaypoints.empty() ? MonsterMode::Idle : MonsterMode::Patrol;
                    m_script->Emit("monster_mode", {LuaArg::Object(id), LuaArg(ModeToString(m.mode))});
                }
            }
            break;
        }

        if (hasGoal) {
            // обход препятствий: если прямой видимости нет — идём через ближайший видимый waypoint
            Vector3 steer = goal;
            Vector3 eyeG{goal.x, m.pos.y + 1.0f, goal.z};
            Vector3 eyeS{m.pos.x, m.pos.y + 1.0f, m.pos.z};
            m_rt[id].repath -= dt;
            if (m_nav.valid && !LineOfSightFree(eyeS, eyeG)) steer = NavNext(id, m.pos, goal); // обход стен и проход в проёмах
            float want = YawTo(m.pos, steer);
            float diff = AngleDiff(want, m.yaw);
            float turn = (m.mode == MonsterMode::Chase ? 420.0f : 220.0f) * dt;
            m.yaw += std::clamp(diff, -turn, turn);

            Vector3 f = Forward(m.yaw);
            float step = speed * dt;
            if (std::fabs(diff) > 70.0f) step *= 0.3f;
            bool hit = false;
            Vector3 np = MoveCollide(m.pos, Vector3{f.x * step, 0, f.z * step}, 0.35f, 1.8f, id, kStepHeight, &hit);
            m.pos = np;
            if (hit) {
                // открываем неблокированную дверь перед собой
                for (const auto& up : m_scene.GetGameObjects()) {
                    const auto& d = *up;
                    if (!d.active || !d.door.enabled || d.door.isLocked) continue;
                    if (Len3D(m_scene.WorldPosition(d), m.pos) < 1.8f && !m_rt[d.id].doorOpen) { OpenDoor(d.id, true); break; }
                }
            }
            float g = GroundHeight(m.pos, 0.35f, kStepHeight);
            if (g > -1e8f) m.pos.y = g;
            m.walkPhase += dt * speed * 3.0f;
        }

        // поимка
        if (!hidden && distP < 1.05f && m.mode == MonsterMode::Chase && std::fabs(m.pos.y - m_player.pos.y) < 1.5f) {
            m_script->Emit("player_caught", {LuaArg::Object(id)});
            if (m_end == EndState::Playing) Lose("");
            return;
        }
        if (hidden && m.mode == MonsterMode::Chase && distP < 1.2f) {
            m_script->Emit("player_caught", {LuaArg::Object(id)});
            if (m_end == EndState::Playing) Lose("Тебя нашли в укрытии...");
            return;
        }

        float t = m.mode == MonsterMode::Chase ? 1.0f - distP / 16.0f : (m.mode == MonsterMode::Idle ? 0.0f : 0.5f * (1.0f - distP / 9.0f));
        threat = std::max(threat, Clamp01(t));
    }
    m_threat += (threat - m_threat) * std::min(1.0f, dt * 2.5f);
}

// ---------------------------------------------------------------------------------------------
// Свет
// ---------------------------------------------------------------------------------------------
void PlaySession::CollectLights() {
    m_lights.clear();
    for (const auto& up : m_scene.GetGameObjects()) {
        const auto& o = *up;
        if (!o.active || !o.light.enabled) continue;
        auto it = m_rt.find(o.id);
        if (it != m_rt.end() && !it->second.lightOn) continue;
        float k = o.light.intensity;
        if (o.light.flicker) {
            float seed = it != m_rt.end() ? it->second.flickerSeed : 0.0f;
            float n = std::sin(m_flickerTime * 17.0f + seed) * std::sin(m_flickerTime * 5.3f + seed * 2.0f);
            k *= (n > 0.55f) ? 0.15f : (0.8f + 0.2f * n);
        }
        Vector3 c{o.light.color.r / 255.0f * k, o.light.color.g / 255.0f * k, o.light.color.b / 255.0f * k};
        m_lights.push_back(LightSample{m_scene.WorldPosition(o), c, std::max(0.5f, o.light.radius)});
    }
}

Color PlaySession::ShadePoint(Vector3 p, Vector3 n, float extraAmbient) const {
    const scene::SceneEnvironment& env = m_scene.Env();
    float amb = env.ambient + extraAmbient;
    Vector3 c{amb, amb, amb};
    for (const auto& l : m_lights) {
        Vector3 d{l.pos.x - p.x, l.pos.y - p.y, l.pos.z - p.z};
        float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (dist >= l.radius) continue;
        float att = 1.0f - dist / l.radius;
        att *= att;
        float ndl = dist > 0.001f ? (n.x * d.x + n.y * d.y + n.z * d.z) / dist : 1.0f;
        float lam = 0.4f + 0.6f * std::max(0.0f, ndl);
        float w = att * lam;
        c.x += l.color.x * w; c.y += l.color.y * w; c.z += l.color.z * w;
    }
    if (m_player.flashlight) {
        Vector3 eye = m_camera.position;
        Vector3 fdir = Forward(m_player.yaw, m_player.pitch);
        Vector3 to{p.x - eye.x, p.y - eye.y, p.z - eye.z};
        float dist = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
        if (dist < 16.0f && dist > 0.01f) {
            float cosA = (to.x * fdir.x + to.y * fdir.y + to.z * fdir.z) / dist;
            if (cosA > 0.68f) {
                float spot = std::clamp((cosA - 0.68f) / 0.26f, 0.0f, 1.0f);
                float att = 1.0f - dist / 16.0f;
                float ndl = std::max(0.0f, -(n.x * to.x + n.y * to.y + n.z * to.z) / dist);
                float w = spot * att * att * (0.5f + 0.5f * ndl) * 1.5f * (0.4f + 0.6f * std::min(1.0f, m_player.battery * 4.0f));
                c.x += 1.0f * w; c.y += 0.95f * w; c.z += 0.8f * w;
            }
        }
    }
    Vector3 cam = m_camera.position;
    float cd = std::sqrt((p.x - cam.x) * (p.x - cam.x) + (p.y - cam.y) * (p.y - cam.y) + (p.z - cam.z) * (p.z - cam.z));
    float fog = std::clamp(1.0f - cd / std::max(5.0f, env.fogEnd), 0.0f, 1.0f);
    fog = 0.15f + 0.85f * fog;
    auto cv = [&](float v) { return (unsigned char)(std::clamp(v * fog, 0.0f, 1.0f) * 255.0f); };
    return Color{cv(c.x), cv(c.y), cv(c.z), 255};
}

// ---------------------------------------------------------------------------------------------
// Рендер
// ---------------------------------------------------------------------------------------------
void PlaySession::DrawBox(Vector3 center, Vector3 size, float yawDeg, const std::string& tex, Color tint, bool emissive) {
    Color lit = emissive ? Color{255, 255, 255, 255} : ShadePoint(center, Vector3{0, 1, 0}, 0.0f);
    Color col{(unsigned char)(tint.r * lit.r / 255), (unsigned char)(tint.g * lit.g / 255), (unsigned char)(tint.b * lit.b / 255), tint.a};
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(yawDeg, 0, 1, 0);
    if (tex.empty()) DrawCube(Vector3{0, 0, 0}, size.x, size.y, size.z, col);
    else render::DrawTexturedBox(render::TextureManager::Instance().GetTexture(tex), Vector3{0, 0, 0}, size.x, size.y, size.z, col);
    rlPopMatrix();
}

void PlaySession::DrawShape(const scene::GameObject& o, Vector3 wp) {
    const auto& mr = o.meshRenderer;
    if (!mr.enabled) return;
    std::string shape = mr.meshName;
    std::string tex = mr.textureName;
    Vector3 size = o.transform.scale;
    Color tint = mr.tint;

    if (shape == "Auto" || shape.empty()) {
        if (o.door.enabled) { shape = "Cube"; if (tex.empty()) tex = "motel_door"; }
        else if (o.hidingSpot.enabled) { shape = "Cube"; if (tex.empty()) tex = "motel_wall_wood"; }
        else if (o.searchSlot.enabled) { shape = "Cube"; if (tex.empty()) tex = "motel_cardboard"; }
        else if (o.itemPickup.enabled) {
            shape = "Cube";
            size = Vector3{0.22f, 0.22f, 0.22f};
            tint = Color{255, 230, 120, 255};
            wp.y += 0.08f * std::sin((float)m_time * 2.0f);
        }
        else return;
    }
    if (shape == "None") return;

    if (o.door.enabled && shape == "Cube") {
        auto it = m_rt.find(o.id);
        float t = it != m_rt.end() ? it->second.doorT : 0.0f;
        float yaw = o.transform.rotation.y;
        float ang = o.door.openAngle * t;
        // шарнир на локальном -X краю двери
        float yr = yaw * DEG2RAD;
        Vector3 hinge{wp.x - std::cos(yr) * size.x * 0.5f, wp.y, wp.z + std::sin(yr) * size.x * 0.5f};
        Color lit = ShadePoint(wp, Vector3{0, 0, 1}, 0.04f);
        Color col{(unsigned char)(tint.r * lit.r / 255), (unsigned char)(tint.g * lit.g / 255), (unsigned char)(tint.b * lit.b / 255), 255};
        rlPushMatrix();
        rlTranslatef(hinge.x, hinge.y, hinge.z);
        rlRotatef(yaw - ang, 0, 1, 0);
        rlTranslatef(size.x * 0.5f, 0, 0);
        if (tex.empty()) DrawCube(Vector3{0, 0, 0}, size.x, size.y, size.z, col);
        else render::DrawTexturedBox(render::TextureManager::Instance().GetTexture(tex), Vector3{0, 0, 0}, size.x, size.y, size.z, col);
        rlPopMatrix();
        return;
    }

    if (shape == "Cube") {
        DrawBox(wp, size, o.transform.rotation.y, tex, tint, mr.emissive || o.itemPickup.enabled);
    } else if (shape == "Sphere") {
        Color lit = mr.emissive ? WHITE : ShadePoint(wp, Vector3{0, 1, 0});
        Color col{(unsigned char)(tint.r * lit.r / 255), (unsigned char)(tint.g * lit.g / 255), (unsigned char)(tint.b * lit.b / 255), tint.a};
        DrawSphereEx(wp, size.x * 0.5f, 12, 12, col);
    } else if (shape == "Cylinder") {
        Color lit = mr.emissive ? WHITE : ShadePoint(wp, Vector3{0, 1, 0});
        Color col{(unsigned char)(tint.r * lit.r / 255), (unsigned char)(tint.g * lit.g / 255), (unsigned char)(tint.b * lit.b / 255), tint.a};
        DrawCylinder(Vector3{wp.x, wp.y - size.y * 0.5f, wp.z}, size.x * 0.5f, size.x * 0.5f, size.y, 12, col);
    }
}

void PlaySession::DrawMonster(const MonsterState& m) {
    auto* o = m_scene.FindGameObject(m.objId);
    if (!o || !o->active) return;
    bool mimic = o->tag == "mimic";
    bool chase = m.mode == MonsterMode::Chase;
    float H = mimic ? 2.1f : 2.4f;       // выше человека
    float t = (float)m_time;
    float twitch = (chase || mimic) ? std::sin(t * 47.0f + (float)m.objId) * 0.03f : 0.0f;
    float swing = std::sin(m.walkPhase) * 0.18f;
    Color lit = ShadePoint(Vector3{m.pos.x, m.pos.y + 1.0f, m.pos.z}, Vector3{0, 0, 1}, 0.07f);
    auto shade = [&](int r, int g, int b) {
        return Color{(unsigned char)std::min(255, r * (60 + lit.r) / 255), (unsigned char)std::min(255, g * (60 + lit.g) / 255), (unsigned char)std::min(255, b * (60 + lit.b) / 255), 255};
    };
    Color suit = mimic ? shade(60, 56, 64) : shade(16, 16, 20);
    Color skin = mimic ? shade(200, 194, 186) : shade(170, 166, 160);
    Color mouth{4, 2, 2, 255};
    float pulse = 0.5f + 0.5f * std::sin(t * 24.0f);
    Color eye = chase ? Color{255, (unsigned char)(20 + 40 * pulse), 10, 255} : Color{230, 40, 24, 255};

    rlPushMatrix();
    rlTranslatef(m.pos.x + twitch, m.pos.y, m.pos.z);
    rlRotatef(m.yaw, 0, 1, 0);
    float lean = chase ? 0.16f : 0.05f;
    rlRotatef(lean * RAD2DEG, 1, 0, 0);
    // ноги и торс (тонкие, вытянутые)
    DrawCube(Vector3{-0.1f, H * 0.19f, 0}, 0.1f, H * 0.38f, 0.1f, suit);
    DrawCube(Vector3{0.1f, H * 0.19f, 0}, 0.1f, H * 0.38f, 0.1f, suit);
    DrawCube(Vector3{0, H * 0.60f, 0}, 0.34f, H * 0.42f, 0.18f, suit);
    // руки до колен, раскачиваются
    DrawCube(Vector3{-0.27f, H * 0.52f, swing}, 0.07f, H * 0.52f, 0.07f, suit);
    DrawCube(Vector3{0.27f, H * 0.52f, -swing}, 0.07f, H * 0.52f, 0.07f, suit);
    DrawCube(Vector3{-0.27f, H * 0.25f, swing}, 0.08f, 0.16f, 0.06f, skin);   // кисти
    DrawCube(Vector3{0.27f, H * 0.25f, -swing}, 0.08f, 0.16f, 0.06f, skin);
    // шея и голова: бледная, слегка склонённая набок, без лица
    DrawCube(Vector3{0.02f, H * 0.82f, 0}, 0.07f, 0.16f, 0.07f, skin);
    DrawSphereEx(Vector3{0.05f, H * 0.90f, 0.02f}, 0.17f, 10, 10, skin);
    // разрез рта и глазницы
    DrawCube(Vector3{0.05f, H * 0.875f, 0.17f}, 0.035f, 0.16f, 0.02f, mouth);
    DrawCube(Vector3{-0.03f, H * 0.925f, 0.15f}, 0.05f, 0.05f, 0.03f, mouth);
    DrawCube(Vector3{0.13f, H * 0.925f, 0.15f}, 0.05f, 0.05f, 0.03f, mouth);
    // светящиеся глаза (всегда ярко, не затемняются светом)
    DrawSphere(Vector3{-0.03f, H * 0.925f, 0.185f}, 0.024f, eye);
    DrawSphere(Vector3{0.13f, H * 0.925f, 0.185f}, 0.024f, eye);
    rlPopMatrix();
}

void PlaySession::DrawWorld() {
    if (m_shaderOk) {
        float lp[72] = {0}, lc[72] = {0}, lr[24] = {0};
        int n = 0;
        std::vector<const LightSample*> order;
        for (const auto& l : m_lights) order.push_back(&l);
        std::sort(order.begin(), order.end(), [&](const LightSample* a, const LightSample* b) { return Len3D(a->pos, m_camera.position) - a->radius < Len3D(b->pos, m_camera.position) - b->radius; });
        for (const LightSample* l : order) {
            if (n >= 24) break;
            lp[n * 3] = l->pos.x; lp[n * 3 + 1] = l->pos.y; lp[n * 3 + 2] = l->pos.z;
            lc[n * 3] = l->color.x; lc[n * 3 + 1] = l->color.y; lc[n * 3 + 2] = l->color.z;
            lr[n] = l->radius;
            ++n;
        }
        const auto& env = m_scene.Env();
        float cam[3] = {m_camera.position.x, m_camera.position.y, m_camera.position.z};
        Vector3 fd = Forward(m_player.yaw, m_player.pitch);
        float fdir[3] = {fd.x, fd.y, fd.z};
        float fon = m_player.flashlight ? 1.0f : 0.0f;
        float fpow = 0.4f + 0.6f * std::min(1.0f, m_player.battery * 4.0f);
        float amb = env.ambient, fog = env.fogEnd;
        SetShaderValue(m_shader, m_locView, cam, SHADER_UNIFORM_VEC3);
        SetShaderValue(m_shader, m_locAmbient, &amb, SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_shader, m_locFogEnd, &fog, SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_shader, m_locCount, &n, SHADER_UNIFORM_INT);
        if (n > 0) {
            SetShaderValueV(m_shader, m_locLPos, lp, SHADER_UNIFORM_VEC3, n);
            SetShaderValueV(m_shader, m_locLCol, lc, SHADER_UNIFORM_VEC3, n);
            SetShaderValueV(m_shader, m_locLRad, lr, SHADER_UNIFORM_FLOAT, n);
        }
        SetShaderValue(m_shader, m_locFlashOn, &fon, SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_shader, m_locFlashDir, fdir, SHADER_UNIFORM_VEC3);
        SetShaderValue(m_shader, m_locFlashPow, &fpow, SHADER_UNIFORM_FLOAT);
        rlDisableBackfaceCulling();
        auto& tm2 = render::TextureManager::Instance();
        for (auto& g : m_gpu) {
            m_material.maps[MATERIAL_MAP_DIFFUSE].texture = tm2.GetTexture(g.texture);
            DrawMesh(g.mesh, m_material, MatrixIdentity());
        }
        rlEnableBackfaceCulling();
        return;
    }
    rlDisableBackfaceCulling();
    auto& tm = render::TextureManager::Instance();
    for (auto& batch : m_batches) {
        Texture2D tex = tm.GetTexture(batch.texture);
        // Рисуем порциями и сбрасываем батч: при переполнении буфера rlgl теряет текстуру у оставшихся треугольников.
        constexpr size_t kChunk = 6000; // кратно 3
        for (size_t i = 0; i < batch.verts.size(); i += kChunk) {
            rlBegin(RL_TRIANGLES);
            rlSetTexture(tex.id); // ПОСЛЕ rlBegin: смена режима в rlBegin сбрасывает текстуру на дефолтную
            size_t end = std::min(batch.verts.size(), i + kChunk);
            for (size_t k = i; k < end; ++k) {
                const auto& v = batch.verts[k];
                Color c = ShadePoint(v.p, v.n);
                rlColor4ub(c.r, c.g, c.b, 255);
                rlTexCoord2f(v.uv.x, v.uv.y);
                rlVertex3f(v.p.x, v.p.y, v.p.z);
            }
            rlEnd();
            rlSetTexture(0);
            rlDrawRenderBatchActive();
        }
    }
    rlEnableBackfaceCulling();
}

void PlaySession::DrawObjects() {
    for (const auto& up : m_scene.GetGameObjects()) {
        const auto& o = *up;
        if (!o.active) continue;
        auto it = m_rt.find(o.id);
        if (it != m_rt.end() && !it->second.visible) continue;
        if (o.aiSpawn.enabled) continue; // рисуется отдельно
        if (o.player.enabled || o.id == m_playerObj) continue;
        DrawShape(o, m_scene.WorldPosition(o));
    }
    for (const auto& m : m_monsters) DrawMonster(m);
}

void PlaySession::Render3D(float aspect) {
    if (!m_running) return;
    m_aspect = aspect;
    ClearBackground(m_scene.Env().background);

    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    double top = 0.05 * std::tan(m_camera.fovy * 0.5 * DEG2RAD);
    double right = top * aspect;
    rlFrustum(-right, right, -top, top, 0.05, 150.0);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    Matrix view = MatrixLookAt(m_camera.position, m_camera.target, m_camera.up);
    rlMultMatrixf(MatrixToFloat(view));
    rlEnableDepthTest();

    DrawWorld();
    DrawObjects();
    rlDrawRenderBatchActive();
    EndMode3D();
}

// ---------------------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------------------
void PlaySession::DrawHudText(const std::string& text, float x, float y, float size, Color c, bool centered) {
    Font f = m_hasFont ? m_font : GetFontDefault();
    Vector2 ts = MeasureTextEx(f, text.c_str(), size, 1.0f);
    if (centered) x -= ts.x * 0.5f;
    DrawTextEx(f, text.c_str(), Vector2{x + 1.5f, y + 1.5f}, size, 1.0f, Color{0, 0, 0, (unsigned char)(c.a * 0.8f)});
    DrawTextEx(f, text.c_str(), Vector2{x, y}, size, 1.0f, c);
}

void PlaySession::RenderHUD(int w, int h) {
    if (!m_running) return;
    Font font = m_hasFont ? m_font : GetFontDefault();
    float fs = std::max(14.0f, h / 34.0f);
    float cx = w * 0.5f, cy = h * 0.5f;

    // угроза: красная виньетка
    if (m_threat > 0.02f) {
        float pulse = 0.65f + 0.35f * std::sin((float)m_time * (4.0f + m_threat * 8.0f));
        unsigned char a = (unsigned char)(std::clamp(m_threat * 150.0f * pulse, 0.0f, 170.0f));
        int bw = (int)(w * 0.18f), bh = (int)(h * 0.22f);
        DrawRectangleGradientV(0, 0, w, bh, Color{120, 0, 0, a}, Color{120, 0, 0, 0});
        DrawRectangleGradientV(0, h - bh, w, bh, Color{120, 0, 0, 0}, Color{120, 0, 0, a});
        DrawRectangleGradientH(0, 0, bw, h, Color{120, 0, 0, a}, Color{120, 0, 0, 0});
        DrawRectangleGradientH(w - bw, 0, bw, h, Color{120, 0, 0, 0}, Color{120, 0, 0, a});
    }

    // в укрытии: щель
    if (m_player.hiddenIn != 0) {
        float sx = w * 0.14f, sy = h * 0.36f, sw = w - 2 * sx, sh = h * 0.22f;
        Color frame{4, 3, 3, 245};
        DrawRectangle(0, 0, w, (int)sy, frame);
        DrawRectangle(0, (int)(sy + sh), w, h - (int)(sy + sh), frame);
        DrawRectangle(0, (int)sy, (int)sx, (int)sh, frame);
        DrawRectangle((int)(sx + sw), (int)sy, w - (int)(sx + sw), (int)sh, frame);
        for (float y = sy + 6; y < sy + sh; y += 16) DrawRectangle((int)sx, (int)y, (int)sw, 4, Color{0, 0, 0, 140});
    }

    if (m_tvActive) {
        int tw = (int)(w * 0.62f);
        int th = (int)(tw * 3.0f / 4.0f);
        if (th > h * 0.72f) {
            th = (int)(h * 0.72f);
            tw = (int)(th * 4.0f / 3.0f);
        }
        int tx = (w - tw) / 2;
        int ty = (h - th) / 2 - 25;

        DrawRectangle(0, 0, w, h, Color{0, 0, 0, 160});
        DrawRectangleRounded(Rectangle{(float)tx - 16, (float)ty - 16, (float)tw + 32, (float)th + 48}, 0.04f, 8, Color{36, 26, 20, 255});
        DrawRectangleRoundedLinesEx(Rectangle{(float)tx - 16, (float)ty - 16, (float)tw + 32, (float)th + 48}, 0.04f, 8, 3.0f, Color{85, 62, 48, 255});
        DrawHudText("ГОРИЗОНТ-Ц257 // ВИДЕОМАГНИТОФОН", tx + 8, ty + th + 10, fs * 0.75f, Color{195, 165, 110, 220}, false);
        DrawCircle(tx + tw - 20, ty + th + 16, 5.0f, Color{255, 40, 20, 255});

        std::string frameTexName = "tape" + std::to_string(m_tvTape) + "_frame" + std::to_string(m_tvFrame);
        Texture2D ft = render::TextureManager::Instance().GetTexture(frameTexName);
        if (ft.id != 0) {
            DrawTexturePro(ft, Rectangle{0, 0, (float)ft.width, (float)ft.height},
                           Rectangle{(float)tx, (float)ty, (float)tw, (float)th},
                           Vector2{0, 0}, 0.0f, WHITE);
        }

        for (int sy = ty; sy < ty + th; sy += 3) {
            DrawRectangle(tx, sy, tw, 1, Color{0, 0, 0, 65});
        }

        float barPhase = std::fmod(m_tvTimer * 0.35f, 1.0f);
        int by = ty + (int)(barPhase * th);
        DrawRectangle(tx, by, tw, 12, Color{220, 230, 255, 40});

        DrawRectangleLinesEx(Rectangle{(float)tx, (float)ty, (float)tw, (float)th}, 4, Color{10, 14, 18, 200});
        DrawHudText("[ПРОБЕЛ / ESC] — Пропустить видео", w * 0.5f, ty + th + 54, fs * 0.8f, Color{220, 210, 180, 220}, true);
    }

    if (m_end == EndState::Playing && !m_cutCam && !m_tvActive) {
        // прицел
        DrawCircle((int)cx, (int)cy, m_focusObj ? 3.5f : 2.0f, Color{255, 255, 255, (unsigned char)(m_focusObj ? 230 : 120)});
        if (!m_prompt.empty()) {
            DrawHudText(m_prompt, cx, cy + fs * 1.4f, fs, Color{245, 230, 190, 255}, true);
        }
        if (m_focusHold > 0.0f) {
            float bw = w * 0.14f;
            DrawRectangle((int)(cx - bw * 0.5f), (int)(cy + fs * 3.2f), (int)bw, 6, Color{0, 0, 0, 160});
            DrawRectangle((int)(cx - bw * 0.5f), (int)(cy + fs * 3.2f), (int)(bw * std::min(1.0f, m_focusHold)), 6, Color{244, 184, 96, 255});
        }
    }

    // уведомления
    float ny = h * 0.05f;
    for (const auto& t : m_toasts) {
        float a = Clamp01(std::min(t.t / 0.2f, (t.dur - t.t) / 0.5f));
        DrawHudText(t.text, w * 0.03f, ny, fs, Color{244, 214, 150, (unsigned char)(255 * a)});
        ny += fs * 1.4f;
    }

    // субтитр
    if (!m_subtitle.text.empty()) {
        float a = Clamp01(std::min(m_subtitle.t / 0.3f, (m_subtitle.dur - m_subtitle.t) / 0.5f));
        auto lines = WrapText(font, m_subtitle.text, fs * 1.1f, w * 0.7f);
        float lh = fs * 1.3f;
        float boxH = lines.size() * lh + 16;
        float y0 = h * 0.86f - boxH;
        DrawRectangle((int)(w * 0.12f), (int)y0, (int)(w * 0.76f), (int)boxH, Color{0, 0, 0, (unsigned char)(150 * a)});
        for (size_t i = 0; i < lines.size(); ++i) {
            DrawHudText(lines[i], cx, y0 + 8 + i * lh, fs * 1.1f, Color{240, 235, 225, (unsigned char)(255 * a)}, true);
        }
    }

    // выносливость и батарея
    if (m_end == EndState::Playing) {
        if (m_player.stamina < 0.98f) {
            float bw = w * 0.16f;
            DrawRectangle((int)(w * 0.03f), (int)(h * 0.93f), (int)bw, 6, Color{0, 0, 0, 140});
            DrawRectangle((int)(w * 0.03f), (int)(h * 0.93f), (int)(bw * m_player.stamina), 6, Color{190, 220, 190, 220});
        }
        if (m_player.hasFlashlight) {
            Color bc = m_player.battery < 0.2f ? Color{230, 80, 60, 230} : Color{230, 220, 150, 220};
            std::string txt = std::string(m_player.flashlight ? "ФОНАРЬ [F]: ВКЛ " : "ФОНАРЬ [F]: выкл ") + std::to_string((int)(m_player.battery * 100)) + "%";
            DrawHudText(txt, w * 0.03f, h * 0.95f, fs * 0.8f, bc);
        }
        DrawHudText("[Tab] Инвентарь", w * 0.97f - MeasureTextEx(font, "[Tab] Инвентарь", fs * 0.75f, 1.0f).x, h * 0.95f, fs * 0.75f, Color{200, 200, 200, 150});
    }

    // инвентарь
    if (m_invOpen) {
        float pw = w * 0.4f, ph = h * 0.62f, px = w * 0.06f, py = h * 0.14f;
        DrawRectangle((int)px, (int)py, (int)pw, (int)ph, Color{12, 10, 10, 235});
        DrawRectangleLines((int)px, (int)py, (int)pw, (int)ph, Color{196, 122, 59, 220});
        DrawHudText("ИНВЕНТАРЬ", px + 16, py + 10, fs * 1.1f, Color{244, 184, 96, 255});
        if (m_inventory.empty()) DrawHudText("Пусто", px + 16, py + fs * 3, fs, Color{160, 160, 160, 255});
        for (size_t i = 0; i < m_inventory.size(); ++i) {
            bool sel = (int)i == m_invCursor;
            float y = py + fs * 3 + i * fs * 1.5f;
            if (y > py + ph - fs * 4) break;
            if (sel) DrawRectangle((int)px + 8, (int)y - 2, (int)pw - 16, (int)(fs * 1.4f), Color{107, 51, 34, 200});
            DrawHudText(ItemName(m_inventory[i]), px + 16, y, fs, sel ? Color{255, 220, 150, 255} : Color{230, 225, 215, 255});
        }
        if (!m_inventory.empty()) {
            const ItemDef* d = m_project.FindItem(m_inventory[std::min<size_t>(m_invCursor, m_inventory.size() - 1)]);
            if (d && !d->description.empty()) {
                auto lines = WrapText(font, d->description, fs * 0.9f, pw - 32);
                float y = py + ph - fs * 4.5f;
                for (const auto& ln : lines) { DrawHudText(ln, px + 16, y, fs * 0.9f, Color{190, 185, 175, 255}); y += fs * 1.1f; }
            }
        }
        DrawHudText("[Enter] использовать   [Tab] закрыть", px + 16, py + ph - fs * 1.4f, fs * 0.8f, Color{150, 150, 150, 255});
    }

    // ввод кода
    if (m_codeOverlay.active) {
        float pw = w * 0.34f, ph = h * 0.3f, px = cx - pw * 0.5f, py = cy - ph * 0.5f;
        DrawRectangle((int)px, (int)py, (int)pw, (int)ph, Color{10, 14, 12, 245});
        DrawRectangleLines((int)px, (int)py, (int)pw, (int)ph, Color{90, 200, 120, 255});
        DrawHudText(m_codeOverlay.prompt, cx, py + ph * 0.12f, fs, Color{190, 240, 200, 255}, true);
        std::string shown = m_codeOverlay.entered;
        while (shown.size() < std::max<size_t>(m_codeOverlay.expected.size(), 4)) shown += "_";
        std::string spaced;
        for (char c : shown) { spaced += c; spaced += ' '; }
        DrawHudText(spaced, cx, py + ph * 0.42f, fs * 2.0f, Color{120, 255, 150, 255}, true);
        DrawHudText("[0-9] ввод   [Backspace] стереть   [Enter] ок   [Esc] отмена", cx, py + ph * 0.82f, fs * 0.7f, Color{140, 170, 150, 255}, true);
    }

    // затемнение
    if (m_fade > 0.003f) DrawRectangle(0, 0, w, h, Color{0, 0, 0, (unsigned char)(255 * Clamp01(m_fade))});

    // конец
    if (m_end != EndState::Playing && m_endTimer > 0.5f) {
        float a = Clamp01((m_endTimer - 0.5f) * 1.2f);
        bool won = m_end == EndState::Won;
        DrawHudText(m_endTitle, cx, h * 0.34f, fs * 2.6f, won ? Color{244, 214, 150, (unsigned char)(255 * a)} : Color{220, 60, 50, (unsigned char)(255 * a)}, true);
        auto lines = WrapText(font, m_endText, fs * 1.1f, w * 0.6f);
        for (size_t i = 0; i < lines.size(); ++i) DrawHudText(lines[i], cx, h * 0.5f + i * fs * 1.5f, fs * 1.1f, Color{230, 225, 215, (unsigned char)(255 * a)}, true);
        if (m_endTimer > 1.0f) {
            DrawHudText(won ? "[Enter] Продолжить" : "[R] Заново     [Q] В меню", cx, h * 0.8f, fs, Color{190, 190, 190, (unsigned char)(220 * a)}, true);
        }
    }

    // пауза
    if (m_paused) {
        DrawRectangle(0, 0, w, h, Color{0, 0, 0, 170});
        DrawHudText("ПАУЗА", cx, h * 0.35f, fs * 2.4f, Color{244, 184, 96, 255}, true);
        DrawHudText("[Esc] Продолжить", cx, h * 0.52f, fs * 1.1f, Color{230, 225, 215, 255}, true);
        DrawHudText("[Q] Выйти в меню", cx, h * 0.58f, fs * 1.1f, Color{230, 225, 215, 255}, true);
    }
}

}
