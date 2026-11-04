#pragma once
#include "engine/core/Engine.h"
#include "engine/math/Math2D.h"
#include <string>
#include <vector>

namespace game::systems {

enum class MinigameType {
    Night1_RadioTuner2D = 1,
    Night2_BasementFuse3D = 2,
    Night3_FreezerMaze3D = 3,
    Night4_MirrorHunt3D = 4,
    Night5_HighwayChase3D = 5,
    Night_JukeboxTuner2D = 6
};

enum class ChaseState {
    Briefing,
    Running,
    Caught,
    Victory
};

struct ChaseObstacle {
    float x = 0.0f;
    float y = 0.0f;
    int type = 0;
    bool collected = false;
    bool burned = false;
};

struct Node3DTarget {
    float x = 0.0f;
    float z = 0.0f;
    bool active = true;
    bool isFriendly = false;
};

class ChaseMinigame {
public:
    void Start(MinigameType type);
    void Reset();
    void Update(engine::core::Engine& eng, float dt);
    void RenderWorld(engine::core::Engine& eng);
    void RenderUI(engine::core::Engine& eng) const;

    bool IsActive() const { return m_state == ChaseState::Briefing || m_state == ChaseState::Running; }
    bool IsVictory() const { return m_state == ChaseState::Victory; }
    ChaseState GetState() const { return m_state; }
    MinigameType GetType() const { return m_type; }
    int GetShardsCollected() const { return m_shardsCollected; }

private:
    void SpawnObstacles();
    void UpdateRadio2D(engine::core::Engine& eng, float dt);
    void UpdateJukebox2D(engine::core::Engine& eng, float dt);
    void UpdateExploration3D(engine::core::Engine& eng, float dt);
    void UpdateMirrorHunt3D(engine::core::Engine& eng, float dt);
    void UpdateHighwayChase3D(engine::core::Engine& eng, float dt);

    void RenderRadio2D(engine::core::Engine& eng) const;
    void RenderJukebox2D(engine::core::Engine& eng) const;
    void RenderExploration3D(engine::core::Engine& eng) const;
    void RenderMirrorHunt3D(engine::core::Engine& eng) const;
    void RenderHighwayChase3D(engine::core::Engine& eng) const;

    MinigameType m_type = MinigameType::Night5_HighwayChase3D;
    ChaseState m_state = ChaseState::Briefing;

    float m_timer = 0.0f;
    float m_progress = 0.0f;
    float m_radioTargetFreq = 0.5f;
    float m_radioPlayerFreq = 0.5f;
    float m_vinylRotation = 0.0f;

    float m_camX = 0.0f;
    float m_camZ = 0.0f;
    float m_camYaw = 0.0f;
    float m_stalkerX = 8.0f;
    float m_stalkerZ = 8.0f;
    std::vector<Node3DTarget> m_nodes3D;

    float m_playerDist = 0.0f;
    float m_playerLane = 0.0f;
    float m_targetLane = 0.0f;
    float m_playerSpeed = 28.0f;
    float m_stamina = 100.0f;
    bool m_isSprinting = false;
    float m_monsterDist = -24.0f;
    float m_flashlightCooldown = 0.0f;
    float m_flashlightActive = 0.0f;
    int m_shardsCollected = 0;
    int m_shardsTotal = 3;
    float m_stumbleTimer = 0.0f;
    float m_roadScroll = 0.0f;
    float m_pantTimer = 0.0f;
    float m_carIntegrity = 100.0f;
    bool m_isBraking = false;
    float m_skidCooldown = 0.0f;

    std::vector<ChaseObstacle> m_obstacles;
};

}
