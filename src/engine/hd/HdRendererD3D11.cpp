#include "engine/hd/HdRenderer.h"
#include "engine/hd/HdShaderCache.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace engine::hd {

using Microsoft::WRL::ComPtr;

namespace {

constexpr int CUBE_SLOTS = 4;
constexpr int MAX_LIGHTS = 8;
constexpr int BLOOM_LEVELS = 6;
constexpr int LUM_SIZE = 128;
constexpr float PI_F = 3.14159265f;

struct F3 {
    float x, y, z;
};

F3 Sub(F3 a, F3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float Dot(F3 a, F3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
F3 Cross(F3 a, F3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float Len(F3 a) { return std::sqrt(Dot(a, a)); }
F3 Norm(F3 a) {
    float l = Len(a);
    return l > 1e-8f ? F3{a.x / l, a.y / l, a.z / l} : F3{0.0f, 0.0f, 1.0f};
}
F3 FromArr(const float* a) { return {a[0], a[1], a[2]}; }

struct Mat {
    float m[16];
};

Mat Identity() {
    Mat r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat Mul(const Mat& a, const Mat& b) {
    Mat r{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[i * 4 + k] * b.m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    }
    return r;
}

Mat ViewFromBasis(F3 eye, F3 x, F3 y, F3 z) {
    Mat r = Identity();
    r.m[0] = x.x; r.m[1] = y.x; r.m[2] = z.x;
    r.m[4] = x.y; r.m[5] = y.y; r.m[6] = z.y;
    r.m[8] = x.z; r.m[9] = y.z; r.m[10] = z.z;
    r.m[12] = -Dot(x, eye);
    r.m[13] = -Dot(y, eye);
    r.m[14] = -Dot(z, eye);
    return r;
}

Mat ViewFromRightHanded(F3 eye, F3 forward, F3 up) {
    F3 f = Norm(forward);
    F3 right = Norm(Cross(f, up));
    F3 u = Cross(right, f);
    return ViewFromBasis(eye, right, u, f);
}

Mat LookAtLH(F3 eye, F3 forward, F3 up) {
    F3 z = Norm(forward);
    F3 x = Norm(Cross(up, z));
    F3 y = Cross(z, x);
    return ViewFromBasis(eye, x, y, z);
}

Mat PerspectiveLH(float fovY, float aspect, float n, float f) {
    float h = 1.0f / std::tan(fovY * 0.5f);
    float w = h / aspect;
    Mat r{};
    r.m[0] = w;
    r.m[5] = h;
    r.m[10] = f / (f - n);
    r.m[11] = 1.0f;
    r.m[14] = -n * f / (f - n);
    return r;
}

Mat Inverse(const Mat& s) {
    const float* m = s.m;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    Mat r = Identity();
    if (std::abs(det) < 1e-12f) return r;
    float id = 1.0f / det;
    for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * id;
    return r;
}

struct Frustum {
    float p[6][4];
};

Frustum MakeFrustum(const Mat& m) {
    auto col = [&](int k, int r) { return m.m[r * 4 + k]; };
    Frustum f{};
    for (int r = 0; r < 6; ++r) {
        float sign = (r % 2 == 0) ? 1.0f : -1.0f;
        int axis = r / 2;
        for (int k = 0; k < 4; ++k) {
            float w = col(3, k);
            float a = col(axis, k);
            if (axis == 2) f.p[r][k] = (r % 2 == 0) ? a : (w - a);
            else f.p[r][k] = w + sign * a;
        }
        float inv = 1.0f / std::max(1e-6f, std::sqrt(f.p[r][0] * f.p[r][0] + f.p[r][1] * f.p[r][1] + f.p[r][2] * f.p[r][2]));
        for (int k = 0; k < 4; ++k) f.p[r][k] *= inv;
    }
    return f;
}

bool AabbVisible(const Frustum& f, F3 mn, F3 mx) {
    for (int r = 0; r < 6; ++r) {
        float x = f.p[r][0] >= 0.0f ? mx.x : mn.x;
        float y = f.p[r][1] >= 0.0f ? mx.y : mn.y;
        float z = f.p[r][2] >= 0.0f ? mx.z : mn.z;
        if (f.p[r][0] * x + f.p[r][1] * y + f.p[r][2] * z + f.p[r][3] < 0.0f) return false;
    }
    return true;
}

bool AabbInSphere(F3 mn, F3 mx, F3 c, float r) {
    float dx = std::max({mn.x - c.x, 0.0f, c.x - mx.x});
    float dy = std::max({mn.y - c.y, 0.0f, c.y - mx.y});
    float dz = std::max({mn.z - c.z, 0.0f, c.z - mx.z});
    return dx * dx + dy * dy + dz * dz <= r * r;
}

struct CBFrame {
    float viewProj[16];
    float invViewProj[16];
    float spotViewProj[16];
    float camPos[4];
    float camFwd[4];
    float screen[4];
    float proj[4];
    float ambientSky[4];
    float ambientGround[4];
    float fog[4];
    float spotPos[4];
    float spotDir[4];
    float spotColor[4];
    float spotCones[4];
    float lightPos[MAX_LIGHTS][4];
    float lightColor[MAX_LIGHTS][4];
    float counts[4];
    float grade[4];
    float fx[4];
    float rt[4];
    float prevViewProj[16];
};

struct CBObject {
    float tint[4];
    float mat[4];
};

struct CBPost {
    float p0[4];
    float p1[4];
};

struct BillboardVertex {
    float x, y, z;
    float u, v;
    float r, g, b, a;
};

struct Target {
    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
    int w = 0;
    int h = 0;
};

struct MeshGpu {
    ComPtr<ID3D11Buffer> vb;
    UINT count = 0;
    HdMaterialDesc material;
    F3 bmin{};
    F3 bmax{};
    int texture = 0;
    bool visible = true;
};

}

class D3D11Renderer final : public IHdRenderer {
public:
    ~D3D11Renderer() override { Shutdown(); }

    bool Init(const HdInitDesc& desc) override;
    void Shutdown() override;
    bool Resize(int width, int height) override;

    int CreateTexture(const uint8_t* rgba, int width, int height) override;
    int CreateMesh(const std::vector<HdVertex>& vertices, const HdMaterialDesc& material, int albedoTexture) override;
    void SetMeshVisible(int mesh, bool visible) override {
        if (mesh >= 0 && mesh < static_cast<int>(m_meshes.size())) m_meshes[mesh].visible = visible;
    }

    bool RenderFrame(const HdFrameParams& frame) override;
    const uint8_t* Pixels() const override { return m_pixels.data(); }
    int Width() const override { return m_width; }
    int Height() const override { return m_height; }

    const char* ApiName() const override { return "Direct3D 11"; }
    const std::string& AdapterName() const override { return m_adapter; }
    bool SupportsRayTracing() const override { return false; }
    int DrawnMeshes() const override { return m_drawn; }
    const std::string& LastError() const override { return m_error; }

private:
    bool Fail(const std::string& msg) {
        m_error = msg;
        std::fprintf(stderr, "HD D3D11: %s\n", msg.c_str());
        return false;
    }
    bool CompileShaders(const std::string& dir);
    ComPtr<ID3DBlob> Compile(const std::string& src, const char* entry, const char* target);
    bool CreateStates();
    bool CreateFixedTargets();
    bool CreateScreenTargets();
    bool MakeTarget(Target& t, int w, int h, DXGI_FORMAT fmt, bool mips = false);
    void ReleaseScreenTargets();

    void UpdateFrameCB(const CBFrame& cb);
    void UpdatePostCB(float a, float b, float c, float d, float e = 0, float f = 0, float g = 0, float h = 0);
    void UnbindSrvs();
    void SetViewport(int w, int h);
    void DrawFull(ID3D11PixelShader* ps);
    void DrawMeshes(bool shadowOnly, bool useMaterials);
    void RenderShadowView(const Mat& viewProj, F3 origin, float radius, ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv, int size, CBFrame& cb);
    void DrawBillboards(const HdFrameParams& frame, F3 camPos, F3 camFwd, F3 camUp);

    ComPtr<ID3D11Device> m_dev;
    ComPtr<ID3D11DeviceContext> m_ctx;
    int m_spotSize = 2048;
    int m_cubeSize = 512;
    int m_drawn = 0;
    Frustum m_cullFrustum{};
    bool m_cullUseFrustum = false;
    bool m_cullUseSphere = false;
    F3 m_cullCenter{};
    float m_cullRadius = 0.0f;
    HdQuality m_quality;
    std::string m_adapter;
    std::string m_error;
    int m_width = 0;
    int m_height = 0;
    uint32_t m_frame = 0;
    bool m_ready = false;

    ComPtr<ID3D11VertexShader> m_vsScene, m_vsShadow, m_vsBillboard, m_vsFull;
    ComPtr<ID3D11PixelShader> m_psPrepass, m_psScene, m_psShadow, m_psBillboard, m_psSsao, m_psBlurAo, m_psBloomPre,
        m_psBloomDown, m_psBloomUp, m_psLum, m_psAdapt, m_psVol, m_psBlurVol, m_psComposite, m_psFxaa, m_psCopy;
    ComPtr<ID3D11InputLayout> m_layoutScene, m_layoutBillboard;

    ComPtr<ID3D11RasterizerState> m_rsBack, m_rsNone;
    ComPtr<ID3D11DepthStencilState> m_dsLess, m_dsEqual, m_dsOff, m_dsRead;
    ComPtr<ID3D11BlendState> m_bsOpaque, m_bsPremult, m_bsAdd;
    ComPtr<ID3D11SamplerState> m_sampLinear, m_sampClamp;
    ComPtr<ID3D11Buffer> m_cbFrame, m_cbObject, m_cbPost, m_billboardVb;

    ComPtr<ID3D11Texture2D> m_spotTex, m_spotDepthTex, m_cubeTex, m_cubeDepthTex;
    ComPtr<ID3D11RenderTargetView> m_spotRtv;
    ComPtr<ID3D11ShaderResourceView> m_spotSrv, m_cubeSrv;
    ComPtr<ID3D11DepthStencilView> m_spotDsv, m_cubeDsv;
    std::array<ComPtr<ID3D11RenderTargetView>, CUBE_SLOTS * 6> m_cubeRtv;
    std::array<int, CUBE_SLOTS> m_slotLight{-1, -1, -1, -1};
    std::array<F3, CUBE_SLOTS> m_slotPos{};
    std::array<float, CUBE_SLOTS> m_slotRadius{};

    Target m_hdr, m_normal, m_ao, m_aoBlur, m_vol, m_volBlur, m_ldr, m_final, m_lum;
    std::array<Target, BLOOM_LEVELS> m_bloom;
    std::array<Target, 2> m_adapt;
    int m_adaptIndex = 0;
    bool m_adaptInit = false;
    ComPtr<ID3D11Texture2D> m_depthTex;
    ComPtr<ID3D11DepthStencilView> m_dsv;
    ComPtr<ID3D11ShaderResourceView> m_depthSrv;
    ComPtr<ID3D11Texture2D> m_staging;

    std::vector<ComPtr<ID3D11ShaderResourceView>> m_textures;
    std::vector<MeshGpu> m_meshes;
    std::vector<uint8_t> m_pixels;
};

bool D3D11Renderer::Init(const HdInitDesc& desc) {
    Shutdown();
    m_width = std::max(320, desc.width);
    m_height = std::max(180, desc.height);

    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &m_dev, &got, &m_ctx);
    if (FAILED(hr)) return Fail("D3D11CreateDevice failed");

    ComPtr<IDXGIDevice> dxgiDev;
    if (SUCCEEDED(m_dev.As(&dxgiDev))) {
        ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(dxgiDev->GetAdapter(&adapter))) {
            DXGI_ADAPTER_DESC ad{};
            adapter->GetDesc(&ad);
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%ls", ad.Description);
            m_adapter = buf;
        }
    }

    m_quality = desc.quality;
    int sq = desc.quality.shadowQuality;
    m_spotSize = sq <= 0 ? 1024 : (sq == 1 ? 2048 : 4096);
    m_cubeSize = sq <= 0 ? 256 : (sq == 1 ? 512 : 768);
    if (!CompileShaders(desc.shaderDir)) return false;
    if (!CreateStates()) return false;
    if (!CreateFixedTargets()) return false;
    if (!CreateScreenTargets()) return false;

    uint8_t white[4] = {255, 255, 255, 255};
    m_textures.clear();
    CreateTexture(white, 1, 1);
    m_ready = true;
    return true;
}

void D3D11Renderer::Shutdown() {
    if (m_ctx) m_ctx->ClearState();
    m_meshes.clear();
    m_textures.clear();
    ReleaseScreenTargets();
    m_ready = false;
    m_ctx.Reset();
    m_dev.Reset();
}

ComPtr<ID3DBlob> D3D11Renderer::Compile(const std::string& src, const char* entry, const char* target) {
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> err;
    UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
    static const char* dbg = std::getenv("LO_HD_DEBUG");
    std::string cacheKey = src + "|" + entry + "|" + target + "|d3d11|dbg" + (dbg ? dbg : "0") + "|v3";
    std::vector<uint8_t> cached;
    if (ShaderCacheLoad(cacheKey, cached)) {
        ComPtr<ID3DBlob> cb;
        if (SUCCEEDED(D3DCreateBlob(cached.size(), &cb))) {
            std::memcpy(cb->GetBufferPointer(), cached.data(), cached.size());
            return cb;
        }
    }
    D3D_SHADER_MACRO macros[] = {{"DEBUG_VIEW", dbg ? dbg : "0"}, {nullptr, nullptr}};
    HRESULT hr = D3DCompile(src.data(), src.size(), "hd.hlsl", macros, nullptr, entry, target, flags, 0, &blob, &err);
    if (FAILED(hr)) {
        std::string msg = std::string("shader ") + entry + ": " + (err ? static_cast<const char*>(err->GetBufferPointer()) : "compile error");
        Fail(msg);
        return nullptr;
    }
    {
        const uint8_t* bp = static_cast<const uint8_t*>(blob->GetBufferPointer());
        ShaderCacheStore(cacheKey, std::vector<uint8_t>(bp, bp + blob->GetBufferSize()));
    }
    return blob;
}

bool D3D11Renderer::CompileShaders(const std::string& dir) {
    std::ifstream f(dir + "/hd.hlsl", std::ios::binary);
    if (!f) return Fail("cannot open " + dir + "/hd.hlsl");
    std::stringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    auto vs = [&](const char* entry, ComPtr<ID3D11VertexShader>& out, ComPtr<ID3DBlob>* keep = nullptr) {
        ComPtr<ID3DBlob> b = Compile(src, entry, "vs_5_0");
        if (!b) return false;
        if (FAILED(m_dev->CreateVertexShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &out))) return Fail("CreateVertexShader");
        if (keep) *keep = b;
        return true;
    };
    auto ps = [&](const char* entry, ComPtr<ID3D11PixelShader>& out) {
        ComPtr<ID3DBlob> b = Compile(src, entry, "ps_5_0");
        if (!b) return false;
        if (FAILED(m_dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &out))) return Fail("CreatePixelShader");
        return true;
    };

    ComPtr<ID3DBlob> sceneBlob;
    ComPtr<ID3DBlob> billBlob;
    if (!vs("VSScene", m_vsScene, &sceneBlob)) return false;
    if (!vs("VSShadowPos", m_vsShadow)) return false;
    if (!vs("VSBillboard", m_vsBillboard, &billBlob)) return false;
    if (!vs("VSFull", m_vsFull)) return false;
    if (!ps("PSPrepass", m_psPrepass) || !ps("PSScene", m_psScene) || !ps("PSShadow", m_psShadow) || !ps("PSBillboard", m_psBillboard) ||
        !ps("PSSsao", m_psSsao) || !ps("PSBlurAo", m_psBlurAo) || !ps("PSBloomPrefilter", m_psBloomPre) || !ps("PSBloomDown", m_psBloomDown) ||
        !ps("PSBloomUp", m_psBloomUp) || !ps("PSLum", m_psLum) || !ps("PSAdapt", m_psAdapt) || !ps("PSVolumetric", m_psVol) ||
        !ps("PSBlurVol", m_psBlurVol) || !ps("PSComposite", m_psComposite) || !ps("PSFxaa", m_psFxaa) || !ps("PSCopy", m_psCopy)) {
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC sceneLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (FAILED(m_dev->CreateInputLayout(sceneLayout, 4, sceneBlob->GetBufferPointer(), sceneBlob->GetBufferSize(), &m_layoutScene))) {
        return Fail("CreateInputLayout scene");
    }
    D3D11_INPUT_ELEMENT_DESC billLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (FAILED(m_dev->CreateInputLayout(billLayout, 3, billBlob->GetBufferPointer(), billBlob->GetBufferSize(), &m_layoutBillboard))) {
        return Fail("CreateInputLayout billboard");
    }
    return true;
}

bool D3D11Renderer::CreateStates() {
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_BACK;
    rs.FrontCounterClockwise = TRUE;
    rs.DepthClipEnable = TRUE;
    if (FAILED(m_dev->CreateRasterizerState(&rs, &m_rsBack))) return Fail("RS back");
    rs.CullMode = D3D11_CULL_NONE;
    if (FAILED(m_dev->CreateRasterizerState(&rs, &m_rsNone))) return Fail("RS none");

    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = TRUE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D11_COMPARISON_LESS;
    if (FAILED(m_dev->CreateDepthStencilState(&ds, &m_dsLess))) return Fail("DS less");
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(m_dev->CreateDepthStencilState(&ds, &m_dsEqual))) return Fail("DS equal");
    ds.DepthFunc = D3D11_COMPARISON_LESS;
    if (FAILED(m_dev->CreateDepthStencilState(&ds, &m_dsRead))) return Fail("DS read");
    ds.DepthEnable = FALSE;
    if (FAILED(m_dev->CreateDepthStencilState(&ds, &m_dsOff))) return Fail("DS off");

    D3D11_BLEND_DESC bs{};
    bs.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_dev->CreateBlendState(&bs, &m_bsOpaque))) return Fail("BS opaque");
    bs.RenderTarget[0].BlendEnable = TRUE;
    bs.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bs.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bs.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bs.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bs.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bs.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    if (FAILED(m_dev->CreateBlendState(&bs, &m_bsPremult))) return Fail("BS premult");
    bs.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bs.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    if (FAILED(m_dev->CreateBlendState(&bs, &m_bsAdd))) return Fail("BS add");

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_ANISOTROPIC;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
    sd.MaxAnisotropy = 8;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_dev->CreateSamplerState(&sd, &m_sampLinear))) return Fail("sampler linear");
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy = 1;
    if (FAILED(m_dev->CreateSamplerState(&sd, &m_sampClamp))) return Fail("sampler clamp");

    auto cb = [&](ComPtr<ID3D11Buffer>& out, UINT size) {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = size;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(m_dev->CreateBuffer(&bd, nullptr, &out));
    };
    if (!cb(m_cbFrame, sizeof(CBFrame)) || !cb(m_cbObject, sizeof(CBObject)) || !cb(m_cbPost, sizeof(CBPost))) return Fail("constant buffers");

    D3D11_BUFFER_DESC vb{};
    vb.ByteWidth = sizeof(BillboardVertex) * 6 * 64;
    vb.Usage = D3D11_USAGE_DYNAMIC;
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(m_dev->CreateBuffer(&vb, nullptr, &m_billboardVb))) return Fail("billboard vb");
    return true;
}

bool D3D11Renderer::MakeTarget(Target& t, int w, int h, DXGI_FORMAT fmt, bool mips) {
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w;
    td.Height = h;
    td.MipLevels = mips ? 0 : 1;
    td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags = mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
    if (FAILED(m_dev->CreateTexture2D(&td, nullptr, &t.tex))) return Fail("CreateTexture2D target");
    if (FAILED(m_dev->CreateRenderTargetView(t.tex.Get(), nullptr, &t.rtv))) return Fail("CreateRTV");
    if (FAILED(m_dev->CreateShaderResourceView(t.tex.Get(), nullptr, &t.srv))) return Fail("CreateSRV");
    t.w = w;
    t.h = h;
    return true;
}

bool D3D11Renderer::CreateFixedTargets() {
    D3D11_TEXTURE2D_DESC td{};
    td.Width = m_spotSize;
    td.Height = m_spotSize;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(m_dev->CreateTexture2D(&td, nullptr, &m_spotTex))) return Fail("spot tex");
    if (FAILED(m_dev->CreateRenderTargetView(m_spotTex.Get(), nullptr, &m_spotRtv))) return Fail("spot rtv");
    if (FAILED(m_dev->CreateShaderResourceView(m_spotTex.Get(), nullptr, &m_spotSrv))) return Fail("spot srv");
    td.Format = DXGI_FORMAT_D32_FLOAT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(m_dev->CreateTexture2D(&td, nullptr, &m_spotDepthTex))) return Fail("spot depth");
    if (FAILED(m_dev->CreateDepthStencilView(m_spotDepthTex.Get(), nullptr, &m_spotDsv))) return Fail("spot dsv");

    D3D11_TEXTURE2D_DESC cd{};
    cd.Width = m_cubeSize;
    cd.Height = m_cubeSize;
    cd.MipLevels = 1;
    cd.ArraySize = CUBE_SLOTS * 6;
    cd.Format = DXGI_FORMAT_R32_FLOAT;
    cd.SampleDesc.Count = 1;
    cd.Usage = D3D11_USAGE_DEFAULT;
    cd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    cd.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    if (FAILED(m_dev->CreateTexture2D(&cd, nullptr, &m_cubeTex))) return Fail("cube tex");
    D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = DXGI_FORMAT_R32_FLOAT;
    sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
    sv.TextureCubeArray.MostDetailedMip = 0;
    sv.TextureCubeArray.MipLevels = 1;
    sv.TextureCubeArray.First2DArrayFace = 0;
    sv.TextureCubeArray.NumCubes = CUBE_SLOTS;
    if (FAILED(m_dev->CreateShaderResourceView(m_cubeTex.Get(), &sv, &m_cubeSrv))) return Fail("cube srv");
    for (int s = 0; s < CUBE_SLOTS * 6; ++s) {
        D3D11_RENDER_TARGET_VIEW_DESC rv{};
        rv.Format = DXGI_FORMAT_R32_FLOAT;
        rv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rv.Texture2DArray.MipSlice = 0;
        rv.Texture2DArray.FirstArraySlice = s;
        rv.Texture2DArray.ArraySize = 1;
        if (FAILED(m_dev->CreateRenderTargetView(m_cubeTex.Get(), &rv, &m_cubeRtv[s]))) return Fail("cube rtv");
    }
    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = m_cubeSize;
    dd.Height = m_cubeSize;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D32_FLOAT;
    dd.SampleDesc.Count = 1;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(m_dev->CreateTexture2D(&dd, nullptr, &m_cubeDepthTex))) return Fail("cube depth");
    if (FAILED(m_dev->CreateDepthStencilView(m_cubeDepthTex.Get(), nullptr, &m_cubeDsv))) return Fail("cube dsv");
    m_slotLight = {-1, -1, -1, -1};
    return true;
}

void D3D11Renderer::ReleaseScreenTargets() {
    m_hdr = Target{};
    m_normal = Target{};
    m_ao = Target{};
    m_aoBlur = Target{};
    m_vol = Target{};
    m_volBlur = Target{};
    m_ldr = Target{};
    m_final = Target{};
    m_lum = Target{};
    for (auto& b : m_bloom) b = Target{};
    for (auto& a : m_adapt) a = Target{};
    m_depthTex.Reset();
    m_dsv.Reset();
    m_depthSrv.Reset();
    m_staging.Reset();
}

bool D3D11Renderer::CreateScreenTargets() {
    ReleaseScreenTargets();
    int w = m_width;
    int h = m_height;
    int hw = std::max(1, w / 2);
    int hh = std::max(1, h / 2);

    if (!MakeTarget(m_hdr, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
    if (!MakeTarget(m_normal, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
    if (!MakeTarget(m_ao, hw, hh, DXGI_FORMAT_R8_UNORM)) return false;
    if (!MakeTarget(m_aoBlur, hw, hh, DXGI_FORMAT_R8_UNORM)) return false;
    if (!MakeTarget(m_vol, hw, hh, DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
    if (!MakeTarget(m_volBlur, hw, hh, DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
    if (!MakeTarget(m_ldr, w, h, DXGI_FORMAT_R8G8B8A8_UNORM)) return false;
    if (!MakeTarget(m_final, w, h, DXGI_FORMAT_R8G8B8A8_UNORM)) return false;
    if (!MakeTarget(m_lum, LUM_SIZE, LUM_SIZE, DXGI_FORMAT_R16_FLOAT, true)) return false;
    int bw = hw;
    int bh = hh;
    for (auto& b : m_bloom) {
        if (!MakeTarget(b, std::max(1, bw), std::max(1, bh), DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
        bw /= 2;
        bh /= 2;
    }
    for (auto& a : m_adapt) {
        if (!MakeTarget(a, 1, 1, DXGI_FORMAT_R32_FLOAT)) return false;
    }
    m_adaptInit = false;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32_TYPELESS;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(m_dev->CreateTexture2D(&td, nullptr, &m_depthTex))) return Fail("depth tex");
    D3D11_DEPTH_STENCIL_VIEW_DESC dv{};
    dv.Format = DXGI_FORMAT_D32_FLOAT;
    dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(m_dev->CreateDepthStencilView(m_depthTex.Get(), &dv, &m_dsv))) return Fail("depth dsv");
    D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = DXGI_FORMAT_R32_FLOAT;
    sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sv.Texture2D.MipLevels = 1;
    if (FAILED(m_dev->CreateShaderResourceView(m_depthTex.Get(), &sv, &m_depthSrv))) return Fail("depth srv");

    D3D11_TEXTURE2D_DESC st{};
    st.Width = w;
    st.Height = h;
    st.MipLevels = 1;
    st.ArraySize = 1;
    st.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    st.SampleDesc.Count = 1;
    st.Usage = D3D11_USAGE_STAGING;
    st.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(m_dev->CreateTexture2D(&st, nullptr, &m_staging))) return Fail("staging");
    m_pixels.assign(static_cast<size_t>(w) * h * 4, 0);
    return true;
}

bool D3D11Renderer::Resize(int width, int height) {
    width = std::max(320, width);
    height = std::max(180, height);
    if (width == m_width && height == m_height) return true;
    m_width = width;
    m_height = height;
    return CreateScreenTargets();
}

int D3D11Renderer::CreateTexture(const uint8_t* rgba, int width, int height) {
    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.MipLevels = (width > 1 || height > 1) ? 0 : 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = td.MipLevels == 0 ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(m_dev->CreateTexture2D(&td, nullptr, &tex))) return -1;
    m_ctx->UpdateSubresource(tex.Get(), 0, nullptr, rgba, width * 4, 0);
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(m_dev->CreateShaderResourceView(tex.Get(), nullptr, &srv))) return -1;
    if (td.MipLevels == 0) m_ctx->GenerateMips(srv.Get());
    m_textures.push_back(srv);
    return static_cast<int>(m_textures.size()) - 1;
}

int D3D11Renderer::CreateMesh(const std::vector<HdVertex>& vertices, const HdMaterialDesc& material, int albedoTexture) {
    if (vertices.empty()) return -1;
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(HdVertex));
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertices.data();
    MeshGpu mesh;
    if (FAILED(m_dev->CreateBuffer(&bd, &sd, &mesh.vb))) return -1;
    mesh.count = static_cast<UINT>(vertices.size());
    mesh.bmin = {1e9f, 1e9f, 1e9f};
    mesh.bmax = {-1e9f, -1e9f, -1e9f};
    for (const HdVertex& v : vertices) {
        mesh.bmin = {std::min(mesh.bmin.x, v.px), std::min(mesh.bmin.y, v.py), std::min(mesh.bmin.z, v.pz)};
        mesh.bmax = {std::max(mesh.bmax.x, v.px), std::max(mesh.bmax.y, v.py), std::max(mesh.bmax.z, v.pz)};
    }
    mesh.material = material;
    mesh.texture = (albedoTexture >= 0 && albedoTexture < static_cast<int>(m_textures.size())) ? albedoTexture : 0;
    m_meshes.push_back(std::move(mesh));
    return static_cast<int>(m_meshes.size()) - 1;
}

void D3D11Renderer::UpdateFrameCB(const CBFrame& cb) {
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (SUCCEEDED(m_ctx->Map(m_cbFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        std::memcpy(ms.pData, &cb, sizeof(cb));
        m_ctx->Unmap(m_cbFrame.Get(), 0);
    }
}

void D3D11Renderer::UpdatePostCB(float a, float b, float c, float d, float e, float f, float g, float h) {
    CBPost cb{{a, b, c, d}, {e, f, g, h}};
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (SUCCEEDED(m_ctx->Map(m_cbPost.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        std::memcpy(ms.pData, &cb, sizeof(cb));
        m_ctx->Unmap(m_cbPost.Get(), 0);
    }
}

void D3D11Renderer::UnbindSrvs() {
    ID3D11ShaderResourceView* none[10] = {};
    m_ctx->PSSetShaderResources(0, 10, none);
}

void D3D11Renderer::SetViewport(int w, int h) {
    D3D11_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f};
    m_ctx->RSSetViewports(1, &vp);
}

void D3D11Renderer::DrawFull(ID3D11PixelShader* ps) {
    m_ctx->IASetInputLayout(nullptr);
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_ctx->VSSetShader(m_vsFull.Get(), nullptr, 0);
    m_ctx->PSSetShader(ps, nullptr, 0);
    m_ctx->Draw(3, 0);
}

void D3D11Renderer::DrawMeshes(bool shadowOnly, bool useMaterials) {
    m_ctx->IASetInputLayout(m_layoutScene.Get());
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(HdVertex);
    UINT offset = 0;
    for (const MeshGpu& mesh : m_meshes) {
        if (!mesh.visible) continue;
        if (shadowOnly && !mesh.material.castShadow) continue;
        if (m_cullUseFrustum && !AabbVisible(m_cullFrustum, mesh.bmin, mesh.bmax)) continue;
        if (m_cullUseSphere && !AabbInSphere(mesh.bmin, mesh.bmax, m_cullCenter, m_cullRadius)) continue;
        ++m_drawn;
        if (useMaterials) {
            CBObject ob{{mesh.material.tint[0], mesh.material.tint[1], mesh.material.tint[2], mesh.material.tint[3]},
                        {mesh.material.roughness, mesh.material.metallic, mesh.material.emissive, mesh.material.bump}};
            D3D11_MAPPED_SUBRESOURCE ms{};
            if (SUCCEEDED(m_ctx->Map(m_cbObject.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
                std::memcpy(ms.pData, &ob, sizeof(ob));
                m_ctx->Unmap(m_cbObject.Get(), 0);
            }
            ID3D11ShaderResourceView* srv = m_textures[mesh.texture].Get();
            m_ctx->PSSetShaderResources(0, 1, &srv);
        }
        ID3D11Buffer* vb = mesh.vb.Get();
        m_ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        m_ctx->Draw(mesh.count, 0);
    }
}

void D3D11Renderer::RenderShadowView(const Mat& viewProj, F3 origin, float radius, ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv,
                                     int size, CBFrame& cb) {
    std::memcpy(cb.spotViewProj, viewProj.m, sizeof(cb.spotViewProj));
    UpdateFrameCB(cb);
    UpdatePostCB(origin.x, origin.y, origin.z, 1.0f / radius);
    const float one[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_ctx->ClearRenderTargetView(rtv, one);
    m_ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
    m_ctx->OMSetRenderTargets(1, &rtv, dsv);
    SetViewport(size, size);
    m_ctx->VSSetShader(m_vsShadow.Get(), nullptr, 0);
    m_ctx->PSSetShader(m_psShadow.Get(), nullptr, 0);
    DrawMeshes(true, false);
}

void D3D11Renderer::DrawBillboards(const HdFrameParams& frame, F3 camPos, F3 camFwd, F3 camUp) {
    if (frame.billboards.empty()) return;
    F3 right = Norm(Cross(camFwd, camUp));
    F3 up = Cross(right, camFwd);

    std::vector<size_t> order(std::min<size_t>(frame.billboards.size(), 64));
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return Len(Sub(FromArr(frame.billboards[a].center), camPos)) > Len(Sub(FromArr(frame.billboards[b].center), camPos));
    });

    std::vector<BillboardVertex> verts;
    verts.reserve(order.size() * 6);
    for (size_t idx : order) {
        const HdBillboard& bb = frame.billboards[idx];
        F3 c = FromArr(bb.center);
        float hw = bb.size[0] * 0.5f;
        float hh = bb.size[1] * 0.5f;
        F3 p0{c.x - right.x * hw - up.x * hh, c.y - right.y * hw - up.y * hh, c.z - right.z * hw - up.z * hh};
        F3 p1{c.x + right.x * hw - up.x * hh, c.y + right.y * hw - up.y * hh, c.z + right.z * hw - up.z * hh};
        F3 p2{c.x + right.x * hw + up.x * hh, c.y + right.y * hw + up.y * hh, c.z + right.z * hw + up.z * hh};
        F3 p3{c.x - right.x * hw + up.x * hh, c.y - right.y * hw + up.y * hh, c.z - right.z * hw + up.z * hh};
        BillboardVertex v0{p0.x, p0.y, p0.z, 0.0f, 1.0f, bb.tint[0], bb.tint[1], bb.tint[2], bb.tint[3]};
        BillboardVertex v1{p1.x, p1.y, p1.z, 1.0f, 1.0f, bb.tint[0], bb.tint[1], bb.tint[2], bb.tint[3]};
        BillboardVertex v2{p2.x, p2.y, p2.z, 1.0f, 0.0f, bb.tint[0], bb.tint[1], bb.tint[2], bb.tint[3]};
        BillboardVertex v3{p3.x, p3.y, p3.z, 0.0f, 0.0f, bb.tint[0], bb.tint[1], bb.tint[2], bb.tint[3]};
        verts.insert(verts.end(), {v0, v1, v2, v0, v2, v3});
    }

    D3D11_MAPPED_SUBRESOURCE ms{};
    if (FAILED(m_ctx->Map(m_billboardVb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return;
    std::memcpy(ms.pData, verts.data(), verts.size() * sizeof(BillboardVertex));
    m_ctx->Unmap(m_billboardVb.Get(), 0);

    m_ctx->IASetInputLayout(m_layoutBillboard.Get());
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(BillboardVertex);
    UINT offset = 0;
    ID3D11Buffer* vb = m_billboardVb.Get();
    m_ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    m_ctx->VSSetShader(m_vsBillboard.Get(), nullptr, 0);
    m_ctx->PSSetShader(m_psBillboard.Get(), nullptr, 0);
    m_ctx->OMSetBlendState(m_bsPremult.Get(), nullptr, 0xFFFFFFFF);
    m_ctx->OMSetDepthStencilState(m_dsRead.Get(), 0);

    for (size_t k = 0; k < order.size(); ++k) {
        const HdBillboard& bb = frame.billboards[order[k]];
        CBObject ob{{1, 1, 1, 1}, {0, 0, bb.emissive, 0}};
        D3D11_MAPPED_SUBRESOURCE oms{};
        if (SUCCEEDED(m_ctx->Map(m_cbObject.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &oms))) {
            std::memcpy(oms.pData, &ob, sizeof(ob));
            m_ctx->Unmap(m_cbObject.Get(), 0);
        }
        int t = (bb.texture >= 0 && bb.texture < static_cast<int>(m_textures.size())) ? bb.texture : 0;
        ID3D11ShaderResourceView* srv = m_textures[t].Get();
        m_ctx->PSSetShaderResources(0, 1, &srv);
        m_ctx->Draw(6, static_cast<UINT>(k * 6));
    }
    m_ctx->OMSetBlendState(m_bsOpaque.Get(), nullptr, 0xFFFFFFFF);
}

bool D3D11Renderer::RenderFrame(const HdFrameParams& frame) {
    if (!m_ready) return false;
    ++m_frame;
    m_drawn = 0;
    const int w = m_width;
    const int h = m_height;
    const int hw = std::max(1, w / 2);
    const int hh = std::max(1, h / 2);

    F3 camPos = FromArr(frame.camPos);
    F3 camFwd = Norm(Sub(FromArr(frame.camTarget), camPos));
    F3 camUp = FromArr(frame.camUp);
    float aspect = static_cast<float>(w) / static_cast<float>(h);
    Mat view = ViewFromRightHanded(camPos, camFwd, camUp);
    Mat proj = PerspectiveLH(frame.fovY * PI_F / 180.0f, aspect, frame.nearZ, frame.farZ);
    Mat viewProj = Mul(view, proj);

    CBFrame cb{};
    std::memcpy(cb.viewProj, viewProj.m, sizeof(cb.viewProj));
    Mat invVP = Inverse(viewProj);
    std::memcpy(cb.invViewProj, invVP.m, sizeof(cb.invViewProj));
    Mat identity = Identity();
    std::memcpy(cb.spotViewProj, identity.m, sizeof(cb.spotViewProj));
    cb.camPos[0] = camPos.x; cb.camPos[1] = camPos.y; cb.camPos[2] = camPos.z; cb.camPos[3] = frame.time;
    cb.camFwd[0] = camFwd.x; cb.camFwd[1] = camFwd.y; cb.camFwd[2] = camFwd.z; cb.camFwd[3] = frame.nearZ;
    cb.screen[0] = static_cast<float>(w); cb.screen[1] = static_cast<float>(h);
    cb.screen[2] = 1.0f / w; cb.screen[3] = 1.0f / h;
    cb.proj[0] = frame.nearZ; cb.proj[1] = frame.farZ; cb.proj[2] = std::tan(frame.fovY * PI_F / 360.0f); cb.proj[3] = aspect;
    for (int i = 0; i < 3; ++i) {
        cb.ambientSky[i] = frame.ambientSky[i];
        cb.ambientGround[i] = frame.ambientGround[i];
        cb.fog[i] = frame.fogColor[i];
        cb.spotColor[i] = frame.spot.color[i] * frame.spot.intensity;
    }
    cb.ambientGround[3] = frame.bakedScale;
    cb.fog[3] = frame.fogDensity;
    cb.spotPos[0] = frame.spot.pos[0]; cb.spotPos[1] = frame.spot.pos[1]; cb.spotPos[2] = frame.spot.pos[2]; cb.spotPos[3] = frame.spot.range;
    F3 spotDir = Norm(FromArr(frame.spot.dir));
    cb.spotDir[0] = spotDir.x; cb.spotDir[1] = spotDir.y; cb.spotDir[2] = spotDir.z; cb.spotDir[3] = frame.spot.enabled ? 1.0f : 0.0f;
    cb.spotCones[0] = frame.spot.innerCos; cb.spotCones[1] = frame.spot.outerCos; cb.spotCones[2] = 1.0f / m_spotSize;

    struct Candidate {
        float score;
        const HdLightDesc* light;
    };
    std::vector<Candidate> cands;
    for (const HdLightDesc& l : frame.lights) {
        float d = Len(Sub(FromArr(l.pos), camPos)) - l.radius;
        if (d > 40.0f) continue;
        cands.push_back({d, &l});
    }
    std::sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) { return a.score < b.score; });
    if (cands.size() > MAX_LIGHTS) cands.resize(MAX_LIGHTS);

    std::array<bool, CUBE_SLOTS> slotUsed{};
    std::array<int, MAX_LIGHTS> lightSlot{};
    lightSlot.fill(-1);
    std::array<bool, CUBE_SLOTS> slotDirty{};
    for (size_t i = 0; i < cands.size(); ++i) {
        const HdLightDesc& l = *cands[i].light;
        if (l.id < 0) continue;
        for (int s = 0; s < CUBE_SLOTS; ++s) {
            if (m_slotLight[s] == l.id && !slotUsed[s]) {
                lightSlot[i] = s;
                slotUsed[s] = true;
                break;
            }
        }
    }
    for (size_t i = 0; i < cands.size(); ++i) {
        const HdLightDesc& l = *cands[i].light;
        if (l.id < 0 || lightSlot[i] >= 0) continue;
        for (int s = 0; s < CUBE_SLOTS; ++s) {
            if (!slotUsed[s]) {
                lightSlot[i] = s;
                slotUsed[s] = true;
                m_slotLight[s] = l.id;
                m_slotPos[s] = FromArr(l.pos);
                m_slotRadius[s] = l.radius;
                slotDirty[s] = true;
                break;
            }
        }
    }
    static const bool noShadow = std::getenv("LO_HD_NOSHADOW") != nullptr;
    if (noShadow) lightSlot.fill(-1);
    for (size_t i = 0; i < cands.size(); ++i) {
        const HdLightDesc& l = *cands[i].light;
        cb.lightPos[i][0] = l.pos[0]; cb.lightPos[i][1] = l.pos[1]; cb.lightPos[i][2] = l.pos[2]; cb.lightPos[i][3] = l.radius;
        cb.lightColor[i][0] = l.color[0] * l.intensity;
        cb.lightColor[i][1] = l.color[1] * l.intensity;
        cb.lightColor[i][2] = l.color[2] * l.intensity;
        cb.lightColor[i][3] = static_cast<float>(lightSlot[i]);
    }
    cb.counts[0] = static_cast<float>(cands.size());
    cb.counts[1] = static_cast<float>(m_frame % 64);
    cb.counts[2] = frame.exposureBias;
    cb.counts[3] = frame.bloomIntensity;
    cb.grade[0] = frame.grain;
    cb.grade[1] = frame.vignette;
    cb.grade[2] = frame.saturation;
    cb.grade[3] = frame.dt;
    cb.fx[0] = frame.fear; cb.fx[1] = frame.horror; cb.fx[2] = frame.chromatic; cb.fx[3] = frame.heartbeat;

    ID3D11Buffer* cbs[3] = {m_cbFrame.Get(), m_cbObject.Get(), m_cbPost.Get()};
    m_ctx->VSSetConstantBuffers(0, 3, cbs);
    m_ctx->PSSetConstantBuffers(0, 3, cbs);
    ID3D11SamplerState* samps[2] = {m_sampLinear.Get(), m_sampClamp.Get()};
    m_ctx->PSSetSamplers(0, 2, samps);
    m_ctx->OMSetBlendState(m_bsOpaque.Get(), nullptr, 0xFFFFFFFF);

    m_ctx->RSSetState(m_rsNone.Get());
    m_ctx->OMSetDepthStencilState(m_dsLess.Get(), 0);
    UnbindSrvs();

    static const F3 faceDir[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    static const F3 faceUp[6] = {{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
    for (int s = 0; s < CUBE_SLOTS; ++s) {
        if (!slotDirty[s]) continue;
        Mat p = PerspectiveLH(PI_F * 0.5f, 1.0f, 0.05f, m_slotRadius[s]);
        m_cullUseFrustum = false;
        m_cullUseSphere = true;
        m_cullCenter = m_slotPos[s];
        m_cullRadius = m_slotRadius[s];
        for (int f = 0; f < 6; ++f) {
            Mat v = LookAtLH(m_slotPos[s], faceDir[f], faceUp[f]);
            RenderShadowView(Mul(v, p), m_slotPos[s], m_slotRadius[s], m_cubeRtv[s * 6 + f].Get(), m_cubeDsv.Get(), m_cubeSize, cb);
        }
    }
    if (frame.spot.enabled) {
        float fov = std::clamp(2.0f * std::acos(std::clamp(frame.spot.outerCos, 0.1f, 0.999f)) * 1.15f, 0.3f, 2.4f);
        Mat v = LookAtLH(FromArr(frame.spot.pos), spotDir, std::abs(spotDir.y) > 0.95f ? F3{1, 0, 0} : F3{0, 1, 0});
        Mat p = PerspectiveLH(fov, 1.0f, 0.1f, frame.spot.range);
        Mat vp = Mul(v, p);
        m_cullUseSphere = false;
        m_cullUseFrustum = true;
        m_cullFrustum = MakeFrustum(vp);
        RenderShadowView(vp, FromArr(frame.spot.pos), frame.spot.range, m_spotRtv.Get(), m_spotDsv.Get(), m_spotSize, cb);
        std::memcpy(cb.spotViewProj, vp.m, sizeof(cb.spotViewProj));
    }
    std::memcpy(cb.viewProj, viewProj.m, sizeof(cb.viewProj));
    UpdateFrameCB(cb);

    m_cullUseSphere = false;
    m_cullUseFrustum = true;
    m_cullFrustum = MakeFrustum(viewProj);
    m_ctx->RSSetState(m_rsNone.Get());
    SetViewport(w, h);
    ID3D11RenderTargetView* nullRtv[1] = {nullptr};
    m_ctx->OMSetRenderTargets(1, nullRtv, nullptr);

    {
        const float zero[4] = {0, 0, 1, 0};
        m_ctx->ClearRenderTargetView(m_normal.rtv.Get(), zero);
        m_ctx->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        ID3D11RenderTargetView* rtv = m_normal.rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, m_dsv.Get());
        m_ctx->OMSetDepthStencilState(m_dsLess.Get(), 0);
        m_ctx->VSSetShader(m_vsScene.Get(), nullptr, 0);
        m_ctx->PSSetShader(m_psPrepass.Get(), nullptr, 0);
        DrawMeshes(false, false);
    }

    {
        m_ctx->OMSetRenderTargets(1, nullRtv, nullptr);
        m_ctx->OMSetDepthStencilState(m_dsOff.Get(), 0);
        SetViewport(hw, hh);
        ID3D11ShaderResourceView* srvs[6] = {};
        srvs[4] = m_depthSrv.Get();
        srvs[5] = m_normal.srv.Get();
        ID3D11RenderTargetView* rtv = m_ao.rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        m_ctx->PSSetShaderResources(0, 6, srvs);
        if (frame.quality.ssao) DrawFull(m_psSsao.Get());
        else {
            const float white[4] = {1, 1, 1, 1};
            m_ctx->ClearRenderTargetView(m_ao.rtv.Get(), white);
        }

        UnbindSrvs();
        rtv = m_aoBlur.rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ID3D11ShaderResourceView* blurSrvs[9] = {};
        blurSrvs[4] = m_depthSrv.Get();
        blurSrvs[6] = m_ao.srv.Get();
        m_ctx->PSSetShaderResources(0, 9, blurSrvs);
        if (frame.quality.ssao) DrawFull(m_psBlurAo.Get());
        else {
            const float white[4] = {1, 1, 1, 1};
            m_ctx->ClearRenderTargetView(m_aoBlur.rtv.Get(), white);
        }
        UnbindSrvs();
    }

    {
        SetViewport(w, h);
        const float clear[4] = {frame.fogColor[0], frame.fogColor[1], frame.fogColor[2], 1.0f};
        m_ctx->ClearRenderTargetView(m_hdr.rtv.Get(), clear);
        ID3D11RenderTargetView* rtv = m_hdr.rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, m_dsv.Get());
        m_ctx->OMSetDepthStencilState(m_dsEqual.Get(), 0);
        m_ctx->VSSetShader(m_vsScene.Get(), nullptr, 0);
        m_ctx->PSSetShader(m_psScene.Get(), nullptr, 0);
        ID3D11ShaderResourceView* common[4] = {nullptr, m_aoBlur.srv.Get(), m_spotSrv.Get(), m_cubeSrv.Get()};
        m_ctx->PSSetShaderResources(0, 4, common);
        DrawMeshes(false, true);
        ID3D11ShaderResourceView* keep[4] = {nullptr, m_aoBlur.srv.Get(), m_spotSrv.Get(), m_cubeSrv.Get()};
        m_ctx->PSSetShaderResources(0, 4, keep);
        DrawBillboards(frame, camPos, camFwd, camUp);
        UnbindSrvs();
    }

    {
        m_ctx->OMSetDepthStencilState(m_dsOff.Get(), 0);
        SetViewport(hw, hh);
        ID3D11RenderTargetView* rtv = m_vol.rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        UpdatePostCB(frame.scatterDensity, static_cast<float>(std::clamp(frame.quality.volumetricSteps, 8, 64)), 0, 0);
        ID3D11ShaderResourceView* srvs[6] = {nullptr, nullptr, m_spotSrv.Get(), m_cubeSrv.Get(), m_depthSrv.Get(), nullptr};
        m_ctx->PSSetShaderResources(0, 6, srvs);
        const bool volOn = frame.quality.volumetrics;
        const float black[4] = {0, 0, 0, 1};
        if (volOn) DrawFull(m_psVol.Get());
        else m_ctx->ClearRenderTargetView(m_vol.rtv.Get(), black);
        UnbindSrvs();

        rtv = m_volBlur.rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        UpdatePostCB(1.0f / hw, 1.0f / hh, 0, 0);
        ID3D11ShaderResourceView* b[7] = {};
        b[6] = m_vol.srv.Get();
        m_ctx->PSSetShaderResources(0, 7, b);
        DrawFull(m_psBlurVol.Get());
        UnbindSrvs();
    }

    if (!frame.quality.bloom) {
        const float black[4] = {0, 0, 0, 1};
        m_ctx->ClearRenderTargetView(m_bloom[0].rtv.Get(), black);
    } else {
        ID3D11ShaderResourceView* srvs[7] = {};
        UpdatePostCB(1.0f / w, 1.0f / h, 0.8f, 0.5f);
        ID3D11RenderTargetView* rtv = m_bloom[0].rtv.Get();
        SetViewport(m_bloom[0].w, m_bloom[0].h);
        m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        srvs[6] = m_hdr.srv.Get();
        m_ctx->PSSetShaderResources(0, 7, srvs);
        DrawFull(m_psBloomPre.Get());
        UnbindSrvs();

        for (int i = 1; i < BLOOM_LEVELS; ++i) {
            rtv = m_bloom[i].rtv.Get();
            SetViewport(m_bloom[i].w, m_bloom[i].h);
            m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
            UpdatePostCB(1.0f / m_bloom[i - 1].w, 1.0f / m_bloom[i - 1].h, 0, 0);
            srvs[6] = m_bloom[i - 1].srv.Get();
            m_ctx->PSSetShaderResources(0, 7, srvs);
            DrawFull(m_psBloomDown.Get());
            UnbindSrvs();
        }
        m_ctx->OMSetBlendState(m_bsAdd.Get(), nullptr, 0xFFFFFFFF);
        for (int i = BLOOM_LEVELS - 2; i >= 0; --i) {
            rtv = m_bloom[i].rtv.Get();
            SetViewport(m_bloom[i].w, m_bloom[i].h);
            m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
            UpdatePostCB(1.0f / m_bloom[i + 1].w, 1.0f / m_bloom[i + 1].h, 0.85f, 0);
            srvs[6] = m_bloom[i + 1].srv.Get();
            m_ctx->PSSetShaderResources(0, 7, srvs);
            DrawFull(m_psBloomUp.Get());
            UnbindSrvs();
        }
        m_ctx->OMSetBlendState(m_bsOpaque.Get(), nullptr, 0xFFFFFFFF);
    }

    {
        ID3D11ShaderResourceView* srvs[8] = {};
        SetViewport(LUM_SIZE, LUM_SIZE);
        ID3D11RenderTargetView* rtv = m_lum.rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        srvs[6] = m_hdr.srv.Get();
        m_ctx->PSSetShaderResources(0, 8, srvs);
        DrawFull(m_psLum.Get());
        UnbindSrvs();
        m_ctx->GenerateMips(m_lum.srv.Get());

        if (!m_adaptInit) {
            const float init[4] = {1.0f, 0, 0, 0};
            m_ctx->ClearRenderTargetView(m_adapt[0].rtv.Get(), init);
            m_ctx->ClearRenderTargetView(m_adapt[1].rtv.Get(), init);
            m_adaptInit = true;
        }
        int prev = m_adaptIndex;
        int next = 1 - m_adaptIndex;
        SetViewport(1, 1);
        rtv = m_adapt[next].rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        UpdatePostCB(frame.dt, 7.0f, 0, 0);
        srvs[6] = m_lum.srv.Get();
        srvs[7] = m_adapt[prev].srv.Get();
        m_ctx->PSSetShaderResources(0, 8, srvs);
        DrawFull(m_psAdapt.Get());
        UnbindSrvs();
        m_adaptIndex = next;
    }

    {
        SetViewport(w, h);
        ID3D11ShaderResourceView* srvs[10] = {};
        ID3D11RenderTargetView* rtv = m_ldr.rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        srvs[6] = m_hdr.srv.Get();
        srvs[7] = m_volBlur.srv.Get();
        srvs[8] = m_bloom[0].srv.Get();
        srvs[9] = m_adapt[m_adaptIndex].srv.Get();
        m_ctx->PSSetShaderResources(0, 10, srvs);
        DrawFull(m_psComposite.Get());
        UnbindSrvs();

        rtv = m_final.rtv.Get();
        m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ID3D11ShaderResourceView* f[7] = {};
        f[6] = m_ldr.srv.Get();
        m_ctx->PSSetShaderResources(0, 7, f);
        DrawFull(frame.quality.fxaa ? m_psFxaa.Get() : m_psCopy.Get());
        UnbindSrvs();
        m_ctx->OMSetRenderTargets(1, nullRtv, nullptr);
    }

    m_ctx->CopyResource(m_staging.Get(), m_final.tex.Get());
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (FAILED(m_ctx->Map(m_staging.Get(), 0, D3D11_MAP_READ, 0, &ms))) return Fail("map staging");
    const uint8_t* src = static_cast<const uint8_t*>(ms.pData);
    for (int y = 0; y < h; ++y) {
        std::memcpy(&m_pixels[static_cast<size_t>(y) * w * 4], src + static_cast<size_t>(y) * ms.RowPitch, static_cast<size_t>(w) * 4);
    }
    m_ctx->Unmap(m_staging.Get(), 0);
    return true;
}


std::unique_ptr<IHdRenderer> CreateHdRenderer(HdApi api) {
    if (api == HdApi::D3D11) return std::make_unique<D3D11Renderer>();
    if (api == HdApi::D3D12) return CreateD3D12Renderer();
    return nullptr;
}

}
