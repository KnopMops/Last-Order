#pragma once
#include "engine/core/Engine.h"
#include "engine/math/Math2D.h"
#include "engine/physics/PhysicsWorld.h"
#include <optional>
#include <string>
#include <vector>

namespace game::world {

struct GuestNpc {
    std::string id;
    std::string name;
    std::string spritePath;
    std::string triggerId;
    std::string dialogueId;
    engine::math::Vec2 pos;
    int spawnMinuteOffset = 0;
    bool spawned = false;
    bool anomalous = false;
    bool hasReflection = true;
    int spriteFrame = 0;
};

class CafeWorld {
public:
    static constexpr float WORLD_W = 640.0f;
    static constexpr float WORLD_H = 360.0f;

    void BuildCollidersAndTriggers(engine::core::Engine& eng, int nightNumber);
    void BuildLighting(engine::core::Engine& eng, int nightNumber);

    engine::math::Vec2 ComputeCameraOffset(engine::math::Vec2 playerCenter) const;

    void Render(engine::core::Engine& eng,
                int nightNumber,
                const engine::physics::KinematicBody& playerBody,
                int playerFacing,
                bool isMoving,
                float walkAnimTimer,
                bool lightsFlickering,
                bool blackoutMode,
                bool showDebugPhysics,
                const std::optional<engine::physics::TriggerVolume>& nearestTrigger,
                const std::vector<GuestNpc>& guests) const;

    void Render3DVisor(engine::core::Engine& eng,
                       int nightNumber,
                       engine::math::Vec2 playerCenter,
                       float visorYawRad,
                       float visorPitchRad,
                       const std::vector<GuestNpc>& guests) const;
};

}
