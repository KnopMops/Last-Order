#pragma once
#include "raylib.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::physics3d {

using BodyHandle = uint32_t;
constexpr BodyHandle INVALID_BODY = 0xFFFFFFFFu;

struct RayHit {
    bool hit = false;
    float distance = 0.0f;
    Vector3 point{};
    Vector3 normal{};
    BodyHandle body = INVALID_BODY;
};

struct CharacterConfig {
    float radius = 0.3f;
    float standHeight = 1.75f;
    float crouchHeight = 1.1f;
    float mass = 70.0f;
    float pushStrength = 90.0f;
};

class Physics3D {
public:
    Physics3D();
    ~Physics3D();
    Physics3D(const Physics3D&) = delete;
    Physics3D& operator=(const Physics3D&) = delete;

    bool Init();
    void Shutdown();
    bool IsReady() const;

    BodyHandle AddStaticHull(const std::vector<Vector3>& worldPoints);
    BodyHandle AddDynamicHull(const std::vector<Vector3>& worldPoints, float mass, Vector3& outOrigin);
    BodyHandle AddKinematicHull(const std::vector<Vector3>& worldPoints, Vector3& outOrigin);
    BodyHandle AddDoor(const std::vector<Vector3>& worldPoints, Vector3 hingePoint, float mass,
                       float minAngle, float maxAngle, Vector3& outOrigin);
    void FinalizeStatic();

    void SetDoorLimits(BodyHandle door, float minAngle, float maxAngle);
    void SetDoorTarget(BodyHandle door, float angle);
    float GetDoorAngle(BodyHandle door) const;

    void Step(float dt);

    Matrix GetBodyTransform(BodyHandle body) const;
    Vector3 GetBodyPosition(BodyHandle body) const;
    Vector3 GetBodyVelocity(BodyHandle body) const;
    void SetBodyVelocity(BodyHandle body, Vector3 linear, Vector3 angular);
    Vector3 GetBodyAngularVelocity(BodyHandle body) const;
    void AddImpulse(BodyHandle body, Vector3 impulse);
    void WakeBody(BodyHandle body);
    void MoveKinematic(BodyHandle body, const Matrix& transform, float dt);
    void TeleportBody(BodyHandle body, const Matrix& transform);

    RayHit CastRay(Vector3 origin, Vector3 direction, float maxDistance, BodyHandle ignore = INVALID_BODY) const;
    bool IsStaticOccluded(Vector3 from, Vector3 to) const;
    float CastStaticDistance(Vector3 origin, Vector3 direction, float maxDistance) const;
    RayHit CastStaticRay(Vector3 origin, Vector3 direction, float maxDistance) const;

    void CreateCharacter(Vector3 feetPosition, const CharacterConfig& config);
    void UpdateCharacter(float dt, Vector3 desiredHorizontalVelocity, bool wantCrouch, BodyHandle ignoreBody);
    void TeleportCharacter(Vector3 feetPosition);
    Vector3 GetCharacterPosition() const;
    Vector3 GetCharacterVelocity() const;
    bool IsCharacterOnGround() const;
    bool IsCharacterCrouched() const;
    BodyHandle GetCharacterGroundBody() const;
    BodyHandle GetCharacterInnerBody() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}
