#include "game/chapter3/HouseHd.h"
#include "game/chapter3/Ch3Graphics.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>

namespace game::chapter3 {

using engine::hd::HdBillboard;
using engine::hd::HdFrameParams;
using engine::hd::HdLightDesc;
using engine::hd::HdMaterialDesc;
using engine::hd::HdVertex;

namespace {

float SrgbToLinear(unsigned char c) {
    return std::pow(static_cast<float>(c) / 255.0f, 2.2f);
}

void MaterialFor(const std::string& tex, HdMaterialDesc& m) {
    m.roughness = 0.85f;
    m.metallic = 0.0f;
    m.bump = 0.012f;
    if (tex == "motel_metal") {
        m.roughness = 0.36f;
        m.metallic = 0.85f;
        m.bump = 0.004f;
    } else if (tex == "motel_tile" || tex == "motel_porcelain") {
        m.roughness = 0.2f;
        m.bump = 0.004f;
    } else if (tex == "motel_window" || tex == "motel_mirror_glass") {
        m.roughness = 0.06f;
        m.bump = 0.0f;
    } else if (tex == "ch3/house_floor_parquet") {
        m.roughness = 0.42f;
        m.bump = 0.008f;
    } else if (tex == "ch3/house_wood_planks" || tex == "motel_trim" || tex == "motel_wall_wood" || tex == "ch3/house_door_interior") {
        m.roughness = 0.6f;
        m.bump = 0.012f;
    } else if (tex == "ch3/house_basement_brick") {
        m.roughness = 0.78f;
        m.bump = 0.02f;
    } else if (tex == "ch3/house_garage_wall" || tex == "motel_concrete") {
        m.roughness = 0.92f;
        m.bump = 0.016f;
    } else if (tex == "motel_fabric" || tex == "motel_carpet") {
        m.roughness = 0.97f;
        m.bump = 0.005f;
    } else if (tex == "motel_grass" || tex == "motel_gravel" || tex == "motel_bark" || tex == "motel_pine") {
        m.roughness = 1.0f;
        m.bump = 0.016f;
    } else if (tex == "motel_roof") {
        m.roughness = 0.7f;
    }
}

void AppendMesh(const Mesh& mesh, const Matrix* world, std::vector<HdVertex>& out) {
    if (mesh.vertices == nullptr) return;
    auto vertexAt = [&](int idx) {
        HdVertex v{};
        Vector3 p{mesh.vertices[idx * 3], mesh.vertices[idx * 3 + 1], mesh.vertices[idx * 3 + 2]};
        Vector3 n{0.0f, 1.0f, 0.0f};
        if (mesh.normals != nullptr) n = Vector3{mesh.normals[idx * 3], mesh.normals[idx * 3 + 1], mesh.normals[idx * 3 + 2]};
        if (world != nullptr) {
            p = Vector3Transform(p, *world);
            Matrix rot = *world;
            rot.m12 = rot.m13 = rot.m14 = 0.0f;
            n = Vector3Normalize(Vector3Transform(n, rot));
        }
        v.px = p.x; v.py = p.y; v.pz = p.z;
        v.nx = n.x; v.ny = n.y; v.nz = n.z;
        if (mesh.texcoords != nullptr) {
            v.u = mesh.texcoords[idx * 2];
            v.v = mesh.texcoords[idx * 2 + 1];
        }
        if (mesh.colors != nullptr) {
            v.bakeR = static_cast<float>(mesh.colors[idx * 4]) / 255.0f * 2.0f;
            v.bakeG = static_cast<float>(mesh.colors[idx * 4 + 1]) / 255.0f * 2.0f;
            v.bakeB = static_cast<float>(mesh.colors[idx * 4 + 2]) / 255.0f * 2.0f;
            v.ao = static_cast<float>(mesh.colors[idx * 4 + 3]) / 255.0f;
        } else {
            v.ao = 1.0f;
        }
        return v;
    };
    if (mesh.indices != nullptr) {
        for (int i = 0; i < mesh.triangleCount * 3; ++i) out.push_back(vertexAt(mesh.indices[i]));
    } else {
        for (int i = 0; i < mesh.triangleCount * 3; ++i) out.push_back(vertexAt(i));
    }
}

float Noise(int a) {
    unsigned int x = static_cast<unsigned int>(a) * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return static_cast<float>(x & 0xFFFFu) / 65535.0f;
}

Image MakePerson(Color vest, Color sleeve, Color pants, Color hair, Color helmet, bool hasHelmet, bool longHair, Color stripe) {
    Image img = GenImageColor(48, 96, BLANK);
    Color skin{214, 170, 140, 255};
    Color boot{28, 26, 26, 255};
    ImageDrawRectangle(&img, 15, 58, 8, 32, pants);
    ImageDrawRectangle(&img, 25, 58, 8, 32, pants);
    ImageDrawRectangle(&img, 14, 88, 10, 7, boot);
    ImageDrawRectangle(&img, 25, 88, 10, 7, boot);
    ImageDrawRectangle(&img, 12, 26, 24, 34, vest);
    ImageDrawRectangle(&img, 12, 38, 24, 3, stripe);
    ImageDrawRectangle(&img, 12, 46, 24, 3, stripe);
    ImageDrawRectangle(&img, 5, 28, 7, 26, sleeve);
    ImageDrawRectangle(&img, 36, 28, 7, 26, sleeve);
    ImageDrawRectangle(&img, 5, 54, 7, 6, skin);
    ImageDrawRectangle(&img, 36, 54, 7, 6, skin);
    ImageDrawRectangle(&img, 21, 20, 6, 7, skin);
    ImageDrawCircle(&img, 24, 14, 9, skin);
    if (hasHelmet) {
        ImageDrawRectangle(&img, 14, 3, 20, 9, helmet);
        ImageDrawRectangle(&img, 12, 11, 24, 2, helmet);
    } else {
        ImageDrawRectangle(&img, 15, 3, 18, 8, hair);
        if (longHair) {
            ImageDrawRectangle(&img, 14, 8, 4, 24, hair);
            ImageDrawRectangle(&img, 30, 8, 4, 24, hair);
        }
    }
    ImageDrawRectangle(&img, 20, 14, 2, 2, Color{30, 24, 24, 255});
    ImageDrawRectangle(&img, 26, 14, 2, 2, Color{30, 24, 24, 255});
    return img;
}

Image MakeFlashlightIcon() {
    Image img = GenImageColor(64, 64, BLANK);
    ImageDrawRectangle(&img, 8, 27, 30, 11, Color{50, 52, 58, 255});
    ImageDrawRectangle(&img, 10, 29, 26, 2, Color{110, 112, 120, 255});
    ImageDrawRectangle(&img, 38, 23, 13, 19, Color{150, 154, 164, 255});
    ImageDrawRectangle(&img, 51, 25, 5, 15, Color{255, 236, 160, 255});
    ImageDrawRectangle(&img, 22, 25, 4, 15, Color{200, 60, 40, 255});
    return img;
}

}

int HouseHd::TextureHandle(engine::core::Engine& eng, const std::string& name) {
    if (name == "@white") return 0;
    for (const auto& [n, h] : m_cur->textureCache) {
        if (n == name) return h;
    }
    std::string path = eng.Assets().GetBaseDir() + "/sprites/3d/" + name + ".png";
    if (!FileExists(path.c_str())) path = eng.Assets().GetBaseDir() + "/sprites/3d/motel_concrete.png";
    Image img = LoadImage(path.c_str());
    int handle = 0;
    if (img.data != nullptr) {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        handle = m_cur->renderer->CreateTexture(static_cast<const uint8_t*>(img.data), img.width, img.height);
        if (handle < 0) handle = 0;
    }
    UnloadImage(img);
    m_cur->textureCache.emplace_back(name, handle);
    return handle;
}

int HouseHd::ItemTexture(engine::core::Engine& eng, const std::string& icon) {
    if (icon == "@flashlight") return m_cur->flashlightTex;
    if (icon == "@batteries" || icon.empty()) return m_cur->glowTex;
    for (const auto& [n, h] : m_cur->textureCache) {
        if (n == icon) return h;
    }
    std::string path = eng.Assets().GetBaseDir() + "/" + icon;
    int handle = m_cur->glowTex;
    if (FileExists(path.c_str())) {
        Image img = LoadImage(path.c_str());
        if (img.data != nullptr) {
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            int t = m_cur->renderer->CreateTexture(static_cast<const uint8_t*>(img.data), img.width, img.height);
            if (t >= 0) handle = t;
        }
        UnloadImage(img);
    }
    m_cur->textureCache.emplace_back(icon, handle);
    return handle;
}

void HouseHd::AddTrees(engine::core::Engine& eng, const HouseLevel& level) {
    const float layers[3][2] = {{2.2f, 1.0f}, {3.6f, 0.78f}, {5.0f, 0.55f}};
    std::map<std::pair<int, int>, std::pair<std::vector<HdVertex>, std::vector<HdVertex>>> cells;
    for (const HouseTree& t : level.Trees()) {
        auto key = std::make_pair(static_cast<int>(std::floor(t.position.x / 14.0f)), static_cast<int>(std::floor(t.position.z / 14.0f)));
        auto& cell = cells[key];
        Matrix world = MatrixMultiply(MatrixScale(t.scale, t.scale, t.scale), MatrixTranslate(t.position.x, t.position.y, t.position.z));
        AppendMesh(level.TrunkMesh(), &world, cell.first);
        for (const auto& layer : layers) {
            Matrix local = MatrixMultiply(MatrixScale(layer[1], layer[1], layer[1]), MatrixTranslate(0.0f, layer[0], 0.0f));
            Matrix full = MatrixMultiply(local, world);
            AppendMesh(level.CrownMesh(), &full, cell.second);
        }
    }
    HdMaterialDesc bark;
    bark.castShadow = false;
    MaterialFor("motel_bark", bark);
    HdMaterialDesc pine;
    pine.castShadow = false;
    MaterialFor("motel_pine", pine);
    pine.tint[0] = SrgbToLinear(150);
    pine.tint[1] = SrgbToLinear(175);
    pine.tint[2] = SrgbToLinear(150);
    int barkTex = TextureHandle(eng, "motel_bark");
    int pineTex = TextureHandle(eng, "motel_pine");
    for (auto& [key, cell] : cells) {
        int bm = m_cur->renderer->CreateMesh(cell.first, bark, barkTex);
        if (bm >= 0) m_cur->meshTags.emplace_back(bm, "");
        int pm = m_cur->renderer->CreateMesh(cell.second, pine, pineTex);
        if (pm >= 0) m_cur->meshTags.emplace_back(pm, "");
    }
}

bool HouseHd::Init(engine::core::Engine& eng, const HouseLevel& level, engine::hd::HdApi api) {
    Shutdown(eng);
    m_level = &level;
    const Ch3Graphics& cfg = Ch3Graphics::Get();
    bool ok = false;
    if (api == engine::hd::HdApi::D3D11) {
        engine::hd::HdCapabilities caps = engine::hd::ProbeCapabilities(eng.Assets().GetBaseDir());
        if (caps.d3d12 && caps.dxr && caps.dxc) {
            engine::hd::HdQuality q = cfg.ToQuality();
            q.rayTracing = 3;
            m_forceRt = true;
            ok = InitBackend(m_main, eng, level, engine::hd::HdApi::D3D12, q);
            if (ok && !m_main.renderer->SupportsRayTracing()) ok = false;
            if (!ok) {
                TraceLog(LOG_WARNING, "CH3 HD: DXR renderer unavailable (%s), using DX11", m_info.c_str());
                ShutdownBackend(m_main);
                m_forceRt = false;
            }
        }
    }
    if (!ok) ok = InitBackend(m_main, eng, level, api, cfg.ToQuality());
    m_cur = &m_main;
    return ok;
}

bool HouseHd::InitBackend(Backend& b, engine::core::Engine& eng, const HouseLevel& level, engine::hd::HdApi api, const engine::hd::HdQuality& quality) {
    m_cur = &b;
    m_cur->renderer = engine::hd::CreateHdRenderer(api);
    if (!m_cur->renderer) {
        m_info = "HD renderer is not available for the requested API";
        return false;
    }
    engine::hd::HdInitDesc desc;
    desc.width = 1280;
    desc.height = 720;
    desc.shaderDir = eng.Assets().GetBaseDir() + "/shaders/hd";
    desc.quality = quality;
    if (!m_cur->renderer->Init(desc)) {
        m_info = m_cur->renderer->LastError();
        m_cur->renderer.reset();
        return false;
    }

    m_cur->textureCache.clear();
    m_cur->meshTags.clear();
    m_cur->phantomTex = TextureHandle(eng, "motel_phantom");
    {
        Image resc = MakePerson(Color{34, 48, 92, 255}, Color{34, 48, 92, 255}, Color{22, 28, 48, 255}, BLACK, Color{26, 34, 66, 255}, true, false, Color{58, 76, 128, 255});
        ImageDrawRectangle(&resc, 21, 30, 5, 5, Color{210, 190, 90, 255});
        int h = m_cur->renderer->CreateTexture(static_cast<const uint8_t*>(resc.data), resc.width, resc.height);
        m_cur->rescuerTex = h >= 0 ? h : m_cur->glowTex;
        UnloadImage(resc);
        Image lera = MakePerson(Color{170, 40, 50, 255}, Color{170, 40, 50, 255}, Color{50, 65, 95, 255}, Color{45, 28, 20, 255}, WHITE, false, true, Color{235, 235, 235, 255});
        int h2 = m_cur->renderer->CreateTexture(static_cast<const uint8_t*>(lera.data), lera.width, lera.height);
        m_cur->leraTex = h2 >= 0 ? h2 : m_cur->glowTex;
        UnloadImage(lera);
    }
    {
        std::string path = eng.Assets().GetBaseDir() + "/sprites/3d/motel_phantom.png";
        Image img = LoadImage(path.c_str());
        if (img.data != nullptr) {
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            unsigned char* px = static_cast<unsigned char*>(img.data);
            for (int y = 0; y < img.height; ++y) {
                for (int x = 0; x < img.width; ++x) {
                    float fx = (static_cast<float>(x) / static_cast<float>(img.width) - 0.465f) / 0.13f;
                    float fy = (static_cast<float>(y) / static_cast<float>(img.height) - 0.178f) / 0.095f;
                    if (fx * fx + fy * fy > 1.0f) continue;
                    unsigned char* p = px + (static_cast<size_t>(y) * img.width + x) * 4;
                    if (p[3] < 8) continue;
                    float streak = 0.55f + 0.45f * Noise(x * 7 + (y / 3) * 131);
                    unsigned char v = static_cast<unsigned char>(22.0f + 30.0f * streak);
                    p[0] = v;
                    p[1] = static_cast<unsigned char>(v * 0.95f);
                    p[2] = static_cast<unsigned char>(v * 0.9f);
                }
            }
            int h = m_cur->renderer->CreateTexture(px, img.width, img.height);
            m_cur->phantomBackTex = h >= 0 ? h : m_cur->phantomTex;
        } else {
            m_cur->phantomBackTex = m_cur->phantomTex;
        }
        UnloadImage(img);
    }

    std::vector<uint8_t> glow(32 * 32 * 4);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            float dx = (static_cast<float>(x) + 0.5f) / 16.0f - 1.0f;
            float dy = (static_cast<float>(y) + 0.5f) / 16.0f - 1.0f;
            float r = std::sqrt(dx * dx + dy * dy);
            float a = std::clamp(1.0f - r, 0.0f, 1.0f);
            a = a * a * a;
            uint8_t* px = &glow[static_cast<size_t>(y * 32 + x) * 4];
            px[0] = px[1] = px[2] = 255;
            px[3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    m_cur->glowTex = m_cur->renderer->CreateTexture(glow.data(), 32, 32);
    Image fl = MakeFlashlightIcon();
    m_cur->flashlightTex = m_cur->renderer->CreateTexture(static_cast<const uint8_t*>(fl.data), fl.width, fl.height);
    UnloadImage(fl);

    size_t totalVerts = 0;
    for (const HouseBatch& b : level.Batches()) {
        std::vector<HdVertex> all;
        all.reserve(static_cast<size_t>(b.mesh.triangleCount) * 3);
        AppendMesh(b.mesh, nullptr, all);
        totalVerts += all.size();

        HdMaterialDesc m;
        MaterialFor(b.texture, m);
        Color tint = b.material.maps[MATERIAL_MAP_DIFFUSE].color;
        m.tint[0] = SrgbToLinear(tint.r);
        m.tint[1] = SrgbToLinear(tint.g);
        m.tint[2] = SrgbToLinear(tint.b);
        const bool fitted = b.texture == "ch3/house_door_interior" || b.texture == "ch3/house_workbench" || b.texture == "motel_radio" || b.texture == "motel_window" || b.texture == "motel_mirror_glass" || b.texture == "@white" || b.emissive > 0.0f;
        m.tint[3] = fitted ? 1.0f : 2.0f;
        bool bulb = b.emissive >= 0.99f;
        m.emissive = bulb ? 7.0f : b.emissive * 3.0f;
        int texHandle = TextureHandle(eng, b.texture);

        std::map<std::tuple<int, int, int>, std::vector<HdVertex>> cells;
        for (size_t t = 0; t + 2 < all.size(); t += 3) {
            float cx = (all[t].px + all[t + 1].px + all[t + 2].px) / 3.0f;
            float cy = (all[t].py + all[t + 1].py + all[t + 2].py) / 3.0f;
            float cz = (all[t].pz + all[t + 1].pz + all[t + 2].pz) / 3.0f;
            auto key = std::make_tuple(static_cast<int>(std::floor(cx / 5.0f)), static_cast<int>(std::floor(cy / 3.4f)),
                                       static_cast<int>(std::floor(cz / 5.0f)));
            auto& dst = cells[key];
            dst.push_back(all[t]);
            dst.push_back(all[t + 1]);
            dst.push_back(all[t + 2]);
        }
        for (auto& [key, verts] : cells) {
            int handle = m_cur->renderer->CreateMesh(verts, m, texHandle);
            if (handle >= 0) m_cur->meshTags.emplace_back(handle, b.tag);
        }
    }
    AddTrees(eng, level);
    m_cur->renderer->FinishLoading();

    m_info = std::string(m_cur->renderer->ApiName()) + " / " + m_cur->renderer->AdapterName() + (m_cur->renderer->SupportsRayTracing() ? " / DXR" : "");
    TraceLog(LOG_WARNING, "CH3 HD: %s, %d meshes, %d textures, %d vertices", m_info.c_str(), static_cast<int>(m_cur->meshTags.size()),
             static_cast<int>(m_cur->textureCache.size()), static_cast<int>(totalVerts));
    return true;
}

void HouseHd::Shutdown(engine::core::Engine& eng) {
    eng.Renderer().SetNativeOverlay(nullptr);
    if (m_overlay.id != 0) UnloadTexture(m_overlay);
    m_overlay = Texture2D{};
    ShutdownBackend(m_main);
    ShutdownBackend(m_rt);
    m_cur = &m_main;
}

void HouseHd::ShutdownBackend(Backend& b) {
    b.meshTags.clear();
    b.textureCache.clear();
    if (b.renderer) b.renderer->Shutdown();
    b.renderer.reset();
}

void HouseHd::EnsureOverlay(int w, int h) {
    if (m_overlay.id != 0 && m_overlay.width == w && m_overlay.height == h) return;
    if (m_overlay.id != 0) UnloadTexture(m_overlay);
    Image img = GenImageColor(w, h, BLACK);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    m_overlay = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(m_overlay, TEXTURE_FILTER_BILINEAR);
}

void HouseHd::Render(engine::core::Engine& eng, const HouseHdInput& in) {
    m_cur = &m_main;
    static Backend* lastLogged = nullptr;
    if (lastLogged != m_cur) {
        lastLogged = m_cur;
        TraceLog(LOG_WARNING, "CH3 HD: active backend -> %s", m_cur == &m_rt ? "DX12 ray tracing (bathroom)" : "DX11");
    }
    if (!m_cur->renderer) return;
    const Ch3Graphics& cfg = Ch3Graphics::Get();

    Rectangle present = eng.Renderer().GetPresentRect();
    float scale = cfg.RenderScaleValue();
    int w = static_cast<int>(present.width * scale);
    int h = static_cast<int>(present.height * scale);
    const int maxW = 2560;
    if (w > maxW) {
        h = h * maxW / w;
        w = maxW;
    }
    w = std::max(640, w & ~1);
    h = std::max(360, h & ~1);
    if (w != m_cur->renderer->Width() || h != m_cur->renderer->Height()) m_cur->renderer->Resize(w, h);

    for (const auto& [mesh, tag] : m_cur->meshTags) {
        m_cur->renderer->SetMeshVisible(mesh, in.batchVisible ? in.batchVisible(tag) : true);
    }

    HdFrameParams f;
    f.quality = cfg.ToQuality();
    if (m_forceRt) f.quality.rayTracing = 3;
    f.camPos[0] = in.camera.position.x; f.camPos[1] = in.camera.position.y; f.camPos[2] = in.camera.position.z;
    f.camTarget[0] = in.camera.target.x; f.camTarget[1] = in.camera.target.y; f.camTarget[2] = in.camera.target.z;
    f.camUp[0] = in.camera.up.x; f.camUp[1] = in.camera.up.y; f.camUp[2] = in.camera.up.z;
    f.fovY = in.camera.fovy;
    f.time = in.time;
    f.dt = in.dt;
    f.exposureBias = cfg.BrightnessValue();
    f.grain = cfg.grain ? 0.006f : 0.0f;
    f.chromatic = 0.0f;
    f.horror = cfg.HorrorValue();
    f.fear = cfg.SharpenValue();
    f.heartbeat = 0.0f;

    bool outside = in.camera.position.x < -13.4f || in.camera.position.x > 6.4f || in.camera.position.z > 6.4f || in.camera.position.z < -6.4f ||
                   in.camera.position.y < -3.4f;
    float target = outside ? 1.0f : 0.0f;
    m_outside += (target - m_outside) * std::min(1.0f, in.dt * 3.0f);
    float o = m_outside;
    f.ambientSky[0] = 0.008f + 0.022f * o; f.ambientSky[1] = 0.010f + 0.036f * o; f.ambientSky[2] = 0.015f + 0.075f * o;
    f.ambientGround[0] = 0.004f + 0.007f * o; f.ambientGround[1] = 0.005f + 0.011f * o; f.ambientGround[2] = 0.007f + 0.02f * o;
    f.fogColor[0] = 0.008f + 0.008f * o; f.fogColor[1] = 0.010f + 0.015f * o; f.fogColor[2] = 0.015f + 0.03f * o;
    f.fogDensity = 0.032f + 0.05f * o;
    f.scatterDensity = 0.08f * (1.0f - o) + 0.035f * o;
    f.bakedScale = 0.0f;
    if (in.day >= 3) {
        f.ambientSky[0] *= 0.8f; f.ambientSky[1] *= 0.8f; f.ambientSky[2] *= 0.8f;
    }

    if (m_level != nullptr) {
        const auto& lamps = m_level->Lamps();
        for (size_t i = 0; i < lamps.size(); ++i) {
            bool on = i < in.lampOn.size() ? in.lampOn[i] != 0 : true;
            if (!on) continue;
            const HouseLamp& l = lamps[i];
            HdLightDesc d;
            d.pos[0] = l.position.x; d.pos[1] = l.position.y - 0.12f; d.pos[2] = l.position.z;
            d.color[0] = l.color.x; d.color[1] = l.color.y; d.color[2] = l.color.z;
            d.radius = l.radius * 1.2f;
            float flick = 1.0f;
            float dg = Vector3Distance(in.prowlerPos, l.position);
            bool unstable = false;
            if (unstable) {
                float rate = in.prowlerChasing ? 24.0f : 11.0f;
                int tick = static_cast<int>(in.time * rate) + static_cast<int>(i) * 13;
                float n = Noise(tick);
                float thr = 0.18f;
                flick = n < thr * 0.5f ? 0.0f : (n < thr ? 0.5f : 1.0f);
            }
            d.intensity = l.intensity * 13.0f * flick;
            d.id = static_cast<int>(i);
            f.lights.push_back(d);
        }
    }

    if (!in.prowlerStunned && !in.hideProwler) {
        HdLightDesc g;
        g.pos[0] = in.prowlerPos.x; g.pos[1] = in.prowlerPos.y + 1.7f; g.pos[2] = in.prowlerPos.z;
        float pulse = 0.7f + 0.3f * std::sin(in.time * 6.0f);
        g.color[0] = 1.0f; g.color[1] = 0.05f; g.color[2] = 0.03f;
        g.radius = in.prowlerChasing ? 5.0f : 3.2f;
        g.intensity = (in.prowlerChasing ? 20.0f : 7.0f) * pulse;
        g.id = -1;
        f.lights.push_back(g);
    }

    f.spot.enabled = in.flashOn;
    f.spot.pos[0] = in.flashPos.x; f.spot.pos[1] = in.flashPos.y; f.spot.pos[2] = in.flashPos.z;
    f.spot.dir[0] = in.flashDir.x; f.spot.dir[1] = in.flashDir.y; f.spot.dir[2] = in.flashDir.z;
    f.spot.innerCos = std::cos(9.0f * DEG2RAD);
    f.spot.outerCos = std::cos(30.0f * DEG2RAD);
    f.spot.range = 15.0f;
    f.spot.intensity = 11.0f * in.flashPower;
    f.spot.color[0] = 1.0f; f.spot.color[1] = 0.9f; f.spot.color[2] = 0.74f;

    for (const HouseHdActor& a : in.actors) {
        if (f.billboards.size() >= 48) break;
        float h = a.dead ? 0.55f : 1.9f;
        float sink = a.dead ? std::min(1.0f, a.deadT * 3.0f) : 0.0f;
        float hh = 1.9f * (1.0f - 0.7f * sink);
        HdBillboard b;
        b.center[0] = a.pos.x; b.center[1] = a.pos.y + hh * 0.5f - 0.04f * sink; b.center[2] = a.pos.z;
        b.size[0] = 0.95f * (a.dead ? 1.0f + 0.5f * sink : 1.0f);
        b.size[1] = hh;
        float shade = a.dead ? 0.55f - 0.3f * sink : 0.95f;
        b.tint[0] = shade * (a.dead ? 1.3f : 1.0f); b.tint[1] = shade * (a.dead ? 0.5f : 1.0f); b.tint[2] = shade * (a.dead ? 0.5f : 1.0f); b.tint[3] = a.alpha;
        b.emissive = 0.35f;
        b.texture = a.kind == 2 ? m_cur->leraTex : m_cur->rescuerTex;
        (void)h;
        f.billboards.push_back(b);
    }

    if (!in.hideProwler && !(in.prowlerStunned && in.prowlerStunLeft <= 0.05f)) {
        float dist = Vector3Distance(in.camera.position, in.prowlerPos);
        float fog = std::clamp(1.0f - (dist - 7.0f) / 25.0f, 0.1f, 1.0f);
        float jitter = std::sin(in.time * 55.0f) > 0.3f ? 0.03f : -0.03f;
        float rise = in.prowlerStunned ? 0.25f : 1.0f;
        float alpha = in.prowlerStunned ? std::clamp(in.prowlerStunLeft / 4.0f, 0.0f, 0.55f) : 1.0f;
        float sizeY = 2.5f * (0.3f + 0.7f * rise);
        HdBillboard b;
        b.center[0] = in.prowlerPos.x + jitter;
        b.center[1] = in.prowlerPos.y + sizeY * 0.5f - (1.0f - rise) * 0.2f;
        b.center[2] = in.prowlerPos.z;
        b.size[0] = 1.25f;
        b.size[1] = sizeY;
        float shade = (0.25f + 0.75f * fog) * (in.prowlerChasing ? 1.0f : 0.8f);
        b.tint[0] = shade * (in.prowlerChasing ? 1.0f : 0.9f);
        b.tint[1] = shade * (in.prowlerChasing ? 0.45f : 0.85f);
        b.tint[2] = shade * (in.prowlerChasing ? 0.45f : 0.9f);
        b.tint[3] = alpha;
        b.emissive = 0.45f;
        Vector3 fwdG{std::cos(in.prowlerYaw), 0.0f, std::sin(in.prowlerYaw)};
        Vector3 toCamG{in.camera.position.x - in.prowlerPos.x, 0.0f, in.camera.position.z - in.prowlerPos.z};
        float tcl = Vector3Length(toCamG);
        bool seenFromFront = tcl < 0.01f || Vector3DotProduct(fwdG, Vector3Scale(toCamG, 1.0f / tcl)) > -0.1f;
        b.texture = seenFromFront ? m_cur->phantomTex : m_cur->phantomBackTex;
        f.billboards.push_back(b);

        if (!in.prowlerStunned && seenFromFront) {
            float pulse = 0.5f + 0.5f * std::sin(in.time * 24.0f);
            for (float side : {-1.0f, 1.0f}) {
                Vector3 fwd = Vector3Normalize(Vector3Subtract(in.camera.target, in.camera.position));
                Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, in.camera.up));
                Vector3 head{b.center[0], b.center[1] + sizeY * 0.28f, b.center[2]};
                Vector3 toCam = Vector3Normalize(Vector3Subtract(in.camera.position, head));
                Vector3 p = Vector3Add(head, Vector3Add(Vector3Scale(right, side * 0.055f), Vector3Scale(toCam, 0.04f)));
                HdBillboard eye;
                eye.center[0] = p.x; eye.center[1] = p.y; eye.center[2] = p.z;
                eye.size[0] = eye.size[1] = 0.075f;
                eye.tint[0] = 1.0f; eye.tint[1] = 0.12f + 0.1f * pulse; eye.tint[2] = 0.06f; eye.tint[3] = 1.0f;
                eye.emissive = 30.0f;
                eye.texture = m_cur->glowTex;
                f.billboards.push_back(eye);
                HdBillboard halo = eye;
                halo.size[0] = halo.size[1] = 0.4f;
                halo.tint[3] = 0.6f;
                halo.emissive = 3.0f + 2.0f * pulse;
                f.billboards.push_back(halo);
            }
        }
    }

    for (size_t k = 0; k < in.items.size() && f.billboards.size() < 56; ++k) {
        const HouseHdItem& it = in.items[k];
        float pulse = 0.65f + 0.35f * std::sin(in.time * 2.6f + static_cast<float>(k) * 1.7f);
        HdBillboard halo;
        halo.center[0] = it.pos.x; halo.center[1] = it.pos.y + 0.06f; halo.center[2] = it.pos.z;
        halo.size[0] = halo.size[1] = 0.42f;
        halo.tint[0] = 0.9f; halo.tint[1] = 0.8f; halo.tint[2] = 0.55f; halo.tint[3] = 0.5f;
        halo.emissive = 0.9f * pulse;
        halo.texture = m_cur->glowTex;
        f.billboards.push_back(halo);
        HdBillboard icon;
        icon.center[0] = it.pos.x; icon.center[1] = it.pos.y + 0.07f; icon.center[2] = it.pos.z;
        icon.size[0] = icon.size[1] = 0.17f;
        icon.tint[0] = icon.tint[1] = icon.tint[2] = 1.0f;
        icon.tint[3] = 1.0f;
        icon.emissive = 0.9f + 0.7f * pulse;
        icon.texture = ItemTexture(eng, it.icon);
        f.billboards.push_back(icon);
    }

    if (!m_cur->renderer->RenderFrame(f)) return;
    EnsureOverlay(m_cur->renderer->Width(), m_cur->renderer->Height());
    UpdateTexture(m_overlay, m_cur->renderer->Pixels());
    eng.Renderer().SetNativeOverlay(&m_overlay);
}

}
