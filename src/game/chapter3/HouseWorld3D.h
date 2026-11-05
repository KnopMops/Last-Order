#pragma once
#include "engine/3d/CameraFPS.h"
#include "engine/3d/Lighting3D.h"
#include "engine/3d/Physics3D.h"
#include "engine/core/Engine.h"
#include "game/chapter3/Cutscene3D.h"
#include "game/chapter3/EscapeManager.h"
#include "game/chapter3/ProwlerAI.h"
#include "game/chapter3/HouseHd.h"
#include "game/chapter3/HouseLevel.h"
#include "game/chapter3/ItemRandomizer.h"
#include "game/chapter3/WeaponAssembly.h"
#include "game/ui/SettingsOverlay.h"
#include "raylib.h"
#include <string>
#include <vector>

namespace game::chapter3 {

enum class PropKind {
    Slot,
    Hide,
    Workbench,
    FrontDoor,
    Car,
    Sewer,
    Radio,
    Vent,
    FlashlightPickup
};

struct DroppedItemWorld {
    std::string itemId;
    Vector3 pos{};
    float rotY = 0.0f;
};

struct CutActor {
    std::string id;
    int kind = 0;
    Vector3 pos{};
    Vector3 from{};
    Vector3 to{};
    float moveT = 0.0f;
    float moveDur = 0.0f;
    float yaw = 0.0f;
    float walkPhase = 0.0f;
    bool dead = false;
    float deadT = 0.0f;
};

struct InteractiveProp {
    std::string id;
    std::string name;
    PropKind kind = PropKind::Slot;
    Vector3 center{};
    float radius = 0.5f;
    std::string slotId;
    Vector3 hideEye{};
    float hideYaw = 0.0f;
};

class HouseWorld3D {
public:
    void Init(engine::core::Engine& eng, int rendererOverride = -1);
    void Update(engine::core::Engine& eng, float dt);
    void FixedUpdate(engine::core::Engine& eng, float fixedDt);
    void RenderOffscreen(engine::core::Engine& eng);
    void Render3D(engine::core::Engine& eng);
    void RenderUI(engine::core::Engine& eng);
    void Shutdown(engine::core::Engine& eng);

    bool WantsReturnToMenu() const { return m_returnToMenu; }
    bool WantsChapterComplete() const { return m_chapterComplete; }
    const std::string& EndingId() const { return m_endingId; }
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    void StartTour();
    void StartCutsceneTour();
    void StartHoldoutTest();
#endif

private:
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    struct TourStop {
        std::string name;
        Vector3 feet{};
        float yawDeg = 0.0f;
        float pitchDeg = 0.0f;
        float walkSeconds = 0.0f;
    };
    void UpdateTour(engine::core::Engine& eng, float dt);
    void UpdateCutsceneTour(engine::core::Engine& eng, float dt);
    std::vector<std::string> m_csTourList;
    int m_csTourIdx = -1;
    bool m_csStarted = false;
    int m_csShots = 0;
    Image m_csSheet{};
    std::vector<TourStop> m_tour;
    int m_tourIndex = -1;
    int m_tourFrames = 0;
    float m_tourWalk = 0.0f;
    float m_tourTime = 0.0f;
    int m_tourTotalFrames = 0;
#endif

    void HandleCutsceneEvent(const CutsceneEvent& ev);
    void UpdateActors(float dt);
    void StartHoldout(engine::core::Engine& eng);
    void RespawnHoldout(engine::core::Engine& eng);
    void SetupProps();
    void BuildEntityMeshes();
    void UnloadEntityMeshes();
    void StartNewDay(engine::core::Engine& eng, int dayNumber);
    void PlayCutscene(const std::string& name);
    void HandlePlayerMovement(engine::core::Engine& eng, float dt);
    void UpdateFlashlight(float dt);
    void CheckInteractions(engine::core::Engine& eng);
    void ConsumeActiveItem();
    void UpdateInventory(engine::core::Engine& eng);
    void RenderInventory(engine::core::Engine& eng);
    void RenderJournal(engine::core::Engine& eng);
    void RenderHelp(engine::core::Engine& eng);
    void TakeSlotItem(engine::core::Engine& eng, SearchSlot& slot);
    void InitRenderer(engine::core::Engine& eng);
    void ApplyGraphics(engine::core::Engine& eng);
    void ApplyDisplay(engine::core::Engine& eng);
    void UpdateFear(engine::core::Engine& eng, float dt);
    void TriggerNoise(engine::core::Engine& eng, Vector3 pos, float radius, float volume);
    void ShowNotification(const std::string& text, float duration = 2.5f);
    bool BatchVisible(const std::string& tag) const;
    void DrawScene(const Shader* overrideShader);
    void DrawEntity(const Shader* overrideShader);
    void DrawDroppedItems(const Shader* overrideShader);
    void DrawInspectOverlay(engine::core::Engine& eng);
    void BeginCamera(const Camera3D& camera) const;
    engine::render3d::SceneEnvironment BuildEnvironment() const;
    void RenderHUD(engine::core::Engine& eng);

    std::vector<CutActor> m_actors;
    bool m_holdout = false;
    float m_holdoutTimer = 0.0f;
    bool m_radioDone = false;
    bool m_ready = false;
    bool m_hd = false;
    HouseHd m_hdBridge;
    bool m_hasFlashlight = false;
    float m_battery = 1.0f;
    float m_fear = 0.0f;
    float m_heartTimer = 0.0f;
    float m_heartPulse = 0.0f;
    float m_searchProgress = 0.0f;
    int m_searchProp = -1;
    float m_frameDt = 0.016f;
    std::string m_alertText;
    std::string m_subText;
    float m_subTimer = 0.0f;
    bool m_subRed = false;
    float m_alertTimer = 0.0f;
    bool m_alertRed = false;
    bool m_invOpen = false;
    bool m_journalOpen = false;
    bool m_helpOpen = false;
    float m_helpTimer = 14.0f;
    int m_invCursor = 0;
    float m_flashPower = 1.0f;
    std::vector<unsigned char> m_lampOn;
    int m_appliedRevision = -1;
    int m_savedVsync = -1;
    int m_savedFpsIndex = 0;
    int m_appliedRenderer = -1;
    int m_appliedRt = -1;
    int m_appliedShadow = -1;
    engine::core::Engine* m_eng = nullptr;
    engine::render3d::Lighting3D m_lighting;
    engine::physics3d::Physics3D m_physics;
    engine::camera3d::CameraFPS m_fps;
    HouseLevel m_level;
    Camera3D m_viewCam{};

    engine::render3d::Flashlight m_flash;
    Vector3 m_flashDir{0.0f, 0.0f, -1.0f};
    bool m_flashlightOn = false;
    Vector3 m_moveVelocity{};
    bool m_sprinting = false;
    bool m_isHiding = false;
    int m_hideProp = -1;
    float m_time = 0.0f;
    int m_stepIndex = 0;

    Texture2D m_phantomTex{};

    std::vector<std::string> m_inventory;
    int m_activeSlot = 0;
    std::vector<InteractiveProp> m_props;
    std::vector<DroppedItemWorld> m_droppedItems;
    bool m_inspectMode = false;
    std::string m_inspectItem;
    float m_inspectYaw = 0.4f;
    float m_inspectPitch = 0.15f;
    float m_inspectZoom = 1.0f;

    ItemRandomizer m_randomizer;
    WeaponAssembly m_weapon;
    ProwlerAI m_prowler;
    EscapeManager m_escape;
    CutsceneEngine m_cutscene;

    std::string m_focusPrompt;
    std::string m_notificationText;
    float m_notificationTimer = 0.0f;
    float m_dayTitleTimer = 0.0f;
    float m_dayTitleAlpha = 0.0f;

    bool m_returnToMenu = false;
    bool m_chapterComplete = false;
    std::string m_endingId;
    bool m_usedRadio = false;
    bool m_inCutscene = false;

    std::string m_enteredKeypadCode;

    ui::SettingsOverlay m_settingsUI{};
};

}
