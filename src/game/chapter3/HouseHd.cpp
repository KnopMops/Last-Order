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
    for (const auto& [n, h] : m_textureCache) {
        if (n == name) return h;
    }
    std::string path = eng.Assets().GetBaseDir() + "/sprites/3d/" + name + ".png";
    if (!FileExists(path.c_str())) path = eng.Assets().GetBaseDir() + "/sprites/3d/motel_concrete.png";
    Image img = LoadImage(path.c_str());
    int handle = 0;
    if (img.data != nullptr) {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        handle = m_renderer->CreateTexture(static_cast<const uint8_t*>(img.data), img.width, img.height);
        if (handle < 0) handle = 0;
    }
    UnloadImage(img);
    m_textureCache.emplace_back(name, handle);
    return handle;
}

int HouseHd::ItemTexture(engine::core::Engine& eng, const std::string& icon) {
    if (icon == "@flashlight") return m_flashlightTex;
    if (icon == "@batteries" || icon.empty()) return m_glowTex;
    for (const auto& [n, h] : m_textureCache) {
        if (n == icon) return h;
    }
    std::string path = eng.Assets().GetBaseDir() + "/" + icon;
    int handle = m_glowTex;
    if (FileExists(path.c_str())) {
        Image img = LoadImage(path.c_str());
        if (img.data != nullptr) {
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            int t = m_renderer->CreateTexture(static_cast<const uint8_t*>(img.data), img.width, img.height);
            if (t >= 0) handle = t;
        }
        UnloadImage(img);
    }
    m_textureCache.emplace_back(icon, handle);
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
        int bm = m_renderer->CreateMesh(cell.first, bark, barkTex);
        if (bm >= 0) m_meshTags.emplace_back(bm, "");
        int pm = m_renderer->CreateMesh(cell.second, pine, pineTex);
        if (pm >= 0) m_meshTags.emplace_back(pm, "");
    }
}

bool HouseHd::Init(engine::core::Engine& eng, const HouseLevel& level, engine::hd::HdApi api) {
    Shutdown(eng);
    m_level = &level;
    m_renderer = engine::hd::CreateHdRenderer(api);
    if (!m_renderer) {
        m_info = "HD renderer is not available for the requested API";
        return false;
    }
    const Ch3Graphics& cfg = Ch3Graphics::Get();
    engine::hd::HdInitDesc desc;
    desc.width = 1280;
    desc.height = 720;
    desc.shaderDir = eng.Assets().GetBaseDir() + "/shaders/hd";
    desc.quality = cfg.ToQuality();
    if (!m_renderer->Init(desc)) {
        m_info = m_renderer->LastError();
        m_renderer.reset();
        return false;
    }

    m_textureCache.clear();
    m_meshTags.clear();
    m_phantomTex = TextureHandle(eng, "motel_phantom");

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
    m_glowTex = m_renderer->CreateTexture(glow.data(), 32, 32);
    Image fl = MakeFlashlightIcon();
    m_flashlightTex = m_renderer->CreateTexture(static_cast<const uint8_t*>(fl.data), fl.width, fl.height);
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
            int handle = m_renderer->CreateMesh(verts, m, texHandle);
            if (handle >= 0) m_meshTags.emplace_back(handle, b.tag);
        }
    }
    AddTrees(eng, level);
    m_renderer->FinishLoading();

    m_info = std::string(m_renderer->ApiName()) + " / " + m_renderer->AdapterName() + (m_renderer->SupportsRayTracing() ? " / DXR" : "");
    TraceLog(LOG_WARNING, "CH3 HD: %s, %d meshes, %d textures, %d vertices", m_info.c_str(), static_cast<int>(m_meshTags.size()),
             static_cast<int>(m_textureCache.size()), static_cast<int>(totalVerts));
    return true;
}

void HouseHd::Shutdown(engine::core::Engine& eng) {
    eng.Renderer().SetNativeOverlay(nullptr);
    if (m_overlay.id != 0) UnloadTexture(m_overlay);
    m_overlay = Texture2D{};
    m_meshTags.clear();
    m_textureCache.clear();
    if (m_renderer) m_renderer->Shutdown();
    m_renderer.reset();
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
    if (!m_renderer) return;
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
    if (w != m_renderer->Width() || h != m_renderer->Height()) m_renderer->Resize(w, h);

    for (const auto& [mesh, tag] : m_meshTags) {
        m_renderer->SetMeshVisible(mesh, in.batchVisible ? in.batchVisible(tag) : true);
    }

    HdFrameParams f;
    f.quality = cfg.ToQuality();
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
    f.bakedScale = 0.1f;
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
            float dg = Vector3Distance(in.grannyPos, l.position);
            bool unstable = false;
            if (unstable) {
                float rate = in.grannyChasing ? 24.0f : 11.0f;
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

    if (!in.grannyStunned) {
        HdLightDesc g;
        g.pos[0] = in.grannyPos.x; g.pos[1] = in.grannyPos.y + 1.7f; g.pos[2] = in.grannyPos.z;
        float pulse = 0.7f + 0.3f * std::sin(in.time * 6.0f);
        g.color[0] = 1.0f; g.color[1] = 0.05f; g.color[2] = 0.03f;
        g.radius = in.grannyChasing ? 5.0f : 3.2f;
        g.intensity = (in.grannyChasing ? 20.0f : 7.0f) * pulse;
        g.id = -1;
        f.lights.push_back(g);
    }

    f.spot.enabled = in.flashOn;
    f.spot.pos[0] = in.flashPos.x; f.spot.pos[1] = in.flashPos.y; f.spot.pos[2] = in.flashPos.z;
    f.spot.dir[0] = in.flashDir.x; f.spot.dir[1] = in.flashDir.y; f.spot.dir[2] = in.flashDir.z;
    f.spot.innerCos = std::cos(14.0f * DEG2RAD);
    f.spot.outerCos = std::cos(27.0f * DEG2RAD);
    f.spot.range = 15.0f;
    f.spot.intensity = 32.0f * in.flashPower;
    f.spot.color[0] = 1.0f; f.spot.color[1] = 0.9f; f.spot.color[2] = 0.74f;

    if (!(in.grannyStunned && in.grannyStunLeft <= 0.05f)) {
        float dist = Vector3Distance(in.camera.position, in.grannyPos);
        float fog = std::clamp(1.0f - (dist - 7.0f) / 25.0f, 0.1f, 1.0f);
        float jitter = std::sin(in.time * 55.0f) > 0.3f ? 0.03f : -0.03f;
        float rise = in.grannyStunned ? 0.25f : 1.0f;
        float alpha = in.grannyStunned ? std::clamp(in.grannyStunLeft / 4.0f, 0.0f, 0.55f) : 1.0f;
        float sizeY = 2.5f * (0.3f + 0.7f * rise);
        HdBillboard b;
        b.center[0] = in.grannyPos.x + jitter;
        b.center[1] = in.grannyPos.y + sizeY * 0.5f - (1.0f - rise) * 0.2f;
        b.center[2] = in.grannyPos.z;
        b.size[0] = 1.25f;
        b.size[1] = sizeY;
        float shade = (0.25f + 0.75f * fog) * (in.grannyChasing ? 1.0f : 0.8f);
        b.tint[0] = shade * (in.grannyChasing ? 1.0f : 0.9f);
        b.tint[1] = shade * (in.grannyChasing ? 0.45f : 0.85f);
        b.tint[2] = shade * (in.grannyChasing ? 0.45f : 0.9f);
        b.tint[3] = alpha;
        b.emissive = 0.45f;
        b.texture = m_phantomTex;
        f.billboards.push_back(b);

        if (!in.grannyStunned) {
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
                eye.texture = m_glowTex;
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
        halo.texture = m_glowTex;
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

    if (!m_renderer->RenderFrame(f)) return;
    EnsureOverlay(m_renderer->Width(), m_renderer->Height());
    UpdateTexture(m_overlay, m_renderer->Pixels());
    eng.Renderer().SetNativeOverlay(&m_overlay);
}

}
