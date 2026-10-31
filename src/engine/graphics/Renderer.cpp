#include "engine/graphics/Renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace engine::graphics {

void Renderer::Init(int, int, const std::string& crtShaderPath) {
    m_sceneRT = LoadRenderTexture(NATIVE_W, NATIVE_H);
    m_compositeRT = LoadRenderTexture(NATIVE_W, NATIVE_H);
    m_uiRT = LoadRenderTexture(UI_W, UI_H);

    SetTextureFilter(m_sceneRT.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_compositeRT.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_uiRT.texture, TEXTURE_FILTER_BILINEAR);

    m_crtShader = LoadShader(nullptr, crtShaderPath.c_str());
    m_uTimeLoc         = GetShaderLocation(m_crtShader, "uTime");
    m_uInvertLoc       = GetShaderLocation(m_crtShader, "uInvert");
    m_uDistortionLoc   = GetShaderLocation(m_crtShader, "uDistortion");
    m_uScanlineLoc     = GetShaderLocation(m_crtShader, "uScanline");
    m_uChromaLoc       = GetShaderLocation(m_crtShader, "uChroma");
    m_uBrightnessLoc   = GetShaderLocation(m_crtShader, "uBrightness");
    m_uSanityStressLoc = GetShaderLocation(m_crtShader, "uSanityStress");
}

void Renderer::Shutdown() {
    if (m_crtShader.id != 0) UnloadShader(m_crtShader);
    if (m_uiRT.id != 0) UnloadRenderTexture(m_uiRT);
    if (m_compositeRT.id != 0) UnloadRenderTexture(m_compositeRT);
    if (m_sceneRT.id != 0) UnloadRenderTexture(m_sceneRT);
}

void Renderer::Update(float dt, float totalTime, const core::GameSettings& settings) {
    m_crtEnabled = settings.crtEnabled;
    m_shakeAllowed = settings.screenShakeEnabled;
    m_stretchToUltrawide = settings.stretchToUltrawide;

    float effectiveStress = m_sanityStress;
    if (m_screamerTimer > 0.0f) {
        effectiveStress = 1.0f;
    }

    SetShaderValue(m_crtShader, m_uTimeLoc,         &totalTime,                  SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uDistortionLoc,   &settings.barrelDistortion,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uScanlineLoc,     &settings.scanlineIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uChromaLoc,       &settings.chromaAberration,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uBrightnessLoc,   &settings.brightness,        SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uSanityStressLoc, &effectiveStress,            SHADER_UNIFORM_FLOAT);

    bool strobeInvert = false;
    if (m_invertTimer > 0.0f && !settings.safeScreamerMode) {
        int tick = static_cast<int>(totalTime * 28.0f);
        strobeInvert = (tick % 3 == 0);
    }
    float inv = strobeInvert ? 1.0f : 0.0f;
    SetShaderValue(m_crtShader, m_uInvertLoc, &inv, SHADER_UNIFORM_FLOAT);
    if (m_invertTimer > 0.0f) m_invertTimer = std::max(0.0f, m_invertTimer - dt);

    if (m_shakeTimer > 0.0f) {
        m_shakeTimer = std::max(0.0f, m_shakeTimer - dt);
        if (m_shakeAllowed) {
            float rx = (static_cast<float>(std::rand() % 200) / 100.0f - 1.0f) * m_shakeAmplitude;
            float ry = (static_cast<float>(std::rand() % 200) / 100.0f - 1.0f) * m_shakeAmplitude;
            m_shakeOffset = math::Vec2{rx, ry}.SnapToPixel();
        } else {
            m_shakeOffset = {0.0f, 0.0f};
        }
    } else {
        m_shakeOffset = {0.0f, 0.0f};
    }

    if (m_flashTimer > 0.0f) m_flashTimer = std::max(0.0f, m_flashTimer - dt);
    if (m_screamerTimer > 0.0f) m_screamerTimer = std::max(0.0f, m_screamerTimer - dt);
}

Rectangle Renderer::ComputeTargetViewport() const {
    float sw = static_cast<float>(std::max(1, GetScreenWidth()));
    float sh = static_cast<float>(std::max(1, GetScreenHeight()));

    if (m_stretchToUltrawide) {
        return Rectangle{0.0f, 0.0f, sw, sh};
    }

    float scale = std::min(sw / static_cast<float>(UI_W), sh / static_cast<float>(UI_H));
    float vw = static_cast<float>(UI_W) * scale;
    float vh = static_cast<float>(UI_H) * scale;
    return Rectangle{(sw - vw) * 0.5f, (sh - vh) * 0.5f, vw, vh};
}

void Renderer::BeginWorldPass() {
    BeginTextureMode(m_sceneRT);
    ClearBackground(Color{10, 13, 26, 255});
}

void Renderer::EndWorldPass(LightingSystem& lighting, float totalTime) {
    EndTextureMode();

    if (!m_skip2DLightmap) {
        lighting.RenderLightMap(totalTime);
    }

    BeginTextureMode(m_compositeRT);
    ClearBackground(BLACK);

    Rectangle srcFlip = {0.0f, 0.0f, static_cast<float>(NATIVE_W), -static_cast<float>(NATIVE_H)};
    Vector2 shakePos = m_shakeOffset;

    DrawTextureRec(m_sceneRT.texture, srcFlip, shakePos, WHITE);

    if (!m_skip2DLightmap) {
        BeginBlendMode(BLEND_MULTIPLIED);
        DrawTextureRec(lighting.GetLightMap().texture, srcFlip, Vector2{0.0f, 0.0f}, WHITE);
        EndBlendMode();
    }

    if (m_screamerTimer > 0.0f && m_screamerTex.id != 0) {
        float progress = 1.0f - (m_screamerTimer / std::max(0.1f, m_screamerDuration));
        float zoom = 1.0f + progress * 0.42f + std::sin(totalTime * 55.0f) * 0.05f;
        float w = static_cast<float>(NATIVE_W) * zoom;
        float h = static_cast<float>(NATIVE_H) * zoom;
        float jx = (static_cast<float>(std::rand() % 17) - 8.0f) + shakePos.x;
        float jy = (static_cast<float>(std::rand() % 17) - 8.0f) + shakePos.y;
        Rectangle srcSc = {0.0f, 0.0f, static_cast<float>(m_screamerTex.width), static_cast<float>(m_screamerTex.height)};
        Rectangle dstSc = {(NATIVE_W - w) * 0.5f + jx, (NATIVE_H - h) * 0.5f + jy, w, h};
        Color tint = (static_cast<int>(totalTime * 35.0f) % 4 == 0) ? Color{255, 110, 110, 255} : WHITE;
        DrawTexturePro(m_screamerTex, srcSc, dstSc, Vector2{0.0f, 0.0f}, 0.0f, tint);

        for (int g = 0; g < 6; ++g) {
            int gy = std::rand() % NATIVE_H;
            DrawRectangle(0, gy, NATIVE_W, 2, Color{214, 40, 40, 145});
        }
        DrawRectangleLinesEx(Rectangle{0, 0, NATIVE_W, NATIVE_H}, 6.0f, Color{150, 12, 18, 220});
    }

    if (m_flashTimer > 0.0f && m_flashDuration > 0.0f && m_shakeAllowed) {
        float alpha = (m_flashTimer / m_flashDuration);
        Color c = m_flashColor;
        c.a = static_cast<unsigned char>(math::Clamp(alpha * static_cast<float>(m_flashColor.a), 0.0f, 255.0f));
        DrawRectangle(0, 0, NATIVE_W, NATIVE_H, c);
    }

    EndTextureMode();
}

void Renderer::BeginUIPass() {
    BeginTextureMode(m_uiRT);
    ClearBackground(Color{0, 0, 0, 0});
}

void Renderer::EndUIPassAndPresent() {
    EndTextureMode();

    Rectangle dstScreen = ComputeTargetViewport();

    BeginDrawing();
    ClearBackground(Color{6, 8, 16, 255});

    if (m_crtEnabled) BeginShaderMode(m_crtShader);
    Rectangle srcWorld = {0.0f, 0.0f, static_cast<float>(NATIVE_W), -static_cast<float>(NATIVE_H)};
    DrawTexturePro(m_compositeRT.texture, srcWorld, dstScreen, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    if (m_crtEnabled) EndShaderMode();

    Rectangle srcUI = {0.0f, 0.0f, static_cast<float>(UI_W), -static_cast<float>(UI_H)};
    DrawTexturePro(m_uiRT.texture, srcUI, dstScreen, Vector2{0.0f, 0.0f}, 0.0f, WHITE);

    EndDrawing();
}

math::Vec2 Renderer::GetVirtualMousePos() const {
    Vector2 raw = GetMousePosition();
    Rectangle vp = ComputeTargetViewport();
    float relX = (raw.x - vp.x) / std::max(1.0f, vp.width);
    float relY = (raw.y - vp.y) / std::max(1.0f, vp.height);
    return {
        relX * static_cast<float>(UI_W),
        relY * static_cast<float>(UI_H)
    };
}

void Renderer::TriggerShake(float amplitudePx, float durationSec) {
    m_shakeAmplitude = amplitudePx;
    m_shakeTimer = durationSec;
}

void Renderer::TriggerFlash(Color color, float durationSec) {
    m_flashColor = color;
    m_flashDuration = durationSec;
    m_flashTimer = durationSec;
}

void Renderer::TriggerScreamerOverlay(Texture2D tex, float durationSec, bool invertBrief) {
    m_screamerTex = tex;
    m_screamerDuration = std::max(0.5f, durationSec);
    m_screamerTimer = m_screamerDuration;
    if (invertBrief) {
        m_invertTimer = m_screamerDuration * 0.75f;
    }
}

}
