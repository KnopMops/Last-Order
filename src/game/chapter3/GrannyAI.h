#pragma once
#include "engine/core/Engine.h"
#include "raylib.h"
#include <string>
#include <vector>

namespace game::chapter3 {

enum class EntityState {
    Patrol,
    Investigate,
    Chase,
    Stunned,
    Knockout
};

struct HouseWaypoint {
    int id = 0;
    std::string name;
    Vector3 pos{};
    int floor = 1;
    std::vector<int> neighbors;
};

class GrannyAI {
public:
    void Init();
    void ResetToDay(int dayNumber);
    void Update(engine::core::Engine& eng, float dt, Vector3 playerPos, bool playerHidden);
    void NotifySound(Vector3 soundPos, float radius, float volume);
    void ApplyShotgunStun(engine::core::Engine& eng);

    EntityState GetState() const { return m_state; }
    Vector3 GetPosition() const { return m_position; }
    float GetYaw() const { return m_yaw; }
    float GetStunTimer() const { return m_stunTimer; }
    bool DidCatchPlayer() const { return m_caughtPlayer; }
    void ClearCaughtFlag() { m_caughtPlayer = false; }
    float GetDistanceToPlayer() const { return m_distToPlayer; }

private:
    EntityState m_state = EntityState::Patrol;
    Vector3 m_position{0.0f, 1.0f, -4.0f};
    Vector3 m_targetPos{0.0f, 1.0f, -4.0f};
    float m_yaw = 0.0f;
    float m_speed = 2.4f;

    float m_stunTimer = 0.0f;
    float m_investigateTimer = 0.0f;
    float m_stepTimer = 0.0f;
    float m_growlTimer = 0.0f;
    float m_distToPlayer = 999.0f;
    bool m_caughtPlayer = false;
    bool m_seesPlayer = false;

    int m_currentWaypoint = 0;
    std::vector<HouseWaypoint> m_waypoints;

    void SetupWaypoints();
    int FindNearestWaypoint(Vector3 pos) const;
    std::vector<int> PlanPath(int startNode, int endNode) const;
    bool HasLineOfSight(Vector3 from, Vector3 to) const;
};

}

