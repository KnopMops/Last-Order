#pragma once
#include "engine/3d/Physics3D.h"
#include "raylib.h"

namespace game::chapter2 {

struct VehicleInput {
    float throttle = 0.0f;
    float brake = 0.0f;
    float steer = 0.0f;
    bool handbrake = false;
    bool engineAlive = true;
};

class Vehicle3D {
public:
    void Setup(Vector3 origin, float heading, Vector3 localMin, Vector3 localMax, const engine::physics3d::Physics3D& physics);
    void Reset(Vector3 origin, float heading, const engine::physics3d::Physics3D& physics);
    void Update(float dt, const VehicleInput& input, const engine::physics3d::Physics3D& physics);
    void Kick(Vector3 velocityDelta, float spin);

    Matrix Transform() const;
    Vector3 Position() const { return m_pos; }
    Vector3 GroundPosition() const { return Vector3{m_pos.x, m_groundY, m_pos.z}; }
    Vector3 LocalToWorld(Vector3 local) const;
    Vector3 Forward() const { return m_fwd; }
    Vector3 Right() const { return m_right; }
    Vector3 Up() const { return m_up; }
    float Heading() const { return m_heading; }
    float ForwardSpeed() const;
    float Speed() const;
    Vector3 Velocity() const { return m_vel; }
    float SteerAngle() const { return m_steerAngle; }
    float SteerInput() const { return m_steer; }
    float EngineLoad() const { return m_engineLoad; }
    float Rpm() const { return m_rpm; }
    int Gear() const { return m_gear; }
    bool IsSkidding() const { return m_skid; }
    float TakeImpact();
    float HalfLength() const { return m_halfLength; }
    float HalfWidth() const { return m_halfWidth; }
    float GroundOffset() const { return m_groundOffset; }

private:
    void SampleGround(const engine::physics3d::Physics3D& physics, float dt, bool snap);
    void ResolveCollisions(const engine::physics3d::Physics3D& physics);
    void UpdateBasis();

    Vector3 m_pos{};
    Vector3 m_vel{};
    Vector3 m_fwd{1.0f, 0.0f, 0.0f};
    Vector3 m_right{0.0f, 0.0f, 1.0f};
    Vector3 m_up{0.0f, 1.0f, 0.0f};
    float m_heading = 0.0f;
    float m_spawnHeading = 0.0f;
    float m_pitch = 0.0f;
    float m_roll = 0.0f;
    float m_bodyPitch = 0.0f;
    float m_bodyRoll = 0.0f;
    float m_yawRate = 0.0f;
    float m_steer = 0.0f;
    float m_steerAngle = 0.0f;
    float m_groundY = 0.0f;
    float m_groundOffset = 0.5f;
    float m_halfLength = 2.25f;
    float m_halfWidth = 1.0f;
    float m_centerX = 0.0f;
    float m_centerZ = 0.0f;
    float m_wheelBase = 2.9f;
    float m_track = 1.6f;
    float m_engineLoad = 0.0f;
    float m_rpm = 0.0f;
    int m_gear = 1;
    bool m_skid = false;
    float m_impact = 0.0f;
    float m_lastForwardSpeed = 0.0f;
};

}
