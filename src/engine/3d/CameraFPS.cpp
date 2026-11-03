#include "engine/3d/CameraFPS.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace engine::camera3d {

namespace {
constexpr float PITCH_LIMIT = 85.0f * DEG2RAD;
constexpr float STAND_EYE = 1.62f;
constexpr float CROUCH_EYE = 0.98f;

float WrapAngle(float a) {
    while (a > PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}
}

void CameraFPS::Init(float yaw, float pitch, float fovy) {
    m_yaw = yaw;
    m_pitch = pitch;
    m_roll = 0.0f;
    m_bobPhase = 0.0f;
    m_bobAmount = 0.0f;
    m_eyeHeight = STAND_EYE;
    m_camera = Camera3D{};
    m_camera.fovy = fovy;
    m_camera.projection = CAMERA_PERSPECTIVE;
    m_camera.up = Vector3{0.0f, 1.0f, 0.0f};
}

void CameraFPS::ApplyMouseLook(Vector2 mouseDelta, float sensitivity) {
    float dx = std::clamp(mouseDelta.x, -120.0f, 120.0f);
    float dy = std::clamp(mouseDelta.y, -120.0f, 120.0f);
    m_yaw = WrapAngle(m_yaw + dx * sensitivity);
    m_pitch = std::clamp(m_pitch - dy * sensitivity, -PITCH_LIMIT, PITCH_LIMIT);
}

void CameraFPS::SetLookAngles(float yaw, float pitch) {
    m_yaw = WrapAngle(yaw);
    m_pitch = std::clamp(pitch, -PITCH_LIMIT, PITCH_LIMIT);
}

void CameraFPS::ClampYawAround(float centerYaw, float halfRange, float minPitch, float maxPitch) {
    float delta = WrapAngle(m_yaw - centerYaw);
    delta = std::clamp(delta, -halfRange, halfRange);
    m_yaw = WrapAngle(centerYaw + delta);
    m_pitch = std::clamp(m_pitch, minPitch, maxPitch);
}

Vector3 CameraFPS::GetForward() const {
    float cp = std::cos(m_pitch);
    return Vector3{std::sin(m_yaw) * cp, std::sin(m_pitch), -std::cos(m_yaw) * cp};
}

Vector3 CameraFPS::GetFlatForward() const {
    return Vector3{std::sin(m_yaw), 0.0f, -std::cos(m_yaw)};
}

Vector3 CameraFPS::GetRight() const {
    return Vector3{std::cos(m_yaw), 0.0f, std::sin(m_yaw)};
}

Vector3 CameraFPS::GetUp() const {
    return Vector3Normalize(Vector3CrossProduct(GetRight(), GetForward()));
}

bool CameraFPS::PopFootstep() {
    bool s = m_footstep;
    m_footstep = false;
    return s;
}

void CameraFPS::Update(float dt, Vector3 feetPosition, const CameraMotion& motion) {
    float targetEye = motion.crouched ? CROUCH_EYE : STAND_EYE;
    m_eyeHeight += (targetEye - m_eyeHeight) * std::min(1.0f, dt * 10.0f);

    float speed = motion.grounded ? motion.horizontalSpeed : 0.0f;
    float targetBob = std::clamp(speed / 4.5f, 0.0f, 1.0f);
    m_bobAmount += (targetBob - m_bobAmount) * std::min(1.0f, dt * 8.0f);
    if (speed > 0.15f) {
        float stride = motion.sprinting ? 1.45f : (motion.crouched ? 0.85f : 1.15f);
        m_bobPhase += dt * speed / stride * PI;
        float stepPhase = std::floor(m_bobPhase / PI);
        if (stepPhase != m_lastStepPhase) {
            m_lastStepPhase = stepPhase;
            m_footstep = true;
        }
    }

    float targetRoll = -motion.strafe * 0.018f;
    m_roll += (targetRoll - m_roll) * std::min(1.0f, dt * 8.0f);

    m_jitterTime += dt;
    float jx = m_jitter * (std::sin(m_jitterTime * 37.0f) * 0.6f + std::sin(m_jitterTime * 91.0f) * 0.4f);
    float jy = m_jitter * (std::cos(m_jitterTime * 43.0f) * 0.6f + std::sin(m_jitterTime * 73.0f) * 0.4f);

    float bobY = std::abs(std::sin(m_bobPhase)) * 0.045f * m_bobAmount;
    float bobX = std::cos(m_bobPhase) * 0.025f * m_bobAmount;

    Vector3 right = GetRight();
    Vector3 eye{
        feetPosition.x + right.x * bobX,
        feetPosition.y + m_eyeHeight + bobY,
        feetPosition.z + right.z * bobX,
    };

    float yaw = m_yaw + jx * 0.02f;
    float pitch = std::clamp(m_pitch + jy * 0.02f, -PITCH_LIMIT, PITCH_LIMIT);
    float cp = std::cos(pitch);
    Vector3 fwd{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    Vector3 worldUp{0.0f, 1.0f, 0.0f};
    Vector3 up = Vector3RotateByAxisAngle(worldUp, fwd, m_roll + std::sin(m_bobPhase) * 0.004f * m_bobAmount);

    m_camera.position = eye;
    m_camera.target = Vector3Add(eye, fwd);
    m_camera.up = up;
}

}
