#pragma once
#include "raylib.h"

namespace engine::camera3d {

struct CameraMotion {
    float horizontalSpeed = 0.0f;
    bool grounded = true;
    bool crouched = false;
    bool sprinting = false;
    float strafe = 0.0f;
};

class CameraFPS {
public:
    void Init(float yaw, float pitch, float fovy);
    void ApplyMouseLook(Vector2 mouseDelta, float sensitivity);
    void SetLookAngles(float yaw, float pitch);
    void ClampYawAround(float centerYaw, float halfRange, float minPitch, float maxPitch);
    void Update(float dt, Vector3 feetPosition, const CameraMotion& motion);
    void SetJitter(float amount) { m_jitter = amount; }

    const Camera3D& GetCamera() const { return m_camera; }
    Camera3D& GetCameraMutable() { return m_camera; }
    Vector3 GetForward() const;
    Vector3 GetFlatForward() const;
    Vector3 GetRight() const;
    Vector3 GetUp() const;
    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    float GetBobPhase() const { return m_bobPhase; }
    float GetEyeHeight() const { return m_eyeHeight; }
    bool PopFootstep();

private:
    Camera3D m_camera{};
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_roll = 0.0f;
    float m_bobPhase = 0.0f;
    float m_bobAmount = 0.0f;
    float m_eyeHeight = 1.62f;
    float m_jitter = 0.0f;
    float m_jitterTime = 0.0f;
    bool m_footstep = false;
    float m_lastStepPhase = 0.0f;
};

}
