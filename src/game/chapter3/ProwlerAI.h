#pragma once
#include "engine/core/Engine.h"
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

namespace game::chapter3 {

enum class EntityState {
    Patrol,
    Investigate,
    Chase,
    Search,
    Stunned,
    Knockout
};

struct HouseWaypoint {
    int id = 0;
    std::string name;
    Vector3 pos{};
    int floor = 1;
    std::vector<int> neighbors;
    bool patrol = false;
};

class ProwlerAI {
public:
    using OcclusionQuery = std::function<bool(Vector3, Vector3)>;

    void Init();
    void SetOcclusionQuery(OcclusionQuery q) { m_occluded = std::move(q); }
    void ResetToDay(int dayNumber);
    void Update(engine::core::Engine& eng, float dt, Vector3 playerPos, bool playerHidden, bool playerCrouched, bool playerLit);
    void LogBlockedLinks() const;
    int Teleports() const { return m_teleports; }
    int DebugCheckSpot() const { return m_checkSpot; }
    void NotifySound(Vector3 soundPos, float radius, float volume);
    void SetHideSpots(std::vector<Vector3> spots);
    void SetPlayerHideSpot(int idx) { m_playerSpot = idx; }
    void SetPlayerCrawling(bool crawling) { m_crawling = crawling; }
    void SetHunt(bool hunt) { m_hunt = hunt; m_huntTimer = 4.0f; }
    void ForcePose(Vector3 pos, float yaw, EntityState state) { m_position = pos; m_yaw = yaw; m_state = state; }
    void ApplyShotgunStun(engine::core::Engine& eng);

    EntityState GetState() const { return m_state; }
    Vector3 GetPosition() const { return m_position; }
    float GetYaw() const { return m_yaw; }
    float GetStunTimer() const { return m_stunTimer; }
    float GetWalkPhase() const { return m_walkPhase; }
    bool DidCatchPlayer() const { return m_caughtPlayer; }
    void ClearCaughtFlag() { m_caughtPlayer = false; }
    float GetDistanceToPlayer() const { return m_distToPlayer; }
    Vector3 GetTargetPos() const { return m_targetPos; }
    bool IsForceCatching() const { return m_forceCatch; }
    int ConsumeAlert() {
        int a = m_alert;
        m_alert = 0;
        return a;
    }

private:
    EntityState m_state = EntityState::Patrol;
    Vector3 m_position{0.0f, 0.0f, 0.0f};
    Vector3 m_targetPos{0.0f, 0.0f, 0.0f};
    float m_yaw = 0.0f;
    float m_speed = 1.25f;
    int m_day = 1;

    float m_stunTimer = 0.0f;
    float m_investigateTimer = 0.0f;
    float m_idleTimer = 0.0f;
    float m_stepTimer = 0.0f;
    float m_growlTimer = 0.0f;
    float m_repathTimer = 0.0f;
    float m_walkPhase = 0.0f;
    float m_distToPlayer = 999.0f;
    bool m_caughtPlayer = false;
    bool m_seesPlayer = false;
    bool m_prevHidden = false;
    float m_suspicion = 0.0f;
    float m_lostTimer = 0.0f;
    bool m_lostSaid = false;
    Vector3 m_lastPlayerPos{};
    Vector3 m_lastSeenPos{};
    Vector3 m_lastSeenVel{};
    bool m_forceCatch = false;
    int m_alert = 0;
    float m_searchTimer = 0.0f;
    float m_searchIdle = 0.0f;
    Vector3 m_searchOrigin{};
    Vector3 m_searchPoint{};

    std::vector<Vector3> m_hideSpots;
    std::vector<float> m_spotCooldown;
    float m_aggro = 0.0f;
    bool m_holeEscape = false;
    float m_crawlChase = 0.0f;
    float m_giveUpCooldown = 0.0f;
    std::vector<int> m_checkQueue;
    bool m_hunt = false;
    float m_huntTimer = 0.0f;
    int m_teleports = 0;
    int m_playerSpot = -1;
    bool m_crawling = false;
    float m_trackTimer = 0.0f;
    int m_checkSpot = -1;
    int m_checkPhase = 0;
    float m_checkTimer = 0.0f;
    bool m_searchFirst = false;

    int m_patrolTarget = 0;
    std::vector<HouseWaypoint> m_waypoints;
    std::vector<int> m_path;
    size_t m_pathIndex = 0;
    Vector3 m_pathGoal{};
    OcclusionQuery m_occluded;
    float m_stuckTimer = 0.0f;
    float m_progressTimer = 0.0f;
    Vector3 m_progressPos{};

    void SetupWaypoints();
    void Link(int a, int b);
    int FindNearestWaypoint(Vector3 pos, bool requireSight) const;
    std::vector<int> PlanPath(int startNode, int endNode) const;
    bool HasLineOfSight(Vector3 from, Vector3 to) const;
    bool StepTowards(Vector3 goal, float speed, float dt);
    bool NavigateTo(Vector3 goal, float speed, float dt);
    void PickPatrolTarget();
    void PickSearchPoint();
    bool PickHideSpot(float maxDist, float chance);
    void BuildSpotQueue(Vector3 origin, float maxDist);
    void GiveUpChase(Vector3 playerPos);
    bool AtSpot(int spot) const;
    bool ServiceSpotCheck(engine::core::Engine& eng, float dt, float speed);
};

}
