#pragma once
#include "engine/core/Engine.h"
#include "engine/math/Math2D.h"
#include "engine/physics/PhysicsWorld.h"
#include "game/systems/ChaseMinigame.h"
#include "game/systems/CookingSystem.h"
#include "game/systems/EndlessDirector.h"
#include "game/systems/SanitySystem.h"
#include "game/systems/TutorialSystem.h"
#include "game/ui/InspectionOverlay.h"
#include "game/ui/MainMenuScene.h"
#include "game/ui/SettingsOverlay.h"
#include "game/world/CafeWorld.h"
#include <optional>
#include <string>
#include <vector>

namespace game {

enum class GameScreen {
    MainMenu,
    PlayingShift,
    EndlessMode,
    NightmareChase,
    Paused
};

class LastOrderGame : public engine::core::IGame {
public:
    void OnInit(engine::core::Engine& eng) override;
    void OnFixedUpdate(engine::core::Engine& eng, float fixedDt) override;
    void OnFrameUpdate(engine::core::Engine& eng, float dt) override;
    void OnRenderWorld(engine::core::Engine& eng) override;
    void OnRenderUI(engine::core::Engine& eng) override;
    void OnAction(engine::core::Engine& eng, const engine::core::EngineAction& action) override;
    void OnShutdown(engine::core::Engine& eng) override;

private:
    void StartNewShift(engine::core::Engine& eng, int nightNumber, bool withTutorial);
    void StartEndlessShift(engine::core::Engine& eng);
    void StartMinigame(engine::core::Engine& eng, systems::MinigameType type);
    void CompleteActiveMinigame(engine::core::Engine& eng);
    void TriggerScreamer(engine::core::Engine& eng, const std::string& screamerId);
    ui::MirrorWorldState BuildMirrorState(const engine::core::Engine& eng) const;

    void UpdatePauseMenu(engine::core::Engine& eng);
    void RenderPauseMenu(engine::core::Engine& eng) const;
    void RenderDebugOverlay(engine::core::Engine& eng) const;

    GameScreen m_screen = GameScreen::MainMenu;
    int m_currentNight = 1;

    world::CafeWorld m_world{};
    systems::CookingSystem m_cooking{};
    systems::SanitySystem m_sanity{};
    systems::TutorialSystem m_tutorial{};
    systems::EndlessDirector m_endlessDirector{};
    systems::ChaseMinigame m_chaseMinigame{};
    ui::InspectionOverlay m_inspectionUI{};
    ui::MainMenuScene m_mainMenu{};
    ui::SettingsOverlay m_settingsUI{};
    int m_pauseSelectedBtn = 0;

    engine::physics::KinematicBody m_playerBody{};
    int m_playerFacing = 0;
    float m_walkAnimTimer = 0.0f;
    float m_footstepTimer = 0.0f;
    float m_distMovedThisFrame = 0.0f;
    bool m_isMoving = false;
    bool m_toggledFlashlightThisFrame = false;
    bool m_toggledVisorThisFrame = false;

    bool m_visor3DActive = false;
    float m_visorYaw = -1.57f;
    float m_visorPitch = 0.0f;
    float m_visorBattery = 100.0f;

    std::optional<engine::physics::TriggerVolume> m_nearestTrigger;
    std::vector<world::GuestNpc> m_guests;

    bool m_blackoutMode = false;
    bool m_lightsFlickering = false;
    bool m_debugMode = false;
    bool m_debugNoclip = false;
    int m_demoScreamerIdx = 0;
};

}
