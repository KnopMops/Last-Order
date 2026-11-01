#pragma once
#include "engine/core/Engine.h"
#include "engine/core/StateStore.h"
#include "engine/math/Math2D.h"
#include <string>

namespace game::systems {

enum class GameOverCommand {
    None,
    RestoreCheckpoint,
    RestartNight,
    ReturnToMainMenu
};

struct SanityDrainFactors {
    bool isBlackout = false;
    bool isFlickering = false;
    bool isFlashlightOn = false;
    bool isVisor3D = false;
    int threatCount = 0;
};

class SanitySystem {
public:
    void Init(const std::string& profileSavePath);
    void ResetForShift(engine::math::Vec2 startPos);

    void Update(engine::core::Engine& eng, float dt, engine::math::Vec2 playerPos, const SanityDrainFactors& factors = {});
    GameOverCommand UpdateGameOverInput(engine::core::Engine& eng);
    void RenderGameOverModal(engine::core::Engine& eng, int currentNight) const;

    bool IsGameOver() const { return m_gameOver; }
    bool HasCheckpoint() const { return m_hasCheckpoint; }
    void RestoreCheckpointTo(engine::core::Engine& eng, engine::math::Vec2& outPlayerPos);

    std::string FormatHudClock(const engine::core::Engine& eng) const;

    void MarkNightCompleted(int nightNumber);
    bool IsNightCompleted(int nightNumber) const;

private:
    void LoadProfile();
    void SaveProfile() const;

    std::string m_profilePath;
    bool m_nightCompleted[6] = {false, false, false, false, false, false};
    bool m_endlessUnlocked = false;

    bool m_gameOver = false;
    int m_selectedBtn = 0;

    bool m_hasCheckpoint = false;
    engine::core::StateStore m_checkpointState{};
    int m_checkpointHour = 2;
    int m_checkpointMin = 0;
    engine::math::Vec2 m_checkpointPos = {160.0f, 92.0f};

    float m_currentStress = 0.0f;
    float m_hallucinationTimer = 14.0f;
    float m_clockGlitchTimer = 0.0f;
    float m_drainAccumulator = 0.0f;
};

}
