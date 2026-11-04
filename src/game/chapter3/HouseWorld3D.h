#pragma once
#include "engine/3d/CameraFPS.h"
#include "engine/core/Engine.h"
#include "game/chapter3/Cutscene3D.h"
#include "game/chapter3/EscapeManager.h"
#include "game/chapter3/GrannyAI.h"
#include "game/chapter3/ItemRandomizer.h"
#include "game/chapter3/WeaponAssembly.h"
#include "game/ui/SettingsOverlay.h"
#include "raylib.h"
#include <deque>
#include <string>
#include <vector>

namespace game::chapter3 {

struct HouseRoomBox {
    std::string name;
    BoundingBox bounds{};
    int floor = 1;
};

struct WallQuad {
    Vector3 p0{};
    Vector3 p1{};
    Vector3 p2{};
    Vector3 p3{};
    Vector3 normal{};
    std::string textureId;
    float uvScale = 1.0f;
};

struct InteractiveProp {
    std::string id;
    std::string name;
    BoundingBox bounds{};
    Vector3 center{};
    bool isDoor = false;
    bool isOpen = false;
    bool isHidingSpot = false;
    bool isWorkbench = false;
    bool isFrontDoor = false;
    bool isCar = false;
    bool isSewer = false;
    bool isRadio = false;
    std::string slotId;
};

class HouseWorld3D {
public:
    void Init(engine::core::Engine& eng);
    void Update(engine::core::Engine& eng, float dt);
    void FixedUpdate(engine::core::Engine& eng, float fixedDt);
    void RenderOffscreen(engine::core::Engine& eng);
    void Render3D(engine::core::Engine& eng);
    void RenderUI(engine::core::Engine& eng);
    void Shutdown(engine::core::Engine& eng);

    bool WantsReturnToMenu() const { return m_returnToMenu; }
    bool WantsChapterComplete() const { return m_chapterComplete; }

private:
    void SetupGeometry(engine::core::Engine& eng);
    void SetupProps();
    void StartNewDay(engine::core::Engine& eng, int dayNumber);
    void HandlePlayerMovement(engine::core::Engine& eng, float dt);
    void CheckInteractions(engine::core::Engine& eng);
    void TriggerNoise(engine::core::Engine& eng, Vector3 pos, float radius, float volume);
    void ShowNotification(const std::string& text, float duration = 2.5f);
    void RenderSceneMesh(engine::core::Engine& eng);
    void RenderEntity(engine::core::Engine& eng);
    void RenderHeldItem(engine::core::Engine& eng);
    void RenderHUD(engine::core::Engine& eng);

    bool CheckCollision(Vector3 targetPos, float radius) const;
    void ResolvePlayerPhysics(Vector3& pos, Vector3& vel, float dt);

    Camera3D m_camera{};
    Vector3 m_playerPos{-2.0f, 3.5f, 3.0f};
    Vector3 m_playerVel{};
    float m_playerYaw = 0.0f;
    float m_playerPitch = 0.0f;
    bool m_isCrouching = false;
    bool m_isHiding = false;
    std::string m_currentHidingSpot;

    bool m_flashlightOn = true;
    float m_flashlightBattery = 1.0f;
    float m_stepTimer = 0.0f;
    float m_headBobTimer = 0.0f;
    float m_headBobOffset = 0.0f;

    std::vector<std::string> m_inventory;
    int m_activeSlot = 0;

    std::vector<HouseRoomBox> m_rooms;
    std::vector<WallQuad> m_wallQuads;
    std::vector<InteractiveProp> m_props;
    std::vector<BoundingBox> m_colliders;

    ItemRandomizer m_randomizer;
    WeaponAssembly m_weapon;
    GrannyAI m_granny;
    EscapeManager m_escape;
    CutsceneEngine m_cutscene;

    std::string m_focusPrompt;
    std::string m_notificationText;
    float m_notificationTimer = 0.0f;
    float m_dayTitleTimer = 0.0f;
    float m_dayTitleAlpha = 0.0f;

    bool m_returnToMenu = false;
    bool m_chapterComplete = false;
    bool m_inCutscene = false;

    float m_radioDial = 90.0f;
    std::string m_enteredKeypadCode;
    bool m_keypadModalOpen = false;
    bool m_noteModalOpen = false;

    ui::SettingsOverlay m_settingsUI{};
};

}

