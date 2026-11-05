#pragma once

#include "map/MapData.h"
#include "raylib.h"
#include "runtime/ChapterProject.h"
#include "scene/Scene.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace knopmops::runtime {

class ScriptHost;
struct LuaApi;

// Значение переменной глав (game.set / game.get).
struct Var {
    enum class Type { Nil, Number, String, Bool } type = Type::Nil;
    double number = 0.0;
    std::string str;
    bool boolean = false;
};

enum class MonsterMode { Idle, Patrol, Investigate, Chase, Search };

struct MonsterState {
    uint32_t objId = 0;
    Vector3 pos{};
    float yaw = 0.0f;
    MonsterMode mode = MonsterMode::Patrol;
    int waypoint = 0;
    float waitTimer = 0.0f;
    Vector3 target{};
    Vector3 lastSeen{};
    float searchTimer = 0.0f;
    float lostTimer = 0.0f;
    float walkPhase = 0.0f;
    float suspicion = 0.0f;
    bool frozen = false;
};

// Состояние объекта, существующее только во время игры.
struct ObjRuntime {
    bool doorOpen = false;
    float doorT = 0.0f; // 0..1
    float searchProgress = 0.0f;
    bool searched = false;
    bool lightOn = true;
    bool visible = true;
    bool triggerInside = false;
    float flickerSeed = 0.0f;
    bool hasScript = false;
    Vector3 vel{};          // скорость Rigidbody
    bool grounded = false;
    std::vector<Vector3> path;   // путь навигации (для ИИ)
    size_t pathIdx = 0;
    Vector3 pathGoal{};
    float repath = 0.0f;
};

struct PlayerState {
    Vector3 pos{};
    float yaw = 0.0f;   // градусы, 0 = смотрит в -Z
    float pitch = 0.0f;
    float velY = 0.0f;
    float stamina = 1.0f;
    float bob = 0.0f;
    bool crouching = false;
    bool sprinting = false;
    bool onGround = true;
    bool locked = false;
    bool moveLocked = false; // нельзя ходить, но можно смотреть
    int hiddenIn = 0; // id объекта-укрытия, 0 = нет
    bool flashlight = false;
    bool hasFlashlight = false;
    float battery = 1.0f;
    float stepTimer = 0.0f;
    float eyeHeight = 1.65f;
    float fov = 70.0f;
    float radius = 0.3f;
    float height = 1.8f;
    float jumpSpeed = 4.0f;
    float gravity = 18.0f;
    float sens = 0.11f;
};

enum class EndState { Playing, Won, Lost };

// Игровая симуляция главы: используется и Play-In-Editor, и самой игрой.
class PlaySession {
public:
    struct Hooks {
        std::function<float()> sfxVolume;   // множитель громкости (настройки игры); пусто = 1.0
        std::string gameAssetsDir;          // корень assets игры для поиска audio/...
        std::map<std::string, Var> initialVars;   // переменные, заданные игрой до старта (например, концовка прошлой главы)
    };

    PlaySession();
    ~PlaySession();
    PlaySession(const PlaySession&) = delete;
    PlaySession& operator=(const PlaySession&) = delete;

    void SetHooks(Hooks h) { m_hooks = std::move(h); }
    void SetFont(Font f, bool owned = false) { m_font = f; m_hasFont = true; (void)owned; }

    // Игра: читает уровень с диска. Редактор: может передать сцену/карту из памяти.
    bool Start(const ChapterProject& project, const std::string& level);
    bool StartFromMemory(const ChapterProject& project, const std::string& level, const scene::Scene& sc, const map::MapDocument& doc);
    void Shutdown();
    bool IsRunning() const { return m_running; }

    void Update(float dt, bool inputActive);
    void Render3D(float aspect);          // вызывать внутри BeginTextureMode / мирового прохода
    void RenderHUD(int w, int h);         // 2D-оверлей

    EndState State() const { return m_end; }
    bool WantsQuit() const { return m_wantsQuit; }
    bool IsPaused() const { return m_paused; }
    void SetPauseEnabled(bool v) { m_pauseEnabled = v; }
    const std::string& EndTitle() const { return m_endTitle; }
    const std::string& EndText() const { return m_endText; }
    const std::string& CurrentLevel() const { return m_level; }
    Camera3D GetCamera() const { return m_camera; }
    bool WantsMouseCapture() const { return m_running && !m_paused && !m_codeOverlay.active && m_end == EndState::Playing; }

    // --- API для скриптов (ScriptHost) ---
    scene::Scene& GetScene() { return m_scene; }
    const ChapterProject& Project() const { return m_project; }
    PlayerState& Player() { return m_player; }
    double Time() const { return m_time; }

    void Notify(const std::string& text, float seconds = 3.0f);
    void Subtitle(const std::string& text, float seconds = 4.0f);
    bool HasItem(const std::string& id) const;
    bool GiveItem(const std::string& id, bool silent = false);
    bool TakeItem(const std::string& id);
    const std::vector<std::string>& Inventory() const { return m_inventory; }

    void SetVar(const std::string& key, const Var& v);
    Var GetVar(const std::string& key) const;
    const std::map<std::string, Var>& AllVars() const { return m_vars; }

    void PlaySfx(const std::string& path, float volume = 1.0f);
    void PlayAmbient(const std::string& path, float volume = 0.5f);
    void StopAmbient();

    void MakeNoise(Vector3 pos, float radius);
    void AlertMonsters(Vector3 pos);
    MonsterState* FindMonster(uint32_t objId);

    ObjRuntime& Rt(uint32_t id) { return m_rt[id]; }
    void RestartLevel() { Restart(); }
    void SetFovOverride(float f) { m_fovOverride = f; }
    bool HasOverlay() const { return m_codeOverlay.active || m_invOpen; }
    void AdoptObject(uint32_t id) { if (auto* o = m_scene.FindGameObject(id)) RegisterObject(*o); }
    scene::GameObject* Spawn(const std::string& prefabFile, Vector3 pos);
    void DestroyObject(uint32_t id);
    void SetObjectActive(uint32_t id, bool active);

    void TeleportPlayer(Vector3 pos, float yawDeg);
    void LoadLevel(const std::string& level, const std::string& spawnObject = "");
    void Win(const std::string& title, const std::string& text);
    void Lose(const std::string& text);
    void Fade(float targetAlpha, float seconds);
    void Shake(float intensity, float seconds);
    void SetCutsceneCamera(bool enabled, Vector3 pos = {}, Vector3 lookAt = {});
    void AskCode(const std::string& prompt, const std::string& expected, std::function<void(bool, const std::string&)> cb);
    void OpenDoor(uint32_t id, bool open);
    bool LineOfSightFree(Vector3 a, Vector3 b) const;
    // Луч по миру и коллайдерам объектов. Возвращает true при попадании.
    bool Raycast(Vector3 origin, Vector3 dir, float maxDist, float* outDist, std::string* outName);

    const std::string& ItemName(const std::string& id) const;

    void PlayTvVideo(int tape, float duration = 12.0f);
    void StopTvVideo();
    bool IsTvVideoPlaying() const { return m_tvActive; }
    int TvTape() const { return m_tvTape; }
    int TvFrame() const { return m_tvFrame; }

    // Для автотестов: имитация нажатия E на объекте по имени и ввода кода.
    bool DebugUse(const std::string& objectName);
    bool DebugAnswerCode(const std::string& code);
    bool CodeOverlayActive() const { return m_codeOverlay.active; }
    size_t DebugTriCount() const { return m_tris.size(); }
    size_t DebugBoxCount() const { return m_staticBoxes.size(); }

private:
    friend class ScriptHost;
    friend struct LuaApi;

    struct Tri { Vector3 a, b, c; };
    struct LitVertex { Vector3 p; Vector2 uv; Vector3 n; };
    struct Batch {
        std::string texture;
        std::vector<LitVertex> verts; // по 3 на треугольник
    };
    struct LightSample { Vector3 pos; Vector3 color; float radius; };
    struct CodeOverlay {
        bool active = false;
        std::string prompt, expected, entered;
        std::function<void(bool, const std::string&)> cb;
        float shakeT = 0.0f;
    };
    struct Toast { std::string text; float t = 0.0f; float dur = 0.0f; };

public:
    // Для альтернативных рендереров (DirectX HD в игре): геометрия, свет, монстры, версия геометрии.
    const std::vector<Batch>& RawBatches() const { return m_batches; }
    const std::vector<LightSample>& LightSamples() const { return m_lights; }
    const std::vector<MonsterState>& Monsters() const { return m_monsters; }
    int GeometryRevision() const { return m_geoRevision; }
    // Движение объекта с коллизиями (для Lua: obj:walk). Возвращает true, если упёрся в препятствие.
    bool MoveObjectCollide(uint32_t id, Vector3 delta, float radius);
    // Навигация по сетке карты (A*): обход стен, проходы в проёмах. NavNext — следующая точка пути, NavMove — шаг к цели.
    Vector3 NavNext(uint32_t id, Vector3 from, Vector3 goal);
    float NavMove(uint32_t id, Vector3 goal, float speed, float dt);
    // Виден ли объект игроку с учётом стен (луч от глаз до груди/головы).
    bool VisibleToPlayer(uint32_t id);
private:

    bool Begin(const ChapterProject& project, const std::string& level);
    bool LoadLevelData(const std::string& level);
    void ApplyPendingLoad();
    void Restart();
    void RegisterObject(scene::GameObject& o);
    void FlushDestroyed();
    void BuildBoxCache();
    void SyncTransforms();
    void UpdateToasts(float dt);
    void DrawHudText(const std::string& text, float x, float y, float size, Color c, bool centered = false);
    std::string ResolveAudio(const std::string& path) const;
    void BuildGeometry();
    void UploadGeometry();   // меш на видеокарту (один раз на уровень)
    void FreeGpu();
    void SpawnObjects();
    void SetupPlayerFromScene(const std::string& spawnObject);

    void UpdatePlayer(float dt, bool input);
    void UpdateInteraction(float dt, bool input);
    void UpdateDoors(float dt);
    void UpdatePhysics(float dt);
    Vector3 ColliderSize(const scene::GameObject& o) const;
    void UpdateMonsters(float dt);
    void UpdateTriggers();
    void UpdateCodeOverlay(bool input);
    void UpdateCamera(float dt);

    bool InteractWith(scene::GameObject& obj);
    bool TryUnlockSlot(scene::GameObject& obj);
    void EnterHiding(scene::GameObject& obj);
    void ExitHiding();
    void FinishSearch(scene::GameObject& obj);

    // коллизии
    struct Box { Vector3 min, max; uint32_t owner; };
    void CollectColliders(std::vector<Box>& out, uint32_t ignore = 0) const;
    Vector3 MoveCollide(Vector3 from, Vector3 delta, float radius, float height, uint32_t ignore, float stepHeight, bool* hitWall = nullptr) const;
    float GroundHeight(Vector3 pos, float radius, float maxStepUp) const;
    bool RayWorld(Vector3 from, Vector3 dir, float maxDist, float* outDist) const;
    BoundingBox ObjectBounds(const scene::GameObject& o) const;
    scene::GameObject* PickInteractable(float* outDist);
    std::string PromptFor(const scene::GameObject& o) const;

    // свет и рендер
    void CollectLights();
    Color ShadePoint(Vector3 p, Vector3 n, float extraAmbient = 0.0f) const;
    void DrawWorld();
    void DrawObjects();
    void DrawMonster(const MonsterState& m);
    void DrawBox(Vector3 center, Vector3 size, float yawDeg, const std::string& tex, Color tint, bool emissive);
    void DrawShape(const scene::GameObject& o, Vector3 worldPos);

    bool m_running = false;
    bool m_paused = false;
    bool m_pauseEnabled = true;
    bool m_wantsQuit = false;
    EndState m_end = EndState::Playing;
    std::string m_endTitle, m_endText;

    Hooks m_hooks;
    Font m_font{};
    bool m_hasFont = false;

    ChapterProject m_project;
    scene::Scene m_scene;
    map::MapDocument m_doc;
    std::string m_level;
    std::unique_ptr<ScriptHost> m_script;

    PlayerState m_player;
    uint32_t m_playerObj = 0;
    std::vector<MonsterState> m_monsters;
    std::unordered_map<uint32_t, ObjRuntime> m_rt;
    std::vector<std::string> m_inventory;
    std::map<std::string, Var> m_vars;

    std::vector<Tri> m_tris;
    std::vector<Batch> m_batches;
    struct GpuMesh { Mesh mesh{}; std::string texture; };
    std::vector<GpuMesh> m_gpu;
    Shader m_shader{};
    Material m_material{};
    bool m_shaderOk = false;
    int m_locView = -1, m_locAmbient = -1, m_locFogEnd = -1, m_locCount = -1, m_locLPos = -1, m_locLCol = -1, m_locLRad = -1;
    int m_locFlashOn = -1, m_locFlashDir = -1, m_locFlashPow = -1;
    std::vector<Box> m_staticBoxes;
    std::vector<LightSample> m_lights;

    Camera3D m_camera{};
    float m_aspect = 16.0f / 9.0f;
    double m_time = 0.0;
    float m_fade = 0.0f, m_fadeTarget = 0.0f, m_fadeSpeed = 0.0f;
    float m_shakeT = 0.0f, m_shakeI = 0.0f;
    bool m_cutCam = false;
    Vector3 m_cutPos{}, m_cutLook{};

    std::vector<Toast> m_toasts;
    Toast m_subtitle;
    std::string m_prompt;
    uint32_t m_focusObj = 0;
    float m_focusHold = 0.0f;
    bool m_invOpen = false;
    int m_invCursor = 0;
    float m_threat = 0.0f;
    float m_lowHeart = 0.0f;
    CodeOverlay m_codeOverlay;

    bool m_pendingLoad = false;
    std::string m_pendingLevel, m_pendingSpawn;
    bool m_started = false;
    bool m_inputActive = false;
    int m_geoRevision = 0;
    struct NavGrid {
        float ox = 0, oz = 0, cell = 0.5f;
        int w = 0, h = 0;
        std::vector<uint8_t> blocked;
        bool valid = false;
    } m_nav;
    void BuildNav();
    bool NavFree(int x, int z) const { return x >= 0 && z >= 0 && x < m_nav.w && z < m_nav.h && !m_nav.blocked[(size_t)z * m_nav.w + x]; }
    bool NavLine(int x0, int z0, int x1, int z1) const;
    std::vector<Vector3> FindPath(Vector3 a, Vector3 b);
    float m_fovOverride = 0.0f;
    bool m_fromMemory = false;
    std::string m_startLevel;
    scene::Scene m_origScene;
    map::MapDocument m_origDoc;
    std::vector<std::string> m_levelStartInventory;
    std::map<std::string, Var> m_levelStartVars;
    std::vector<uint32_t> m_destroyQueue;
    std::vector<Box> m_boxCache;
    bool m_restartPending = false;
    float m_endTimer = 0.0f;
    float m_flickerTime = 0.0f;
    float m_hideBaseYaw = 0.0f;
    Vector3 m_hideExitPos{};

    Music m_ambient{};
    bool m_hasAmbient = false;
    std::unordered_map<std::string, Sound> m_sounds;

    bool m_tvActive = false;
    int m_tvTape = 0;
    float m_tvTimer = 0.0f;
    float m_tvDuration = 12.0f;
    int m_tvFrame = 1;
    float m_tvGlitch = 0.0f;
};

}
