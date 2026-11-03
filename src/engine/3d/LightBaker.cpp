#include "engine/3d/LightBaker.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace engine::render3d {

namespace {

uint64_t HashSample(Vector3 p, Vector3 n) {
    auto q = [](float v, float step) { return static_cast<int64_t>(std::llround(v / step)); };
    uint64_t h = 1469598103934665603ull;
    const int64_t parts[6] = {q(p.x, 0.01f), q(p.y, 0.01f), q(p.z, 0.01f), q(n.x, 0.05f), q(n.y, 0.05f), q(n.z, 0.05f)};
    for (int64_t part : parts) {
        h ^= static_cast<uint64_t>(part) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    return h;
}

void TangentBasis(Vector3 n, Vector3& t, Vector3& b) {
    Vector3 ref = std::abs(n.y) < 0.9f ? Vector3{0.0f, 1.0f, 0.0f} : Vector3{1.0f, 0.0f, 0.0f};
    t = Vector3Normalize(Vector3CrossProduct(ref, n));
    b = Vector3CrossProduct(n, t);
}

}

LightBaker::LightBaker(DistanceQuery query, std::vector<BakeLight> lights, Vector3 skyLight)
    : m_query(std::move(query)), m_lights(std::move(lights)), m_skyLight(skyLight) {}

BakeSample LightBaker::Sample(Vector3 position, Vector3 normal) {
    uint64_t key = HashSample(position, normal);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) return it->second;
    BakeSample s = Compute(position, normal);
    m_cache.emplace(key, s);
    return s;
}

BakeSample LightBaker::Compute(Vector3 position, Vector3 normal) const {
    BakeSample out;
    Vector3 origin = Vector3Add(position, Vector3Scale(normal, 0.03f));

    Vector3 t, b;
    TangentBasis(normal, t, b);
    constexpr int AO_RAYS = 8;
    constexpr float AO_LENGTH = 0.75f;
    float occlusion = 0.0f;
    for (int i = 0; i < AO_RAYS; ++i) {
        float angle = (static_cast<float>(i) + 0.5f) * (2.0f * PI / AO_RAYS);
        float elevation = (i % 2 == 0) ? 0.35f : 0.75f;
        float horizontal = std::sqrt(1.0f - elevation * elevation);
        Vector3 dir = Vector3Add(Vector3Add(Vector3Scale(t, std::cos(angle) * horizontal),
                                            Vector3Scale(b, std::sin(angle) * horizontal)),
                                 Vector3Scale(normal, elevation));
        float hit = m_query(origin, Vector3Normalize(dir), AO_LENGTH);
        occlusion += 1.0f - std::clamp(hit / AO_LENGTH, 0.0f, 1.0f);
    }
    out.ao = std::clamp(1.0f - 0.85f * occlusion / AO_RAYS, 0.2f, 1.0f);

    float skyVisible = 0.0f;
    const Vector3 skyDirs[4] = {
        {0.0f, 1.0f, 0.0f}, {0.45f, 0.89f, 0.0f}, {-0.45f, 0.89f, 0.0f}, {0.0f, 0.89f, 0.45f},
    };
    for (const Vector3& d : skyDirs) {
        if (m_query(origin, Vector3Normalize(d), 40.0f) >= 39.9f) skyVisible += 0.25f;
    }
    float skyFacing = 0.35f + 0.65f * std::max(0.0f, normal.y * 0.5f + 0.5f);
    out.base = Vector3Scale(m_skyLight, skyVisible * skyFacing * out.ao);

    for (const BakeLight& light : m_lights) {
        Vector3 toLight = Vector3Subtract(light.position, position);
        float dist = Vector3Length(toLight);
        if (dist >= light.radius || dist < 0.001f) continue;
        Vector3 l = Vector3Scale(toLight, 1.0f / dist);
        float ndl = Vector3DotProduct(normal, l);
        if (ndl <= 0.0f) continue;
        float hit = m_query(origin, l, dist - 0.06f);
        if (hit < dist - 0.07f) continue;
        float att = 1.0f - (dist * dist) / (light.radius * light.radius);
        att *= att;
        float wrapped = ndl * 0.8f + 0.2f;
        Vector3 c = Vector3Scale(light.color, att * wrapped * (0.55f + 0.45f * out.ao));
        if (light.powerGroup) {
            out.power = Vector3Add(out.power, c);
        } else {
            out.base = Vector3Add(out.base, c);
        }
    }
    return out;
}

}
