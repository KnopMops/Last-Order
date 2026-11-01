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

    void UpdatePlayerMovement(engine::core::Engine& eng, float dt);
    void SpawnInitial3DAnomalies(int nightNumber);
    void Spawn3DShooterWave(engine::core::Engine& eng, int count);
    void Update3DVisorShooter(engine::core::Engine& eng, float dt);
    void Fire3DVisorWeapon(engine::core::Engine& eng);
    void SetVisor3DActive(engine::core::Engine& eng, bool active);
    void SaveCurrentGame(engine::core::Engine& eng);
    void LoadSavedGame(engine::core::Engine& eng);

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
    std::string m_saveBannerText;
    float m_saveBannerTimer = 0.0f;
    int m_maxUnlockedNight = 1;

    engine::physics::KinematicBody m_playerBody{};
    int m_playerFacing = 0;
    float m_walkAnimTimer = 0.0f;
    float m_footstepTimer = 0.0f;
    float m_distMovedThisFrame = 0.0f;
    bool m_isMoving = false;
    bool m_toggledFlashlightThisFrame = false;
    bool m_toggledVisorThisFrame = false;

    bool m_visor3DActive = false;
    float m_visorYaw = 1.57f;
    float m_visorPitch = 0.0f;
    float m_gunRecoilTimer = 0.0f;
    float m_gunCooldown = 0.0f;
    float m_hitMarkerTimer = 0.0f;
    int m_anomaliesKilled = 0;
    int m_shooterScore = 0;
    int m_nextAnomalyId = 1;
    float m_anomalySpawnTimer = 10.0f;
    float m_ambientScreamerTimer = 22.0f;
    std::string m_scannerTargetText;
    float m_scannerTargetDist = 0.0f;
    std::string m_visorKillBanner;
    float m_visorKillBannerTimer = 0.0f;

    std::vector<world::VisorAnomaly3D> m_anomalies3D;
    std::optional<engine::physics::TriggerVolume> m_nearestTrigger;
    std::vector<world::GuestNpc> m_guests;

    bool m_blackoutMode = false;
    bool m_lightsFlickering = false;
    bool m_debugMode = false;
    bool m_debugNoclip = false;
    int m_demoScreamerIdx = 0;
};

}
