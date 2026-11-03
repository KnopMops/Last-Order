#pragma once
#include "raylib.h"
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace engine::render3d {

struct BakeLight {
    Vector3 position{};
    Vector3 color{1.0f, 1.0f, 1.0f};
    float radius = 5.0f;
    bool powerGroup = false;
};

struct BakeSample {
    Vector3 base{};
    Vector3 power{};
    float ao = 1.0f;
};

class LightBaker {
public:
    using DistanceQuery = std::function<float(Vector3 origin, Vector3 direction, float maxDistance)>;

    LightBaker(DistanceQuery query, std::vector<BakeLight> lights, Vector3 skyLight);

    BakeSample Sample(Vector3 position, Vector3 normal);
    size_t CacheSize() const { return m_cache.size(); }

private:
    BakeSample Compute(Vector3 position, Vector3 normal) const;

    DistanceQuery m_query;
    std::vector<BakeLight> m_lights;
    Vector3 m_skyLight{};
    std::unordered_map<uint64_t, BakeSample> m_cache;
};

}
