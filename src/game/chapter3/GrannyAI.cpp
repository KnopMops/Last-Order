#include "game/chapter3/GrannyAI.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace game::chapter3 {

void GrannyAI::Init() {
    SetupWaypoints();
    ResetToDay(1);
}

void GrannyAI::SetupWaypoints() {
    m_waypoints.clear();
    m_waypoints.push_back({0, "LivingRoom", {0.0f, 1.0f, -4.0f}, 1, {1, 2, 6}});
    m_waypoints.push_back({1, "Kitchen", {3.0f, 1.0f, -4.0f}, 1, {0, 3}});
    m_waypoints.push_back({2, "FrontHall", {0.0f, 1.0f, 0.0f}, 1, {0, 4, 7}});
    m_waypoints.push_back({3, "BackPorch", {3.5f, 1.0f, -6.0f}, 1, {1}});
    m_waypoints.push_back({4, "Stairs1F", {0.0f, 1.0f, 2.0f}, 1, {2, 5, 8}});
    m_waypoints.push_back({5, "Stairs2F", {0.0f, 3.5f, 2.0f}, 2, {4, 9, 10, 11}});
    m_waypoints.push_back({6, "Garage", {-7.0f, 1.0f, -6.0f}, 1, {0}});
    m_waypoints.push_back({7, "BasementStairs", {0.0f, 0.0f, -1.0f}, 0, {2, 13}});
    m_waypoints.push_back({8, "AtticLadder", {0.0f, 3.5f, 4.0f}, 2, {4, 12}});

    m_waypoints.push_back({9, "Bedroom2F", {-2.0f, 3.5f, 3.0f}, 2, {5}});
    m_waypoints.push_back({10, "GuestRoom2F", {2.5f, 3.5f, 3.5f}, 2, {5}});
    m_waypoints.push_back({11, "Bathroom2F", {3.0f, 3.5f, 1.0f}, 2, {5}});
    m_waypoints.push_back({12, "AtticRadio", {0.0f, 5.5f, 1.0f}, 3, {8}});
    m_waypoints.push_back({13, "BasementWorkbench", {-2.0f, -2.5f, -1.0f}, 0, {7, 14}});
    m_waypoints.push_back({14, "BasementSewer", {2.5f, -2.5f, 3.0f}, 0, {13}});
}

void GrannyAI::ResetToDay(int dayNumber) {
    m_state = EntityState::Patrol;
    m_currentWaypoint = (dayNumber % 2 == 0) ? 6 : 0;
    if (m_currentWaypoint < static_cast<int>(m_waypoints.size())) {
        m_position = m_waypoints[m_currentWaypoint].pos;
        m_targetPos = m_position;
    }
    m_stunTimer = 0.0f;
    m_investigateTimer = 0.0f;
    m_stepTimer = 0.0f;
    m_growlTimer = 5.0f;
    m_caughtPlayer = false;
    m_seesPlayer = false;
    m_speed = 2.4f + (dayNumber - 1) * 0.15f;
}

int GrannyAI::FindNearestWaypoint(Vector3 pos) const {
    int best = 0;
    float bestDist = 9999.0f;
    for (size_t i = 0; i < m_waypoints.size(); ++i) {
        float d = Vector3Distance(pos, m_waypoints[i].pos);
        if (d < bestDist) {
            bestDist = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

std::vector<int> GrannyAI::PlanPath(int startNode, int endNode) const {
    if (startNode == endNode) return {startNode};
    if (startNode < 0 || startNode >= static_cast<int>(m_waypoints.size()) ||
        endNode < 0 || endNode >= static_cast<int>(m_waypoints.size())) {
        return {startNode};
    }

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

        for (int neighbor : m_waypoints[curr].neighbors) {
            if (neighbor >= 0 && neighbor < static_cast<int>(m_waypoints.size()) && !visited[neighbor]) {
                visited[neighbor] = true;
                prev[neighbor] = curr;
                q.push(neighbor);
            }
        }
    }

    if (!reached) return {startNode};

    std::vector<int> path;
    int curr = endNode;
    while (curr != -1) {
        path.push_back(curr);
        curr = prev[curr];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool GrannyAI::HasLineOfSight(Vector3 from, Vector3 to) const {
    if (std::abs(from.y - to.y) > 1.8f) return false;
    float dist = Vector3Distance(from, to);
    if (dist > 18.0f) return false;
    return true;
}

void GrannyAI::NotifySound(Vector3 soundPos, float radius, float volume) {
    if (m_state == EntityState::Stunned) return;

    float dist = Vector3Distance(m_position, soundPos);
    if (dist <= radius) {
        m_targetPos = soundPos;
        m_state = EntityState::Investigate;
        m_investigateTimer = 7.0f;
    }
}

void GrannyAI::ApplyShotgunStun(engine::core::Engine& eng) {
    m_state = EntityState::Stunned;
    m_stunTimer = 30.0f;
    eng.Audio().PlaySfx("audio/sfx/sfx_ch3_entity_stun.wav", 1.0f);
}

void GrannyAI::Update(engine::core::Engine& eng, float dt, Vector3 playerPos, bool playerHidden) {
    m_distToPlayer = Vector3Distance(m_position, playerPos);

    if (m_state == EntityState::Stunned) {
        m_stunTimer -= dt;
        if (m_stunTimer <= 0.0f) {
            m_state = EntityState::Patrol;
            m_currentWaypoint = FindNearestWaypoint(m_position);
        }
        return;
    }

    m_seesPlayer = false;
    if (!playerHidden) {
        Vector3 forward = {std::cos(m_yaw), 0.0f, std::sin(m_yaw)};
        Vector3 toPlayer = Vector3Subtract(playerPos, m_position);
        toPlayer.y = 0.0f;
        float d2D = Vector3Length(toPlayer);
        if (d2D > 0.001f) {
            toPlayer = Vector3Scale(toPlayer, 1.0f / d2D);
            float dot = Vector3DotProduct(forward, toPlayer);
            if ((dot > 0.35f && d2D < 16.0f && HasLineOfSight(m_position, playerPos)) || (d2D < 2.5f)) {
                m_seesPlayer = true;
            }
        }
    }

    if (m_seesPlayer) {
        m_state = EntityState::Chase;
        m_targetPos = playerPos;
    }

    if (m_state == EntityState::Chase) {
        float chaseSpeed = 4.2f;
        Vector3 moveDir = Vector3Subtract(m_targetPos, m_position);
        moveDir.y = 0.0f;
        float len = Vector3Length(moveDir);
        if (len > 0.05f) {
            moveDir = Vector3Scale(moveDir, 1.0f / len);
            m_yaw = std::atan2(moveDir.z, moveDir.x);
            m_position = Vector3Add(m_position, Vector3Scale(moveDir, chaseSpeed * dt));
        }

        if (m_distToPlayer <= 1.35f && !playerHidden) {
            m_caughtPlayer = true;
            m_state = EntityState::Knockout;
        }

        if (!m_seesPlayer && len < 0.8f) {
            m_state = EntityState::Investigate;
            m_investigateTimer = 5.0f;
        }
    } else if (m_state == EntityState::Investigate) {
        float walkSpeed = 2.8f;
        Vector3 moveDir = Vector3Subtract(m_targetPos, m_position);
        moveDir.y = 0.0f;
        float len = Vector3Length(moveDir);
        if (len > 0.4f) {
            moveDir = Vector3Scale(moveDir, 1.0f / len);
            m_yaw = std::atan2(moveDir.z, moveDir.x);
            m_position = Vector3Add(m_position, Vector3Scale(moveDir, walkSpeed * dt));
        } else {
            m_yaw += 0.8f * dt;
            m_investigateTimer -= dt;
            if (m_investigateTimer <= 0.0f) {
                m_state = EntityState::Patrol;
                m_currentWaypoint = FindNearestWaypoint(m_position);
            }
        }
    } else if (m_state == EntityState::Patrol) {
        if (!m_waypoints.empty()) {
            const auto& wp = m_waypoints[m_currentWaypoint];
            Vector3 moveDir = Vector3Subtract(wp.pos, m_position);
            moveDir.y = 0.0f;
            float len = Vector3Length(moveDir);
            if (len > 0.4f) {
                moveDir = Vector3Scale(moveDir, 1.0f / len);
                m_yaw = std::atan2(moveDir.z, moveDir.x);
                m_position = Vector3Add(m_position, Vector3Scale(moveDir, m_speed * dt));
            } else {
                if (!wp.neighbors.empty()) {
                    int nextIdx = wp.neighbors[std::rand() % wp.neighbors.size()];
                    m_currentWaypoint = nextIdx;
                }
            }
        }
    }

    m_stepTimer += dt;
    float stepInterval = (m_state == EntityState::Chase) ? 0.32f : 0.65f;
    if (m_stepTimer >= stepInterval) {
        m_stepTimer = 0.0f;
        if (m_distToPlayer < 24.0f) {
            float vol = std::clamp(1.0f - (m_distToPlayer / 24.0f), 0.1f, 0.9f);
            eng.Audio().PlaySfx("audio/sfx/sfx_footstep_wood_01.wav", vol);
        }
    }

    m_growlTimer -= dt;
    if (m_growlTimer <= 0.0f) {
        m_growlTimer = 12.0f + static_cast<float>(std::rand() % 8);
        if (m_distToPlayer < 20.0f) {
            float vol = std::clamp(1.0f - (m_distToPlayer / 20.0f), 0.15f, 0.85f);
            eng.Audio().PlaySfx("audio/sfx/sfx_stalker_breathe.wav", vol);
        }
    }
}

}

