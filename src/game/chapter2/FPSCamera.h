#pragma once
#include "raylib.h"
#include <vector>

namespace game::chapter2 {

class FPSCamera {
public:
    void Init(Vector3 startPos, float startYaw, float startPitch);
    void Update(float dt, const std::vector<BoundingBox>& colliders);

    const Camera3D& GetCamera() const { return m_camera; }
    Vector3 GetPosition() const { return m_position; }
    Vector3 GetForward() const;
    Vector3 GetRight() const;
    Ray GetViewRay() const;

    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    float GetStamina() const { return m_stamina; }
    bool IsCrouching() const { return m_isCrouching; }
    bool IsSprinting() const { return m_isSprinting; }
    bool IsMoving() const { return m_isMoving; }
    float GetBobTimer() const { return m_bobTimer; }

    void SetPosition(Vector3 pos) { m_position = pos; }
    void SetLookAngle(float yaw, float pitch);

private:
    Camera3D m_camera{};
    Vector3 m_position{0.0f, 1.65f, 0.0f};
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_bobTimer = 0.0f;
    float m_stamina = 100.0f;
    bool m_isCrouching = false;
    bool m_isSprinting = false;
    bool m_isMoving = false;
};

}
