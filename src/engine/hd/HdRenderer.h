#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace engine::hd {

enum class HdApi {
    D3D11,
    D3D12
};

struct HdCapabilities {
    bool d3d11 = false;
    bool d3d12 = false;
    bool dxr = false;
    bool dxc = false;
    std::string adapter;
};

HdCapabilities ProbeCapabilities(const std::string& assetDir);

struct HdVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float bakeR, bakeG, bakeB;
    float ao;
};

struct HdMaterialDesc {
    float tint[4]{1.0f, 1.0f, 1.0f, 1.0f};
    float roughness = 0.8f;
    float metallic = 0.0f;
    float emissive = 0.0f;
    float bump = 0.012f;
    bool castShadow = true;
};

struct HdLightDesc {
    float pos[3]{};
    float color[3]{1.0f, 1.0f, 1.0f};
    float radius = 6.0f;
    float intensity = 1.0f;
    int id = -1;
};

struct HdSpotDesc {
    bool enabled = false;
    float pos[3]{};
    float dir[3]{0.0f, 0.0f, 1.0f};
    float color[3]{1.0f, 0.9f, 0.75f};
    float innerCos = 0.96f;
    float outerCos = 0.85f;
    float range = 16.0f;
    float intensity = 40.0f;
};

struct HdBillboard {
    float center[3]{};
    float size[2]{1.0f, 1.0f};
    float tint[4]{1.0f, 1.0f, 1.0f, 1.0f};
    float emissive = 1.0f;
    int texture = -1;
};

struct HdQuality {
    int shadowQuality = 2;
    int rayTracing = 0;
    bool ssao = true;
    bool bloom = true;
    bool volumetrics = true;
    int volumetricSteps = 24;
    bool fxaa = true;
};

struct HdFrameParams {
    float camPos[3]{};
    float camTarget[3]{0.0f, 0.0f, 1.0f};
    float camUp[3]{0.0f, 1.0f, 0.0f};
    float fovY = 68.0f;
    float nearZ = 0.05f;
    float farZ = 140.0f;
    float time = 0.0f;
    float dt = 0.016f;
    float ambientSky[3]{0.02f, 0.025f, 0.035f};
    float ambientGround[3]{0.01f, 0.01f, 0.012f};
    float bakedScale = 0.25f;
    float fogColor[3]{0.012f, 0.014f, 0.02f};
    float fogDensity = 0.035f;
    float scatterDensity = 0.05f;
    float exposureBias = 1.0f;
    float bloomIntensity = 0.55f;
    float grain = 0.025f;
    float vignette = 0.45f;
    float saturation = 0.92f;
    float fear = 0.0f;
    float horror = 1.0f;
    float chromatic = 1.0f;
    float heartbeat = 0.0f;
    std::vector<HdLightDesc> lights;
    HdSpotDesc spot;
    std::vector<HdBillboard> billboards;
    HdQuality quality;
};

struct HdInitDesc {
    int width = 1280;
    int height = 720;
    std::string shaderDir;
    HdQuality quality;
};

class IHdRenderer {
public:
    virtual ~IHdRenderer() = default;
    virtual bool Init(const HdInitDesc& desc) = 0;
    virtual void Shutdown() = 0;
    virtual bool Resize(int width, int height) = 0;

    virtual int CreateTexture(const uint8_t* rgba, int width, int height) = 0;
    virtual int CreateMesh(const std::vector<HdVertex>& vertices, const HdMaterialDesc& material, int albedoTexture) = 0;
    virtual void SetMeshVisible(int mesh, bool visible) = 0;
    virtual void FinishLoading() {}

    virtual bool RenderFrame(const HdFrameParams& frame) = 0;
    virtual const uint8_t* Pixels() const = 0;
    virtual int Width() const = 0;
    virtual int Height() const = 0;

    virtual const char* ApiName() const = 0;
    virtual const std::string& AdapterName() const = 0;
    virtual bool SupportsRayTracing() const = 0;
    virtual const std::string& LastError() const = 0;
    virtual int DrawnMeshes() const { return 0; }
};

std::unique_ptr<IHdRenderer> CreateHdRenderer(HdApi api);
std::unique_ptr<IHdRenderer> CreateD3D12Renderer();

}
