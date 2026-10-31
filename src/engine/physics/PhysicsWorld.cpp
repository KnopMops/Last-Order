#include "engine/physics/PhysicsWorld.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace engine::physics {

void PhysicsWorld::Clear() {
    m_colliders.clear();
    m_triggers.clear();
}

void PhysicsWorld::AddStaticCollider(const std::string& id, math::Rect bounds, bool blocksMovement, bool blocksLight) {
    m_colliders.push_back(StaticCollider{id, bounds, blocksMovement, blocksLight});
}

void PhysicsWorld::AddTrigger(const TriggerVolume& trigger) {
    for (auto& t : m_triggers) {
        if (t.id == trigger.id) {
            t = trigger;
            return;
        }
    }
    m_triggers.push_back(trigger);
}

void PhysicsWorld::SetTriggerEnabled(const std::string& id, bool enabled) {
    for (auto& t : m_triggers) {
        if (t.id == id) {
            t.enabled = enabled;
        }
    }
}

void PhysicsWorld::UpdateTriggerDialogue(const std::string& id, const std::string& dialogueId, const std::string& label) {
    for (auto& t : m_triggers) {
        if (t.id == id) {
            t.dialogueId = dialogueId;
            if (!label.empty()) t.label = label;
        }
    }
}

bool PhysicsWorld::OverlapsSolid(const math::Rect& rect) const {
    for (const auto& c : m_colliders) {
        if (c.blocksMovement && c.bounds.Intersects(rect)) {
            return true;
        }
    }
    return false;
}

void PhysicsWorld::MoveAndSlide(KinematicBody& body, float dt) const {
    math::Vec2 delta = body.velocity * dt + body.subpixelRemainder;
    int moveX = static_cast<int>(std::round(delta.x));
    int moveY = static_cast<int>(std::round(delta.y));
    body.subpixelRemainder.x = delta.x - static_cast<float>(moveX);
    body.subpixelRemainder.y = delta.y - static_cast<float>(moveY);

    int signX = (moveX > 0) ? 1 : -1;
    for (int i = 0; i < std::abs(moveX); ++i) {
        math::Rect nextRect = body.WorldBounds();
        nextRect.x += static_cast<float>(signX);
        if (!OverlapsSolid(nextRect)) {
            body.position.x += static_cast<float>(signX);
        } else {
            body.subpixelRemainder.x = 0.0f;
            break;
        }
    }

    int signY = (moveY > 0) ? 1 : -1;
    for (int i = 0; i < std::abs(moveY); ++i) {
        math::Rect nextRect = body.WorldBounds();
        nextRect.y += static_cast<float>(signY);
        if (!OverlapsSolid(nextRect)) {
            body.position.y += static_cast<float>(signY);
        } else {
            body.subpixelRemainder.y = 0.0f;
            break;
        }
    }
}

std::optional<TriggerVolume> PhysicsWorld::FindNearestTrigger(math::Vec2 center, float maxRadius) const {
    float bestDistSq = maxRadius * maxRadius;
    std::optional<TriggerVolume> best;

    for (const auto& t : m_triggers) {
        if (!t.enabled) continue;

        float cx = math::Clamp(center.x, t.bounds.Left(), t.bounds.Right());
        float cy = math::Clamp(center.y, t.bounds.Top(), t.bounds.Bottom());
        float dx = center.x - cx;
        float dy = center.y - cy;
        float distSq = dx * dx + dy * dy;
        if (distSq <= bestDistSq) {
            bestDistSq = distSq;
            best = t;
        }
    }
    return best;
}

RaycastHit PhysicsWorld::Raycast(math::Vec2 origin, math::Vec2 dirNormalized, float maxDistance, bool lightOnly) const {
    RaycastHit result;
    const float step = 2.0f;
    for (float d = 0.0f; d <= maxDistance; d += step) {
        math::Vec2 pt = origin + dirNormalized * d;
        for (const auto& c : m_colliders) {
            if (lightOnly && !c.blocksLight) continue;
            if (!lightOnly && !c.blocksMovement) continue;
            if (c.bounds.Contains(pt)) {
                result.hit = true;
                result.point = pt;
                result.distance = d;
                result.colliderId = c.id;
                return result;
            }
        }
    }
    return result;
}

}
