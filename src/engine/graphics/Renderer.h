#pragma once
#include "engine/core/SettingsSystem.h"
#include "engine/graphics/LightingSystem.h"
#include "engine/math/Math2D.h"
#include "raylib.h"
#include <string>

namespace engine::graphics {

class Renderer {
public:
    static constexpr int NATIVE_W = 320;
    static constexpr int NATIVE_H = 180;
    static constexpr int UI_W = 1280;
    static constexpr int UI_H = 720;

    void Init(int windowWidth, int windowHeight, const std::string& crtShaderPath);
    void Shutdown();

    void Update(float dt, float totalTime, const core::GameSettings& settings);

    void BeginWorldPass();
    void EndWorldPass(LightingSystem& lighting, float totalTime);

    void BeginUIPass();
    void EndUIPassAndPresent();

    math::Vec2 GetVirtualMousePos() const;

    void TriggerShake(float amplitudePx, float durationSec);
    void TriggerFlash(Color color, float durationSec);
    void TriggerScreamerOverlay(Texture2D tex, float durationSec, bool invertBrief = false);
    void SetSanityStress(float stress) { m_sanityStress = math::Clamp(stress, 0.0f, 1.0f); }
    float GetSanityStress() const { return m_sanityStress; }

    void SetSkip2DLightmap(bool skip) { m_skip2DLightmap = skip; }
    bool IsSkip2DLightmap() const { return m_skip2DLightmap; }

    bool IsScreamerActive() const { return m_screamerTimer > 0.0f; }
    void SetCrtEnabled(bool enabled) { m_crtEnabled = enabled; }
    bool IsCrtEnabled() const { return m_crtEnabled; }

    int GetUIWidth() const { return UI_W; }
    int GetUIHeight() const { return UI_H; }

private:
    Rectangle ComputeTargetViewport() const;

    RenderTexture2D m_sceneRT{};
    RenderTexture2D m_compositeRT{};
    RenderTexture2D m_uiRT{};

    Shader m_crtShader{};
    int m_uTimeLoc = -1;
    int m_uInvertLoc = -1;
    int m_uDistortionLoc = -1;
    int m_uScanlineLoc = -1;
    int m_uChromaLoc = -1;
    int m_uBrightnessLoc = -1;
    int m_uSanityStressLoc = -1;

    bool m_crtEnabled = true;
    bool m_shakeAllowed = true;
    bool m_stretchToUltrawide = true;
    bool m_skip2DLightmap = false;
    float m_sanityStress = 0.0f;

    float m_shakeAmplitude = 0.0f;
    float m_shakeTimer = 0.0f;
    math::Vec2 m_shakeOffset = {0.0f, 0.0f};

    Color m_flashColor = WHITE;
    float m_flashDuration = 0.0f;
    float m_flashTimer = 0.0f;

    Texture2D m_screamerTex{};
    float m_screamerDuration = 2.1f;
    float m_screamerTimer = 0.0f;
    float m_invertTimer = 0.0f;
    float m_aftershockTimer = 0.0f;
};

}
