#pragma once
#include "engine/3d/CameraFPS.h"
#include "engine/3d/Lighting3D.h"
#include "engine/3d/Physics3D.h"
#include "engine/core/Engine.h"
#include "game/chapter2/Chapter2Script.h"
#include "game/chapter2/ItemVisuals.h"
#include "game/chapter2/MotelLevel.h"
#include "raylib.h"
#include <optional>
#include <string>
#include <vector>

namespace game::chapter2 {

enum class FocusKind {
    None,
    Door,
    Prop,
    Interact,
    Item,
    Hide
};

struct RainDrop3D {
    Vector3 pos{};
    float speed = 12.0f;
    float len = 0.5f;
    bool visible = false;
    float stopY = 0.0f;
};

class MotelWorld3D : private IScriptHost {
public:
    void Init(engine::core::Engine& eng, int nightNumber);
    void Update(engine::core::Engine& eng, float dt);
    void FixedUpdate(engine::core::Engine& eng, float fixedDt);
    void RenderOffscreen(engine::core::Engine& eng);
    void Render3D(engine::core::Engine& eng);
    void RenderUI(engine::core::Engine& eng);
    void Shutdown(engine::core::Engine& eng);

    bool WantsReturnToMenu() const { return m_returnToMenu; }
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    void StartTour(const std::string& outputDir);
#endif

private:
    void ShowMessage(const std::string& text, float seconds) override;
    void SetObjective(const std::string& text) override;
    void PlaySound(const std::string& path, float volume, std::optional<Vector3> at) override;
    void Flicker(float seconds) override;
    void SetPower(bool on) override;
    void UnlockDoor(const std::string& name) override;
    void OpenDoor(const std::string& name) override;
    void CloseDoor(const std::string& name) override;
    void Shake(float amount, float seconds) override;
    void AddSanity(float delta) override;
    void AddBattery(float amount) override;
    void SetAmbient(const std::string& path, float volume) override;
    void TriggerScreamer(const std::string& texture, float duration, float shake) override;
    void EndAct(const std::string& text) override;

    void UpdateFlashlight(float dt);
    void UpdateMovement(float dt);
    void UpdateFocus();
    void HandleUse();
    void UseDoor(int index, bool forceOpen);
    void UpdateTriggers();
    void UpdateMirrorEvents();
    void UpdateSoundEmitters(float dt);
    void UpdateRain(float dt);
    void UpdateSanity(float dt);
    void UpdateFootsteps();
    void RespawnDrop(RainDrop3D& drop, bool anyHeight);

    float LightStyleFactor(const LightObject& light) const;
    float CurrentPowerLevel() const;
    std::vector<engine::render3d::DynamicLight> GatherLights(Vector3 viewPos) const;
    engine::render3d::SceneEnvironment BuildEnvironment() const;
    float ViewAspect() const;
    void BeginCamera(const Camera3D& camera) const;

    void DrawScene(const Shader* overrideShader, bool reflectionPass);
    void DrawPhantoms(bool reflectionPass) const;
    void DrawViewmodel() const;
    void DrawInspectOverlay(engine::core::Engine& eng);
    bool ItemVisible(const ItemObject& item) const;

    engine::physics3d::Physics3D m_physics;
    engine::render3d::Lighting3D m_lighting;
    engine::camera3d::CameraFPS m_camera;
    MotelLevel m_level;
    Chapter2Script m_script;
    ItemVisualLibrary m_itemVisuals;
    engine::core::Engine* m_eng = nullptr;

    RenderTexture2D m_reflectionRT{};
    Camera3D m_reflectionCam{};
    int m_activeMirror = -1;

    engine::render3d::Flashlight m_flash;
    bool m_flashOn = true;
    bool m_flashFocus = false;
    float m_battery = 100.0f;
    Vector3 m_flashDir{0.0f, 0.0f, -1.0f};
    float m_flickerTimer = 0.0f;
    bool m_flickerOn = true;
    float m_flickerSwitch = 0.0f;

    bool m_power = false;
    float m_sanity = 100.0f;
    float m_stamina = 100.0f;
    float m_whisperTimer = 12.0f;
    bool m_sprinting = false;
    Vector3 m_moveVelocity{};

    FocusKind m_focusKind = FocusKind::None;
    int m_focusIndex = -1;
    std::string m_focusLabel;
    int m_carriedProp = -1;
    int m_hiddenSpot = -1;
    float m_breath = 100.0f;
    bool m_holdingBreath = false;

    bool m_inspectMode = false;
    std::string m_inspectItem;
    float m_inspectYaw = 0.0f;
    float m_inspectPitch = 0.0f;
    float m_inspectZoom = 1.0f;

    std::string m_message;
    float m_messageTimer = 0.0f;
    std::string m_objective;
    float m_objectiveFlash = 0.0f;
    std::string m_endActText;
    float m_endActTimer = 0.0f;

    bool m_screamerActive = false;
    float m_screamerTimer = 0.0f;
    float m_screamerDuration = 0.8f;
    float m_screamerRedFlash = 0.0f;
    Texture2D m_screamerTex{};
    Texture2D m_phantomTex{};
    float m_rainAmbientVolume = 0.28f;

    std::vector<RainDrop3D> m_rain;
    int m_stepIndex = 0;
    bool m_ready = false;
    bool m_returnToMenu = false;
    float m_time = 0.0f;
    std::string m_loadError;
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    struct TourStop {
        std::string name;
        Vector3 feet{};
        float yawDeg = 0.0f;
        float pitchDeg = 0.0f;
        bool power = false;
        std::string openDoor;
        bool flashlight = true;
        int frames = 40;
    };
    void UpdateTour(engine::core::Engine& eng);
    std::vector<TourStop> m_tour;
    int m_tourIndex = -1;
    int m_tourFrames = 0;
    std::string m_tourDir;
#endif
};

}
