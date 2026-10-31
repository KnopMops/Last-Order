#pragma once
#include "engine/math/Math2D.h"
#include "raylib.h"
#include <string>
#include <vector>

namespace engine::graphics {

struct PointLight {
    std::string id;
    math::Vec2 position;
    float radius = 64.0f;
    Color color = Color{244, 184, 96, 255};
    float intensity = 1.0f;
    float flickerHz = 0.0f;
    float flickerAmp = 0.0f;
    bool enabled = true;
};

struct ConeLight {
    math::Vec2 position;
    float directionDeg = 90.0f;
    float fovDeg = 65.0f;
    float range = 96.0f;
    Color color = Color{216, 230, 242, 255};
    float intensity = 1.0f;
    bool enabled = false;
};

class LightingSystem {
public:
    void Init(int nativeWidth, int nativeHeight);
    void Shutdown();

    void ClearLights();
    void AddPointLight(const PointLight& light);
    void SetPointLightEnabled(const std::string& id, bool enabled);
    void SetAllPointLightsFlicker(float hz, float amp);

    void SetAmbientColor(Color ambient) { m_ambientColor = ambient; }
    Color GetAmbientColor() const { return m_ambientColor; }

    void SetCameraOffset(math::Vec2 offset) { m_cameraOffset = offset; }
    math::Vec2 GetCameraOffset() const { return m_cameraOffset; }

    ConeLight& GetFlashlight() { return m_flashlight; }
    const ConeLight& GetFlashlight() const { return m_flashlight; }

    void RenderLightMap(float timeSeconds);

    const RenderTexture2D& GetLightMap() const { return m_lightMap; }

private:
    RenderTexture2D m_lightMap{};
    Color m_ambientColor = Color{115, 98, 92, 255};
    math::Vec2 m_cameraOffset = {0.0f, 0.0f};
    std::vector<PointLight> m_pointLights;
    ConeLight m_flashlight{};
};

}
