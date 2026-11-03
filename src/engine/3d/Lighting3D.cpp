#include "engine/3d/Lighting3D.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

namespace engine::render3d {

bool Lighting3D::Init(const std::string& assetDir, int shadowSize) {
    Shutdown();
    std::string vs = assetDir + "/shaders/lit3d.vs";
    m_lit = LoadShader(vs.c_str(), (assetDir + "/shaders/lit3d.fs").c_str());
    m_depth = LoadShader(vs.c_str(), (assetDir + "/shaders/depth3d.fs").c_str());
    m_mirror = LoadShader(vs.c_str(), (assetDir + "/shaders/mirror3d.fs").c_str());
    if (m_lit.id == 0 || m_lit.id == rlGetShaderIdDefault()) return false;

    m_locViewPos = GetShaderLocation(m_lit, "uViewPos");
    m_locFlashIntensity = GetShaderLocation(m_lit, "uFlashIntensity");
    m_locFlashPos = GetShaderLocation(m_lit, "uFlashPos");
    m_locFlashDir = GetShaderLocation(m_lit, "uFlashDir");
    m_locFlashColor = GetShaderLocation(m_lit, "uFlashColor");
    m_locFlashInner = GetShaderLocation(m_lit, "uFlashInner");
    m_locFlashOuter = GetShaderLocation(m_lit, "uFlashOuter");
    m_locFlashRange = GetShaderLocation(m_lit, "uFlashRange");
    m_locShadowMap = GetShaderLocation(m_lit, "uShadowMap");
    m_locFlashViewProj = GetShaderLocation(m_lit, "uFlashViewProj");
    m_locShadowEnabled = GetShaderLocation(m_lit, "uShadowEnabled");
    m_locShadowNear = GetShaderLocation(m_lit, "uShadowNear");
    m_locShadowFar = GetShaderLocation(m_lit, "uShadowFar");
    m_locShadowTexel = GetShaderLocation(m_lit, "uShadowTexel");
    m_locLightPos = GetShaderLocation(m_lit, "uLightPos");
    m_locLightColor = GetShaderLocation(m_lit, "uLightColor");
    m_locLightRadius = GetShaderLocation(m_lit, "uLightRadius");
    m_locLightCount = GetShaderLocation(m_lit, "uLightCount");
    m_locAmbientSky = GetShaderLocation(m_lit, "uAmbientSky");
    m_locAmbientGround = GetShaderLocation(m_lit, "uAmbientGround");
    m_locBakedScale = GetShaderLocation(m_lit, "uBakedScale");
    m_locPowerLevel = GetShaderLocation(m_lit, "uPowerLevel");
    m_locExtraLight = GetShaderLocation(m_lit, "uExtraLight");
    m_locEmissive = GetShaderLocation(m_lit, "uEmissive");
    m_locFogColor = GetShaderLocation(m_lit, "uFogColor");
    m_locFogStart = GetShaderLocation(m_lit, "uFogStart");
    m_locFogEnd = GetShaderLocation(m_lit, "uFogEnd");
    m_locClipPlane = GetShaderLocation(m_lit, "uClipPlane");
    m_locClipEnabled = GetShaderLocation(m_lit, "uClipEnabled");
    m_locDither = GetShaderLocation(m_lit, "uDither");
    m_locSnap = GetShaderLocation(m_lit, "uSnapResolution");

    m_locMirrorScreen = GetShaderLocation(m_mirror, "uScreenSize");
    m_locMirrorViewPos = GetShaderLocation(m_mirror, "uViewPos");
    m_locMirrorFogColor = GetShaderLocation(m_mirror, "uFogColor");
    m_locMirrorFogStart = GetShaderLocation(m_mirror, "uFogStart");
    m_locMirrorFogEnd = GetShaderLocation(m_mirror, "uFogEnd");
    m_locMirrorTime = GetShaderLocation(m_mirror, "uTime");
    m_locMirrorDistort = GetShaderLocation(m_mirror, "uDistort");
    m_locMirrorSnap = GetShaderLocation(m_mirror, "uSnapResolution");

    m_shadowSize = shadowSize;
    m_shadowFbo = rlLoadFramebuffer();
    if (m_shadowFbo > 0) {
        rlEnableFramebuffer(m_shadowFbo);
        m_shadowDepth = rlLoadTextureDepth(m_shadowSize, m_shadowSize, false);
        rlFramebufferAttach(m_shadowFbo, m_shadowDepth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
        if (!rlFramebufferComplete(m_shadowFbo)) {
            TraceLog(LOG_WARNING, "CH2: shadow framebuffer incomplete");
        }
        rlDisableFramebuffer();
    }

    SetClipPlane(false, Vector4{0.0f, 1.0f, 0.0f, 0.0f});
    SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, 0.0f);
    m_ready = true;
    return true;
}

void Lighting3D::Shutdown() {
    if (m_shadowDepth != 0) rlUnloadTexture(m_shadowDepth);
    if (m_shadowFbo != 0) rlUnloadFramebuffer(m_shadowFbo);
    m_shadowDepth = 0;
    m_shadowFbo = 0;
    if (m_lit.id != 0 && m_lit.id != rlGetShaderIdDefault()) UnloadShader(m_lit);
    if (m_depth.id != 0 && m_depth.id != rlGetShaderIdDefault()) UnloadShader(m_depth);
    if (m_mirror.id != 0 && m_mirror.id != rlGetShaderIdDefault()) UnloadShader(m_mirror);
    m_lit = Shader{};
    m_depth = Shader{};
    m_mirror = Shader{};
    m_ready = false;
}

void Lighting3D::BeginShadowPass(const Flashlight& flash) {
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(m_shadowFbo);
    rlViewport(0, 0, m_shadowSize, m_shadowSize);
    rlClearColor(255, 255, 255, 255);
    rlClearScreenBuffers();

    float fov = std::acos(std::clamp(flash.outerCos, -1.0f, 1.0f)) * 2.0f * RAD2DEG + 8.0f;
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    double top = RL_CULL_DISTANCE_NEAR * std::tan(fov * 0.5 * DEG2RAD);
    rlFrustum(-top, top, -top, top, RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    Vector3 up = std::abs(flash.direction.y) > 0.98f ? Vector3{0.0f, 0.0f, 1.0f} : Vector3{0.0f, 1.0f, 0.0f};
    Matrix view = MatrixLookAt(flash.position, Vector3Add(flash.position, flash.direction), up);
    rlMultMatrixf(MatrixToFloat(view));
    rlEnableDepthTest();

    m_flashViewProj = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

void Lighting3D::EndShadowPass() {
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlDisableDepthTest();
    rlDisableFramebuffer();
    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
}

void Lighting3D::ApplyFrame(Vector3 viewPos, const Flashlight& flash, const std::vector<DynamicLight>& lights,
                            const SceneEnvironment& env, bool shadows) {
    SetShaderValue(m_lit, m_locViewPos, &viewPos, SHADER_UNIFORM_VEC3);
    float intensity = flash.enabled ? flash.intensity : 0.0f;
    SetShaderValue(m_lit, m_locFlashIntensity, &intensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lit, m_locFlashPos, &flash.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lit, m_locFlashDir, &flash.direction, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lit, m_locFlashColor, &flash.color, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lit, m_locFlashInner, &flash.innerCos, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lit, m_locFlashOuter, &flash.outerCos, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lit, m_locFlashRange, &flash.range, SHADER_UNIFORM_FLOAT);

    int shadowEnabled = (shadows && m_shadowFbo != 0) ? 1 : 0;
    SetShaderValue(m_lit, m_locShadowEnabled, &shadowEnabled, SHADER_UNIFORM_INT);
    SetShaderValueMatrix(m_lit, m_locFlashViewProj, m_flashViewProj);
    float nearPlane = static_cast<float>(RL_CULL_DISTANCE_NEAR);
    float farPlane = static_cast<float>(RL_CULL_DISTANCE_FAR);
    float texel = 1.0f / static_cast<float>(m_shadowSize);
    SetShaderValue(m_lit, m_locShadowNear, &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lit, m_locShadowFar, &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lit, m_locShadowTexel, &texel, SHADER_UNIFORM_FLOAT);
    int slot = SHADOW_SLOT;
    SetShaderValue(m_lit, m_locShadowMap, &slot, SHADER_UNIFORM_INT);

    Vector3 positions[MAX_LIGHTS]{};
    Vector3 colors[MAX_LIGHTS]{};
    float radii[MAX_LIGHTS]{};
    int count = std::min(static_cast<int>(lights.size()), MAX_LIGHTS);
    for (int i = 0; i < count; ++i) {
        positions[i] = lights[i].position;
        colors[i] = lights[i].color;
        radii[i] = lights[i].radius;
    }
    SetShaderValueV(m_lit, m_locLightPos, positions, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(m_lit, m_locLightColor, colors, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(m_lit, m_locLightRadius, radii, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValue(m_lit, m_locLightCount, &count, SHADER_UNIFORM_INT);

    SetShaderValue(m_lit, m_locAmbientSky, &env.ambientSky, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lit, m_locAmbientGround, &env.ambientGround, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lit, m_locPowerLevel, &env.powerLevel, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lit, m_locFogColor, &env.fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lit, m_locFogStart, &env.fogStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lit, m_locFogEnd, &env.fogEnd, SHADER_UNIFORM_FLOAT);
    int dither = env.dither ? 1 : 0;
    SetShaderValue(m_lit, m_locDither, &dither, SHADER_UNIFORM_INT);
    SetShaderValue(m_lit, m_locSnap, &env.snapResolution, SHADER_UNIFORM_VEC2);
}

void Lighting3D::BindShadowMap() const {
    if (m_shadowDepth == 0) return;
    rlActiveTextureSlot(SHADOW_SLOT);
    rlEnableTexture(m_shadowDepth);
    rlActiveTextureSlot(0);
}

void Lighting3D::SetClipPlane(bool enabled, Vector4 plane) {
    int flag = enabled ? 1 : 0;
    SetShaderValue(m_lit, m_locClipEnabled, &flag, SHADER_UNIFORM_INT);
    SetShaderValue(m_lit, m_locClipPlane, &plane, SHADER_UNIFORM_VEC4);
}

void Lighting3D::SetObjectParams(float bakedScale, Vector3 extraLight, float emissive) {
    SetShaderValue(m_lit, m_locBakedScale, &bakedScale, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lit, m_locExtraLight, &extraLight, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lit, m_locEmissive, &emissive, SHADER_UNIFORM_FLOAT);
}

void Lighting3D::SetMirrorParams(Vector3 viewPos, const SceneEnvironment& env, float time, Vector2 screenSize, float distort) {
    SetShaderValue(m_mirror, m_locMirrorScreen, &screenSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(m_mirror, m_locMirrorViewPos, &viewPos, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_mirror, m_locMirrorFogColor, &env.fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_mirror, m_locMirrorFogStart, &env.fogStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_mirror, m_locMirrorFogEnd, &env.fogEnd, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_mirror, m_locMirrorTime, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_mirror, m_locMirrorDistort, &distort, SHADER_UNIFORM_FLOAT);
    Vector2 noSnap{0.0f, 0.0f};
    SetShaderValue(m_mirror, m_locMirrorSnap, &noSnap, SHADER_UNIFORM_VEC2);
}

}
