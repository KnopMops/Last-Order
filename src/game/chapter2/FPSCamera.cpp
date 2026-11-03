#include "game/chapter2/FPSCamera.h"
#include <cmath>
#include <algorithm>

namespace game::chapter2 {

void FPSCamera::Init(Vector3 startPos, float startYaw, float startPitch) {
    m_position = startPos;
    m_yaw = startYaw;
    m_pitch = startPitch;
    m_stamina = 100.0f;
    m_bobTimer = 0.0f;
    m_isCrouching = false;
    m_isSprinting = false;
    m_isMoving = false;

    m_camera.position = m_position;
    m_camera.up = Vector3{0.0f, 1.0f, 0.0f};
    m_camera.fovy = 68.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;
    SetLookAngle(m_yaw, m_pitch);
}

void FPSCamera::SetLookAngle(float yaw, float pitch) {
    m_yaw = yaw;
    m_pitch = std::clamp(pitch, -1.48f, 1.48f);
    Vector3 fwd = GetForward();
    m_camera.target = Vector3{
        m_camera.position.x + fwd.x,
        m_camera.position.y + fwd.y,
        m_camera.position.z + fwd.z
    };
}

Vector3 FPSCamera::GetForward() const {
    return Vector3{
        std::sin(m_yaw) * std::cos(m_pitch),
        std::sin(m_pitch),
        std::cos(m_yaw) * std::cos(m_pitch)
    };
}

Vector3 FPSCamera::GetRight() const {
    return Vector3{
        std::cos(m_yaw),
        0.0f,
        -std::sin(m_yaw)
    };
}

Ray FPSCamera::GetViewRay() const {
    return Ray{m_camera.position, GetForward()};
}

void FPSCamera::Update(float dt, const std::vector<BoundingBox>& colliders) {
    Vector2 md = GetMouseDelta();
    m_yaw += md.x * 0.0024f;
    m_pitch -= md.y * 0.0024f;
    m_pitch = std::clamp(m_pitch, -1.48f, 1.48f);

    Vector3 flatForward = Vector3{std::sin(m_yaw), 0.0f, std::cos(m_yaw)};
    Vector3 right = GetRight();

    Vector3 inputDir = {0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_W)) { inputDir.x += flatForward.x; inputDir.z += flatForward.z; }
    if (IsKeyDown(KEY_S)) { inputDir.x -= flatForward.x; inputDir.z -= flatForward.z; }
    if (IsKeyDown(KEY_D)) { inputDir.x -= right.x; inputDir.z -= right.z; }
    if (IsKeyDown(KEY_A)) { inputDir.x += right.x; inputDir.z += right.z; }

    float inputLen = std::sqrt(inputDir.x * inputDir.x + inputDir.z * inputDir.z);
    m_isMoving = (inputLen > 0.001f);

    if (m_isMoving) {
        inputDir.x /= inputLen;
        inputDir.z /= inputLen;
    }

    m_isCrouching = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C);
    m_isSprinting = (!m_isCrouching && IsKeyDown(KEY_LEFT_SHIFT) && m_isMoving && m_stamina > 4.0f);

    if (m_isSprinting) {
        m_stamina = std::max(0.0f, m_stamina - dt * 26.0f);
    } else {
        m_stamina = std::min(100.0f, m_stamina + dt * 18.0f);
    }

    float moveSpeed = m_isCrouching ? 1.6f : (m_isSprinting ? 5.2f : 3.1f);

    if (m_isMoving) {
        m_bobTimer += dt * (m_isSprinting ? 14.0f : 8.5f);
    } else {
        m_bobTimer = std::fmod(m_bobTimer, 6.283185f);
    }

    constexpr float playerRadius = 0.32f;
    constexpr float playerHeight = 1.75f;

    auto testCollision = [&](Vector3 pos) -> bool {
        BoundingBox pBox = {
            Vector3{pos.x - playerRadius, pos.y, pos.z - playerRadius},
            Vector3{pos.x + playerRadius, pos.y + playerHeight, pos.z + playerRadius}
        };
        for (const auto& c : colliders) {
            if (CheckCollisionBoxes(pBox, c)) return true;
        }
        return false;
    };

    Vector3 targetPos = m_position;
    if (m_isMoving) {
        Vector3 stepX = targetPos;
        stepX.x += inputDir.x * moveSpeed * dt;
        if (!testCollision(stepX)) {
            targetPos.x = stepX.x;
        }

        Vector3 stepZ = targetPos;
        stepZ.z += inputDir.z * moveSpeed * dt;
        if (!testCollision(stepZ)) {
            targetPos.z = stepZ.z;
        }
    }

    m_position = targetPos;

    float baseEyeY = m_isCrouching ? 0.95f : 1.62f;
    float bobOffset = m_isMoving ? (std::sin(m_bobTimer) * 0.042f) : 0.0f;
    m_camera.position = Vector3{m_position.x, m_position.y + baseEyeY + bobOffset, m_position.z};

    Vector3 lookDir = GetForward();
    m_camera.target = Vector3{
        m_camera.position.x + lookDir.x,
        m_camera.position.y + lookDir.y,
        m_camera.position.z + lookDir.z
    };
}

}
