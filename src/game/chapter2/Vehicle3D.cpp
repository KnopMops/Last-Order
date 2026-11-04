#include "game/chapter2/Vehicle3D.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace game::chapter2 {

namespace {

constexpr float MAX_SPEED = 41.0f;
constexpr float MAX_REVERSE = 8.5f;
constexpr float ENGINE_ACCEL = 8.2f;
constexpr float BRAKE_DECEL = 12.0f;
constexpr float GRIP = 9.5f;
constexpr float HANDBRAKE_GRIP = 1.3f;
constexpr float RAY_HEIGHT = 0.55f;
constexpr float GEAR_TOPS[] = {0.0f, 11.0f, 19.0f, 27.0f, 35.0f, 44.0f};

float Approach(float value, float target, float rate) {
    if (value < target) return std::min(target, value + rate);
    return std::max(target, value - rate);
}

}

void Vehicle3D::Setup(Vector3 origin, float heading, Vector3 localMin, Vector3 localMax,
                      const engine::physics3d::Physics3D& physics) {
    m_spawnHeading = heading;
    float c = std::cos(heading);
    float s = std::sin(heading);
    float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
    for (int i = 0; i < 4; ++i) {
        float x = (i & 1) ? localMax.x : localMin.x;
        float z = (i & 2) ? localMax.z : localMin.z;
        float lx = x * c + z * s;
        float lz = -x * s + z * c;
        minX = std::min(minX, lx);
        maxX = std::max(maxX, lx);
        minZ = std::min(minZ, lz);
        maxZ = std::max(maxZ, lz);
    }
    m_halfLength = std::max(0.8f, (maxX - minX) * 0.5f);
    m_halfWidth = std::max(0.5f, (maxZ - minZ) * 0.5f);
    m_centerX = (maxX + minX) * 0.5f;
    m_centerZ = (maxZ + minZ) * 0.5f;
    m_wheelBase = m_halfLength * 1.32f;
    m_track = m_halfWidth * 1.65f;
    m_groundOffset = std::max(0.2f, -localMin.y);
    Reset(origin, heading, physics);
    float dist = physics.CastStaticDistance(Vector3Add(origin, Vector3{0.0f, 0.2f, 0.0f}), Vector3{0.0f, -1.0f, 0.0f}, 6.0f);
    if (dist < 5.9f) m_groundOffset = std::max(0.2f, dist - 0.2f);
    Reset(origin, heading, physics);
}

void Vehicle3D::Reset(Vector3 origin, float heading, const engine::physics3d::Physics3D& physics) {
    m_pos = origin;
    m_heading = heading;
    m_vel = Vector3{0.0f, 0.0f, 0.0f};
    m_yawRate = 0.0f;
    m_steer = 0.0f;
    m_steerAngle = 0.0f;
    m_pitch = m_roll = m_bodyPitch = m_bodyRoll = 0.0f;
    m_impact = 0.0f;
    m_skid = false;
    m_gear = 1;
    m_rpm = 0.0f;
    m_lastForwardSpeed = 0.0f;
    m_groundY = origin.y - m_groundOffset;
    UpdateBasis();
    SampleGround(physics, 0.0f, true);
    UpdateBasis();
}

float Vehicle3D::ForwardSpeed() const {
    return m_vel.x * std::cos(m_heading) + m_vel.z * std::sin(m_heading);
}

float Vehicle3D::Speed() const {
    return std::sqrt(m_vel.x * m_vel.x + m_vel.z * m_vel.z);
}

float Vehicle3D::TakeImpact() {
    float v = m_impact;
    m_impact = 0.0f;
    return v;
}

void Vehicle3D::Kick(Vector3 velocityDelta, float spin) {
    m_vel = Vector3Add(m_vel, Vector3{velocityDelta.x, 0.0f, velocityDelta.z});
    m_yawRate += spin;
}

Vector3 Vehicle3D::LocalToWorld(Vector3 local) const {
    return Vector3Add(m_pos, Vector3Add(Vector3Add(Vector3Scale(m_fwd, local.x), Vector3Scale(m_up, local.y)), Vector3Scale(m_right, local.z)));
}

void Vehicle3D::UpdateBasis() {
    Vector3 f0{std::cos(m_heading), 0.0f, std::sin(m_heading)};
    Vector3 r0{-std::sin(m_heading), 0.0f, std::cos(m_heading)};
    Vector3 u0{0.0f, 1.0f, 0.0f};
    float p = m_pitch + m_bodyPitch;
    float r = m_roll + m_bodyRoll;
    Vector3 f = Vector3Add(Vector3Scale(f0, std::cos(p)), Vector3Scale(u0, std::sin(p)));
    Vector3 u = Vector3Subtract(Vector3Scale(u0, std::cos(p)), Vector3Scale(f0, std::sin(p)));
    Vector3 rr = Vector3Add(Vector3Scale(r0, std::cos(r)), Vector3Scale(u, std::sin(r)));
    u = Vector3Normalize(Vector3CrossProduct(rr, f));
    rr = Vector3Normalize(Vector3CrossProduct(f, u));
    m_fwd = Vector3Normalize(f);
    m_up = u;
    m_right = rr;
}

Matrix Vehicle3D::Transform() const {
    Matrix basis{};
    basis.m0 = m_fwd.x;   basis.m1 = m_fwd.y;   basis.m2 = m_fwd.z;
    basis.m4 = m_up.x;    basis.m5 = m_up.y;    basis.m6 = m_up.z;
    basis.m8 = m_right.x; basis.m9 = m_right.y; basis.m10 = m_right.z;
    basis.m12 = m_pos.x;  basis.m13 = m_pos.y;  basis.m14 = m_pos.z;
    basis.m15 = 1.0f;
    return MatrixMultiply(MatrixRotateY(m_spawnHeading), basis);
}

void Vehicle3D::SampleGround(const engine::physics3d::Physics3D& physics, float dt, bool snap) {
    Vector3 f0{std::cos(m_heading), 0.0f, std::sin(m_heading)};
    Vector3 r0{-std::sin(m_heading), 0.0f, std::cos(m_heading)};
    float halfBase = m_wheelBase * 0.5f;
    float halfTrack = m_track * 0.5f;
    float h[4];
    int hits = 0;
    float sum = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float lx = m_centerX + ((i < 2) ? halfBase : -halfBase);
        float lz = m_centerZ + ((i % 2 == 0) ? -halfTrack : halfTrack);
        Vector3 p = Vector3Add(Vector3Add(Vector3{m_pos.x, m_groundY + 1.4f, m_pos.z}, Vector3Scale(f0, lx)), Vector3Scale(r0, lz));
        float d = physics.CastStaticDistance(p, Vector3{0.0f, -1.0f, 0.0f}, 4.0f);
        if (d < 3.99f) {
            h[i] = p.y - d;
            sum += h[i];
            ++hits;
        } else {
            h[i] = m_groundY;
        }
    }
    if (hits == 0) return;
    float avg = sum / static_cast<float>(hits);
    float front = (h[0] + h[1]) * 0.5f;
    float rear = (h[2] + h[3]) * 0.5f;
    float left = (h[0] + h[2]) * 0.5f;
    float right = (h[1] + h[3]) * 0.5f;
    float targetPitch = std::atan2(front - rear, m_wheelBase);
    float targetRoll = std::atan2(right - left, m_track);
    if (snap) {
        m_groundY = avg;
        m_pitch = targetPitch;
        m_roll = targetRoll;
    } else {
        float k = 1.0f - std::exp(-dt * 14.0f);
        m_groundY += (avg - m_groundY) * k;
        m_pitch += (targetPitch - m_pitch) * k;
        m_roll += (targetRoll - m_roll) * k;
    }
    m_pos.y = m_groundY + m_groundOffset;
}

void Vehicle3D::Update(float dt, const VehicleInput& input, const engine::physics3d::Physics3D& physics) {
    if (dt <= 0.0f) return;
    Vector3 f0{std::cos(m_heading), 0.0f, std::sin(m_heading)};
    Vector3 r0{-std::sin(m_heading), 0.0f, std::cos(m_heading)};
    float vF = m_vel.x * f0.x + m_vel.z * f0.z;
    float vR = m_vel.x * r0.x + m_vel.z * r0.z;
    float speed = std::abs(vF);

    float steerRate = (std::abs(input.steer) > 0.01f) ? 3.2f : 4.5f;
    m_steer = Approach(m_steer, std::clamp(input.steer, -1.0f, 1.0f), steerRate * dt);
    float maxSteer = Lerp(0.62f, 0.11f, std::clamp(speed / 36.0f, 0.0f, 1.0f));
    m_steerAngle = m_steer * maxSteer;

    float accel = 0.0f;
    float throttle = input.engineAlive ? std::clamp(input.throttle, 0.0f, 1.0f) : 0.0f;
    float brake = std::clamp(input.brake, 0.0f, 1.0f);
    if (throttle > 0.0f) {
        if (vF < -0.4f) {
            accel += BRAKE_DECEL * throttle;
        } else {
            float t = std::clamp(vF / MAX_SPEED, 0.0f, 1.0f);
            accel += ENGINE_ACCEL * throttle * (1.0f - t * t) * (vF < 6.0f ? 1.15f : 1.0f);
        }
    }
    if (brake > 0.0f) {
        if (vF > 0.4f) {
            accel -= BRAKE_DECEL * brake;
        } else if (input.engineAlive && vF > -MAX_REVERSE) {
            accel -= 4.6f * brake;
        }
    }
    float rolling = 0.35f + (throttle <= 0.0f ? 1.1f : 0.0f);
    float drag = rolling + 0.0016f * vF * vF;
    if (input.handbrake) drag += 5.5f;
    if (std::abs(vF) > 0.05f) {
        float d = std::min(std::abs(vF), drag * dt);
        vF -= (vF > 0.0f ? d : -d);
    } else if (throttle <= 0.0f && brake <= 0.0f) {
        vF = 0.0f;
    }
    vF += accel * dt;
    if (std::abs(accel) < 0.01f && std::abs(vF) < 0.08f) vF = 0.0f;
    vF = std::clamp(vF, -MAX_REVERSE, MAX_SPEED);

    float grip = input.handbrake ? HANDBRAKE_GRIP : GRIP;
    if (speed > 22.0f && std::abs(m_steer) > 0.75f) grip *= 0.6f;
    vR *= std::exp(-grip * dt);

    float targetYaw = vF * std::tan(m_steerAngle) / m_wheelBase;
    if (input.handbrake && speed > 4.0f) targetYaw *= 1.45f;
    m_yawRate += (targetYaw - m_yawRate) * std::min(1.0f, dt * (input.handbrake ? 3.0f : 9.0f));
    m_skid = (std::abs(vR) > 2.6f) || (input.handbrake && speed > 7.0f) || (brake > 0.5f && vF > 14.0f);

    float longAccel = (vF - m_lastForwardSpeed) / dt;
    m_lastForwardSpeed = vF;
    m_bodyPitch += (std::clamp(longAccel * 0.004f, -0.05f, 0.05f) - m_bodyPitch) * std::min(1.0f, dt * 6.0f);
    m_bodyRoll += (std::clamp(m_yawRate * vF * 0.0028f, -0.06f, 0.06f) - m_bodyRoll) * std::min(1.0f, dt * 5.0f);

    float travel = (std::abs(vF) + std::abs(vR)) * dt;
    int steps = std::clamp(static_cast<int>(std::ceil(travel / 0.25f)), 1, 8);
    float sub = dt / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        m_heading += m_yawRate * sub;
        Vector3 f{std::cos(m_heading), 0.0f, std::sin(m_heading)};
        Vector3 r{-std::sin(m_heading), 0.0f, std::cos(m_heading)};
        m_vel = Vector3Add(Vector3Scale(f, vF), Vector3Scale(r, vR));
        m_pos = Vector3Add(m_pos, Vector3Scale(m_vel, sub));
        ResolveCollisions(physics);
        vF = m_vel.x * f.x + m_vel.z * f.z;
        vR = m_vel.x * r.x + m_vel.z * r.z;
    }
    m_heading = std::remainder(m_heading, 2.0f * PI);
    SampleGround(physics, dt, false);
    UpdateBasis();

    float absV = std::abs(vF);
    if (vF < -0.3f) {
        m_gear = -1;
        m_rpm = std::clamp(absV / MAX_REVERSE, 0.0f, 1.0f);
    } else {
        int g = 1;
        while (g < 5 && absV > GEAR_TOPS[g]) ++g;
        m_gear = g;
        float lo = GEAR_TOPS[g - 1] * 0.82f;
        m_rpm = std::clamp((absV - lo) / std::max(1.0f, GEAR_TOPS[g] - lo), 0.0f, 1.0f);
    }
    m_engineLoad += (throttle - m_engineLoad) * std::min(1.0f, dt * 5.0f);
}

void Vehicle3D::ResolveCollisions(const engine::physics3d::Physics3D& physics) {
    Vector3 f{std::cos(m_heading), 0.0f, std::sin(m_heading)};
    Vector3 r{-std::sin(m_heading), 0.0f, std::cos(m_heading)};
    float y = m_groundY + RAY_HEIGHT;
    for (int pass = 0; pass < 2; ++pass) {
        Vector3 center = Vector3Add(Vector3Add(Vector3{m_pos.x, y, m_pos.z}, Vector3Scale(f, m_centerX)), Vector3Scale(r, m_centerZ));
        bool any = false;
        auto probe = [&](float lx, float lz) {
            Vector3 target = Vector3Add(Vector3Add(center, Vector3Scale(f, lx)), Vector3Scale(r, lz));
            Vector3 delta = Vector3Subtract(target, center);
            float d = Vector3Length(delta);
            if (d < 0.01f) return;
            Vector3 dir = Vector3Scale(delta, 1.0f / d);
            engine::physics3d::RayHit hit = physics.CastStaticRay(center, dir, d);
            if (!hit.hit || hit.distance >= d) return;
            Vector3 n{hit.normal.x, 0.0f, hit.normal.z};
            float nl = Vector3Length(n);
            n = nl > 0.2f ? Vector3Scale(n, 1.0f / nl) : Vector3Negate(dir);
            if (Vector3DotProduct(n, dir) > -0.05f) n = Vector3Negate(dir);
            float pen = d - hit.distance;
            float push = pen / std::max(0.25f, -Vector3DotProduct(n, dir));
            m_pos = Vector3Add(m_pos, Vector3Scale(n, std::min(push, 0.6f) + 0.005f));
            center = Vector3Add(center, Vector3Scale(n, std::min(push, 0.6f) + 0.005f));
            float vn = Vector3DotProduct(m_vel, n);
            if (vn < 0.0f) {
                m_impact = std::max(m_impact, -vn);
                m_vel = Vector3Subtract(m_vel, Vector3Scale(n, vn * 1.2f));
                m_vel = Vector3Scale(m_vel, 0.97f);
                float side = Vector3DotProduct(Vector3Subtract(target, center), r);
                m_yawRate = m_yawRate * 0.5f + std::clamp(-vn * 0.05f * (side > 0.0f ? 1.0f : -1.0f) * (lx > 0.0f ? 1.0f : -1.0f), -1.5f, 1.5f);
            }
            any = true;
        };
        const float hl = m_halfLength;
        const float hw = m_halfWidth;
        for (int i = 0; i <= 6; ++i) probe(hl, -hw + 2.0f * hw * static_cast<float>(i) / 6.0f);
        for (int i = 0; i <= 4; ++i) probe(-hl, -hw + 2.0f * hw * static_cast<float>(i) / 4.0f);
        for (int i = 1; i <= 5; ++i) {
            float lx = -hl + 2.0f * hl * static_cast<float>(i) / 6.0f;
            probe(lx, hw);
            probe(lx, -hw);
        }
        if (!any) break;
    }
}

}
