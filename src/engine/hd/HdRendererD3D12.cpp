#include "engine/hd/HdRenderer.h"
#include "engine/hd/HdShaderCache.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

namespace engine::hd {

using Microsoft::WRL::ComPtr;

namespace {

constexpr int CUBE_SLOTS = 4;
constexpr int MAX_LIGHTS = 8;
constexpr int BLOOM_LEVELS = 6;
constexpr int LUM_LEVELS = 8;
constexpr int SRV_TABLE = 10;
constexpr int TEX_BASE = 1024;
constexpr int MAX_TEXTURES = 512;
constexpr float PI_F = 3.14159265f;
constexpr UINT64 UPLOAD_SIZE = 16ull * 1024 * 1024;
constexpr UINT STATE_AS = 0x400000;

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
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[i * 4 + k] * b.m[k * 4 + j];
            r.m[i * 4 + j] = s;
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

struct ObjectConstants {
    float tint[4];
    float mat[4];
    UINT idx[4];
};

struct BillboardVertex {
    float x, y, z, u, v, r, g, b, a;
};

struct DxcBuffer {
    const void* Ptr;
    SIZE_T Size;
    UINT Encoding;
};
struct IDxcBlobX : IUnknown {
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual SIZE_T STDMETHODCALLTYPE GetBufferSize() = 0;
};
struct IDxcResultX : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetStatus(HRESULT* st) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResult(void** r) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetErrorBuffer(void** r) = 0;
    virtual BOOL STDMETHODCALLTYPE HasOutput(UINT kind) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOutput(UINT kind, REFIID iid, void** obj, void** name) = 0;
};
struct IDxcCompiler3X : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Compile(const DxcBuffer* src, LPCWSTR* args, UINT argc, void* inc, REFIID riid, LPVOID* result) = 0;
};
const GUID CLSID_Dxc = {0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}};
const GUID IID_Dxc3 = {0x228b4687, 0x5a6a, 0x4730, {0x90, 0x0c, 0x97, 0x02, 0xb2, 0x20, 0x3f, 0x54}};
const GUID IID_DxcResult = {0x58346cda, 0xdde7, 0x4497, {0x94, 0x61, 0x6f, 0x87, 0xaf, 0x5e, 0x06, 0x59}};
const GUID IID_DxcBlob = {0x8ba5fb08, 0x5195, 0x40e2, {0xac, 0x58, 0x0d, 0x98, 0x9c, 0x3a, 0x01, 0x02}};
const GUID IID_Device5 = {0x8b4f173b, 0x2fea, 0x4b80, {0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d}};
const GUID IID_List4 = {0x8754318e, 0xd3a9, 0x4541, {0x98, 0xcf, 0x64, 0x5b, 0x50, 0xdc, 0x48, 0x74}};

struct RtGeomTriangles {
    D3D12_GPU_VIRTUAL_ADDRESS transform;
    DXGI_FORMAT indexFormat;
    DXGI_FORMAT vertexFormat;
    UINT indexCount;
    UINT vertexCount;
    D3D12_GPU_VIRTUAL_ADDRESS indexBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS vertexAddr;
    UINT64 vertexStride;
};
struct RtGeomDesc {
    UINT type;
    UINT flags;
    RtGeomTriangles tri;
};
struct RtAsInputs {
    UINT type;
    UINT flags;
    UINT numDescs;
    UINT layout;
    D3D12_GPU_VIRTUAL_ADDRESS addrOrPtr;
};
struct RtAsBuildDesc {
    D3D12_GPU_VIRTUAL_ADDRESS dest;
    RtAsInputs inputs;
    D3D12_GPU_VIRTUAL_ADDRESS source;
    D3D12_GPU_VIRTUAL_ADDRESS scratch;
};
struct RtPrebuild {
    UINT64 result;
    UINT64 scratch;
    UINT64 update;
};
struct RtInstance {
    float transform[3][4];
    UINT idMask;
    UINT contribFlags;
    D3D12_GPU_VIRTUAL_ADDRESS blas;
};

struct Res {
    ComPtr<ID3D12Resource> r;
    D3D12_RESOURCE_STATES st = D3D12_RESOURCE_STATE_COMMON;
    int w = 0;
    int h = 0;
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
};

struct MeshGpu {
    ComPtr<ID3D12Resource> vb;
    UINT count = 0;
    HdMaterialDesc material;
    int texture = 0;
    bool visible = true;
    F3 bmin{};
    F3 bmax{};
    ComPtr<ID3D12Resource> blas;
    bool rt = false;
};

HMODULE g_dxcModule = nullptr;

std::wstring FindDxcDir(const std::string& assetDir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<fs::path> candidates = {fs::path(assetDir) / "dxc", fs::path(assetDir) / ".." , fs::current_path()};
    for (const auto& c : candidates) {
        if (fs::exists(c / "dxcompiler.dll", ec) && fs::exists(c / "dxil.dll", ec)) return fs::absolute(c, ec).wstring();
    }
    fs::path kits = "C:/Program Files (x86)/Windows Kits/10/bin";
    if (fs::exists(kits, ec)) {
        std::vector<fs::path> versions;
        for (const auto& e : fs::directory_iterator(kits, ec)) versions.push_back(e.path());
        std::sort(versions.rbegin(), versions.rend());
        for (const auto& v : versions) {
            fs::path d = v / "x64";
            if (fs::exists(d / "dxcompiler.dll", ec) && fs::exists(d / "dxil.dll", ec)) return fs::absolute(d, ec).wstring();
        }
    }
    return L"";
}

}

HdCapabilities ProbeCapabilities(const std::string& assetDir) {
    HdCapabilities caps;
    caps.d3d11 = true;
    caps.dxc = !FindDxcDir(assetDir).empty();
    ComPtr<ID3D12Device> dev;
    if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) {
        caps.d3d12 = true;
        struct Opt5 {
            BOOL tiled;
            UINT renderPasses;
            UINT raytracing;
        } o{};
        if (SUCCEEDED(dev->CheckFeatureSupport(static_cast<D3D12_FEATURE>(27), &o, sizeof(o)))) caps.dxr = o.raytracing >= 11;
        ComPtr<IDXGIFactory4> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
            ComPtr<IDXGIAdapter1> ad;
            if (SUCCEEDED(factory->EnumAdapters1(0, &ad))) {
                DXGI_ADAPTER_DESC1 d{};
                ad->GetDesc1(&d);
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%ls", d.Description);
                caps.adapter = buf;
            }
        }
    }
    return caps;
}

class D3D12Renderer final : public IHdRenderer {
public:
    ~D3D12Renderer() override { Shutdown(); }

    bool Init(const HdInitDesc& desc) override;
    void Shutdown() override;
    bool Resize(int width, int height) override;
    int CreateTexture(const uint8_t* rgba, int width, int height) override;
    int CreateMesh(const std::vector<HdVertex>& vertices, const HdMaterialDesc& material, int albedoTexture) override;
    void SetMeshVisible(int mesh, bool visible) override {
        if (mesh >= 0 && mesh < static_cast<int>(m_meshes.size())) m_meshes[mesh].visible = visible;
    }
    void FinishLoading() override;
    bool RenderFrame(const HdFrameParams& frame) override;
    const uint8_t* Pixels() const override { return m_pixels.data(); }
    int Width() const override { return m_width; }
    int Height() const override { return m_height; }
    const char* ApiName() const override { return "Direct3D 12"; }
    const std::string& AdapterName() const override { return m_adapter; }
    bool SupportsRayTracing() const override { return m_rtReady; }
    const std::string& LastError() const override { return m_error; }
    int DrawnMeshes() const override { return m_drawn; }

private:
    bool Fail(const std::string& msg) {
        m_error = msg;
        std::fprintf(stderr, "HD D3D12: %s\n", msg.c_str());
        return false;
    }
    ComPtr<ID3D12Resource> MakeBuffer(UINT64 size, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    bool MakeTexture(Res& t, int w, int h, DXGI_FORMAT fmt, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, int arraySize = 1, bool cube = false);
    void Tr(Res& r, D3D12_RESOURCE_STATES st);
    void TrRaw(ID3D12Resource* res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);
    void UavBarrier(ID3D12Resource* r);
    void Flush();
    void WaitIdle();
    D3D12_CPU_DESCRIPTOR_HANDLE RtvCpu(int i) const { return {m_rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(i) * m_rtvInc}; }
    D3D12_CPU_DESCRIPTOR_HANDLE DsvCpu(int i) const { return {m_dsvHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(i) * m_dsvInc}; }
    D3D12_CPU_DESCRIPTOR_HANDLE SrvCpu(int i) const { return {m_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(i) * m_srvInc}; }
    D3D12_GPU_DESCRIPTOR_HANDLE SrvGpu(int i) const { return {m_srvHeap->GetGPUDescriptorHandleForHeapStart().ptr + static_cast<UINT64>(i) * m_srvInc}; }
    void WriteSrv(int index, ID3D12Resource* res, DXGI_FORMAT fmt, int kind, int mips = 1);
    int NewTable();
    void SetTable(int table, std::initializer_list<std::pair<int, Res*>> entries);
    UINT64 Alloc(UINT64 size, void** cpu);

    bool InitDxc(const std::string& assetDir);
    bool Compile(const std::string& entry, const char* stage, std::vector<uint8_t>& out);
    bool BuildShaders(const std::string& dir);
    bool BuildRootSignature();
    ComPtr<ID3D12PipelineState> MakePso(const char* vs, const char* ps, int layout, DXGI_FORMAT rtv, DXGI_FORMAT dsv, int blend, int depth);
    bool BuildPipelines();
    bool CreateFixedTargets();
    bool CreateScreenTargets();
    void BuildTables();
    void BuildAccelerationStructures();
    void SetupFrameCommon(UINT64 cbAddr);
    UINT64 PushFrameCB(const CBFrame& cb);
    void DrawMeshes(bool shadowOnly, bool materials);
    void DrawBillboards(const HdFrameParams& frame, F3 camPos, F3 camFwd, F3 camUp);
    void DrawFull(ID3D12PipelineState* pso);
    void SetRt(int rtvIndex, int dsvIndex, int w, int h);
    void RenderShadowView(const Mat& vp, F3 origin, float radius, int rtvIndex, int dsvIndex, int size, CBFrame& cb);
    void Post(float a, float b, float c, float d, float e = 0, float f = 0, float g = 0, float h = 0);

    ComPtr<ID3D12Device> m_dev;
    ComPtr<ID3D12CommandQueue> m_queue;
    ComPtr<ID3D12CommandAllocator> m_alloc;
    ComPtr<ID3D12GraphicsCommandList> m_list;
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_event = nullptr;
    UINT64 m_fenceValue = 0;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap, m_dsvHeap, m_srvHeap;
    UINT m_rtvInc = 0, m_dsvInc = 0, m_srvInc = 0;
    ComPtr<ID3D12RootSignature> m_root;
    std::string m_adapter, m_error, m_shaderDir, m_assetDir;
    int m_width = 0, m_height = 0, m_spotSize = 2048, m_cubeSize = 512, m_drawn = 0;
    uint32_t m_frame = 0;
    bool m_ready = false;
    HdQuality m_quality;

    bool m_dxc = false;
    bool m_rtReady = false;
    IDxcCompiler3X* m_compiler = nullptr;
    std::map<std::string, std::vector<uint8_t>> m_shaders;
    std::map<std::string, ComPtr<ID3D12PipelineState>> m_psos;

    ComPtr<ID3D12Resource> m_upload;
    uint8_t* m_uploadMapped = nullptr;
    UINT64 m_uploadOffset = 0;
    std::vector<ComPtr<ID3D12Resource>> m_pending;
    ComPtr<ID3D12Resource> m_readback;
    UINT64 m_readbackSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_readbackFp{};

    Res m_hdr, m_normal, m_ao, m_aoBlur, m_vol, m_volBlur, m_ldr, m_final, m_depth, m_spot, m_spotDepth, m_cube, m_cubeDepth;
    std::array<Res, BLOOM_LEVELS> m_bloom;
    std::array<Res, LUM_LEVELS> m_lum;
    std::array<Res, 2> m_adapt;
    std::array<Res, 2> m_aoHist;
    int m_aoHistIndex = 0;
    Mat m_prevVP = Identity();
    std::array<int, 2> tAoTemp{}, tBlurAoHist{};
    int m_adaptIndex = 0;
    bool m_adaptInit = false;

    int m_tableCount = 0;
    int tMain = 0, tSsao = 0, tBlurAo = 0, tVol = 0, tBlurVol = 0, tBloomPre = 0, tLum = 0, tFxaa = 0, tCopy = 0;
    std::array<int, BLOOM_LEVELS> tBloomDown{}, tBloomUp{};
    std::array<int, LUM_LEVELS> tLumDown{};
    std::array<int, 2> tAdapt{}, tComp{};
    std::array<int, 2> tAdaptSrc{};

    std::vector<ComPtr<ID3D12Resource>> m_textures;
    std::vector<float> m_texAvg;
    std::vector<MeshGpu> m_meshes;
    std::vector<uint8_t> m_pixels;
    ComPtr<ID3D12Resource> m_billboardVb;
    void* m_device5 = nullptr;
    void* m_list4 = nullptr;
    ComPtr<ID3D12Resource> m_tlas, m_tlasScratch, m_instColor;
    UINT64 m_tlasScratchSize = 0;
    size_t m_rtMeshCount = 0;

    std::array<int, CUBE_SLOTS> m_slotLight{-1, -1, -1, -1};
    std::array<F3, CUBE_SLOTS> m_slotPos{};
    std::array<float, CUBE_SLOTS> m_slotRadius{};
    Frustum m_cullFrustum{};
    bool m_cullUseFrustum = false, m_cullUseSphere = false;
    F3 m_cullCenter{};
    float m_cullRadius = 0.0f;
};

ComPtr<ID3D12Resource> D3D12Renderer::MakeBuffer(UINT64 size, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags) {
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = heap;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = size;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags = flags;
    ComPtr<ID3D12Resource> r;
    if (FAILED(m_dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, state, nullptr, IID_PPV_ARGS(&r)))) return nullptr;
    return r;
}

bool D3D12Renderer::MakeTexture(Res& t, int w, int h, DXGI_FORMAT fmt, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, int arraySize, bool cube) {
    (void)cube;
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = w;
    rd.Height = h;
    rd.DepthOrArraySize = static_cast<UINT16>(arraySize);
    rd.MipLevels = 1;
    rd.Format = fmt;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    rd.Flags = flags;
    D3D12_CLEAR_VALUE cv{};
    D3D12_CLEAR_VALUE* pcv = nullptr;
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
        cv.Format = fmt;
        pcv = &cv;
    } else if (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
        cv.Format = fmt == DXGI_FORMAT_R32_TYPELESS ? DXGI_FORMAT_D32_FLOAT : fmt;
        cv.DepthStencil.Depth = 1.0f;
        pcv = &cv;
    }
    if (FAILED(m_dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, state, pcv, IID_PPV_ARGS(&t.r)))) return false;
    t.st = state;
    t.w = w;
    t.h = h;
    t.fmt = fmt;
    return true;
}

void D3D12Renderer::TrRaw(ID3D12Resource* res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    if (from == to) return;
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = res;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    m_list->ResourceBarrier(1, &b);
}

void D3D12Renderer::Tr(Res& r, D3D12_RESOURCE_STATES st) {
    if (!r.r || r.st == st) return;
    TrRaw(r.r.Get(), r.st, st);
    r.st = st;
}

void D3D12Renderer::UavBarrier(ID3D12Resource* r) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    b.UAV.pResource = r;
    m_list->ResourceBarrier(1, &b);
}

void D3D12Renderer::WaitIdle() {
    ++m_fenceValue;
    m_queue->Signal(m_fence.Get(), m_fenceValue);
    if (m_fence->GetCompletedValue() < m_fenceValue) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_event);
        WaitForSingleObject(m_event, INFINITE);
    }
}

void D3D12Renderer::Flush() {
    m_list->Close();
    ID3D12CommandList* lists[] = {m_list.Get()};
    m_queue->ExecuteCommandLists(1, lists);
    WaitIdle();
    m_pending.clear();
    m_alloc->Reset();
    m_list->Reset(m_alloc.Get(), nullptr);
}

UINT64 D3D12Renderer::Alloc(UINT64 size, void** cpu) {
    UINT64 off = (m_uploadOffset + 255) & ~255ull;
    if (off + size > UPLOAD_SIZE) {
        Fail("upload ring exhausted");
        off = 0;
    }
    m_uploadOffset = off + size;
    if (cpu) *cpu = m_uploadMapped + off;
    return m_upload->GetGPUVirtualAddress() + off;
}

void D3D12Renderer::WriteSrv(int index, ID3D12Resource* res, DXGI_FORMAT fmt, int kind, int mips) {
    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = fmt;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (kind == 1) {
        sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        sd.TextureCubeArray.MostDetailedMip = 0;
        sd.TextureCubeArray.MipLevels = 1;
        sd.TextureCubeArray.First2DArrayFace = 0;
        sd.TextureCubeArray.NumCubes = CUBE_SLOTS;
    } else {
        sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MipLevels = mips;
    }
    m_dev->CreateShaderResourceView(res, &sd, SrvCpu(index));
}

int D3D12Renderer::NewTable() {
    int base = m_tableCount * SRV_TABLE;
    ++m_tableCount;
    for (int s = 0; s < SRV_TABLE; ++s) WriteSrv(base + s, nullptr, s == 3 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM, s == 3 ? 1 : 0);
    return base;
}

void D3D12Renderer::SetTable(int table, std::initializer_list<std::pair<int, Res*>> entries) {
    for (const auto& e : entries) {
        Res* r = e.second;
        DXGI_FORMAT f = r->fmt == DXGI_FORMAT_R32_TYPELESS ? DXGI_FORMAT_R32_FLOAT : r->fmt;
        WriteSrv(table + e.first, r->r.Get(), f, e.first == 3 ? 1 : 0);
    }
}

bool D3D12Renderer::InitDxc(const std::string& assetDir) {
    std::wstring dir = FindDxcDir(assetDir);
    if (dir.empty()) return false;
    LoadLibraryW((dir + L"\\dxil.dll").c_str());
    g_dxcModule = LoadLibraryW((dir + L"\\dxcompiler.dll").c_str());
    if (!g_dxcModule) return false;
    typedef HRESULT(WINAPI * CreateFn)(REFCLSID, REFIID, LPVOID*);
    CreateFn create = reinterpret_cast<CreateFn>(reinterpret_cast<void*>(GetProcAddress(g_dxcModule, "DxcCreateInstance")));
    if (!create) return false;
    void* obj = nullptr;
    if (FAILED(create(CLSID_Dxc, IID_Dxc3, &obj)) || !obj) return false;
    m_compiler = static_cast<IDxcCompiler3X*>(obj);
    return true;
}

bool D3D12Renderer::Compile(const std::string& entry, const char* stage, std::vector<uint8_t>& out) {
    std::ifstream f(m_shaderDir + "/hd.hlsl", std::ios::binary);
    if (!f) return Fail("cannot open hd.hlsl");
    std::stringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();
    const char* dbg = std::getenv("LO_HD_DEBUG");
    std::string rt = m_rtReady ? "1" : "0";
    std::string cacheKey = src + "|" + entry + "|" + stage + "|" + (m_dxc ? "dxc" : "fxc") + "|rt" + rt + "|dbg" + (dbg ? dbg : "0") + "|v3";
    if (ShaderCacheLoad(cacheKey, out)) return true;
    if (m_dxc) {
        std::wstring wentry(entry.begin(), entry.end());
        std::wstring target = std::wstring(stage, stage + std::strlen(stage)) + L"_6_5";
        std::wstring dbgDef = L"DEBUG_VIEW=" + std::wstring(dbg ? dbg : "0", dbg ? dbg + std::strlen(dbg) : "0" + 1);
        std::wstring rtDef = L"HD_RT=" + std::wstring(rt.begin(), rt.end());
        std::vector<LPCWSTR> args = {L"-E", wentry.c_str(), L"-T", target.c_str(), L"-D", L"HD_DX12=1", L"-D", rtDef.c_str(), L"-D", dbgDef.c_str(), L"-O3"};
        DxcBuffer buf{src.data(), src.size(), 65001};
        void* resObj = nullptr;
        if (FAILED(m_compiler->Compile(&buf, args.data(), static_cast<UINT>(args.size()), nullptr, IID_DxcResult, &resObj)) || !resObj) return Fail("dxc compile call failed");
        IDxcResultX* res = static_cast<IDxcResultX*>(resObj);
        HRESULT st = E_FAIL;
        res->GetStatus(&st);
        if (FAILED(st)) {
            void* errObj = nullptr;
            std::string msg = "dxc error";
            if (SUCCEEDED(res->GetOutput(2, IID_DxcBlob, &errObj, nullptr)) && errObj) {
                IDxcBlobX* eb = static_cast<IDxcBlobX*>(errObj);
                msg = std::string(static_cast<const char*>(eb->GetBufferPointer()), eb->GetBufferSize());
                eb->Release();
            }
            res->Release();
            return Fail("shader " + entry + ": " + msg);
        }
        void* objObj = nullptr;
        if (FAILED(res->GetOutput(1, IID_DxcBlob, &objObj, nullptr)) || !objObj) {
            res->Release();
            return Fail("dxc no object for " + entry);
        }
        IDxcBlobX* ob = static_cast<IDxcBlobX*>(objObj);
        out.assign(static_cast<uint8_t*>(ob->GetBufferPointer()), static_cast<uint8_t*>(ob->GetBufferPointer()) + ob->GetBufferSize());
        ob->Release();
        res->Release();
        ShaderCacheStore(cacheKey, out);
        return true;
    }
    std::string target = std::string(stage) + "_5_1";
    D3D_SHADER_MACRO macros[] = {{"HD_DX12", "1"}, {"HD_RT", "0"}, {"DEBUG_VIEW", dbg ? dbg : "0"}, {nullptr, nullptr}};
    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3DCompile(src.data(), src.size(), "hd.hlsl", macros, nullptr, entry.c_str(), target.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err))) {
        return Fail("fxc " + entry + ": " + (err ? static_cast<const char*>(err->GetBufferPointer()) : "error"));
    }
    out.assign(static_cast<uint8_t*>(blob->GetBufferPointer()), static_cast<uint8_t*>(blob->GetBufferPointer()) + blob->GetBufferSize());
    ShaderCacheStore(cacheKey, out);
    return true;
}

bool D3D12Renderer::BuildShaders(const std::string& dir) {
    m_shaderDir = dir;
    struct E {
        const char* name;
        const char* stage;
    };
    std::vector<E> entries = {{"VSScene", "vs"},         {"VSShadowPos", "vs"},     {"VSBillboard", "vs"},   {"VSFull", "vs"},        {"PSPrepass", "ps"},
                              {"PSScene", "ps"},         {"PSAoTemporal", "ps"}, {"PSShadow", "ps"},        {"PSBillboard", "ps"},   {"PSSsao", "ps"},        {"PSBlurAo", "ps"},
                              {"PSBloomPrefilter", "ps"}, {"PSBloomDown", "ps"},    {"PSBloomUp", "ps"},     {"PSLum", "ps"},         {"PSLumDown", "ps"},
                              {"PSAdapt", "ps"},         {"PSVolumetric", "ps"},    {"PSBlurVol", "ps"},     {"PSComposite", "ps"},   {"PSFxaa", "ps"},
                              {"PSCopy", "ps"}};
    if (m_rtReady) entries.push_back({"PSRtAo", "ps"});
    for (const E& e : entries) {
        std::vector<uint8_t> blob;
        if (!Compile(e.name, e.stage, blob)) return false;
        m_shaders[e.name] = std::move(blob);
    }
    return true;
}

bool D3D12Renderer::BuildRootSignature() {
    D3D12_DESCRIPTOR_RANGE rangeTable{};
    rangeTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeTable.NumDescriptors = SRV_TABLE;
    rangeTable.BaseShaderRegister = 0;
    rangeTable.RegisterSpace = 0;
    rangeTable.OffsetInDescriptorsFromTableStart = 0;
    D3D12_DESCRIPTOR_RANGE rangeTex = rangeTable;
    rangeTex.NumDescriptors = MAX_TEXTURES;
    rangeTex.RegisterSpace = 1;

    D3D12_ROOT_PARAMETER p[7]{};
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p[0].Descriptor.ShaderRegister = 0;
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p[1].Constants.ShaderRegister = 1;
    p[1].Constants.Num32BitValues = 12;
    p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p[2].Constants.ShaderRegister = 2;
    p[2].Constants.Num32BitValues = 8;
    p[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    p[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[3].DescriptorTable.NumDescriptorRanges = 1;
    p[3].DescriptorTable.pDescriptorRanges = &rangeTable;
    p[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[4].DescriptorTable.NumDescriptorRanges = 1;
    p[4].DescriptorTable.pDescriptorRanges = &rangeTex;
    p[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    p[5].Descriptor.ShaderRegister = 10;
    p[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    p[6].Descriptor.ShaderRegister = 11;
    p[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC s[2]{};
    s[0].Filter = D3D12_FILTER_ANISOTROPIC;
    s[0].AddressU = s[0].AddressV = s[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    s[0].MaxAnisotropy = 8;
    s[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    s[0].MaxLOD = D3D12_FLOAT32_MAX;
    s[0].ShaderRegister = 0;
    s[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    s[1] = s[0];
    s[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    s[1].AddressU = s[1].AddressV = s[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s[1].MaxAnisotropy = 1;
    s[1].ShaderRegister = 1;

    D3D12_ROOT_SIGNATURE_DESC rsd{};
    rsd.NumParameters = 7;
    rsd.pParameters = p;
    rsd.NumStaticSamplers = 2;
    rsd.pStaticSamplers = s;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) {
        return Fail(std::string("root signature: ") + (err ? static_cast<const char*>(err->GetBufferPointer()) : ""));
    }
    if (FAILED(m_dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_root)))) return Fail("CreateRootSignature");
    return true;
}

ComPtr<ID3D12PipelineState> D3D12Renderer::MakePso(const char* vs, const char* ps, int layout, DXGI_FORMAT rtv, DXGI_FORMAT dsv, int blend, int depth) {
    std::string key = std::string(vs) + "|" + ps + "|" + std::to_string(layout) + "|" + std::to_string(rtv) + "|" + std::to_string(blend) + "|" + std::to_string(depth);
    auto it = m_psos.find(key);
    if (it != m_psos.end()) return it->second;

    static const D3D12_INPUT_ELEMENT_DESC sceneLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    static const D3D12_INPUT_ELEMENT_DESC billLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = m_root.Get();
    auto& vb = m_shaders[vs];
    auto& pb = m_shaders[ps];
    d.VS = {vb.data(), vb.size()};
    d.PS = {pb.data(), pb.size()};
    if (layout == 1) d.InputLayout = {sceneLayout, 4};
    if (layout == 2) d.InputLayout = {billLayout, 3};
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    D3D12_RENDER_TARGET_BLEND_DESC& bt = d.BlendState.RenderTarget[0];
    bt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    bt.LogicOp = D3D12_LOGIC_OP_NOOP;
    if (blend != 0) {
        bt.BlendEnable = TRUE;
        bt.SrcBlend = D3D12_BLEND_ONE;
        bt.DestBlend = blend == 1 ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_ONE;
        bt.BlendOp = D3D12_BLEND_OP_ADD;
        bt.SrcBlendAlpha = D3D12_BLEND_ONE;
        bt.DestBlendAlpha = bt.DestBlend;
        bt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }
    d.DepthStencilState.DepthEnable = depth != 0;
    d.DepthStencilState.DepthWriteMask = (depth == 1) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.DepthFunc = (depth == 2) ? D3D12_COMPARISON_FUNC_LESS_EQUAL : D3D12_COMPARISON_FUNC_LESS;
    d.SampleMask = 0xFFFFFFFF;
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = rtv;
    d.DSVFormat = depth != 0 ? dsv : DXGI_FORMAT_UNKNOWN;
    d.SampleDesc.Count = 1;
    ComPtr<ID3D12PipelineState> pso;
    if (FAILED(m_dev->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso)))) {
        Fail(std::string("PSO ") + vs + "/" + ps);
        return nullptr;
    }
    m_psos[key] = pso;
    return pso;
}

bool D3D12Renderer::BuildPipelines() {
    const DXGI_FORMAT H = DXGI_FORMAT_R16G16B16A16_FLOAT;
    const DXGI_FORMAT D = DXGI_FORMAT_D32_FLOAT;
    bool ok = true;
    ok &= MakePso("VSScene", "PSPrepass", 1, H, D, 0, 1) != nullptr;
    ok &= MakePso("VSScene", "PSScene", 1, H, D, 0, 2) != nullptr;
    ok &= MakePso("VSShadowPos", "PSShadow", 1, DXGI_FORMAT_R32_FLOAT, D, 0, 1) != nullptr;
    ok &= MakePso("VSBillboard", "PSBillboard", 2, H, D, 1, 3) != nullptr;
    ok &= MakePso("VSFull", "PSSsao", 0, DXGI_FORMAT_R8_UNORM, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSBlurAo", 0, DXGI_FORMAT_R8_UNORM, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSAoTemporal", 0, DXGI_FORMAT_R8_UNORM, D, 0, 0) != nullptr;
    if (m_rtReady) ok &= MakePso("VSFull", "PSRtAo", 0, DXGI_FORMAT_R8_UNORM, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSBloomPrefilter", 0, H, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSBloomDown", 0, H, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSBloomUp", 0, H, D, 2, 0) != nullptr;
    ok &= MakePso("VSFull", "PSLum", 0, DXGI_FORMAT_R16_FLOAT, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSLumDown", 0, DXGI_FORMAT_R16_FLOAT, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSAdapt", 0, DXGI_FORMAT_R32_FLOAT, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSVolumetric", 0, H, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSBlurVol", 0, H, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSComposite", 0, DXGI_FORMAT_R8G8B8A8_UNORM, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSFxaa", 0, DXGI_FORMAT_R8G8B8A8_UNORM, D, 0, 0) != nullptr;
    ok &= MakePso("VSFull", "PSCopy", 0, DXGI_FORMAT_R8G8B8A8_UNORM, D, 0, 0) != nullptr;
    return ok;
}

bool D3D12Renderer::CreateFixedTargets() {
    const D3D12_RESOURCE_FLAGS RT = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    const D3D12_RESOURCE_FLAGS DS = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    const D3D12_RESOURCE_STATES SR = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (!MakeTexture(m_spot, m_spotSize, m_spotSize, DXGI_FORMAT_R32_FLOAT, RT, SR)) return Fail("spot texture");
    if (!MakeTexture(m_spotDepth, m_spotSize, m_spotSize, DXGI_FORMAT_D32_FLOAT, DS, D3D12_RESOURCE_STATE_DEPTH_WRITE)) return Fail("spot depth");
    if (!MakeTexture(m_cube, m_cubeSize, m_cubeSize, DXGI_FORMAT_R32_FLOAT, RT, SR, CUBE_SLOTS * 6)) return Fail("cube texture");
    if (!MakeTexture(m_cubeDepth, m_cubeSize, m_cubeSize, DXGI_FORMAT_D32_FLOAT, DS, D3D12_RESOURCE_STATE_DEPTH_WRITE)) return Fail("cube depth");
    m_dev->CreateRenderTargetView(m_spot.r.Get(), nullptr, RtvCpu(24));
    for (int s = 0; s < CUBE_SLOTS * 6; ++s) {
        D3D12_RENDER_TARGET_VIEW_DESC rv{};
        rv.Format = DXGI_FORMAT_R32_FLOAT;
        rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rv.Texture2DArray.FirstArraySlice = s;
        rv.Texture2DArray.ArraySize = 1;
        m_dev->CreateRenderTargetView(m_cube.r.Get(), &rv, RtvCpu(25 + s));
    }
    D3D12_DEPTH_STENCIL_VIEW_DESC dv{};
    dv.Format = DXGI_FORMAT_D32_FLOAT;
    dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_dev->CreateDepthStencilView(m_spotDepth.r.Get(), &dv, DsvCpu(1));
    m_dev->CreateDepthStencilView(m_cubeDepth.r.Get(), &dv, DsvCpu(2));
    return true;
}

bool D3D12Renderer::CreateScreenTargets() {
    const D3D12_RESOURCE_FLAGS RT = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    const D3D12_RESOURCE_STATES SR = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    int w = m_width, h = m_height;
    int hw = std::max(1, w / 2), hh = std::max(1, h / 2);
    WaitIdle();
    auto mk = [&](Res& t, int tw, int th, DXGI_FORMAT f, int rtv) {
        if (!MakeTexture(t, tw, th, f, RT, SR)) return false;
        m_dev->CreateRenderTargetView(t.r.Get(), nullptr, RtvCpu(rtv));
        return true;
    };
    bool ok = mk(m_hdr, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, 0) && mk(m_normal, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, 1) &&
              mk(m_ao, hw, hh, DXGI_FORMAT_R8_UNORM, 2) && mk(m_aoBlur, hw, hh, DXGI_FORMAT_R8_UNORM, 3) &&
              mk(m_vol, hw, hh, DXGI_FORMAT_R16G16B16A16_FLOAT, 4) && mk(m_volBlur, hw, hh, DXGI_FORMAT_R16G16B16A16_FLOAT, 5) &&
              mk(m_ldr, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 22) && mk(m_final, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 23);
    int bw = hw, bh = hh;
    for (int i = 0; i < BLOOM_LEVELS && ok; ++i) {
        ok = mk(m_bloom[i], std::max(1, bw), std::max(1, bh), DXGI_FORMAT_R16G16B16A16_FLOAT, 6 + i);
        bw /= 2;
        bh /= 2;
    }
    int ls = 128;
    for (int i = 0; i < LUM_LEVELS && ok; ++i) {
        ok = mk(m_lum[i], ls, ls, DXGI_FORMAT_R16_FLOAT, 12 + i);
        ls = std::max(1, ls / 2);
    }
    for (int i = 0; i < 2 && ok; ++i) ok = mk(m_adapt[i], 1, 1, DXGI_FORMAT_R32_FLOAT, 20 + i);
    for (int i = 0; i < 2 && ok; ++i) ok = mk(m_aoHist[i], hw, hh, DXGI_FORMAT_R8_UNORM, 49 + i);
    if (!ok) return Fail("screen targets");
    m_adaptInit = false;

    if (!MakeTexture(m_depth, w, h, DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE)) return Fail("depth");
    D3D12_DEPTH_STENCIL_VIEW_DESC dv{};
    dv.Format = DXGI_FORMAT_D32_FLOAT;
    dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_dev->CreateDepthStencilView(m_depth.r.Get(), &dv, DsvCpu(0));

    D3D12_RESOURCE_DESC fd = m_final.r->GetDesc();
    UINT rows = 0;
    UINT64 rowSize = 0;
    m_dev->GetCopyableFootprints(&fd, 0, 1, 0, &m_readbackFp, &rows, &rowSize, &m_readbackSize);
    m_readback = MakeBuffer(m_readbackSize, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
    if (!m_readback) return Fail("readback buffer");
    m_pixels.assign(static_cast<size_t>(w) * h * 4, 0);
    BuildTables();
    return true;
}

void D3D12Renderer::BuildTables() {
    m_tableCount = 0;
    tMain = NewTable();
    SetTable(tMain, {{1, &m_aoBlur}, {2, &m_spot}, {3, &m_cube}});
    tSsao = NewTable();
    SetTable(tSsao, {{4, &m_depth}, {5, &m_normal}});
    tBlurAo = NewTable();
    SetTable(tBlurAo, {{4, &m_depth}, {6, &m_ao}});
    for (int i = 0; i < 2; ++i) {
        tAoTemp[i] = NewTable();
        SetTable(tAoTemp[i], {{4, &m_depth}, {6, &m_ao}, {7, &m_aoHist[1 - i]}});
        tBlurAoHist[i] = NewTable();
        SetTable(tBlurAoHist[i], {{4, &m_depth}, {6, &m_aoHist[i]}});
    }
    tVol = NewTable();
    SetTable(tVol, {{2, &m_spot}, {3, &m_cube}, {4, &m_depth}});
    tBlurVol = NewTable();
    SetTable(tBlurVol, {{6, &m_vol}});
    tBloomPre = NewTable();
    SetTable(tBloomPre, {{6, &m_hdr}});
    for (int i = 1; i < BLOOM_LEVELS; ++i) {
        tBloomDown[i] = NewTable();
        SetTable(tBloomDown[i], {{6, &m_bloom[i - 1]}});
    }
    for (int i = 0; i < BLOOM_LEVELS - 1; ++i) {
        tBloomUp[i] = NewTable();
        SetTable(tBloomUp[i], {{6, &m_bloom[i + 1]}});
    }
    tLum = NewTable();
    SetTable(tLum, {{6, &m_hdr}});
    for (int i = 1; i < LUM_LEVELS; ++i) {
        tLumDown[i] = NewTable();
        SetTable(tLumDown[i], {{6, &m_lum[i - 1]}});
    }
    for (int i = 0; i < 2; ++i) {
        tAdapt[i] = NewTable();
        SetTable(tAdapt[i], {{6, &m_lum[LUM_LEVELS - 1]}, {7, &m_adapt[1 - i]}});
        tComp[i] = NewTable();
        SetTable(tComp[i], {{6, &m_hdr}, {7, &m_volBlur}, {8, &m_bloom[0]}, {9, &m_adapt[i]}});
    }
    tFxaa = NewTable();
    SetTable(tFxaa, {{6, &m_ldr}});
    tCopy = tFxaa;
}

bool D3D12Renderer::Init(const HdInitDesc& desc) {
    Shutdown();
    m_width = std::max(320, desc.width);
    m_height = std::max(180, desc.height);
    m_quality = desc.quality;
    m_assetDir = desc.shaderDir;
    int sq = desc.quality.shadowQuality;
    m_spotSize = sq <= 0 ? 1024 : (sq == 1 ? 2048 : 4096);
    m_cubeSize = sq <= 0 ? 256 : (sq == 1 ? 512 : 768);

    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_dev)))) return Fail("D3D12CreateDevice failed");
    HdCapabilities caps = ProbeCapabilities(desc.shaderDir + "/../..");
    m_adapter = caps.adapter;
    m_dxc = InitDxc(desc.shaderDir + "/../..");
    m_rtReady = m_dxc && caps.dxr && desc.quality.rayTracing > 0;

    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(m_dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_queue)))) return Fail("command queue");
    if (FAILED(m_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_alloc)))) return Fail("allocator");
    if (FAILED(m_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_alloc.Get(), nullptr, IID_PPV_ARGS(&m_list)))) return Fail("command list");
    if (FAILED(m_dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) return Fail("fence");
    m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.NumDescriptors = 64;
    if (FAILED(m_dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_rtvHeap)))) return Fail("rtv heap");
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    hd.NumDescriptors = 8;
    if (FAILED(m_dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_dsvHeap)))) return Fail("dsv heap");
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = TEX_BASE + MAX_TEXTURES;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_srvHeap)))) return Fail("srv heap");
    m_rtvInc = m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dsvInc = m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_srvInc = m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    m_upload = MakeBuffer(UPLOAD_SIZE, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!m_upload) return Fail("upload buffer");
    D3D12_RANGE none{0, 0};
    m_upload->Map(0, &none, reinterpret_cast<void**>(&m_uploadMapped));
    m_billboardVb = MakeBuffer(sizeof(BillboardVertex) * 6 * 64, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

    if (m_rtReady) {
        m_dev->QueryInterface(IID_Device5, &m_device5);
        m_list->QueryInterface(IID_List4, &m_list4);
        if (!m_device5 || !m_list4) m_rtReady = false;
    }

    if (!BuildShaders(desc.shaderDir)) return false;
    if (!BuildRootSignature()) return false;
    if (!BuildPipelines()) return false;
    if (!CreateFixedTargets()) return false;
    if (!CreateScreenTargets()) return false;

    m_textures.clear();
    m_texAvg.clear();
    uint8_t white[4] = {255, 255, 255, 255};
    CreateTexture(white, 1, 1);
    m_ready = true;
    return true;
}

void D3D12Renderer::Shutdown() {
    if (m_queue && m_fence) WaitIdle();
    m_meshes.clear();
    m_textures.clear();
    m_psos.clear();
    m_shaders.clear();
    if (m_list4) {
        static_cast<IUnknown*>(m_list4)->Release();
        m_list4 = nullptr;
    }
    if (m_device5) {
        static_cast<IUnknown*>(m_device5)->Release();
        m_device5 = nullptr;
    }
    if (m_compiler) {
        m_compiler->Release();
        m_compiler = nullptr;
    }
    m_tlas.Reset();
    m_tlasScratch.Reset();
    m_instColor.Reset();
    m_upload.Reset();
    m_readback.Reset();
    m_billboardVb.Reset();
    m_hdr = Res{};
    m_normal = Res{};
    m_ao = Res{};
    m_aoBlur = Res{};
    m_vol = Res{};
    m_volBlur = Res{};
    m_ldr = Res{};
    m_final = Res{};
    m_depth = Res{};
    m_spot = Res{};
    m_spotDepth = Res{};
    m_cube = Res{};
    m_cubeDepth = Res{};
    for (auto& b : m_bloom) b = Res{};
    for (auto& l : m_lum) l = Res{};
    for (auto& a : m_adapt) a = Res{};
    for (auto& a : m_aoHist) a = Res{};
    m_list.Reset();
    m_alloc.Reset();
    m_fence.Reset();
    m_queue.Reset();
    m_root.Reset();
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_srvHeap.Reset();
    m_dev.Reset();
    if (m_event) {
        CloseHandle(m_event);
        m_event = nullptr;
    }
    m_ready = false;
    m_rtReady = false;
}

bool D3D12Renderer::Resize(int width, int height) {
    width = std::max(320, width);
    height = std::max(180, height);
    if (width == m_width && height == m_height) return true;
    m_width = width;
    m_height = height;
    return CreateScreenTargets();
}

int D3D12Renderer::CreateTexture(const uint8_t* rgba, int width, int height) {
    if (static_cast<int>(m_textures.size()) >= MAX_TEXTURES) return -1;
    int mips = 1;
    while ((width >> mips) > 0 || (height >> mips) > 0) {
        if ((width >> mips) == 0 && (height >> mips) == 0) break;
        ++mips;
        if (mips >= 12) break;
    }
    mips = std::max(1, std::min(mips, 1 + static_cast<int>(std::floor(std::log2(static_cast<float>(std::max(width, height)))))));

    std::vector<std::vector<uint8_t>> levels(mips);
    std::vector<int> lw(mips), lh(mips);
    levels[0].assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
    lw[0] = width;
    lh[0] = height;
    for (int m = 1; m < mips; ++m) {
        lw[m] = std::max(1, lw[m - 1] / 2);
        lh[m] = std::max(1, lh[m - 1] / 2);
        levels[m].resize(static_cast<size_t>(lw[m]) * lh[m] * 4);
        for (int y = 0; y < lh[m]; ++y)
            for (int x = 0; x < lw[m]; ++x)
                for (int c = 0; c < 4; ++c) {
                    int x0 = std::min(lw[m - 1] - 1, x * 2), x1 = std::min(lw[m - 1] - 1, x * 2 + 1);
                    int y0 = std::min(lh[m - 1] - 1, y * 2), y1 = std::min(lh[m - 1] - 1, y * 2 + 1);
                    const std::vector<uint8_t>& p = levels[m - 1];
                    int s = p[(y0 * lw[m - 1] + x0) * 4 + c] + p[(y0 * lw[m - 1] + x1) * 4 + c] + p[(y1 * lw[m - 1] + x0) * 4 + c] + p[(y1 * lw[m - 1] + x1) * 4 + c];
                    levels[m][(y * lw[m] + x) * 4 + c] = static_cast<uint8_t>(s / 4);
                }
    }

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = width;
    rd.Height = height;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = static_cast<UINT16>(mips);
    rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rd.SampleDesc.Count = 1;
    ComPtr<ID3D12Resource> tex;
    if (FAILED(m_dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex)))) return -1;

    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> fps(mips);
    std::vector<UINT> rows(mips);
    std::vector<UINT64> rowSizes(mips);
    UINT64 total = 0;
    m_dev->GetCopyableFootprints(&rd, 0, mips, 0, fps.data(), rows.data(), rowSizes.data(), &total);
    ComPtr<ID3D12Resource> up = MakeBuffer(total, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!up) return -1;
    uint8_t* mapped = nullptr;
    D3D12_RANGE none{0, 0};
    up->Map(0, &none, reinterpret_cast<void**>(&mapped));
    for (int m = 0; m < mips; ++m) {
        for (UINT y = 0; y < rows[m]; ++y) {
            std::memcpy(mapped + fps[m].Offset + static_cast<size_t>(y) * fps[m].Footprint.RowPitch, &levels[m][static_cast<size_t>(y) * lw[m] * 4], static_cast<size_t>(lw[m]) * 4);
        }
    }
    up->Unmap(0, nullptr);
    for (int m = 0; m < mips; ++m) {
        D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
        dst.pResource = tex.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = m;
        src.pResource = up.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = fps[m];
        m_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    TrRaw(tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_pending.push_back(up);

    int handle = static_cast<int>(m_textures.size());
    m_textures.push_back(tex);
    WriteSrv(TEX_BASE + handle, tex.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0, mips);

    double r = 0, g = 0, b = 0;
    size_t px = static_cast<size_t>(width) * height;
    size_t step = std::max<size_t>(1, px / 256);
    size_t cnt = 0;
    for (size_t i = 0; i < px; i += step) {
        r += std::pow(rgba[i * 4] / 255.0, 2.2);
        g += std::pow(rgba[i * 4 + 1] / 255.0, 2.2);
        b += std::pow(rgba[i * 4 + 2] / 255.0, 2.2);
        ++cnt;
    }
    m_texAvg.push_back(static_cast<float>((r + g + b) / 3.0 / std::max<size_t>(1, cnt)));
    return handle;
}

int D3D12Renderer::CreateMesh(const std::vector<HdVertex>& vertices, const HdMaterialDesc& material, int albedoTexture) {
    if (vertices.empty()) return -1;
    UINT64 size = vertices.size() * sizeof(HdVertex);
    ComPtr<ID3D12Resource> vb = MakeBuffer(size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
    ComPtr<ID3D12Resource> up = MakeBuffer(size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!vb || !up) return -1;
    void* mapped = nullptr;
    D3D12_RANGE none{0, 0};
    up->Map(0, &none, &mapped);
    std::memcpy(mapped, vertices.data(), size);
    up->Unmap(0, nullptr);
    m_list->CopyBufferRegion(vb.Get(), 0, up.Get(), 0, size);
    TrRaw(vb.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    m_pending.push_back(up);

    MeshGpu mesh;
    mesh.vb = vb;
    mesh.count = static_cast<UINT>(vertices.size());
    mesh.material = material;
    mesh.texture = (albedoTexture >= 0 && albedoTexture < static_cast<int>(m_textures.size())) ? albedoTexture : 0;
    mesh.bmin = {1e9f, 1e9f, 1e9f};
    mesh.bmax = {-1e9f, -1e9f, -1e9f};
    for (const HdVertex& v : vertices) {
        mesh.bmin = {std::min(mesh.bmin.x, v.px), std::min(mesh.bmin.y, v.py), std::min(mesh.bmin.z, v.pz)};
        mesh.bmax = {std::max(mesh.bmax.x, v.px), std::max(mesh.bmax.y, v.py), std::max(mesh.bmax.z, v.pz)};
    }
    mesh.rt = material.castShadow;
    m_meshes.push_back(std::move(mesh));
    if (m_pending.size() > 400) Flush();
    return static_cast<int>(m_meshes.size()) - 1;
}

void D3D12Renderer::BuildAccelerationStructures() {
    auto prebuild = [&](const RtAsInputs& in, RtPrebuild& out) {
        void** vt = *reinterpret_cast<void***>(m_device5);
        typedef void(STDMETHODCALLTYPE * Fn)(void*, const RtAsInputs*, RtPrebuild*);
        reinterpret_cast<Fn>(vt[63])(m_device5, &in, &out);
    };
    auto build = [&](const RtAsBuildDesc& d) {
        void** vt = *reinterpret_cast<void***>(m_list4);
        typedef void(STDMETHODCALLTYPE * Fn)(void*, const RtAsBuildDesc*, UINT, const void*);
        reinterpret_cast<Fn>(vt[72])(m_list4, &d, 0, nullptr);
    };

    std::vector<RtGeomDesc> geoms(m_meshes.size());
    std::vector<RtPrebuild> infos(m_meshes.size());
    UINT64 maxScratch = 0;
    for (size_t i = 0; i < m_meshes.size(); ++i) {
        MeshGpu& m = m_meshes[i];
        if (!m.rt) continue;
        RtGeomDesc& g = geoms[i];
        g.type = 0;
        g.flags = 1;
        g.tri = {};
        g.tri.vertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        g.tri.vertexCount = m.count;
        g.tri.vertexAddr = m.vb->GetGPUVirtualAddress();
        g.tri.vertexStride = sizeof(HdVertex);
        RtAsInputs in{1, 4, 1, 0, 0};
        in.addrOrPtr = reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(&g);
        prebuild(in, infos[i]);
        maxScratch = std::max(maxScratch, infos[i].scratch);
    }
    ComPtr<ID3D12Resource> scratch = MakeBuffer(std::max<UINT64>(maxScratch, 256), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    for (size_t i = 0; i < m_meshes.size(); ++i) {
        MeshGpu& m = m_meshes[i];
        if (!m.rt) continue;
        m.blas = MakeBuffer(infos[i].result, D3D12_HEAP_TYPE_DEFAULT, static_cast<D3D12_RESOURCE_STATES>(STATE_AS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        if (!m.blas) {
            m.rt = false;
            continue;
        }
        RtAsBuildDesc bd{};
        bd.dest = m.blas->GetGPUVirtualAddress();
        bd.inputs = {1, 4, 1, 0, reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(&geoms[i])};
        bd.scratch = scratch->GetGPUVirtualAddress();
        build(bd);
        UavBarrier(m.blas.Get());
        UavBarrier(scratch.Get());
    }
    m_rtMeshCount = m_meshes.size();
    RtAsInputs tin{0, 4, static_cast<UINT>(m_meshes.size()), 0, 0};
    RtPrebuild tinfo{};
    prebuild(tin, tinfo);
    m_tlas = MakeBuffer(tinfo.result, D3D12_HEAP_TYPE_DEFAULT, static_cast<D3D12_RESOURCE_STATES>(STATE_AS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_tlasScratch = MakeBuffer(std::max<UINT64>(tinfo.scratch, 256), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    std::vector<float> colors(m_meshes.size() * 4);
    for (size_t i = 0; i < m_meshes.size(); ++i) {
        const MeshGpu& m = m_meshes[i];
        float avg = m.texture < static_cast<int>(m_texAvg.size()) ? m_texAvg[m.texture] : 0.5f;
        colors[i * 4] = m.material.tint[0] * avg * 2.0f;
        colors[i * 4 + 1] = m.material.tint[1] * avg * 2.0f;
        colors[i * 4 + 2] = m.material.tint[2] * avg * 2.0f;
        colors[i * 4 + 3] = m.material.emissive * avg;
    }
    UINT64 csize = std::max<UINT64>(colors.size() * 4, 16);
    m_instColor = MakeBuffer(csize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
    ComPtr<ID3D12Resource> up = MakeBuffer(csize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    void* mapped = nullptr;
    D3D12_RANGE none{0, 0};
    up->Map(0, &none, &mapped);
    std::memcpy(mapped, colors.data(), colors.size() * 4);
    up->Unmap(0, nullptr);
    m_list->CopyBufferRegion(m_instColor.Get(), 0, up.Get(), 0, csize);
    TrRaw(m_instColor.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    m_pending.push_back(up);
    m_pending.push_back(scratch);
    Flush();
}

void D3D12Renderer::FinishLoading() {
    if (m_rtReady) BuildAccelerationStructures();
    else Flush();
}

UINT64 D3D12Renderer::PushFrameCB(const CBFrame& cb) {
    void* cpu = nullptr;
    UINT64 addr = Alloc(sizeof(CBFrame), &cpu);
    std::memcpy(cpu, &cb, sizeof(cb));
    return addr;
}

void D3D12Renderer::Post(float a, float b, float c, float d, float e, float f, float g, float h) {
    float v[8] = {a, b, c, d, e, f, g, h};
    m_list->SetGraphicsRoot32BitConstants(2, 8, v, 0);
}

void D3D12Renderer::SetRt(int rtvIndex, int dsvIndex, int w, int h) {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvIndex >= 0 ? RtvCpu(rtvIndex) : D3D12_CPU_DESCRIPTOR_HANDLE{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvIndex >= 0 ? DsvCpu(dsvIndex) : D3D12_CPU_DESCRIPTOR_HANDLE{};
    m_list->OMSetRenderTargets(rtvIndex >= 0 ? 1 : 0, rtvIndex >= 0 ? &rtv : nullptr, FALSE, dsvIndex >= 0 ? &dsv : nullptr);
    D3D12_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f};
    D3D12_RECT sc{0, 0, w, h};
    m_list->RSSetViewports(1, &vp);
    m_list->RSSetScissorRects(1, &sc);
}

void D3D12Renderer::DrawFull(ID3D12PipelineState* pso) {
    m_list->SetPipelineState(pso);
    m_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_list->DrawInstanced(3, 1, 0, 0);
}

void D3D12Renderer::DrawMeshes(bool shadowOnly, bool materials) {
    m_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const MeshGpu& mesh : m_meshes) {
        if (!mesh.visible) continue;
        if (shadowOnly && !mesh.material.castShadow) continue;
        if (m_cullUseFrustum && !AabbVisible(m_cullFrustum, mesh.bmin, mesh.bmax)) continue;
        if (m_cullUseSphere && !AabbInSphere(mesh.bmin, mesh.bmax, m_cullCenter, m_cullRadius)) continue;
        ++m_drawn;
        if (materials) {
            ObjectConstants oc{};
            std::memcpy(oc.tint, mesh.material.tint, sizeof(oc.tint));
            oc.mat[0] = mesh.material.roughness;
            oc.mat[1] = mesh.material.metallic;
            oc.mat[2] = mesh.material.emissive;
            oc.mat[3] = mesh.material.bump;
            oc.idx[0] = static_cast<UINT>(mesh.texture);
            m_list->SetGraphicsRoot32BitConstants(1, 12, &oc, 0);
        }
        D3D12_VERTEX_BUFFER_VIEW vbv{mesh.vb->GetGPUVirtualAddress(), static_cast<UINT>(mesh.count * sizeof(HdVertex)), sizeof(HdVertex)};
        m_list->IASetVertexBuffers(0, 1, &vbv);
        m_list->DrawInstanced(mesh.count, 1, 0, 0);
    }
}

void D3D12Renderer::DrawBillboards(const HdFrameParams& frame, F3 camPos, F3 camFwd, F3 camUp) {
    if (frame.billboards.empty()) return;
    F3 right = Norm(Cross(camFwd, camUp));
    F3 up = Cross(right, camFwd);
    std::vector<size_t> order(std::min<size_t>(frame.billboards.size(), 64));
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return Len(Sub(FromArr(frame.billboards[a].center), camPos)) > Len(Sub(FromArr(frame.billboards[b].center), camPos));
    });
    std::vector<BillboardVertex> verts;
    for (size_t idx : order) {
        const HdBillboard& bb = frame.billboards[idx];
        F3 c = FromArr(bb.center);
        float hw = bb.size[0] * 0.5f, hh = bb.size[1] * 0.5f;
        auto P = [&](float sx, float sy) { return F3{c.x + right.x * hw * sx + up.x * hh * sy, c.y + right.y * hw * sx + up.y * hh * sy, c.z + right.z * hw * sx + up.z * hh * sy}; };
        F3 p0 = P(-1, -1), p1 = P(1, -1), p2 = P(1, 1), p3 = P(-1, 1);
        BillboardVertex v0{p0.x, p0.y, p0.z, 0, 1, bb.tint[0], bb.tint[1], bb.tint[2], bb.tint[3]};
        BillboardVertex v1{p1.x, p1.y, p1.z, 1, 1, bb.tint[0], bb.tint[1], bb.tint[2], bb.tint[3]};
        BillboardVertex v2{p2.x, p2.y, p2.z, 1, 0, bb.tint[0], bb.tint[1], bb.tint[2], bb.tint[3]};
        BillboardVertex v3{p3.x, p3.y, p3.z, 0, 0, bb.tint[0], bb.tint[1], bb.tint[2], bb.tint[3]};
        verts.insert(verts.end(), {v0, v1, v2, v0, v2, v3});
    }
    void* mapped = nullptr;
    D3D12_RANGE none{0, 0};
    m_billboardVb->Map(0, &none, &mapped);
    std::memcpy(mapped, verts.data(), verts.size() * sizeof(BillboardVertex));
    m_billboardVb->Unmap(0, nullptr);
    D3D12_VERTEX_BUFFER_VIEW vbv{m_billboardVb->GetGPUVirtualAddress(), static_cast<UINT>(verts.size() * sizeof(BillboardVertex)), sizeof(BillboardVertex)};
    m_list->IASetVertexBuffers(0, 1, &vbv);
    m_list->SetPipelineState(MakePso("VSBillboard", "PSBillboard", 2, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT, 1, 3).Get());
    m_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (size_t k = 0; k < order.size(); ++k) {
        const HdBillboard& bb = frame.billboards[order[k]];
        ObjectConstants oc{};
        oc.tint[0] = oc.tint[1] = oc.tint[2] = oc.tint[3] = 1.0f;
        oc.mat[2] = bb.emissive;
        int t = (bb.texture >= 0 && bb.texture < static_cast<int>(m_textures.size())) ? bb.texture : 0;
        oc.idx[0] = static_cast<UINT>(t);
        m_list->SetGraphicsRoot32BitConstants(1, 12, &oc, 0);
        m_list->DrawInstanced(6, 1, static_cast<UINT>(k * 6), 0);
    }
}

void D3D12Renderer::RenderShadowView(const Mat& vp, F3 origin, float radius, int rtvIndex, int dsvIndex, int size, CBFrame& cb) {
    std::memcpy(cb.spotViewProj, vp.m, sizeof(cb.spotViewProj));
    m_list->SetGraphicsRootConstantBufferView(0, PushFrameCB(cb));
    Post(origin.x, origin.y, origin.z, 1.0f / radius);
    const float one[4] = {1, 1, 1, 1};
    m_list->ClearRenderTargetView(RtvCpu(rtvIndex), one, 0, nullptr);
    m_list->ClearDepthStencilView(DsvCpu(dsvIndex), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    SetRt(rtvIndex, dsvIndex, size, size);
    m_list->SetPipelineState(MakePso("VSShadowPos", "PSShadow", 1, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D32_FLOAT, 0, 1).Get());
    DrawMeshes(true, false);
}

bool D3D12Renderer::RenderFrame(const HdFrameParams& frame) {
    if (!m_ready) return false;
    ++m_frame;
    m_drawn = 0;
    m_uploadOffset = 0;
    const int w = m_width, h = m_height;
    const int hw = std::max(1, w / 2), hh = std::max(1, h / 2);
    const D3D12_RESOURCE_STATES SR = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    const D3D12_RESOURCE_STATES RTS = D3D12_RESOURCE_STATE_RENDER_TARGET;
    const D3D12_RESOURCE_STATES DSW = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    const int rayMode = m_rtReady ? frame.quality.rayTracing : 0;

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
    Mat ident = Identity();
    std::memcpy(cb.spotViewProj, ident.m, sizeof(cb.spotViewProj));
    cb.camPos[0] = camPos.x; cb.camPos[1] = camPos.y; cb.camPos[2] = camPos.z; cb.camPos[3] = frame.time;
    cb.camFwd[0] = camFwd.x; cb.camFwd[1] = camFwd.y; cb.camFwd[2] = camFwd.z; cb.camFwd[3] = frame.nearZ;
    cb.screen[0] = static_cast<float>(w); cb.screen[1] = static_cast<float>(h); cb.screen[2] = 1.0f / w; cb.screen[3] = 1.0f / h;
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

    struct Cand {
        float score;
        const HdLightDesc* l;
    };
    std::vector<Cand> cands;
    for (const HdLightDesc& l : frame.lights) {
        float d = Len(Sub(FromArr(l.pos), camPos)) - l.radius;
        if (d > 40.0f) continue;
        cands.push_back({d, &l});
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.score < b.score; });
    if (cands.size() > MAX_LIGHTS) cands.resize(MAX_LIGHTS);
    std::array<bool, CUBE_SLOTS> used{}, dirty{};
    std::array<int, MAX_LIGHTS> lightSlot{};
    lightSlot.fill(-1);
    for (size_t i = 0; i < cands.size(); ++i) {
        const HdLightDesc& l = *cands[i].l;
        if (l.id < 0) continue;
        for (int s = 0; s < CUBE_SLOTS; ++s)
            if (m_slotLight[s] == l.id && !used[s]) {
                lightSlot[i] = s;
                used[s] = true;
                break;
            }
    }
    for (size_t i = 0; i < cands.size(); ++i) {
        const HdLightDesc& l = *cands[i].l;
        if (l.id < 0 || lightSlot[i] >= 0) continue;
        for (int s = 0; s < CUBE_SLOTS; ++s)
            if (!used[s]) {
                lightSlot[i] = s;
                used[s] = true;
                m_slotLight[s] = l.id;
                m_slotPos[s] = FromArr(l.pos);
                m_slotRadius[s] = l.radius;
                dirty[s] = true;
                break;
            }
    }
    for (size_t i = 0; i < cands.size(); ++i) {
        const HdLightDesc& l = *cands[i].l;
        cb.lightPos[i][0] = l.pos[0]; cb.lightPos[i][1] = l.pos[1]; cb.lightPos[i][2] = l.pos[2]; cb.lightPos[i][3] = l.radius;
        for (int k = 0; k < 3; ++k) cb.lightColor[i][k] = l.color[k] * l.intensity;
        cb.lightColor[i][3] = static_cast<float>(lightSlot[i]);
    }
    cb.counts[0] = static_cast<float>(cands.size());
    cb.counts[1] = static_cast<float>(m_frame % 64);
    cb.counts[2] = frame.exposureBias;
    cb.counts[3] = frame.bloomIntensity;
    cb.grade[0] = frame.grain; cb.grade[1] = frame.vignette; cb.grade[2] = frame.saturation; cb.grade[3] = frame.dt;
    cb.fx[0] = frame.fear; cb.fx[1] = frame.horror; cb.fx[2] = frame.chromatic; cb.fx[3] = frame.heartbeat;
    cb.rt[0] = rayMode >= 1 ? 1.0f : 0.0f;
    cb.rt[1] = rayMode >= 2 ? 1.0f : 0.0f;
    cb.rt[2] = rayMode >= 3 ? 1.0f : 0.0f;
    std::memcpy(cb.prevViewProj, m_prevVP.m, sizeof(cb.prevViewProj));

    ID3D12DescriptorHeap* heaps[] = {m_srvHeap.Get()};
    m_list->SetDescriptorHeaps(1, heaps);
    m_list->SetGraphicsRootSignature(m_root.Get());
    m_list->SetGraphicsRootDescriptorTable(4, SrvGpu(TEX_BASE));
    if (rayMode > 0) {
        D3D12_RESOURCE_BARRIER none{};
        (void)none;
        m_list->SetGraphicsRootShaderResourceView(6, m_instColor->GetGPUVirtualAddress());
    }

    if (rayMode > 0 && m_tlas) {
        std::vector<RtInstance> inst(m_meshes.size());
        for (size_t i = 0; i < m_meshes.size(); ++i) {
            RtInstance& d = inst[i];
            std::memset(&d, 0, sizeof(d));
            d.transform[0][0] = d.transform[1][1] = d.transform[2][2] = 1.0f;
            bool on = m_meshes[i].rt && m_meshes[i].visible && m_meshes[i].blas;
            d.idMask = static_cast<UINT>(i & 0xFFFFFF) | (on ? 0xFFu << 24 : 0u);
            d.blas = m_meshes[i].blas ? m_meshes[i].blas->GetGPUVirtualAddress() : 0;
        }
        void* cpu = nullptr;
        UINT64 addr = Alloc(inst.size() * sizeof(RtInstance), &cpu);
        std::memcpy(cpu, inst.data(), inst.size() * sizeof(RtInstance));
        RtAsBuildDesc bd{};
        bd.dest = m_tlas->GetGPUVirtualAddress();
        bd.inputs = {0, 4, static_cast<UINT>(inst.size()), 0, addr};
        bd.scratch = m_tlasScratch->GetGPUVirtualAddress();
        void** vt = *reinterpret_cast<void***>(m_list4);
        typedef void(STDMETHODCALLTYPE * Fn)(void*, const RtAsBuildDesc*, UINT, const void*);
        reinterpret_cast<Fn>(vt[72])(m_list4, &bd, 0, nullptr);
        UavBarrier(m_tlas.Get());
        m_list->SetGraphicsRootShaderResourceView(5, m_tlas->GetGPUVirtualAddress());
    }

    static const F3 faceDir[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    static const F3 faceUp[6] = {{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
    bool anyDirty = dirty[0] || dirty[1] || dirty[2] || dirty[3];
    if (anyDirty) Tr(m_cube, RTS);
    m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tMain));
    for (int s = 0; s < CUBE_SLOTS; ++s) {
        if (!dirty[s]) continue;
        Mat p = PerspectiveLH(PI_F * 0.5f, 1.0f, 0.05f, m_slotRadius[s]);
        m_cullUseFrustum = false;
        m_cullUseSphere = true;
        m_cullCenter = m_slotPos[s];
        m_cullRadius = m_slotRadius[s];
        for (int f = 0; f < 6; ++f) {
            Mat v = LookAtLH(m_slotPos[s], faceDir[f], faceUp[f]);
            RenderShadowView(Mul(v, p), m_slotPos[s], m_slotRadius[s], 25 + s * 6 + f, 2, m_cubeSize, cb);
        }
    }
    if (anyDirty) Tr(m_cube, SR);
    if (frame.spot.enabled) {
        Tr(m_spot, RTS);
        float fov = std::clamp(2.0f * std::acos(std::clamp(frame.spot.outerCos, 0.1f, 0.999f)) * 1.15f, 0.3f, 2.4f);
        Mat v = LookAtLH(FromArr(frame.spot.pos), spotDir, std::abs(spotDir.y) > 0.95f ? F3{1, 0, 0} : F3{0, 1, 0});
        Mat p = PerspectiveLH(fov, 1.0f, 0.1f, frame.spot.range);
        Mat vp = Mul(v, p);
        m_cullUseSphere = false;
        m_cullUseFrustum = true;
        m_cullFrustum = MakeFrustum(vp);
        RenderShadowView(vp, FromArr(frame.spot.pos), frame.spot.range, 24, 1, m_spotSize, cb);
        std::memcpy(cb.spotViewProj, vp.m, sizeof(cb.spotViewProj));
        Tr(m_spot, SR);
    }
    std::memcpy(cb.viewProj, viewProj.m, sizeof(cb.viewProj));
    UINT64 frameCb = PushFrameCB(cb);
    m_list->SetGraphicsRootConstantBufferView(0, frameCb);
    m_cullUseSphere = false;
    m_cullUseFrustum = true;
    m_cullFrustum = MakeFrustum(viewProj);

    Tr(m_depth, DSW);
    Tr(m_normal, RTS);
    {
        const float zero[4] = {0, 0, 1, 0};
        m_list->ClearRenderTargetView(RtvCpu(1), zero, 0, nullptr);
        m_list->ClearDepthStencilView(DsvCpu(0), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        SetRt(1, 0, w, h);
        m_list->SetPipelineState(MakePso("VSScene", "PSPrepass", 1, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT, 0, 1).Get());
        DrawMeshes(false, false);
    }

    Tr(m_depth, SR);
    Tr(m_normal, SR);
    Tr(m_ao, RTS);
    SetRt(2, -1, hw, hh);
    m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tSsao));
    if (frame.quality.ssao) {
        DrawFull(MakePso("VSFull", rayMode >= 2 ? "PSRtAo" : "PSSsao", 0, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
    } else {
        const float white[4] = {1, 1, 1, 1};
        m_list->ClearRenderTargetView(RtvCpu(2), white, 0, nullptr);
    }
    Tr(m_ao, SR);
    int blurTable = tBlurAo;
    if (rayMode >= 2 && frame.quality.ssao) {
        int nxt = 1 - m_aoHistIndex;
        Tr(m_aoHist[nxt], RTS);
        SetRt(49 + nxt, -1, hw, hh);
        m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tAoTemp[nxt]));
        DrawFull(MakePso("VSFull", "PSAoTemporal", 0, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
        Tr(m_aoHist[nxt], SR);
        m_aoHistIndex = nxt;
        blurTable = tBlurAoHist[nxt];
    }
    Tr(m_aoBlur, RTS);
    SetRt(3, -1, hw, hh);
    m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(blurTable));
    if (frame.quality.ssao) {
        DrawFull(MakePso("VSFull", "PSBlurAo", 0, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
    } else {
        const float white[4] = {1, 1, 1, 1};
        m_list->ClearRenderTargetView(RtvCpu(3), white, 0, nullptr);
    }
    Tr(m_aoBlur, SR);

    Tr(m_depth, DSW);
    Tr(m_hdr, RTS);
    {
        const float clear[4] = {frame.fogColor[0], frame.fogColor[1], frame.fogColor[2], 1.0f};
        m_list->ClearRenderTargetView(RtvCpu(0), clear, 0, nullptr);
        SetRt(0, 0, w, h);
        m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tMain));
        m_list->SetPipelineState(MakePso("VSScene", "PSScene", 1, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT, 0, 2).Get());
        DrawMeshes(false, true);
        DrawBillboards(frame, camPos, camFwd, camUp);
    }
    Tr(m_hdr, SR);
    Tr(m_depth, SR);

    Tr(m_vol, RTS);
    SetRt(4, -1, hw, hh);
    m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tVol));
    Post(frame.scatterDensity, static_cast<float>(std::clamp(frame.quality.volumetricSteps, 8, 64)), 0, 0);
    if (frame.quality.volumetrics) {
        DrawFull(MakePso("VSFull", "PSVolumetric", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
    } else {
        const float black[4] = {0, 0, 0, 1};
        m_list->ClearRenderTargetView(RtvCpu(4), black, 0, nullptr);
    }
    Tr(m_vol, SR);
    Tr(m_volBlur, RTS);
    SetRt(5, -1, hw, hh);
    m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tBlurVol));
    Post(1.0f / hw, 1.0f / hh, 0, 0);
    DrawFull(MakePso("VSFull", "PSBlurVol", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
    Tr(m_volBlur, SR);

    const DXGI_FORMAT H = DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (frame.quality.bloom) {
        Tr(m_bloom[0], RTS);
        SetRt(6, -1, m_bloom[0].w, m_bloom[0].h);
        m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tBloomPre));
        Post(1.0f / w, 1.0f / h, 0.8f, 0.5f);
        DrawFull(MakePso("VSFull", "PSBloomPrefilter", 0, H, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
        Tr(m_bloom[0], SR);
        for (int i = 1; i < BLOOM_LEVELS; ++i) {
            Tr(m_bloom[i], RTS);
            SetRt(6 + i, -1, m_bloom[i].w, m_bloom[i].h);
            m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tBloomDown[i]));
            Post(1.0f / m_bloom[i - 1].w, 1.0f / m_bloom[i - 1].h, 0, 0);
            DrawFull(MakePso("VSFull", "PSBloomDown", 0, H, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
            Tr(m_bloom[i], SR);
        }
        for (int i = BLOOM_LEVELS - 2; i >= 0; --i) {
            Tr(m_bloom[i], RTS);
            SetRt(6 + i, -1, m_bloom[i].w, m_bloom[i].h);
            m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tBloomUp[i]));
            Post(1.0f / m_bloom[i + 1].w, 1.0f / m_bloom[i + 1].h, 0.85f, 0);
            DrawFull(MakePso("VSFull", "PSBloomUp", 0, H, DXGI_FORMAT_D32_FLOAT, 2, 0).Get());
            Tr(m_bloom[i], SR);
        }
    } else {
        Tr(m_bloom[0], RTS);
        const float black[4] = {0, 0, 0, 1};
        m_list->ClearRenderTargetView(RtvCpu(6), black, 0, nullptr);
        Tr(m_bloom[0], SR);
    }

    Tr(m_lum[0], RTS);
    SetRt(12, -1, 128, 128);
    m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tLum));
    DrawFull(MakePso("VSFull", "PSLum", 0, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
    Tr(m_lum[0], SR);
    for (int i = 1; i < LUM_LEVELS; ++i) {
        Tr(m_lum[i], RTS);
        SetRt(12 + i, -1, m_lum[i].w, m_lum[i].h);
        m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tLumDown[i]));
        Post(1.0f / m_lum[i - 1].w, 1.0f / m_lum[i - 1].h, 0, 0);
        DrawFull(MakePso("VSFull", "PSLumDown", 0, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
        Tr(m_lum[i], SR);
    }
    if (!m_adaptInit) {
        const float init[4] = {1.0f, 0, 0, 0};
        for (int i = 0; i < 2; ++i) {
            Tr(m_adapt[i], RTS);
            m_list->ClearRenderTargetView(RtvCpu(20 + i), init, 0, nullptr);
            Tr(m_adapt[i], SR);
        }
        m_adaptInit = true;
    }
    int prev = m_adaptIndex, next = 1 - m_adaptIndex;
    Tr(m_adapt[next], RTS);
    SetRt(20 + next, -1, 1, 1);
    m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tAdapt[next]));
    Post(frame.dt, 0.0f, 0, 0);
    (void)prev;
    DrawFull(MakePso("VSFull", "PSAdapt", 0, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
    Tr(m_adapt[next], SR);
    m_adaptIndex = next;

    Tr(m_ldr, RTS);
    SetRt(22, -1, w, h);
    m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tComp[m_adaptIndex]));
    DrawFull(MakePso("VSFull", "PSComposite", 0, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());
    Tr(m_ldr, SR);
    Tr(m_final, RTS);
    SetRt(23, -1, w, h);
    m_list->SetGraphicsRootDescriptorTable(3, SrvGpu(tFxaa));
    DrawFull(MakePso("VSFull", frame.quality.fxaa ? "PSFxaa" : "PSCopy", 0, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT, 0, 0).Get());

    Tr(m_final, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
    dst.pResource = m_readback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = m_readbackFp;
    src.pResource = m_final.r.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;
    m_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    Tr(m_final, SR);
    m_prevVP = viewProj;

    m_list->Close();
    ID3D12CommandList* lists[] = {m_list.Get()};
    m_queue->ExecuteCommandLists(1, lists);
    WaitIdle();
    m_alloc->Reset();
    m_list->Reset(m_alloc.Get(), nullptr);

    uint8_t* mapped = nullptr;
    D3D12_RANGE rr{0, static_cast<SIZE_T>(m_readbackSize)};
    if (FAILED(m_readback->Map(0, &rr, reinterpret_cast<void**>(&mapped)))) return Fail("map readback");
    for (int y = 0; y < h; ++y) {
        std::memcpy(&m_pixels[static_cast<size_t>(y) * w * 4], mapped + m_readbackFp.Offset + static_cast<size_t>(y) * m_readbackFp.Footprint.RowPitch, static_cast<size_t>(w) * 4);
    }
    D3D12_RANGE none{0, 0};
    m_readback->Unmap(0, &none);
    return true;
}

std::unique_ptr<IHdRenderer> CreateD3D12Renderer() {
    return std::make_unique<D3D12Renderer>();
}

}
