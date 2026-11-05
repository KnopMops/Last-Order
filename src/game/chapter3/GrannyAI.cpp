#include "game/chapter3/GrannyAI.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>

namespace game::chapter3 {

namespace {
constexpr float EYE = 1.5f;
constexpr float CHASE_SPEED = 2.5f;
constexpr float INVESTIGATE_SPEED = 1.5f;
}

void GrannyAI::Init() {
    SetupWaypoints();
    ResetToDay(1);
}

void GrannyAI::Link(int a, int b) {
    m_waypoints[a].neighbors.push_back(b);
    m_waypoints[b].neighbors.push_back(a);
}

void GrannyAI::SetupWaypoints() {
    m_waypoints.clear();
    auto add = [this](const char* name, Vector3 p, int floor, bool patrol) {
        HouseWaypoint w;
        w.id = static_cast<int>(m_waypoints.size());
        w.name = name;
        w.pos = p;
        w.floor = floor;
        w.patrol = patrol;
        m_waypoints.push_back(w);
    };
    add("Hall1F_S", {0.0f, 0.0f, 4.6f}, 1, true);
    add("Hall1F_Mid", {-0.6f, 0.0f, 0.2f}, 1, false);
    add("Hall1F_N", {-0.6f, 0.0f, -5.5f}, 1, false);
    add("Stair1F_Bottom", {0.85f, 0.0f, -5.6f}, 1, false);
    add("Stair2F_Top", {0.85f, 3.2f, 0.0f}, 2, false);
    add("LivingDoor", {-1.5f, 0.0f, 3.5f}, 1, false);
    add("Living", {-3.6f, 0.0f, 3.1f}, 1, true);
    add("GarageDoor", {-6.1f, 0.0f, 2.7f}, 1, false);
    add("Garage", {-7.5f, 0.0f, 1.0f}, 1, true);
    add("GarageBack", {-7.6f, 0.0f, -4.5f}, 1, false);
    add("PantryDoor", {-1.5f, 0.0f, -2.5f}, 1, false);
    add("Pantry", {-3.6f, 0.0f, -2.5f}, 1, true);
    add("BasementTop", {-5.4f, 0.0f, 0.7f}, 1, false);
    add("BasementBottom", {-5.4f, -3.0f, -4.8f}, 0, false);
    add("BasementWork", {-1.5f, -3.0f, -4.2f}, 0, true);
    add("BasementMid", {0.5f, -3.0f, 0.5f}, 0, false);
    add("BoilerDoor", {3.6f, -3.0f, 1.5f}, 0, false);
    add("Boiler", {4.2f, -3.0f, 3.3f}, 0, true);
    add("DiningDoor", {1.5f, 0.0f, 3.5f}, 1, false);
    add("Dining", {2.4f, 0.0f, 2.0f}, 1, true);
    add("KitchenArch", {4.0f, 0.0f, 1.0f}, 1, false);
    add("KitchenDoor", {1.5f, 0.0f, 0.2f}, 1, false);
    add("Kitchen", {4.6f, 0.0f, -1.0f}, 1, true);
    add("KitchenBack", {4.7f, 0.0f, -4.7f}, 1, false);
    add("Hall2F_S", {0.0f, 3.2f, 3.5f}, 2, true);
    add("Hall2F_Mid", {-0.6f, 3.2f, 0.2f}, 2, false);
    add("Hall2F_N", {-0.6f, 3.2f, -4.0f}, 2, false);
    add("BedroomDoor", {-1.5f, 3.2f, 3.5f}, 2, false);
    add("Bedroom", {-2.8f, 3.2f, 2.4f}, 2, true);
    add("GuestDoor", {1.5f, 3.2f, 3.5f}, 2, false);
    add("Guest", {3.6f, 3.2f, 3.2f}, 2, true);
    add("BathDoor", {1.5f, 3.2f, 0.2f}, 2, false);
    add("Bath", {3.8f, 3.2f, -2.0f}, 2, true);
    add("StorageDoor", {-1.5f, 3.2f, -2.5f}, 2, false);
    add("Storage", {-3.4f, 3.2f, -2.5f}, 2, false);
    add("StorageN", {-4.4f, 3.2f, -5.1f}, 2, false);
    add("AtticBottom", {-5.4f, 3.2f, -5.2f}, 2, false);
    add("AtticTop", {-5.4f, 6.4f, 0.4f}, 3, false);
    add("Attic", {0.8f, 6.4f, 0.3f}, 3, true);
    add("AtticRadio", {3.0f, 6.4f, -4.4f}, 3, true);
    add("AtticSouth", {-1.5f, 6.4f, 4.6f}, 3, true);

    Link(0, 1);
    Link(1, 2);
    Link(2, 3);
    Link(3, 4);
    Link(0, 5);
    Link(5, 6);
    Link(6, 7);
    Link(7, 8);
    Link(8, 9);
    Link(1, 10);
    Link(10, 11);
    Link(11, 12);
    Link(12, 13);
    Link(13, 14);
    Link(14, 15);
    Link(15, 16);
    Link(16, 17);
    Link(0, 18);
    Link(18, 19);
    Link(19, 20);
    Link(20, 22);
    Link(1, 21);
    Link(21, 22);
    Link(22, 23);
    Link(4, 24);
    Link(4, 25);
    Link(24, 25);
    Link(25, 26);
    Link(24, 27);
    Link(27, 28);
    Link(24, 29);
    Link(29, 30);
    Link(25, 31);
    Link(4, 31);
    Link(31, 32);
    Link(26, 33);
    Link(33, 34);
    Link(34, 35);
    Link(35, 36);
    Link(36, 37);
    Link(37, 38);
    Link(38, 39);
    Link(38, 40);
    Link(37, 40);
}

void GrannyAI::ResetToDay(int dayNumber) {
    m_day = dayNumber;
    m_state = EntityState::Patrol;
    int start = (dayNumber % 2 == 0) ? 14 : 6;
    m_position = m_waypoints[start].pos;
    m_targetPos = m_position;
    m_stunTimer = 0.0f;
    m_investigateTimer = 0.0f;
    m_idleTimer = 2.0f;
    m_stepTimer = 0.0f;
    m_growlTimer = 5.0f;
    m_repathTimer = 0.0f;
    m_caughtPlayer = false;
    m_seesPlayer = false;
    m_path.clear();
    m_pathIndex = 0;
    m_speed = 1.8f + static_cast<float>(dayNumber - 1) * 0.1f;
    m_prevHidden = false;
    m_forceCatch = false;
    m_alert = 0;
    m_searchTimer = 0.0f;
    m_suspicion = 0.0f;
    m_lostTimer = 0.0f;
    m_lostSaid = false;
    PickPatrolTarget();
}

void GrannyAI::PickPatrolTarget() {
    std::vector<int> candidates;
    for (const auto& w : m_waypoints) {
        if (w.patrol && Vector3Distance(w.pos, m_position) > 2.0f) candidates.push_back(w.id);
    }
    if (candidates.empty()) return;
    m_patrolTarget = candidates[static_cast<size_t>(std::rand()) % candidates.size()];
}

bool GrannyAI::HasLineOfSight(Vector3 from, Vector3 to) const {
    if (!m_occluded) return true;
    return !m_occluded(Vector3{from.x, from.y + EYE, from.z}, Vector3{to.x, to.y + EYE, to.z});
}

int GrannyAI::FindNearestWaypoint(Vector3 pos, bool requireSight) const {
    int best = -1;
    float bestScore = 1e9f;
    for (size_t i = 0; i < m_waypoints.size(); ++i) {
        const Vector3& p = m_waypoints[i].pos;
        float dy = std::abs(p.y - pos.y);
        if (requireSight && (dy > 1.0f || !HasLineOfSight(pos, p))) continue;
        float score = Vector3Distance(pos, p) + dy * 3.0f;
        if (score < bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    if (best < 0 && requireSight) return FindNearestWaypoint(pos, false);
    return std::max(best, 0);
}

std::vector<int> GrannyAI::PlanPath(int startNode, int endNode) const {
    if (startNode == endNode) return {startNode};
    std::vector<int> prev(m_waypoints.size(), -1);
    std::vector<bool> visited(m_waypoints.size(), false);
    std::queue<int> q;
    q.push(startNode);
    visited[startNode] = true;
    bool reached = false;
    while (!q.empty()) {
        int curr = q.front();
        q.pop();
        if (curr == endNode) {
            reached = true;
            break;
        }
        for (int n : m_waypoints[curr].neighbors) {
            if (!visited[n]) {
                visited[n] = true;
                prev[n] = curr;
                q.push(n);
            }
        }
    }
    if (!reached) return {startNode};
    std::vector<int> path;
    for (int c = endNode; c != -1; c = prev[c]) path.push_back(c);
    std::reverse(path.begin(), path.end());
    return path;
}

bool GrannyAI::StepTowards(Vector3 goal, float speed, float dt) {
    Vector3 d = Vector3Subtract(goal, m_position);
    float len = Vector3Length(d);
    if (len < 0.3f) return true;
    float step = std::min(len, speed * dt);
    float flat = std::sqrt(d.x * d.x + d.z * d.z);
    Vector3 move = Vector3Scale(d, step / len);

    if (m_occluded && flat > 0.01f) {
        Vector3 dir{d.x / flat, 0.0f, d.z / flat};
        static const float angles[] = {0.0f, 0.6f, -0.6f, 1.2f, -1.2f, 1.9f, -1.9f};
        bool moved = false;
        for (float a : angles) {
            float c = std::cos(a);
            float s = std::sin(a);
            Vector3 cd{dir.x * c - dir.z * s, 0.0f, dir.x * s + dir.z * c};
            Vector3 reach = Vector3Add(m_position, Vector3Scale(cd, step + 0.45f));
            bool blocked = false;
            for (float h : {0.45f, 1.0f, 1.5f}) {
                Vector3 from{m_position.x, m_position.y + h, m_position.z};
                Vector3 to{reach.x, m_position.y + h + (a == 0.0f ? d.y / len * (step + 0.45f) : 0.0f), reach.z};
                if (m_occluded(from, to)) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) continue;
            move = Vector3{cd.x * step, a == 0.0f ? move.y : d.y / len * step, cd.z * step};
            moved = true;
            break;
        }
        if (!moved) {
            m_stuckTimer += dt;
            if (m_stuckTimer > 1.2f) {
                int nearest = FindNearestWaypoint(m_position, true);
                m_position = m_waypoints[nearest].pos;
                m_stuckTimer = 0.0f;
                m_path.clear();
                m_repathTimer = 0.0f;
                if (m_state == EntityState::Patrol) PickPatrolTarget();
            }
            return false;
        }
        m_stuckTimer = 0.0f;
        flat = std::sqrt(move.x * move.x + move.z * move.z);
        if (flat > 0.0001f) {
            float targetYaw = std::atan2(move.z, move.x);
            float diff = std::remainder(targetYaw - m_yaw, 2.0f * PI);
            m_yaw += diff * std::min(1.0f, dt * 8.0f);
        }
    } else if (flat > 0.01f) {
        float targetYaw = std::atan2(d.z, d.x);
        float diff = std::remainder(targetYaw - m_yaw, 2.0f * PI);
        m_yaw += diff * std::min(1.0f, dt * 8.0f);
    }
    m_position = Vector3Add(m_position, move);
    m_walkPhase += step * 3.2f;
    return len - step < 0.3f;
}

bool GrannyAI::NavigateTo(Vector3 goal, float speed, float dt) {
    bool pathActive = m_pathIndex < m_path.size();
    if (!pathActive && std::abs(goal.y - m_position.y) < 0.6f && HasLineOfSight(m_position, goal)) {
        m_path.clear();
        return StepTowards(goal, speed, dt);
    }
    bool goalMoved = Vector3Distance(goal, m_pathGoal) > 1.5f;
    if (m_path.empty() || (goalMoved && !pathActive) || (goalMoved && m_repathTimer <= 0.0f)) {
        int s = pathActive ? m_path[m_pathIndex] : FindNearestWaypoint(m_position, true);
        int e = FindNearestWaypoint(goal, true);
        m_path = PlanPath(s, e);
        m_pathIndex = 0;
        m_pathGoal = goal;
        m_repathTimer = 1.5f;
    }
    m_repathTimer -= dt;
    if (m_pathIndex < m_path.size()) {
        if (StepTowards(m_waypoints[m_path[m_pathIndex]].pos, speed, dt)) m_pathIndex++;
        return false;
    }
    bool sameFloor = std::abs(goal.y - m_position.y) < 0.6f;
    if (!sameFloor || !HasLineOfSight(m_position, goal)) {
        m_path.clear();
        return true;
    }
    return StepTowards(goal, speed, dt);
}

void GrannyAI::NotifySound(Vector3 soundPos, float radius, float volume) {
    if (m_state == EntityState::Stunned || m_state == EntityState::Chase || m_state == EntityState::Knockout) return;
    float dist = Vector3Distance(m_position, soundPos);
    float dy = std::abs(soundPos.y - m_position.y);
    float effective = dy > 1.5f ? radius * 0.6f : radius;
    if (dist <= effective && volume > 0.0f) {
        if (m_state != EntityState::Investigate) m_alert = 2;
        m_targetPos = soundPos;
        m_state = EntityState::Investigate;
        m_investigateTimer = 5.0f;
        m_repathTimer = 0.0f;
    }
}

void GrannyAI::ApplyShotgunStun(engine::core::Engine& eng) {
    m_state = EntityState::Stunned;
    m_stunTimer = 30.0f;
    m_path.clear();
    m_forceCatch = false;
    eng.Audio().PlaySfx("audio/sfx/sfx_ch3_entity_stun.wav", 1.0f);
}

void GrannyAI::PickSearchPoint() {
    std::vector<int> candidates;
    for (const auto& w : m_waypoints) {
        if (std::abs(w.pos.y - m_searchOrigin.y) < 1.2f && Vector3Distance(w.pos, m_searchOrigin) < 7.5f) candidates.push_back(w.id);
    }
    if (candidates.empty()) {
        m_searchPoint = m_searchOrigin;
        return;
    }
    m_searchPoint = m_waypoints[candidates[static_cast<size_t>(std::rand()) % candidates.size()]].pos;
}

void GrannyAI::Update(engine::core::Engine& eng, float dt, Vector3 playerPos, bool playerHidden, bool playerCrouched, bool playerLit) {
    m_distToPlayer = Vector3Distance(m_position, playerPos);

    if (m_state == EntityState::Stunned) {
        m_stunTimer -= dt;
        if (m_stunTimer <= 0.0f) {
            int respawn = (std::rand() % 2 == 0) ? 14 : 38;
            m_position = m_waypoints[respawn].pos;
            m_state = EntityState::Patrol;
            m_idleTimer = 1.0f;
            m_path.clear();
            PickPatrolTarget();
        }
        return;
    }
    if (m_state == EntityState::Knockout) return;

    if (Vector3Distance(m_position, m_progressPos) > 0.7f) {
        m_progressPos = m_position;
        m_progressTimer = 0.0f;
    } else if (m_idleTimer <= 0.0f && m_searchIdle <= 0.0f && m_state != EntityState::Investigate) {
        m_progressTimer += dt;
        if (m_progressTimer > 3.5f) {
            m_progressTimer = 0.0f;
            int nearest = FindNearestWaypoint(m_position, true);
            if (m_pathIndex < m_path.size()) nearest = m_path[m_pathIndex];
            m_position = m_waypoints[nearest].pos;
            m_progressPos = m_position;
            m_path.clear();
            m_repathTimer = 0.0f;
            if (m_state == EntityState::Patrol) PickPatrolTarget();
            else if (m_state == EntityState::Search) PickSearchPoint();
        }
    }

    float dy = std::abs(playerPos.y - m_position.y);
    Vector3 toPlayer = Vector3Subtract(playerPos, m_position);
    toPlayer.y = 0.0f;
    float flatDist = Vector3Length(toPlayer);

    Vector3 playerVel = dt > 0.0001f ? Vector3Scale(Vector3Subtract(playerPos, m_lastPlayerPos), 1.0f / dt) : Vector3{0.0f, 0.0f, 0.0f};
    playerVel.y = 0.0f;
    if (Vector3Length(playerVel) > 8.0f) playerVel = Vector3{0.0f, 0.0f, 0.0f};
    m_lastPlayerPos = playerPos;

    bool visible = false;
    if (!playerHidden && dy < 1.6f && flatDist < 18.0f) {
        Vector3 forward{std::cos(m_yaw), 0.0f, std::sin(m_yaw)};
        float dot = flatDist > 0.001f ? Vector3DotProduct(forward, Vector3Scale(toPlayer, 1.0f / flatDist)) : 1.0f;
        bool inCone = dot > 0.5f || flatDist < 1.8f;
        if (inCone && HasLineOfSight(m_position, playerPos)) visible = true;
    }

    float detectRange = playerCrouched ? 5.0f : 11.0f;
    if (!playerLit) detectRange *= 0.65f;
    bool tracking = m_state == EntityState::Chase;
    if (visible && (tracking || flatDist < detectRange)) {
        float rate = 0.5f + (1.0f - std::min(1.0f, flatDist / detectRange)) * 1.8f;
        if (flatDist < 3.0f) rate += 3.0f;
        m_suspicion = std::min(1.2f, m_suspicion + rate * dt);
    } else {
        m_suspicion = std::max(0.0f, m_suspicion - 0.7f * dt);
    }
    m_seesPlayer = visible && (tracking || m_suspicion >= 1.0f);
    if (visible && m_state != EntityState::Chase && m_suspicion >= 1.0f) m_seesPlayer = true;

    if (playerHidden && !m_prevHidden) {
        m_forceCatch = m_seesPlayer || m_state == EntityState::Chase || m_suspicion > 0.55f;
        if (m_forceCatch) {
            m_state = EntityState::Chase;
            m_targetPos = playerPos;
            m_alert = 1;
        }
    }
    if (!playerHidden) m_forceCatch = false;
    m_prevHidden = playerHidden;

    if (!m_seesPlayer && visible && m_suspicion > 0.25f && m_state != EntityState::Chase) {
        float targetYaw = std::atan2(toPlayer.z, toPlayer.x);
        float diff = std::remainder(targetYaw - m_yaw, 2.0f * PI);
        m_yaw += diff * std::min(1.0f, dt * 6.0f);
        m_lastSeenPos = playerPos;
        return;
    }

    if (m_seesPlayer) {
        if (m_state != EntityState::Chase) {
            eng.Audio().PlaySfx("audio/sfx/sfx_stalker_breathe.wav", 0.9f);
            m_alert = 1;
        }
        m_state = EntityState::Chase;
        m_targetPos = playerPos;
        m_lastSeenPos = playerPos;
        m_lastSeenVel = playerVel;
        m_lostTimer = 0.0f;
        m_lostSaid = false;
        if (flatDist <= 1.2f && dy < 1.0f) {
            m_caughtPlayer = true;
            m_state = EntityState::Knockout;
            return;
        }
    }

    if (m_state == EntityState::Chase) {
        if (m_forceCatch) {
            m_targetPos = playerPos;
            if (m_distToPlayer <= 1.5f && dy < 1.2f) {
                m_caughtPlayer = true;
                m_state = EntityState::Knockout;
                m_forceCatch = false;
                return;
            }
        } else if (!m_seesPlayer) {
            m_lostTimer += dt;
            if (!m_lostSaid && m_lostTimer > 0.9f) {
                m_lostSaid = true;
                m_alert = 4;
                m_targetPos = Vector3Add(m_lastSeenPos, Vector3Scale(m_lastSeenVel, 1.6f));
                m_repathTimer = 0.0f;
            }
        }
        bool reached = NavigateTo(m_targetPos, CHASE_SPEED, dt);
        if (!m_seesPlayer && reached && !m_forceCatch) {
            m_state = EntityState::Search;
            m_alert = 5;
            m_searchTimer = 9.0f;
            m_searchOrigin = m_targetPos;
            m_searchIdle = 0.8f;
            PickSearchPoint();
        }
    } else if (m_state == EntityState::Investigate) {
        if (NavigateTo(m_targetPos, INVESTIGATE_SPEED, dt)) {
            m_yaw += 0.9f * dt;
            m_investigateTimer -= dt;
            if (m_investigateTimer <= 0.0f) {
                m_state = EntityState::Search;
                m_alert = 5;
                m_searchTimer = 8.0f;
                m_searchOrigin = m_targetPos;
                m_searchIdle = 0.0f;
                PickSearchPoint();
            }
        }
    } else if (m_state == EntityState::Search) {
        m_searchTimer -= dt;
        if (m_searchIdle > 0.0f) {
            m_searchIdle -= dt;
            m_yaw += 1.2f * dt;
        } else if (NavigateTo(m_searchPoint, INVESTIGATE_SPEED, dt)) {
            m_searchIdle = 1.2f;
            PickSearchPoint();
        }
        if (m_searchTimer <= 0.0f) {
            m_state = EntityState::Patrol;
            m_idleTimer = 1.0f;
            m_alert = 3;
            PickPatrolTarget();
        }
    } else if (m_state == EntityState::Patrol) {
        if (m_idleTimer > 0.0f) {
            m_idleTimer -= dt;
            m_yaw += 0.5f * dt;
        } else if (NavigateTo(m_waypoints[m_patrolTarget].pos, m_speed, dt)) {
            m_idleTimer = 1.5f + static_cast<float>(std::rand() % 20) * 0.1f;
            PickPatrolTarget();
        }
    }
    float floorFactor = dy > 1.5f ? 0.45f : 1.0f;
    m_stepTimer += dt;
    float stepInterval = (m_state == EntityState::Chase) ? 0.32f : 0.65f;
    if (m_stepTimer >= stepInterval && m_idleTimer <= 0.0f) {
        m_stepTimer = 0.0f;
        if (m_distToPlayer < 24.0f) {
            float vol = std::clamp(1.0f - (m_distToPlayer / 24.0f), 0.05f, 0.9f) * floorFactor;
            eng.Audio().PlaySfx("audio/sfx/sfx_footstep_wood_01.wav", vol);
        }
    }

    m_growlTimer -= dt;
    if (m_growlTimer <= 0.0f) {
        m_growlTimer = 12.0f + static_cast<float>(std::rand() % 8);
        if (m_distToPlayer < 20.0f) {
            float vol = std::clamp(1.0f - (m_distToPlayer / 20.0f), 0.15f, 0.85f) * floorFactor;
            eng.Audio().PlaySfx("audio/sfx/sfx_stalker_breathe.wav", vol);
        }
    }
}

}
