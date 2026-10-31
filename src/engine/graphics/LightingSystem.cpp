#include "engine/graphics/LightingSystem.h"
#include <cmath>

namespace engine::graphics {

void LightingSystem::Init(int nativeWidth, int nativeHeight) {
    m_lightMap = LoadRenderTexture(nativeWidth, nativeHeight);
    SetTextureFilter(m_lightMap.texture, TEXTURE_FILTER_POINT);
}

void LightingSystem::Shutdown() {
    if (m_lightMap.id != 0) {
        UnloadRenderTexture(m_lightMap);
        m_lightMap = {};
    }
}

void LightingSystem::ClearLights() {
    m_pointLights.clear();
}

void LightingSystem::AddPointLight(const PointLight& light) {
    for (auto& l : m_pointLights) {
        if (l.id == light.id) {
            l = light;
            return;
        }
    }
    m_pointLights.push_back(light);
}

void LightingSystem::SetPointLightEnabled(const std::string& id, bool enabled) {
    for (auto& l : m_pointLights) {
        if (l.id == id) {
            l.enabled = enabled;
        }
    }
}

void LightingSystem::SetAllPointLightsFlicker(float hz, float amp) {
    for (auto& l : m_pointLights) {
        l.flickerHz = hz;
        l.flickerAmp = amp;
    }
}

void LightingSystem::RenderLightMap(float timeSeconds) {
    BeginTextureMode(m_lightMap);
    ClearBackground(m_ambientColor);

    BeginBlendMode(BLEND_ADDITIVE);

    for (size_t i = 0; i < m_pointLights.size(); ++i) {
        const auto& l = m_pointLights[i];
        if (!l.enabled || l.intensity <= 0.0f) continue;

        float flicker = 1.0f;
        if (l.flickerHz > 0.0f && l.flickerAmp > 0.0f) {
            float phase = timeSeconds * l.flickerHz + static_cast<float>(i) * 1.7f;
            flicker = 1.0f - l.flickerAmp * (0.5f + 0.5f * std::sin(phase));
        }

        float effIntensity = math::Clamp(l.intensity * flicker, 0.0f, 1.5f);
        Color innerCol = l.color;
        innerCol.a = static_cast<unsigned char>(math::Clamp(190.0f * effIntensity, 0.0f, 255.0f));
        Color outerCol = l.color;
        outerCol.a = 0;

        Vector2 center = (l.position - m_cameraOffset).SnapToPixel();
        DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), l.radius, innerCol, outerCol);
        Color coreCol = innerCol;
        coreCol.a = static_cast<unsigned char>(math::Clamp(95.0f * effIntensity, 0.0f, 255.0f));
        DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), l.radius * 0.45f, coreCol, outerCol);
    }

    if (m_flashlight.enabled && m_flashlight.intensity > 0.0f) {
        Vector2 origin = (m_flashlight.position - m_cameraOffset).SnapToPixel();
        float startAng = m_flashlight.directionDeg - m_flashlight.fovDeg * 0.5f;
        float endAng = m_flashlight.directionDeg + m_flashlight.fovDeg * 0.5f;

        constexpr int LAYERS = 6;
        for (int step = LAYERS; step >= 1; --step) {
            float t = static_cast<float>(step) / static_cast<float>(LAYERS);
            float r = m_flashlight.range * t;
            Color c = m_flashlight.color;
            float alpha = (1.0f - (t - 1.0f / LAYERS)) * 52.0f * m_flashlight.intensity;
            c.a = static_cast<unsigned char>(math::Clamp(alpha, 0.0f, 255.0f));
            DrawCircleSector(origin, r, startAng, endAng, 16, c);
        }
        Color halo = m_flashlight.color;
        halo.a = 90;
        Color zero = halo;
        zero.a = 0;
        DrawCircleGradient(static_cast<int>(origin.x), static_cast<int>(origin.y), 20.0f, halo, zero);
    }

    EndBlendMode();
    EndTextureMode();
}

}
