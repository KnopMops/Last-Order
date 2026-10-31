#pragma once
#include "engine/math/Math2D.h"
#include <optional>
#include <string>
#include <vector>

namespace engine::physics {

struct StaticCollider {
    std::string id;
    math::Rect bounds;
    bool blocksMovement = true;
    bool blocksLight = false;
};

struct TriggerVolume {
    std::string id;
    std::string label;
    std::string dialogueId;
    math::Rect bounds;
    bool enabled = true;
};

struct KinematicBody {
    math::Vec2 position;
    math::Vec2 colliderOffset = {8.0f, 18.0f};
    math::Vec2 colliderSize = {16.0f, 12.0f};
    math::Vec2 velocity = {0.0f, 0.0f};
    math::Vec2 subpixelRemainder = {0.0f, 0.0f};

    math::Rect WorldBounds() const {
        return {position.x + colliderOffset.x, position.y + colliderOffset.y, colliderSize.x, colliderSize.y};
    }

    math::Vec2 Center() const {
        return WorldBounds().Center();
    }
};

struct RaycastHit {
    bool hit = false;
    math::Vec2 point;
    float distance = 0.0f;
    std::string colliderId;
};

class PhysicsWorld {
public:
    void Clear();

    void AddStaticCollider(const std::string& id, math::Rect bounds, bool blocksMovement = true, bool blocksLight = false);
    void AddTrigger(const TriggerVolume& trigger);
    void SetTriggerEnabled(const std::string& id, bool enabled);
    void UpdateTriggerDialogue(const std::string& id, const std::string& dialogueId, const std::string& label = "");

    void MoveAndSlide(KinematicBody& body, float dt) const;

    bool OverlapsSolid(const math::Rect& rect) const;

    std::optional<TriggerVolume> FindNearestTrigger(math::Vec2 center, float maxRadius) const;

    RaycastHit Raycast(math::Vec2 origin, math::Vec2 dirNormalized, float maxDistance, bool lightOnly = false) const;

    const std::vector<StaticCollider>& GetColliders() const { return m_colliders; }
    const std::vector<TriggerVolume>& GetTriggers() const { return m_triggers; }

private:
    std::vector<StaticCollider> m_colliders;
    std::vector<TriggerVolume> m_triggers;
};

}
