#include "game/custom/HdPlayView.h"
#include "game/chapter3/Ch3Graphics.h"
#include "raymath.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <cmath>
#include <filesystem>
#include <tuple>

namespace game::custom {

using engine::hd::HdVertex;
namespace km = knopmops;
namespace fs = std::filesystem;

namespace {

float SrgbToLinear(unsigned char c) { return std::pow(static_cast<float>(c) / 255.0f, 2.2f); }

void MaterialFor(const std::string& tex, engine::hd::HdMaterialDesc& m) {
    m.roughness = 0.85f;
    m.metallic = 0.0f;
    m.bump = 0.012f;
    if (tex == "motel_metal") { m.roughness = 0.36f; m.metallic = 0.85f; m.bump = 0.004f; }
    else if (tex == "motel_tile" || tex == "motel_porcelain") { m.roughness = 0.2f; m.bump = 0.004f; }
    else if (tex == "motel_window" || tex == "motel_mirror_glass") { m.roughness = 0.06f; m.bump = 0.0f; }
    else if (tex == "motel_wall_wood" || tex == "motel_trim" || tex == "motel_door") { m.roughness = 0.6f; }
    else if (tex == "motel_concrete") { m.roughness = 0.92f; m.bump = 0.016f; }
    else if (tex == "motel_fabric" || tex == "motel_carpet") { m.roughness = 0.97f; m.bump = 0.005f; }
    else if (tex == "motel_grass" || tex == "motel_gravel" || tex == "motel_bark" || tex == "motel_pine") { m.roughness = 1.0f; m.bump = 0.016f; }
}

HdVertex V(Vector3 p, Vector3 n, float u, float v) {
    HdVertex h{};
    h.px = p.x; h.py = p.y; h.pz = p.z;
    h.nx = n.x; h.ny = n.y; h.nz = n.z;
    h.u = u; h.v = v;
    h.ao = 1.0f;
    return h;
}

Image MakeMonsterImage(bool mimic) {
    Image img = GenImageColor(64, 128, BLANK);
    Color suit = mimic ? Color{54, 50, 58, 255} : Color{14, 14, 18, 255};
    Color skin = mimic ? Color{196, 190, 182, 255} : Color{168, 164, 158, 255};
    ImageDrawRectangle(&img, 24, 36, 16, 48, suit);        // торс
    ImageDrawRectangle(&img, 14, 38, 6, 76, suit);          // левая рука (до колен)
    ImageDrawRectangle(&img, 44, 38, 6, 76, suit);          // правая
    ImageDrawRectangle(&img, 13, 112, 8, 7, skin);          // кисти
    ImageDrawRectangle(&img, 43, 112, 8, 7, skin);
    ImageDrawRectangle(&img, 25, 84, 6, 42, suit);          // ноги
    ImageDrawRectangle(&img, 33, 84, 6, 42, suit);
    ImageDrawRectangle(&img, 29, 28, 6, 10, skin);          // шея
    ImageDrawCircle(&img, 32, 18, 12, skin);                // голова
    ImageDrawRectangle(&img, 21, 10, 22, 4, Color{30, 28, 28, 255}); // тень лба
    ImageDrawRectangle(&img, 30, 21, 4, 14, Color{6, 4, 4, 255});    // длинный разрез рта
    ImageDrawRectangle(&img, 26, 15, 4, 5, Color{4, 3, 3, 255});     // глазницы
    ImageDrawRectangle(&img, 34, 15, 4, 5, Color{4, 3, 3, 255});
    ImageDrawRectangle(&img, 27, 16, 2, 3, Color{255, 60, 30, 255});   // зрачки светятся
    ImageDrawRectangle(&img, 35, 16, 2, 3, Color{255, 60, 30, 255});
    return img;
}

}

int HdPlayView::Tex(const std::string& name) {
    auto it = m_textures.find(name);
    if (it != m_textures.end()) return it->second;
    int handle = -1;
    std::string path = m_texDir + "/" + name + ".png";
    if (!fs::exists(path)) path = m_assetTexDir + "/" + name + ".png";
    Image img{};
    if (!name.empty() && fs::exists(path)) img = LoadImage(path.c_str());
    if (img.data == nullptr) img = GenImageColor(8, 8, WHITE);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    handle = m_renderer->CreateTexture(static_cast<const uint8_t*>(img.data), img.width, img.height);
    UnloadImage(img);
    m_textures[name] = handle;
    return handle;
}

void HdPlayView::AddBox(std::vector<HdVertex>& out, Vector3 c, Vector3 size, float yawDeg) {
    float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;
    float yr = yawDeg * DEG2RAD, cs = std::cos(yr), sn = std::sin(yr);
    auto rot = [&](Vector3 p) { return Vector3{c.x + p.x * cs + p.z * sn, c.y + p.y, c.z - p.x * sn + p.z * cs}; };
    auto rotN = [&](Vector3 n) { return Vector3{n.x * cs + n.z * sn, n.y, -n.x * sn + n.z * cs}; };
    auto quad = [&](Vector3 a, Vector3 b, Vector3 cc, Vector3 d, Vector3 n, float su, float sv) {
        Vector3 wn = rotN(n);
        out.push_back(V(rot(a), wn, 0, 0));
        out.push_back(V(rot(b), wn, su, 0));
        out.push_back(V(rot(cc), wn, su, sv));
        out.push_back(V(rot(a), wn, 0, 0));
        out.push_back(V(rot(cc), wn, su, sv));
        out.push_back(V(rot(d), wn, 0, sv));
    };
    quad({-hx, -hy, hz}, {hx, -hy, hz}, {hx, hy, hz}, {-hx, hy, hz}, {0, 0, 1}, size.x, size.y);
    quad({hx, -hy, -hz}, {-hx, -hy, -hz}, {-hx, hy, -hz}, {hx, hy, -hz}, {0, 0, -1}, size.x, size.y);
    quad({hx, -hy, hz}, {hx, -hy, -hz}, {hx, hy, -hz}, {hx, hy, hz}, {1, 0, 0}, size.z, size.y);
    quad({-hx, -hy, -hz}, {-hx, -hy, hz}, {-hx, hy, hz}, {-hx, hy, -hz}, {-1, 0, 0}, size.z, size.y);
    quad({-hx, hy, hz}, {hx, hy, hz}, {hx, hy, -hz}, {-hx, hy, -hz}, {0, 1, 0}, size.x, size.z);
    quad({-hx, -hy, -hz}, {hx, -hy, -hz}, {hx, -hy, hz}, {-hx, -hy, hz}, {0, -1, 0}, size.x, size.z);
}

void HdPlayView::AddCylinder(std::vector<HdVertex>& out, Vector3 c, float radius, float height) {
    const int N = 12;
    float y0 = c.y - height * 0.5f, y1 = c.y + height * 0.5f;
    for (int i = 0; i < N; ++i) {
        float a0 = (float)i / N * 2.0f * PI, a1 = (float)(i + 1) / N * 2.0f * PI;
        Vector3 p0{c.x + std::cos(a0) * radius, 0, c.z + std::sin(a0) * radius};
        Vector3 p1{c.x + std::cos(a1) * radius, 0, c.z + std::sin(a1) * radius};
        Vector3 n0{std::cos(a0), 0, std::sin(a0)}, n1{std::cos(a1), 0, std::sin(a1)};
        float u0 = (float)i / N * 2.0f, u1 = (float)(i + 1) / N * 2.0f;
        out.push_back(V({p1.x, y0, p1.z}, n1, u1, 0)); out.push_back(V({p0.x, y0, p0.z}, n0, u0, 0)); out.push_back(V({p0.x, y1, p0.z}, n0, u0, height));
        out.push_back(V({p1.x, y0, p1.z}, n1, u1, 0)); out.push_back(V({p0.x, y1, p0.z}, n0, u0, height)); out.push_back(V({p1.x, y1, p1.z}, n1, u1, height));
        out.push_back(V({c.x, y1, c.z}, {0, 1, 0}, 0.5f, 0.5f)); out.push_back(V({p1.x, y1, p1.z}, {0, 1, 0}, 1, 0)); out.push_back(V({p0.x, y1, p0.z}, {0, 1, 0}, 0, 0));
    }
}

bool HdPlayView::Init(engine::core::Engine& eng, km::runtime::PlaySession& s, const km::runtime::ChapterProject& project, engine::hd::HdApi api) {
    Shutdown(eng);
    const auto& cfg = game::chapter3::Ch3Graphics::Get();
    bool ok = false;
    m_forceRt = false;

    auto create = [&](engine::hd::HdApi a, const engine::hd::HdQuality& q) {
        m_renderer = engine::hd::CreateHdRenderer(a);
        if (!m_renderer) { m_info = "HD-рендер недоступен для выбранного API"; return false; }
        engine::hd::HdInitDesc desc;
        desc.width = 1280;
        desc.height = 720;
        desc.shaderDir = eng.Assets().GetBaseDir() + "/shaders/hd";
        desc.quality = q;
        if (!m_renderer->Init(desc)) { m_info = m_renderer->LastError(); m_renderer.reset(); return false; }
        return true;
    };

    if (api == engine::hd::HdApi::D3D11) {
        auto caps = engine::hd::ProbeCapabilities(eng.Assets().GetBaseDir());
        if (caps.d3d12 && caps.dxr && caps.dxc) {
            auto q = cfg.ToQuality();
            q.rayTracing = 3;
            ok = create(engine::hd::HdApi::D3D12, q);
            if (ok && !m_renderer->SupportsRayTracing()) { ok = false; m_renderer->Shutdown(); m_renderer.reset(); }
            m_forceRt = ok;
        }
    }
    if (!ok) ok = create(api, cfg.ToQuality());
    if (!ok) return false;

    m_texDir = project.TexturesDir();
    m_assetTexDir = eng.Assets().GetBaseDir() + "/../KnopMops Engine/assets/textures";
    m_textures.clear();
    m_objMeshes.clear();
    m_slides.clear();
    m_revision = s.GeometryRevision();

    {
        Image im = MakeMonsterImage(false);
        m_monsterTex = m_renderer->CreateTexture(static_cast<const uint8_t*>(im.data), im.width, im.height);
        UnloadImage(im);
        Image im2 = MakeMonsterImage(true);
        m_mimicTex = m_renderer->CreateTexture(static_cast<const uint8_t*>(im2.data), im2.width, im2.height);
        UnloadImage(im2);
        std::vector<uint8_t> glow(32 * 32 * 4);
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                float dx = (x + 0.5f) / 16.0f - 1.0f, dy = (y + 0.5f) / 16.0f - 1.0f;
                float a = std::clamp(1.0f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
                a = a * a * a;
                uint8_t* px = &glow[(size_t)(y * 32 + x) * 4];
                px[0] = px[1] = px[2] = 255;
                px[3] = (uint8_t)(a * 255.0f);
            }
        }
        m_glowTex = m_renderer->CreateTexture(glow.data(), 32, 32);
    }

    size_t total = 0;
    for (const auto& b : s.RawBatches()) {
        std::map<std::tuple<int, int, int>, std::vector<HdVertex>> cells;
        for (size_t t = 0; t + 2 < b.verts.size(); t += 3) {
            const auto& a = b.verts[t]; const auto& bb = b.verts[t + 1]; const auto& c = b.verts[t + 2];
            Vector3 n = a.n;
            Vector3 e1 = Vector3Subtract(bb.p, a.p), e2 = Vector3Subtract(c.p, a.p);
            bool flip = Vector3DotProduct(Vector3CrossProduct(e1, e2), n) < 0.0f;
            const auto* v0 = &a; const auto* v1 = flip ? &c : &bb; const auto* v2 = flip ? &bb : &c;
            Vector3 ctr{(a.p.x + bb.p.x + c.p.x) / 3, (a.p.y + bb.p.y + c.p.y) / 3, (a.p.z + bb.p.z + c.p.z) / 3};
            auto& dst = cells[{(int)std::floor(ctr.x / 8.0f), (int)std::floor(ctr.y / 4.0f), (int)std::floor(ctr.z / 8.0f)}];
            for (const auto* v : {v0, v1, v2}) dst.push_back(V(v->p, v->n, v->uv.x, v->uv.y));
        }
        engine::hd::HdMaterialDesc m;
        MaterialFor(b.texture, m);
        m.tint[3] = 2.0f;
        int th = Tex(b.texture);
        for (auto& [key, verts] : cells) {
            total += verts.size();
            m_renderer->CreateMesh(verts, m, th);
        }
    }

    for (const auto& up : s.GetScene().GetGameObjects()) {
        const auto& o = *up;
        if (o.player.enabled || o.aiSpawn.enabled || o.rigidbody.enabled) continue;
        const bool dynamic = o.tag == "dynamic";
        float slideX0 = 0.0f, slideX1 = 0.0f;
        if (dynamic) {
            auto it = o.props.find("hd_slide"); // "x0,x1": диапазон движения по X
            if (it == o.props.end() || std::sscanf(it->second.c_str(), "%f,%f", &slideX0, &slideX1) != 2) continue;
        }
        std::string shape = o.meshRenderer.meshName, tex = o.meshRenderer.textureName;
        Vector3 size = o.transform.scale;
        if (shape == "Auto" || shape.empty()) {
            if (o.door.enabled) { shape = "Cube"; if (tex.empty()) tex = "motel_door"; }
            else if (o.hidingSpot.enabled) { shape = "Cube"; if (tex.empty()) tex = "motel_wall_wood"; }
            else if (o.searchSlot.enabled) { shape = "Cube"; if (tex.empty()) tex = "motel_cardboard"; }
            else if (o.itemPickup.enabled) { shape = "Cube"; size = {0.22f, 0.22f, 0.22f}; }
            else continue;
        }
        if (shape == "None" || !o.meshRenderer.enabled) continue;
        std::vector<HdVertex> verts;
        Vector3 wp = s.GetScene().WorldPosition(o);
        if (shape == "Cube") AddBox(verts, wp, size, o.transform.rotation.y);
        else if (shape == "Cylinder") AddCylinder(verts, wp, size.x * 0.5f, size.y);
        else if (shape == "Sphere") AddBox(verts, wp, size, 0.0f);
        else continue;
        engine::hd::HdMaterialDesc m;
        MaterialFor(tex, m);
        Color t = o.meshRenderer.tint;
        m.tint[0] = SrgbToLinear(t.r); m.tint[1] = SrgbToLinear(t.g); m.tint[2] = SrgbToLinear(t.b);
        m.tint[3] = 1.0f;
        m.emissive = (o.meshRenderer.emissive || o.itemPickup.enabled) ? 7.0f : 0.0f;
        if (dynamic) {
            SlideGroup grp;
            grp.objId = o.id; grp.x0 = slideX0; grp.step = 0.1f; // шаг копий: чем мельче, тем плавнее едет платформа
            int n = (int)((slideX1 - slideX0) / grp.step) + 1;
            for (int k = 0; k < n; ++k) {
                std::vector<HdVertex> v2;
                Vector3 p2{slideX0 + k * grp.step, wp.y, wp.z};
                if (shape == "Cylinder") AddCylinder(v2, p2, size.x * 0.5f, size.y); else AddBox(v2, p2, size, o.transform.rotation.y);
                int hh = m_renderer->CreateMesh(v2, m, Tex(tex));
                grp.handles.push_back(hh);
            }
            m_slides.push_back(std::move(grp));
            continue;
        }
        int h = m_renderer->CreateMesh(verts, m, Tex(tex));
        if (h >= 0) m_objMeshes.push_back({h, o.id, o.door.enabled});
        total += verts.size();
    }
    m_renderer->FinishLoading();
    m_info = std::string(m_renderer->ApiName()) + " / " + m_renderer->AdapterName() + (m_renderer->SupportsRayTracing() ? " / DXR" : "");
    TraceLog(LOG_WARNING, "CHAPTER HD: %s, %d vertices", m_info.c_str(), (int)total);
    return true;
}

void HdPlayView::Shutdown(engine::core::Engine& eng) {
    eng.Renderer().SetNativeOverlay(nullptr);
    if (m_overlay.id != 0) UnloadTexture(m_overlay);
    m_overlay = Texture2D{};
    m_slides.clear();
    m_objMeshes.clear();
    m_textures.clear();
    if (m_renderer) m_renderer->Shutdown();
    m_renderer.reset();
}

void HdPlayView::EnsureOverlay(int w, int h) {
    if (m_overlay.id != 0 && m_overlay.width == w && m_overlay.height == h) return;
    if (m_overlay.id != 0) UnloadTexture(m_overlay);
    Image img = GenImageColor(w, h, BLACK);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    m_overlay = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(m_overlay, TEXTURE_FILTER_BILINEAR);
}

void HdPlayView::Render(engine::core::Engine& eng, km::runtime::PlaySession& s) {
    if (!m_renderer) return;
    const auto& cfg = game::chapter3::Ch3Graphics::Get();

    Rectangle present = eng.Renderer().GetPresentRect();
    float scale = cfg.RenderScaleValue();
    int w = (int)(present.width * scale), h = (int)(present.height * scale);
    if (w > 2560) { h = h * 2560 / w; w = 2560; }
    w = std::max(640, w & ~1);
    h = std::max(360, h & ~1);
    if (w != m_renderer->Width() || h != m_renderer->Height()) m_renderer->Resize(w, h);

    for (const auto& om : m_objMeshes) {
        const auto* o = s.GetScene().FindGameObject(om.objId);
        bool vis = o && o->active && s.Rt(om.objId).visible;
        if (vis && om.door && s.Rt(om.objId).doorT > 0.5f) vis = false;
        m_renderer->SetMeshVisible(om.handle, vis);
    }

    for (auto& grp : m_slides) {
        const auto* o = s.GetScene().FindGameObject(grp.objId);
        int idx = -2;
        if (o && o->active) {
            float x = s.GetScene().WorldPosition(*o).x;
            idx = std::clamp((int)std::lround((x - grp.x0) / grp.step), 0, (int)grp.handles.size() - 1);
        }
        if (idx != grp.shown) {
            for (size_t i = 0; i < grp.handles.size(); ++i) m_renderer->SetMeshVisible(grp.handles[i], (int)i == idx);
            grp.shown = idx;
        }
    }

    const Camera3D cam = s.GetCamera();
    const auto& env = s.GetScene().Env();
    float time = (float)s.Time();
    engine::hd::HdFrameParams f;
    f.quality = cfg.ToQuality();
    bool indoor = !s.LineOfSightFree(cam.position, Vector3{cam.position.x, cam.position.y + 8.0f, cam.position.z});
    if (m_forceRt && !indoor) f.quality.rayTracing = 3;
    else if (m_forceRt) f.quality.rayTracing = 0;
    f.camPos[0] = cam.position.x; f.camPos[1] = cam.position.y; f.camPos[2] = cam.position.z;
    f.camTarget[0] = cam.target.x; f.camTarget[1] = cam.target.y; f.camTarget[2] = cam.target.z;
    f.camUp[0] = cam.up.x; f.camUp[1] = cam.up.y; f.camUp[2] = cam.up.z;
    f.fovY = cam.fovy;
    f.time = time;
    f.dt = GetFrameTime();
    f.exposureBias = cfg.BrightnessValue();
    f.grain = cfg.grain ? 0.006f : 0.0f;
    f.chromatic = 0.0f;
    f.horror = cfg.HorrorValue();
    f.fear = cfg.SharpenValue();

    float a = env.ambient;
    f.ambientSky[0] = a * 0.20f; f.ambientSky[1] = a * 0.27f; f.ambientSky[2] = a * 0.45f;
    f.ambientGround[0] = a * 0.08f; f.ambientGround[1] = a * 0.10f; f.ambientGround[2] = a * 0.16f;
    f.fogColor[0] = env.background.r / 255.0f * 0.12f; f.fogColor[1] = env.background.g / 255.0f * 0.12f; f.fogColor[2] = env.background.b / 255.0f * 0.12f;
    f.fogDensity = 1.5f / std::max(8.0f, env.fogEnd);
    f.scatterDensity = 0.02f;
    f.bakedScale = 0.0f;

    using LS = std::remove_cv_t<std::remove_reference_t<decltype(s.LightSamples().front())>>;
    std::vector<const LS*> closest;
    for (const auto& l : s.LightSamples()) closest.push_back(&l);
    std::sort(closest.begin(), closest.end(), [&](const auto* a, const auto* b2) { return Vector3Distance(a->pos, cam.position) - a->radius < Vector3Distance(b2->pos, cam.position) - b2->radius; });
    if (closest.size() > 10) closest.resize(10);
    for (const auto* lp : closest) {
        const auto& l = *lp;
        float inten = std::max({l.color.x, l.color.y, l.color.z});
        if (inten < 0.002f) continue;
        engine::hd::HdLightDesc d;
        d.pos[0] = l.pos.x; d.pos[1] = l.pos.y - 0.1f; d.pos[2] = l.pos.z;
        d.color[0] = l.color.x / inten; d.color[1] = l.color.y / inten; d.color[2] = l.color.z / inten;
        d.radius = l.radius * 1.2f;
        d.intensity = inten * 13.0f * std::clamp(9.0f / l.radius, 0.25f, 1.0f); // большие радиусы (луна) не должны засвечивать туман
        d.id = -1;
        f.lights.push_back(d);
        if (f.lights.size() >= 48) break;
    }

    const auto& pl = s.Player();
    f.spot.enabled = pl.flashlight;
    Vector3 fd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    f.spot.pos[0] = cam.position.x; f.spot.pos[1] = cam.position.y - 0.1f; f.spot.pos[2] = cam.position.z;
    f.spot.dir[0] = fd.x; f.spot.dir[1] = fd.y; f.spot.dir[2] = fd.z;
    f.spot.innerCos = std::cos(9.0f * DEG2RAD);
    f.spot.outerCos = std::cos(30.0f * DEG2RAD);
    f.spot.range = 15.0f;
    f.spot.intensity = 11.0f * (0.4f + 0.6f * std::min(1.0f, pl.battery * 4.0f));
    f.spot.color[0] = 1.0f; f.spot.color[1] = 0.9f; f.spot.color[2] = 0.74f;

    for (const auto& m : s.Monsters()) {
        const auto* o = s.GetScene().FindGameObject(m.objId);
        if (!o || !o->active || !s.Rt(m.objId).visible) continue;
        if (!s.LineOfSightFree(cam.position, Vector3{m.pos.x, m.pos.y + 1.2f, m.pos.z}) && !s.LineOfSightFree(cam.position, Vector3{m.pos.x, m.pos.y + 1.9f, m.pos.z})) continue;
        bool mimic = o->tag == "mimic";
        bool chase = m.mode == km::runtime::MonsterMode::Chase;
        float height = mimic ? 2.15f : 2.45f;
        float width = height * 0.5f;
        float jitter = (chase || mimic) && std::sin(time * 55.0f + (float)m.objId) > 0.4f ? 0.025f : -0.025f;
        engine::hd::HdBillboard b;
        b.center[0] = m.pos.x + jitter; b.center[1] = m.pos.y + height * 0.5f; b.center[2] = m.pos.z;
        b.size[0] = width; b.size[1] = height;
        float dist = Vector3Distance(cam.position, m.pos);
        float shade = 0.35f + 0.65f * std::clamp(1.0f - (dist - 6.0f) / 25.0f, 0.1f, 1.0f);
        b.tint[0] = shade; b.tint[1] = shade * (chase ? 0.55f : 0.9f); b.tint[2] = shade * (chase ? 0.55f : 0.95f); b.tint[3] = 1.0f;
        b.emissive = 0.4f;
        b.texture = mimic ? m_mimicTex : m_monsterTex;
        f.billboards.push_back(b);

        float pulse = 0.5f + 0.5f * std::sin(time * 24.0f);
        Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
        Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, cam.up));
        Vector3 head{b.center[0], m.pos.y + height * 0.86f, b.center[2]};
        Vector3 toCam = Vector3Normalize(Vector3Subtract(cam.position, head));
        {
            engine::hd::HdBillboard halo;
            Vector3 hp = Vector3Add(head, Vector3Scale(toCam, 0.06f));
            halo.center[0] = hp.x; halo.center[1] = hp.y; halo.center[2] = hp.z;
            halo.size[0] = halo.size[1] = height * 0.2f;
            halo.tint[0] = 1.0f; halo.tint[1] = 0.12f + 0.1f * pulse; halo.tint[2] = 0.06f; halo.tint[3] = 0.28f;
            halo.emissive = 1.5f + 1.5f * pulse;
            halo.texture = m_glowTex;
            f.billboards.push_back(halo);
        }
        if (chase) {
            engine::hd::HdLightDesc g;
            g.pos[0] = m.pos.x; g.pos[1] = m.pos.y + 1.8f; g.pos[2] = m.pos.z;
            g.color[0] = 1.0f; g.color[1] = 0.05f; g.color[2] = 0.03f;
            g.radius = 5.0f; g.intensity = 18.0f * (0.7f + 0.3f * pulse);
            f.lights.push_back(g);
        }
        if (f.billboards.size() > 120) break;
    }

    if (!m_renderer->RenderFrame(f)) return;
    EnsureOverlay(m_renderer->Width(), m_renderer->Height());
    UpdateTexture(m_overlay, m_renderer->Pixels());
    eng.Renderer().SetNativeOverlay(&m_overlay);
}

}
