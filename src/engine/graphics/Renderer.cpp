#include "engine/graphics/Renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace engine::graphics {

void Renderer::Init(int, int, const std::string& crtShaderPath) {
    m_sceneRT = LoadRenderTexture(NATIVE_W, NATIVE_H);
    m_compositeRT = LoadRenderTexture(NATIVE_W, NATIVE_H);
    m_scene3DRT = LoadRenderTexture(HI3D_W, HI3D_H);
    m_composite3DRT = LoadRenderTexture(HI3D_W, HI3D_H);
    m_uiRT = LoadRenderTexture(UI_W, UI_H);

    SetTextureFilter(m_sceneRT.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_compositeRT.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_scene3DRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(m_composite3DRT.texture, TEXTURE_FILTER_BILINEAR);
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
    if (m_composite3DRT.id != 0) UnloadRenderTexture(m_composite3DRT);
    if (m_scene3DRT.id != 0) UnloadRenderTexture(m_scene3DRT);
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
    } else if (m_aftershockTimer > 0.0f) {
        effectiveStress = std::max(effectiveStress, (m_aftershockTimer / 3.0f) * 0.85f);
    }

    float effectiveChroma = settings.chromaAberration;
    if (m_screamerTimer > 0.0f) {
        effectiveChroma = std::max(effectiveChroma, 1.0f);
    }

    SetShaderValue(m_crtShader, m_uTimeLoc,         &totalTime,                  SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uDistortionLoc,   &settings.barrelDistortion,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uScanlineLoc,     &settings.scanlineIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uChromaLoc,       &effectiveChroma,            SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uBrightnessLoc,   &settings.brightness,        SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_crtShader, m_uSanityStressLoc, &effectiveStress,            SHADER_UNIFORM_FLOAT);

    bool strobeInvert = false;
    if (m_invertTimer > 0.0f && !settings.safeScreamerMode) {
        int tick = static_cast<int>(totalTime * 36.0f);
        strobeInvert = (tick % 2 == 0);
    }
    float inv = strobeInvert ? 1.0f : 0.0f;
    SetShaderValue(m_crtShader, m_uInvertLoc, &inv, SHADER_UNIFORM_FLOAT);
    if (m_invertTimer > 0.0f) m_invertTimer = std::max(0.0f, m_invertTimer - dt);
    if (m_aftershockTimer > 0.0f) m_aftershockTimer = std::max(0.0f, m_aftershockTimer - dt);

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
    BeginTextureMode(m_highRes3DMode ? m_scene3DRT : m_sceneRT);
    ClearBackground(Color{10, 13, 26, 255});
}

void Renderer::EndWorldPass(LightingSystem& lighting, float totalTime) {
    EndTextureMode();

    bool use2DLight = !m_skip2DLightmap && !m_highRes3DMode;
    if (use2DLight) {
        lighting.RenderLightMap(totalTime);
    }

    int targetW = m_highRes3DMode ? HI3D_W : NATIVE_W;
    int targetH = m_highRes3DMode ? HI3D_H : NATIVE_H;
    float scaleMul = m_highRes3DMode ? 4.0f : 1.0f;

    BeginTextureMode(m_highRes3DMode ? m_composite3DRT : m_compositeRT);
    ClearBackground(BLACK);

    Rectangle srcFlip = {0.0f, 0.0f, static_cast<float>(targetW), -static_cast<float>(targetH)};
    Vector2 shakePos = {m_shakeOffset.x * scaleMul, m_shakeOffset.y * scaleMul};

    DrawTextureRec((m_highRes3DMode ? m_scene3DRT : m_sceneRT).texture, srcFlip, shakePos, WHITE);

    if (use2DLight) {
        BeginBlendMode(BLEND_MULTIPLIED);
        DrawTextureRec(lighting.GetLightMap().texture, srcFlip, Vector2{0.0f, 0.0f}, WHITE);
        EndBlendMode();
    }

    if (m_screamerTimer > 0.0f && m_screamerTex.id != 0) {
        float progress = 1.0f - (m_screamerTimer / std::max(0.1f, m_screamerDuration));
        float pulseZoom = 1.06f + progress * 0.62f + std::sin(totalTime * 75.0f) * 0.09f;
        if ((std::rand() % 5) == 0) pulseZoom += 0.18f;

        float w = static_cast<float>(targetW) * pulseZoom;
        float h = static_cast<float>(targetH) * pulseZoom;
        float jx = ((static_cast<float>(std::rand() % 25) - 12.0f) * scaleMul) + shakePos.x;
        float jy = ((static_cast<float>(std::rand() % 25) - 12.0f) * scaleMul) + shakePos.y;
        float rot = std::sin(totalTime * 62.0f) * 4.5f;

        Rectangle srcSc = {0.0f, 0.0f, static_cast<float>(m_screamerTex.width), static_cast<float>(m_screamerTex.height)};
        Vector2 origin = {w * 0.5f, h * 0.5f};
        Rectangle dstCenter = {targetW * 0.5f + jx, targetH * 0.5f + jy, w, h};

        DrawRectangle(0, 0, targetW, targetH, (std::rand() % 4 == 0) ? Color{85, 0, 0, 230} : Color{6, 2, 4, 230});

        Rectangle dstRed = {dstCenter.x - 9.0f * scaleMul, dstCenter.y - 3.0f * scaleMul, w, h};
        Rectangle dstCyan = {dstCenter.x + 9.0f * scaleMul, dstCenter.y + 3.0f * scaleMul, w, h};
        DrawTexturePro(m_screamerTex, srcSc, dstRed, origin, rot - 2.0f, Color{255, 25, 25, 185});
        DrawTexturePro(m_screamerTex, srcSc, dstCyan, origin, rot + 2.0f, Color{40, 220, 255, 150});

        int strobePhase = static_cast<int>(totalTime * 42.0f) % 5;
        Color mainTint = (strobePhase == 0) ? Color{255, 50, 50, 255}
                       : (strobePhase == 2) ? Color{220, 255, 255, 255}
                                            : WHITE;
        DrawTexturePro(m_screamerTex, srcSc, dstCenter, origin, rot, mainTint);

        for (int s = 0; s < 7; ++s) {
            int sliceY = std::rand() % (targetH - 12);
            int sliceH = static_cast<int>((4 + (std::rand() % 12)) * scaleMul);
            float shiftX = static_cast<float>((std::rand() % 33) - 16) * scaleMul;
            float ty = (static_cast<float>(sliceY) / static_cast<float>(targetH)) * static_cast<float>(m_screamerTex.height);
            float th = (static_cast<float>(sliceH) / static_cast<float>(targetH)) * static_cast<float>(m_screamerTex.height);
            DrawTexturePro(m_screamerTex,
                           Rectangle{0.0f, ty, static_cast<float>(m_screamerTex.width), th},
                           Rectangle{shiftX, static_cast<float>(sliceY), static_cast<float>(targetW), static_cast<float>(sliceH)},
                           Vector2{0.0f, 0.0f}, 0.0f, Color{255, 70, 70, 230});
        }

        for (int g = 0; g < 14; ++g) {
            int gy = std::rand() % targetH;
            Color gc = (g % 2 == 0) ? Color{235, 15, 20, 185} : Color{10, 0, 0, 210};
            DrawRectangle(0, gy, targetW, static_cast<int>((1 + (std::rand() % 3)) * scaleMul), gc);
        }

        DrawRectangleLinesEx(Rectangle{0.0f, 0.0f, static_cast<float>(targetW), static_cast<float>(targetH)}, 8.0f * scaleMul, Color{185, 10, 15, 245});
    } else if (m_aftershockTimer > 0.0f) {
        float aRatio = m_aftershockTimer / 3.0f;
        unsigned char ba = static_cast<unsigned char>(std::clamp(aRatio * 140.0f * (0.6f + 0.4f * std::sin(totalTime * 16.0f)), 0.0f, 180.0f));
        DrawRectangleLinesEx(Rectangle{0.0f, 0.0f, static_cast<float>(targetW), static_cast<float>(targetH)}, 7.0f * scaleMul, Color{165, 10, 16, ba});
    }

    if (m_flashTimer > 0.0f && m_flashDuration > 0.0f && m_shakeAllowed) {
        float alpha = (m_flashTimer / m_flashDuration);
        Color c = m_flashColor;
        c.a = static_cast<unsigned char>(math::Clamp(alpha * static_cast<float>(m_flashColor.a), 0.0f, 255.0f));
        DrawRectangle(0, 0, targetW, targetH, c);
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
    int targetW = m_highRes3DMode ? HI3D_W : NATIVE_W;
    int targetH = m_highRes3DMode ? HI3D_H : NATIVE_H;

    BeginDrawing();
    ClearBackground(Color{6, 8, 16, 255});

    if (m_crtEnabled) BeginShaderMode(m_crtShader);
    Rectangle srcWorld = {0.0f, 0.0f, static_cast<float>(targetW), -static_cast<float>(targetH)};
    DrawTexturePro((m_highRes3DMode ? m_composite3DRT : m_compositeRT).texture, srcWorld, dstScreen, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
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
    m_screamerDuration = std::max(0.8f, durationSec);
    m_screamerTimer = m_screamerDuration;
    m_aftershockTimer = 3.0f;
    if (invertBrief) {
        m_invertTimer = m_screamerDuration * 0.90f;
    }
}

}
