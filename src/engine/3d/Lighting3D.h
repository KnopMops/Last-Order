#pragma once
#include "raylib.h"
#include <string>
#include <vector>

namespace engine::render3d {

struct DynamicLight {
    Vector3 position{};
    Vector3 color{1.0f, 1.0f, 1.0f};
    float radius = 4.0f;
};

struct Flashlight {
    bool enabled = false;
    Vector3 position{};
    Vector3 direction{0.0f, 0.0f, -1.0f};
    Vector3 color{1.0f, 0.93f, 0.8f};
    float innerCos = 0.95f;
    float outerCos = 0.82f;
    float range = 14.0f;
    float intensity = 1.0f;
};

struct SceneEnvironment {
    Vector3 ambientSky{0.07f, 0.08f, 0.11f};
    Vector3 ambientGround{0.03f, 0.03f, 0.04f};
    Vector3 fogColor{0.035f, 0.045f, 0.07f};
    float fogStart = 5.0f;
    float fogEnd = 26.0f;
    float powerLevel = 0.0f;
    bool dither = true;
    Vector2 snapResolution{480.0f, 270.0f};
};

class Lighting3D {
public:
    static constexpr int MAX_LIGHTS = 8;
    static constexpr int SHADOW_SLOT = 15;

    bool Init(const std::string& assetDir, int shadowSize);
    void Shutdown();
    bool IsReady() const { return m_ready; }

    Shader LitShader() const { return m_lit; }
    Shader DepthShader() const { return m_depth; }
    Shader MirrorShader() const { return m_mirror; }

    void BeginShadowPass(const Flashlight& flash);
    void EndShadowPass();

    void ApplyFrame(Vector3 viewPos, const Flashlight& flash, const std::vector<DynamicLight>& lights,
                    const SceneEnvironment& env, bool shadows);
    void BindShadowMap() const;
    void SetClipPlane(bool enabled, Vector4 plane);
    void SetObjectParams(float bakedScale, Vector3 extraLight, float emissive);
    void SetMirrorParams(Vector3 viewPos, const SceneEnvironment& env, float time, Vector2 screenSize, float distort);

private:
    bool m_ready = false;
    Shader m_lit{};
    Shader m_depth{};
    Shader m_mirror{};
    unsigned int m_shadowFbo = 0;
    unsigned int m_shadowDepth = 0;
    int m_shadowSize = 1024;
    Matrix m_flashViewProj{};

    int m_locViewPos = -1;
    int m_locFlashIntensity = -1;
    int m_locFlashPos = -1;
    int m_locFlashDir = -1;
    int m_locFlashColor = -1;
    int m_locFlashInner = -1;
    int m_locFlashOuter = -1;
    int m_locFlashRange = -1;
    int m_locShadowMap = -1;
    int m_locFlashViewProj = -1;
    int m_locShadowEnabled = -1;
    int m_locShadowNear = -1;
    int m_locShadowFar = -1;
    int m_locShadowTexel = -1;
    int m_locLightPos = -1;
    int m_locLightColor = -1;
    int m_locLightRadius = -1;
    int m_locLightCount = -1;
    int m_locAmbientSky = -1;
    int m_locAmbientGround = -1;
    int m_locBakedScale = -1;
    int m_locPowerLevel = -1;
    int m_locExtraLight = -1;
    int m_locEmissive = -1;
    int m_locFogColor = -1;
    int m_locFogStart = -1;
    int m_locFogEnd = -1;
    int m_locClipPlane = -1;
    int m_locClipEnabled = -1;
    int m_locDither = -1;
    int m_locSnap = -1;

    int m_locMirrorScreen = -1;
    int m_locMirrorViewPos = -1;
    int m_locMirrorFogColor = -1;
    int m_locMirrorFogStart = -1;
    int m_locMirrorFogEnd = -1;
    int m_locMirrorTime = -1;
    int m_locMirrorDistort = -1;
    int m_locMirrorSnap = -1;
};

}
