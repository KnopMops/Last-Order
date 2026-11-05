#include "render/ViewportRenderer.h"
#include "render/DrawUtil.h"
#include "render/TextureManager.h"
#include "core/Math3D.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace knopmops::render {

namespace {

constexpr Color kSelColor{255, 215, 0, 255};

Vector3 ObjCenter(const scene::GameObject& o, Vector3 wp) {
    // Player/Monster: pivot на полу; остальные — центр меша.
    if (o.player.enabled || o.aiSpawn.enabled) return Vector3{wp.x, wp.y + 0.9f, wp.z};
    return wp;
}

Vector3 ObjSize(const scene::GameObject& o) {
    if (o.player.enabled) return Vector3{o.player.radius * 2, o.player.height, o.player.radius * 2};
    if (o.aiSpawn.enabled) return Vector3{0.6f, 1.8f, 0.6f};
    if (o.collider.enabled) {
        const auto& c = o.collider;
        if (c.shape == 1) return Vector3{c.radius * 2, c.radius * 2, c.radius * 2};
        if (c.shape == 2) return Vector3{c.radius * 2, std::max(c.height, c.radius * 2), c.radius * 2};
        return c.size;
    }
    Vector3 s = o.transform.scale;
    return Vector3{std::max(s.x, 0.4f), std::max(s.y, 0.4f), std::max(s.z, 0.4f)};
}

BoundingBox PickBounds(const scene::GameObject& o, Vector3 wp) {
    Vector3 c = ObjCenter(o, wp);
    Vector3 s = ObjSize(o);
    if (o.collider.enabled && !(o.player.enabled || o.aiSpawn.enabled)) c = Vector3{c.x + o.collider.offset.x, c.y + o.collider.offset.y, c.z + o.collider.offset.z};
    float yaw = std::fmod(std::fabs(o.transform.rotation.y), 180.0f);
    if (yaw > 45.0f && yaw < 135.0f) std::swap(s.x, s.z);
    return BoundingBox{Vector3{c.x - s.x * 0.5f, c.y - s.y * 0.5f, c.z - s.z * 0.5f}, Vector3{c.x + s.x * 0.5f, c.y + s.y * 0.5f, c.z + s.z * 0.5f}};
}

// Параметры ближайшего сближения луча и прямой (p + a*t). Возвращает расстояние; outT — параметр на прямой, outS — на луче.
float RayLineClosest(Ray ray, Vector3 p, Vector3 a, float* outT, float* outS) {
    Vector3 w0 = core::Vec3Sub(ray.position, p);
    float b = core::Vec3Dot(ray.direction, a);
    float d0 = core::Vec3Dot(ray.direction, w0);
    float e = core::Vec3Dot(a, w0);
    float denom = 1.0f - b * b;
    float s, t;
    if (std::fabs(denom) < 1e-5f) { s = 0.0f; t = e; }
    else { s = (b * e - d0) / denom; t = e + b * s; }
    if (outT) *outT = t;
    if (outS) *outS = s;
    Vector3 pr = core::Vec3Add(ray.position, core::Vec3Scale(ray.direction, s));
    Vector3 pa = core::Vec3Add(p, core::Vec3Scale(a, t));
    return core::Vec3Length(core::Vec3Sub(pr, pa));
}

}

ViewportRenderer::ViewportRenderer() {
    ResetCamera();
}

ViewportRenderer::~ViewportRenderer() {
    Shutdown();
}

void ViewportRenderer::Init(int width, int height) {
    m_width = width > 0 ? width : 1280;
    m_height = height > 0 ? height : 720;
    m_target = LoadRenderTexture(m_width, m_height);
    SetTextureFilter(m_target.texture, TEXTURE_FILTER_BILINEAR);
}

void ViewportRenderer::Resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;

    if (m_target.id != 0) UnloadRenderTexture(m_target);
    m_width = width;
    m_height = height;
    m_target = LoadRenderTexture(m_width, m_height);
    SetTextureFilter(m_target.texture, TEXTURE_FILTER_BILINEAR);
}

void ViewportRenderer::Shutdown() {
    if (m_target.id != 0) {
        UnloadRenderTexture(m_target);
        m_target = RenderTexture2D{};
    }
}

void ViewportRenderer::ResetCamera() {
    m_camera.position = Vector3{8.0f, 6.0f, 8.0f};
    m_camera.target = Vector3{0.0f, 1.0f, 0.0f};
    m_camera.up = Vector3{0.0f, 1.0f, 0.0f};
    m_camera.fovy = 55.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;

    m_cameraYaw = 45.0f;
    m_cameraPitch = 30.0f;
    m_cameraDist = 12.0f;
    m_cameraTarget = Vector3{0.0f, 1.0f, 0.0f};
}

void ViewportRenderer::FocusOn(Vector3 targetPos, float dist) {
    m_cameraTarget = targetPos;
    m_cameraDist = dist;

    float yawRad = m_cameraYaw * DEG2RAD;
    float pitchRad = m_cameraPitch * DEG2RAD;

    m_camera.target = m_cameraTarget;
    m_camera.position = Vector3{
        m_cameraTarget.x + m_cameraDist * std::cos(pitchRad) * std::sin(yawRad),
        m_cameraTarget.y + m_cameraDist * std::sin(pitchRad),
        m_cameraTarget.z + m_cameraDist * std::cos(pitchRad) * std::cos(yawRad)
    };
}

void ViewportRenderer::Update(float dt, bool isHovered, bool isFocused) {
    if (!isHovered && !isFocused) return;

    Vector2 mouseDelta = GetMouseDelta();
    float wheel = GetMouseWheelMove();

    if (wheel != 0.0f && isHovered) {
        m_cameraDist = std::clamp(m_cameraDist - wheel * std::max(0.6f, m_cameraDist * 0.12f), 0.8f, 800.0f);
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && isHovered) {
        float rotSpeed = 0.25f;
        m_cameraYaw -= mouseDelta.x * rotSpeed;
        m_cameraPitch = std::clamp(m_cameraPitch - mouseDelta.y * rotSpeed, -85.0f, 85.0f);

        float yawRad = m_cameraYaw * DEG2RAD;
        float pitchRad = m_cameraPitch * DEG2RAD;

        // направление взгляда из точки орбиты в камеру: камера = target + dist*(sin y cos p, sin p, cos y cos p)
        Vector3 forward{
            -std::sin(yawRad) * std::cos(pitchRad),
            -std::sin(pitchRad),
            -std::cos(yawRad) * std::cos(pitchRad)
        };
        Vector3 right{std::cos(yawRad), 0.0f, -std::sin(yawRad)};

        float flySpeed = 8.0f * dt;
        if (IsKeyDown(KEY_LEFT_SHIFT)) flySpeed *= 3.0f;

        Vector3 moveDelta{0.0f, 0.0f, 0.0f};
        if (IsKeyDown(KEY_W)) moveDelta = core::Vec3Add(moveDelta, core::Vec3Scale(forward, flySpeed));
        if (IsKeyDown(KEY_S)) moveDelta = core::Vec3Sub(moveDelta, core::Vec3Scale(forward, flySpeed));
        if (IsKeyDown(KEY_D)) moveDelta = core::Vec3Add(moveDelta, core::Vec3Scale(right, flySpeed));
        if (IsKeyDown(KEY_A)) moveDelta = core::Vec3Sub(moveDelta, core::Vec3Scale(right, flySpeed));
        if (IsKeyDown(KEY_E)) moveDelta.y += flySpeed;
        if (IsKeyDown(KEY_Q)) moveDelta.y -= flySpeed;

        m_cameraTarget = core::Vec3Add(m_cameraTarget, moveDelta);
    } else if (IsKeyDown(KEY_LEFT_ALT) && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && isHovered) {
        float rotSpeed = 0.35f;
        m_cameraYaw -= mouseDelta.x * rotSpeed;
        m_cameraPitch = std::clamp(m_cameraPitch + mouseDelta.y * rotSpeed, -85.0f, 85.0f);
    } else if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) && isHovered) {
        float panSpeed = 0.0025f * m_cameraDist;
        float yawRad = m_cameraYaw * DEG2RAD;
        Vector3 right{std::cos(yawRad), 0.0f, -std::sin(yawRad)};
        Vector3 up{0.0f, 1.0f, 0.0f};
        Vector3 panOffset = core::Vec3Add(
            core::Vec3Scale(right, -mouseDelta.x * panSpeed),
            core::Vec3Scale(up, mouseDelta.y * panSpeed)
        );
        m_cameraTarget = core::Vec3Add(m_cameraTarget, panOffset);
    }

    float yawRad = m_cameraYaw * DEG2RAD;
    float pitchRad = m_cameraPitch * DEG2RAD;

    m_camera.target = m_cameraTarget;
    m_camera.position = Vector3{
        m_cameraTarget.x + m_cameraDist * std::cos(pitchRad) * std::sin(yawRad),
        m_cameraTarget.y + m_cameraDist * std::sin(pitchRad),
        m_cameraTarget.z + m_cameraDist * std::cos(pitchRad) * std::cos(yawRad)
    };
}

void ViewportRenderer::CollectLights(scene::Scene& scene) {
    m_lights.clear();
    m_ambient = std::max(scene.Env().ambient, 0.22f); // в редакторе чуть светлее, чтобы карта читалась
    for (const auto& up : scene.GetGameObjects()) {
        const auto& o = *up;
        if (!o.active || !o.light.enabled) continue;
        float k = o.light.intensity;
        Vector3 c{o.light.color.r / 255.0f * k, o.light.color.g / 255.0f * k, o.light.color.b / 255.0f * k};
        m_lights.push_back(LightS{scene.WorldPosition(o), c, std::max(0.5f, o.light.radius)});
    }
}

Color ViewportRenderer::Shade(Vector3 p, Vector3 n) const {
    Vector3 c{m_ambient, m_ambient, m_ambient};
    for (const auto& l : m_lights) {
        Vector3 d{l.pos.x - p.x, l.pos.y - p.y, l.pos.z - p.z};
        float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (dist >= l.radius) continue;
        float att = 1.0f - dist / l.radius;
        att *= att;
        float ndl = dist > 0.001f ? (n.x * d.x + n.y * d.y + n.z * d.z) / dist : 1.0f;
        float w = att * (0.4f + 0.6f * std::max(0.0f, ndl));
        c.x += l.color.x * w; c.y += l.color.y * w; c.z += l.color.z * w;
    }
    auto cv = [](float v) { return (unsigned char)(std::clamp(v, 0.0f, 1.0f) * 255.0f); };
    return Color{cv(c.x), cv(c.y), cv(c.z), 255};
}

void ViewportRenderer::DrawBrushesLit(map::MapDocument& doc) {
    // ключ геометрии: при изменении браш-данных пересобираем дробление
    size_t key = 1469598103u;
    auto mix = [&](float v) { key = key * 1099511628u + (size_t)(int)(v * 64.0f); };
    auto scan = [&](std::vector<map::MapBrush>& brushes) {
        for (auto& b : brushes) {
            key = key * 31u + b.faces.size() + (b.visible ? 7u : 0u);
            mix(b.bounds.min.x); mix(b.bounds.min.y); mix(b.bounds.min.z); mix(b.bounds.max.x); mix(b.bounds.max.y); mix(b.bounds.max.z);
            for (auto& f : b.faces) key = key * 31u + std::hash<std::string>()(f.texture);
        }
    };
    scan(doc.GetWorldBrushes());
    for (auto& e : doc.GetEntities()) scan(e.brushes);
    key = key * 31u + (m_hideCeilings ? 1u : 0u);

    if (key != m_litKey) {
        m_litKey = key;
        m_litBatches.clear();
        std::function<void(LitBatch&, const map::MapFace&, Vector3, Vector3, Vector3, Vector3, int)> addTri =
            [&](LitBatch& batch, const map::MapFace& face, Vector3 a, Vector3 b, Vector3 c, Vector3 n, int depth) {
                auto len = [](Vector3 p, Vector3 q) { return std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z)); };
                auto mid = [](Vector3 p, Vector3 q) { return Vector3{(p.x + q.x) * 0.5f, (p.y + q.y) * 0.5f, (p.z + q.z) * 0.5f}; };
                float eab = len(a, b), ebc = len(b, c), eca = len(c, a);
                float longest = std::max({eab, ebc, eca});
                if (longest > 2.0f && depth < 12) {
                    if (longest == eab) { Vector3 m = mid(a, b); addTri(batch, face, a, m, c, n, depth + 1); addTri(batch, face, m, b, c, n, depth + 1); }
                    else if (longest == ebc) { Vector3 m = mid(b, c); addTri(batch, face, a, b, m, n, depth + 1); addTri(batch, face, a, m, c, n, depth + 1); }
                    else { Vector3 m = mid(c, a); addTri(batch, face, a, b, m, n, depth + 1); addTri(batch, face, m, b, c, n, depth + 1); }
                    return;
                }
                for (Vector3 p : {a, b, c}) batch.verts.push_back(LitVertex{p, face.CalculateUV(p), n});
            };
        auto addBrush = [&](map::MapBrush& br) {
            if (!br.visible) return;
            if (m_hideCeilings && br.bounds.min.y >= 2.4f) return;
            for (const auto& face : br.faces) {
                const auto& v = face.builtPoly.vertices;
                if (v.size() < 3) continue;
                LitBatch* batch = nullptr;
                for (auto& b : m_litBatches) if (b.texture == face.texture) batch = &b;
                if (!batch) { m_litBatches.push_back(LitBatch{face.texture, {}}); batch = &m_litBatches.back(); }
                for (size_t k = 1; k + 1 < v.size(); ++k) addTri(*batch, face, v[0], v[k], v[k + 1], face.builtPoly.normal, 0);
            }
        };
        for (auto& b : doc.GetWorldBrushes()) addBrush(b);
        for (auto& e : doc.GetEntities()) for (auto& b : e.brushes) addBrush(b);
    }

    auto& tm = TextureManager::Instance();
    rlDisableBackfaceCulling();
    for (auto& batch : m_litBatches) {
        Texture2D tex = tm.GetTexture(batch.texture);
        // Рисуем порциями и сбрасываем батч: при переполнении буфера rlgl теряет текстуру у оставшихся треугольников.
        constexpr size_t kChunk = 6000; // кратно 3
        for (size_t i = 0; i < batch.verts.size(); i += kChunk) {
            rlBegin(RL_TRIANGLES);
            rlSetTexture(tex.id); // ПОСЛЕ rlBegin: смена режима в rlBegin сбрасывает текстуру на дефолтную
            size_t end = std::min(batch.verts.size(), i + kChunk);
            for (size_t k = i; k < end; ++k) {
                const auto& v = batch.verts[k];
                Color c = Shade(v.p, v.n);
                rlColor4ub(c.r, c.g, c.b, 255);
                rlTexCoord2f(v.uv.x, v.uv.y);
                rlVertex3f(v.p.x, v.p.y, v.p.z);
            }
            rlEnd();
            rlSetTexture(0);
            rlDrawRenderBatchActive();
        }
    }
    rlEnableBackfaceCulling();
}

void ViewportRenderer::DrawColliderShape(const scene::GameObject& o, Vector3 wp, Color c) {
    const auto& col = o.collider;
    Vector3 p{wp.x + col.offset.x, wp.y + col.offset.y, wp.z + col.offset.z};
    if (col.shape == 1) {
        DrawSphereWires(p, col.radius, 10, 14, c);
    } else if (col.shape == 2) {
        float h = std::max(col.height, col.radius * 2.0f);
        float cyl = h - col.radius * 2.0f;
        DrawCylinderWires(Vector3{p.x, p.y - cyl * 0.5f, p.z}, col.radius, col.radius, cyl, 12, c);
        DrawSphereWires(Vector3{p.x, p.y + cyl * 0.5f, p.z}, col.radius, 6, 12, c);
        DrawSphereWires(Vector3{p.x, p.y - cyl * 0.5f, p.z}, col.radius, 6, 12, c);
    } else {
        DrawBoundingBox(PickBounds(o, wp), c);
    }
}

void ViewportRenderer::FrameAll(map::MapDocument& doc, scene::Scene& scene) {
    Vector3 mn{1e9f, 1e9f, 1e9f}, mx{-1e9f, -1e9f, -1e9f};
    bool any = false;
    auto add = [&](Vector3 p) {
        mn = Vector3{std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
        mx = Vector3{std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
        any = true;
    };
    for (auto& b : doc.GetWorldBrushes()) { add(b.bounds.min); add(b.bounds.max); }
    for (auto& e : doc.GetEntities()) for (auto& b : e.brushes) { add(b.bounds.min); add(b.bounds.max); }
    for (const auto& o : scene.GetGameObjects()) add(scene.WorldPosition(*o));
    if (!any) { ResetCamera(); return; }
    Vector3 c{(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f};
    float ext = std::max({mx.x - mn.x, mx.y - mn.y, mx.z - mn.z, 4.0f});
    m_cameraYaw = 35.0f;
    m_cameraPitch = 50.0f;
    FocusOn(c, ext * 0.95f);
}

void ViewportRenderer::SetTopView() {
    m_cameraPitch = 89.0f;
    FocusOn(m_cameraTarget, m_cameraDist);
}

void ViewportRenderer::DrawGridFloor() {
    if (!m_showGrid) return;

    int slices = 60;
    float spacing = 1.0f;
    float extent = (slices * spacing) * 0.5f;

    rlBegin(RL_LINES);
    for (int i = -slices / 2; i <= slices / 2; ++i) {
        float pos = i * spacing;
        Color col = (i % 5 == 0) ? Color{70, 75, 85, 255} : Color{45, 48, 55, 180};
        if (i == 0) col = Color{90, 100, 120, 255};

        rlColor4ub(col.r, col.g, col.b, col.a);
        rlVertex3f(pos, 0.0f, -extent);
        rlVertex3f(pos, 0.0f, extent);

        rlVertex3f(-extent, 0.0f, pos);
        rlVertex3f(extent, 0.0f, pos);
    }

    rlColor4ub(220, 50, 50, 255);
    rlVertex3f(0.0f, 0.01f, 0.0f);
    rlVertex3f(2.0f, 0.01f, 0.0f);

    rlColor4ub(50, 100, 240, 255);
    rlVertex3f(0.0f, 0.01f, 0.0f);
    rlVertex3f(0.0f, 0.01f, 2.0f);
    rlEnd();
}

void ViewportRenderer::DrawBrushes(map::MapDocument& doc) {
    if (m_shading == ViewportShading::Lit) DrawBrushesLit(doc);
    auto& texMgr = TextureManager::Instance();
    rlDisableBackfaceCulling();

    auto renderBrushList = [&](std::vector<map::MapBrush>& brushes, bool isSelectedContainer) {
        for (auto& brush : brushes) {
            if (!brush.visible) continue;
            if (m_hideCeilings && !brush.selected && brush.bounds.min.y >= 2.4f) continue;

            for (const auto& face : brush.faces) {
                if (face.builtPoly.vertices.size() < 3) continue;

                Texture2D tex = texMgr.GetTexture(face.texture);
                const auto& verts = face.builtPoly.vertices;

                if (m_shading == ViewportShading::Textured) {
                    rlBegin(RL_TRIANGLES);
                    rlSetTexture(tex.id); // ПОСЛЕ rlBegin: смена режима в rlBegin сбрасывает текстуру на дефолтную
                    rlColor4ub(255, 255, 255, 255);

                    for (size_t k = 1; k + 1 < verts.size(); ++k) {
                        Vector3 v0 = verts[0];
                        Vector3 v1 = verts[k];
                        Vector3 v2 = verts[k + 1];

                        Vector2 uv0 = face.CalculateUV(v0);
                        Vector2 uv1 = face.CalculateUV(v1);
                        Vector2 uv2 = face.CalculateUV(v2);

                        rlTexCoord2f(uv0.x, uv0.y); rlVertex3f(v0.x, v0.y, v0.z);
                        rlTexCoord2f(uv1.x, uv1.y); rlVertex3f(v1.x, v1.y, v1.z);
                        rlTexCoord2f(uv2.x, uv2.y); rlVertex3f(v2.x, v2.y, v2.z);
                    }
                    rlEnd();
                    rlSetTexture(0);
                } else if (m_shading == ViewportShading::FlatLit) {
                    rlBegin(RL_TRIANGLES);
                    float light = std::max(0.2f, std::fabs(core::Vec3Dot(face.builtPoly.normal, core::Vec3Normalize({0.5f, 1.0f, 0.3f}))));
                    unsigned char c = static_cast<unsigned char>(light * 220);
                    rlColor4ub(c, c, c, 255);

                    for (size_t k = 1; k + 1 < verts.size(); ++k) {
                        rlVertex3f(verts[0].x, verts[0].y, verts[0].z);
                        rlVertex3f(verts[k].x, verts[k].y, verts[k].z);
                        rlVertex3f(verts[k + 1].x, verts[k + 1].y, verts[k + 1].z);
                    }
                    rlEnd();
                }

                rlBegin(RL_LINES);
                Color wireCol = (brush.selected || isSelectedContainer) ? kSelColor : Color{25, 25, 30, 160};
                if (m_shading == ViewportShading::Wireframe) wireCol = (brush.selected || isSelectedContainer) ? kSelColor : Color{150, 160, 175, 255};
                rlColor4ub(wireCol.r, wireCol.g, wireCol.b, wireCol.a);
                for (size_t k = 0; k < verts.size(); ++k) {
                    Vector3 p1 = verts[k];
                    Vector3 p2 = verts[(k + 1) % verts.size()];
                    rlVertex3f(p1.x, p1.y, p1.z);
                    rlVertex3f(p2.x, p2.y, p2.z);
                }
                rlEnd();
            }

            if (brush.selected || isSelectedContainer) {
                DrawBoundingBox(brush.bounds, kSelColor);
            }
        }
    };

    renderBrushList(doc.GetWorldBrushes(), false);

    for (size_t i = 0; i < doc.GetEntities().size(); ++i) {
        auto& ent = doc.GetEntities()[i];
        renderBrushList(ent.brushes, ent.selected);
    }
    rlEnableBackfaceCulling();
}

void ViewportRenderer::DrawEntities(map::MapDocument& doc) {
    for (const auto& ent : doc.GetEntities()) {
        Vector3 pos = ent.origin;
        bool sel = ent.selected;

        if (ent.classname == "info_player_start") {
            Color col = sel ? kSelColor : Color{60, 220, 100, 255};
            DrawCylinderWires(pos, 0.35f, 0.35f, 1.75f, 12, col);
            float yawRad = ent.yaw * DEG2RAD;
            Vector3 forward{std::sin(yawRad) * 0.8f, 0.0f, std::cos(yawRad) * 0.8f};
            DrawLine3D(pos, core::Vec3Add(pos, forward), WHITE);
        } else if (ent.classname == "light") {
            Color col = sel ? kSelColor : Color{255, 240, 100, 255};
            DrawSphereWires(pos, 0.25f, 8, 8, col);
        } else {
            Color col = sel ? kSelColor : Color{200, 200, 200, 255};
            DrawCubeWires(pos, 0.4f, 0.4f, 0.4f, col);
        }
    }
}

void ViewportRenderer::DrawSceneObjects(scene::Scene& scene) {
    auto& texMgr = TextureManager::Instance();
    int selId = scene.GetSelectedId();

    for (const auto& up : scene.GetGameObjects()) {
        const scene::GameObject& o = *up;
        Vector3 wp = scene.WorldPosition(o);
        bool sel = (int)o.id == selId;
        unsigned char alpha = o.active ? 255 : 90;

        // Меш-превью
        std::string shape = o.meshRenderer.meshName;
        std::string tex = o.meshRenderer.textureName;
        Vector3 size = o.transform.scale;
        Color tint = o.meshRenderer.tint;
        if (shape == "Auto" || shape.empty()) {
            if (o.door.enabled) { shape = "Cube"; if (tex.empty()) tex = "motel_door"; }
            else if (o.hidingSpot.enabled) { shape = "Cube"; if (tex.empty()) tex = "motel_wall_wood"; }
            else if (o.searchSlot.enabled) { shape = "Cube"; if (tex.empty()) tex = "motel_cardboard"; }
            else if (o.itemPickup.enabled) { shape = "Cube"; size = {0.22f, 0.22f, 0.22f}; tint = Color{255, 230, 120, 255}; }
            else shape = "None";
        }
        if (o.meshRenderer.enabled && shape != "None") {
            tint.a = alpha;
            if (m_shading == ViewportShading::Lit && !o.meshRenderer.emissive) {
                Color l = Shade(wp, Vector3{0, 1, 0});
                tint.r = (unsigned char)(tint.r * l.r / 255); tint.g = (unsigned char)(tint.g * l.g / 255); tint.b = (unsigned char)(tint.b * l.b / 255);
            }
            rlPushMatrix();
            rlTranslatef(wp.x, wp.y, wp.z);
            rlRotatef(o.transform.rotation.y, 0, 1, 0);
            if (shape == "Cube") {
                if (tex.empty()) DrawCube(Vector3{0, 0, 0}, size.x, size.y, size.z, tint);
                else DrawTexturedBox(texMgr.GetTexture(tex), Vector3{0, 0, 0}, size.x, size.y, size.z, tint);
            } else if (shape == "Sphere") {
                DrawSphereEx(Vector3{0, 0, 0}, size.x * 0.5f, 12, 12, tint);
            } else if (shape == "Cylinder") {
                DrawCylinder(Vector3{0, -size.y * 0.5f, 0}, size.x * 0.5f, size.x * 0.5f, size.y, 12, tint);
            }
            rlPopMatrix();
        }

        Color mark = Color{200, 200, 200, alpha};
        if (o.player.enabled) mark = Color{60, 220, 100, alpha};
        else if (o.aiSpawn.enabled) mark = Color{240, 70, 60, alpha};
        else if (o.light.enabled) mark = Color{255, 240, 100, alpha};
        else if (o.collider.enabled && o.collider.isTrigger) mark = Color{255, 150, 40, alpha};
        else if (o.searchSlot.enabled) mark = Color{70, 150, 250, alpha};
        else if (o.hidingSpot.enabled) mark = Color{180, 90, 230, alpha};
        else if (o.door.enabled) mark = Color{230, 160, 90, alpha};
        else if (o.itemPickup.enabled) mark = Color{255, 220, 90, alpha};
        else if (o.script.enabled) mark = Color{120, 220, 255, alpha};
        if (sel) mark = kSelColor;

        // Игрок / монстр
        if (o.player.enabled) {
            DrawCylinderWires(wp, 0.3f, 0.3f, 1.8f, 12, mark);
            float yr = o.transform.rotation.y * DEG2RAD;
            Vector3 eye{wp.x, wp.y + o.player.eyeHeight, wp.z};
            DrawLine3D(eye, Vector3{eye.x + std::sin(yr) * 1.0f, eye.y, eye.z + std::cos(yr) * 1.0f}, WHITE);
        }
        if (o.aiSpawn.enabled) {
            DrawCylinder(wp, 0.3f, 0.35f, 1.4f, 10, Color{60, 20, 20, alpha});
            DrawSphere(Vector3{wp.x, wp.y + 1.6f, wp.z}, 0.2f, Color{90, 60, 55, alpha});
            DrawCylinderWires(wp, 0.35f, 0.35f, 1.8f, 12, mark);
            const auto& ai = o.aiSpawn;
            Vector3 prev = wp;
            for (size_t i = 0; i < ai.patrolWaypoints.size(); ++i) {
                Vector3 w = ai.patrolWaypoints[i];
                Vector3 wv{w.x, w.y + 0.15f, w.z};
                DrawSphere(wv, sel ? 0.2f : 0.12f, Color{255, 120, 60, alpha});
                DrawLine3D(Vector3{prev.x, prev.y + 0.15f, prev.z}, wv, Color{255, 120, 60, 200});
                prev = w;
            }
            if (!ai.patrolWaypoints.empty()) {
                Vector3 f = ai.patrolWaypoints.front();
                DrawLine3D(Vector3{prev.x, prev.y + 0.15f, prev.z}, Vector3{f.x, f.y + 0.15f, f.z}, Color{255, 120, 60, 120});
            }
            if (sel) {
                float yaw = o.transform.rotation.y;
                for (float a : {-ai.visionAngle * 0.5f, ai.visionAngle * 0.5f}) {
                    float r = (yaw + a) * DEG2RAD;
                    DrawLine3D(Vector3{wp.x, wp.y + 1.5f, wp.z}, Vector3{wp.x + std::sin(r) * ai.visionDistance, wp.y + 1.5f, wp.z + std::cos(r) * ai.visionDistance}, Color{255, 80, 80, 200});
                }
                DrawCircle3D(Vector3{wp.x, wp.y + 0.05f, wp.z}, ai.hearingRadius, Vector3{1, 0, 0}, 90.0f, Color{255, 200, 60, 150});
            }
        }
        if (o.light.enabled) {
            DrawSphereWires(wp, 0.18f, 6, 6, mark);
            if (sel) {
                DrawCircle3D(wp, o.light.radius, Vector3{1, 0, 0}, 90.0f, Color{255, 240, 100, 130});
                DrawCircle3D(wp, o.light.radius, Vector3{0, 0, 1}, 0.0f, Color{255, 240, 100, 90});
            }
        }
        if (m_showColliders && o.collider.enabled) {
            DrawColliderShape(o, wp, o.collider.isTrigger ? Color{255, 150, 40, alpha} : Color{70, 220, 90, (unsigned char)(alpha / 2 + 40)});
        }
        if (o.hidingSpot.enabled && sel) {
            float yr = o.hidingSpot.exitYaw * DEG2RAD;
            DrawLine3D(wp, Vector3{wp.x + std::sin(yr) * 1.2f, wp.y, wp.z + std::cos(yr) * 1.2f}, Color{180, 90, 230, 255});
        }
        if (sel) {
            BoundingBox b = PickBounds(o, wp);
            DrawBoundingBox(b, kSelColor);
        }
        if (!o.player.enabled && !o.aiSpawn.enabled && !o.light.enabled && (!o.meshRenderer.enabled || shape == "None") && !o.collider.enabled) {
            DrawCubeWires(wp, 0.3f, 0.3f, 0.3f, mark);
        }
    }
}

void ViewportRenderer::DrawGizmo(Vector3 position) {
    if (m_transformMode == TransformMode::Select) return;
    float s = std::max(0.4f, core::Vec3Length(core::Vec3Sub(m_camera.position, position)) * 0.16f);
    float len = s;
    float head = s * 0.22f;
    rlDisableDepthTest();
    if (m_transformMode == TransformMode::Translate) {
        DrawLine3D(position, Vector3{position.x + len, position.y, position.z}, Color{235, 50, 50, 255});
        DrawCylinder(Vector3{position.x + len, position.y, position.z}, 0.0f, head * 0.35f, head, 8, Color{235, 50, 50, 255});
        DrawLine3D(position, Vector3{position.x, position.y + len, position.z}, Color{50, 225, 50, 255});
        DrawCylinder(Vector3{position.x, position.y + len, position.z}, 0.0f, head * 0.35f, head, 8, Color{50, 225, 50, 255});
        DrawLine3D(position, Vector3{position.x, position.y, position.z + len}, Color{50, 100, 245, 255});
        DrawCylinder(Vector3{position.x, position.y, position.z + len}, 0.0f, head * 0.35f, head, 8, Color{50, 100, 245, 255});
    } else if (m_transformMode == TransformMode::Rotate) {
        DrawCircle3D(position, len, Vector3{1, 0, 0}, 90.0f, Color{50, 225, 50, 255});
    } else {
        DrawCubeWires(position, len, len, len, Color{240, 180, 60, 255});
    }
    DrawCube(position, head * 0.5f, head * 0.5f, head * 0.5f, Color{240, 240, 240, 255});
    rlEnableDepthTest();
}

void ViewportRenderer::DrawLabels(scene::Scene& scene) {
    Font f = m_hasFont ? m_font : GetFontDefault();
    for (const auto& up : scene.GetGameObjects()) {
        const scene::GameObject& o = *up;
        bool sel = (int)o.id == scene.GetSelectedId();
        Vector3 wp = scene.WorldPosition(o);
        float dist = core::Vec3Length(core::Vec3Sub(m_camera.position, wp));
        if (!sel && dist > 14.0f) continue;
        Vector3 top = ObjCenter(o, wp);
        top.y += ObjSize(o).y * 0.5f + 0.25f;
        Vector2 sp = GetWorldToScreenEx(top, m_camera, m_width, m_height);
        if (sp.x < 0 || sp.y < 0 || sp.x > m_width || sp.y > m_height) continue;
        Vector3 toObj = core::Vec3Sub(wp, m_camera.position);
        Vector3 camFwd = core::Vec3Normalize(core::Vec3Sub(m_camera.target, m_camera.position));
        if (core::Vec3Dot(toObj, camFwd) <= 0.0f) continue;
        Vector2 ts = MeasureTextEx(f, o.name.c_str(), 15.0f, 1.0f);
        DrawTextEx(f, o.name.c_str(), Vector2{sp.x - ts.x * 0.5f + 1, sp.y + 1}, 15.0f, 1.0f, Color{0, 0, 0, 200});
        DrawTextEx(f, o.name.c_str(), Vector2{sp.x - ts.x * 0.5f, sp.y}, 15.0f, 1.0f, sel ? kSelColor : Color{220, 225, 235, 190});
    }
}

bool ViewportRenderer::GetSelectionPosition(map::MapDocument& doc, scene::Scene& scene, Vector3* out) const {
    if (auto* o = scene.GetSelected()) {
        if (out) *out = scene.WorldPosition(*o);
        return true;
    }
    int selBrush = doc.GetSelectedBrushIndex();
    int selEnt = doc.GetSelectedEntityIndex();
    if (selBrush >= 0 && selBrush < (int)doc.GetWorldBrushes().size()) {
        if (out) *out = doc.GetWorldBrushes()[selBrush].center;
        return true;
    }
    if (selEnt >= 0 && selEnt < (int)doc.GetEntities().size()) {
        if (out) *out = doc.GetEntities()[selEnt].origin;
        return true;
    }
    return false;
}

void ViewportRenderer::Render(map::MapDocument& doc, scene::Scene& scene) {
    BeginTextureMode(m_target);
    ClearBackground(Color{30, 32, 38, 255});

    BeginMode3D(m_camera);

    CollectLights(scene);
    DrawGridFloor();
    DrawBrushes(doc);
    DrawEntities(doc);
    DrawSceneObjects(scene);

    Vector3 gp;
    if (GetSelectionPosition(doc, scene, &gp)) DrawGizmo(gp);

    EndMode3D();

    DrawLabels(scene);

    Font f = m_hasFont ? m_font : GetFontDefault();
    std::string modeText = "Инструмент: ";
    switch (m_transformMode) {
    case TransformMode::Select: modeText += "Выбор [Q]"; break;
    case TransformMode::Translate: modeText += "Двигать [W]"; break;
    case TransformMode::Rotate: modeText += "Вращать [E] (тяните)"; break;
    case TransformMode::Scale: modeText += "Масштаб [R] (тяните)"; break;
    }
    DrawTextEx(f, modeText.c_str(), Vector2{12, 10}, 15, 1, Color{220, 220, 220, 255});
    std::string snapText = "Сетка: " + (m_gridSnap > 0.0f ? std::to_string(m_gridSnap).substr(0, 4) + "m" : std::string("выкл"));
    DrawTextEx(f, snapText.c_str(), Vector2{12, 28}, 14, 1, Color{180, 180, 180, 255});
    DrawTextEx(f, "ПКМ + WASD/QE: полёт   Alt+ЛКМ: вращение   СКМ: сдвиг   Колесо: масштаб   F: фокус   Home: вся карта", Vector2{12, (float)m_height - 22}, 13, 1, Color{150, 155, 165, 200});

    EndTextureMode();
}

void ViewportRenderer::HandleMousePicking(Vector2 mouse, map::MapDocument& doc, scene::Scene& scene, bool isMouseDown, bool isMousePressed) {
    if (mouse.x < 0 || mouse.y < 0 || mouse.x >= m_width || mouse.y >= m_height) {
        if (!isMouseDown) m_isDraggingGizmo = false;
        if (!m_isDraggingGizmo) return;
    }

    Ray ray = GetScreenToWorldRay(mouse, m_camera);
    scene::GameObject* selObj = scene.GetSelected();
    int selBrush = doc.GetSelectedBrushIndex();
    int selEnt = doc.GetSelectedEntityIndex();

    Vector3 gizmoPos{0, 0, 0};
    bool hasSel = GetSelectionPosition(doc, scene, &gizmoPos);

    auto moveSelection = [&](Vector3 newPos) {
        if (selObj) {
            scene.SetWorldPosition(*selObj, newPos);
            scene.SetModified(true);
        } else if (selBrush >= 0 && selBrush < (int)doc.GetWorldBrushes().size()) {
            auto& b = doc.GetWorldBrushes()[selBrush];
            Vector3 qDelta = core::WorldToQuake(core::Vec3Sub(newPos, b.center));
            for (auto& fc : b.faces) {
                for (int k = 0; k < 3; ++k) fc.planePoints[k] = core::Vec3Add(fc.planePoints[k], qDelta);
            }
            b.BuildPolygons();
            doc.SetModified(true);
        } else if (selEnt >= 0 && selEnt < (int)doc.GetEntities().size()) {
            doc.GetEntities()[selEnt].origin = newPos;
            doc.GetEntities()[selEnt].SyncPropertiesFromOrigin();
            doc.SetModified(true);
        }
    };

    // --- начало перетаскивания ---
    if (hasSel && isMousePressed && !m_isDraggingGizmo && m_transformMode == TransformMode::Translate) {
        float s = std::max(0.4f, core::Vec3Length(core::Vec3Sub(m_camera.position, gizmoPos)) * 0.16f);
        const Vector3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        float bestDist = std::max(0.12f, 0.14f * s);
        int bestAxis = -1;
        float bestT = 0.0f;
        for (int a = 0; a < 3; ++a) {
            float t = 0.0f, sr = 0.0f;
            float d = RayLineClosest(ray, gizmoPos, axes[a], &t, &sr);
            if (sr > 0.0f && t > -0.1f * s && t < s * 1.25f && d < bestDist) {
                bestDist = d;
                bestAxis = a;
                bestT = t;
            }
        }
        if (bestAxis >= 0) {
            m_activeGizmoAxis = bestAxis;
            m_isDraggingGizmo = true;
            m_dragStartT = bestT;
            m_dragObjStartPos = gizmoPos;
            return;
        }
    }
    if (hasSel && isMousePressed && !m_isDraggingGizmo && m_transformMode == TransformMode::Translate) {
        // тащим сам объект мышью по плоскости пола
        BoundingBox sb;
        bool ok = false;
        if (selObj) { sb = PickBounds(*selObj, scene.WorldPosition(*selObj)); ok = true; }
        else if (selBrush >= 0 && selBrush < (int)doc.GetWorldBrushes().size()) { sb = doc.GetWorldBrushes()[selBrush].bounds; ok = true; }
        else if (selEnt >= 0 && selEnt < (int)doc.GetEntities().size()) { Vector3 o = doc.GetEntities()[selEnt].origin; sb = BoundingBox{core::Vec3Sub(o, {0.35f, 0.35f, 0.35f}), core::Vec3Add(o, {0.35f, 0.35f, 0.35f})}; ok = true; }
        if (ok && core::RayIntersectsBox(ray, sb) && std::fabs(ray.direction.y) > 1e-4f) {
            float t = (gizmoPos.y - ray.position.y) / ray.direction.y;
            if (t > 0) {
                Vector3 hit = core::Vec3Add(ray.position, core::Vec3Scale(ray.direction, t));
                m_dragOffset = core::Vec3Sub(gizmoPos, hit);
                m_dragObjStartPos = gizmoPos;
                m_activeGizmoAxis = 20;
                m_isDraggingGizmo = true;
                return;
            }
        }
    }
    if (hasSel && isMousePressed && !m_isDraggingGizmo && selObj && (m_transformMode == TransformMode::Rotate || m_transformMode == TransformMode::Scale)) {
        BoundingBox b = PickBounds(*selObj, scene.WorldPosition(*selObj));
        if (core::RayIntersectsBox(ray, b)) {
            m_activeGizmoAxis = (m_transformMode == TransformMode::Rotate) ? 10 : 11;
            m_isDraggingGizmo = true;
            m_dragStartMouse = mouse;
            m_dragStartValue = selObj->transform.rotation.y;
            m_dragStartScale = selObj->transform.scale;
            return;
        }
    }

    // --- перетаскивание ---
    if (m_isDraggingGizmo) {
        if (!isMouseDown) {
            m_isDraggingGizmo = false;
            m_activeGizmoAxis = -1;
            return;
        }
        if (m_activeGizmoAxis >= 0 && m_activeGizmoAxis < 3) {
            const Vector3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            Vector3 axis = axes[m_activeGizmoAxis];
            float t = 0.0f, sr = 0.0f;
            RayLineClosest(ray, m_dragObjStartPos, axis, &t, &sr);
            float delta = t - m_dragStartT;
            Vector3 np = core::Vec3Add(m_dragObjStartPos, core::Vec3Scale(axis, delta));
            if (m_gridSnap > 0.001f) {
                if (m_activeGizmoAxis == 0) np.x = core::SnapValue(np.x, m_gridSnap);
                if (m_activeGizmoAxis == 1) np.y = core::SnapValue(np.y, m_gridSnap);
                if (m_activeGizmoAxis == 2) np.z = core::SnapValue(np.z, m_gridSnap);
            }
            moveSelection(np);
        } else if (m_activeGizmoAxis == 20) {
            if (std::fabs(ray.direction.y) > 1e-4f) {
                float t = (m_dragObjStartPos.y - ray.position.y) / ray.direction.y;
                if (t > 0) {
                    Vector3 hit = core::Vec3Add(ray.position, core::Vec3Scale(ray.direction, t));
                    Vector3 np = core::Vec3Add(hit, m_dragOffset);
                    np.y = m_dragObjStartPos.y;
                    if (m_gridSnap > 0.001f) { np.x = core::SnapValue(np.x, m_gridSnap); np.z = core::SnapValue(np.z, m_gridSnap); }
                    moveSelection(np);
                }
            }
        } else if (m_activeGizmoAxis == 10 && selObj) {
            float yaw = m_dragStartValue + (mouse.x - m_dragStartMouse.x) * 0.6f;
            if (m_gridSnap > 0.001f) yaw = std::round(yaw / 15.0f) * 15.0f;
            selObj->transform.rotation.y = yaw;
            scene.SetModified(true);
        } else if (m_activeGizmoAxis == 11 && selObj) {
            float f = std::clamp(1.0f - (mouse.y - m_dragStartMouse.y) * 0.006f, 0.05f, 20.0f);
            selObj->transform.scale = Vector3{m_dragStartScale.x * f, m_dragStartScale.y * f, m_dragStartScale.z * f};
            if (m_gridSnap > 0.001f) {
                selObj->transform.scale.x = core::SnapValue(selObj->transform.scale.x, 0.05f);
                selObj->transform.scale.y = core::SnapValue(selObj->transform.scale.y, 0.05f);
                selObj->transform.scale.z = core::SnapValue(selObj->transform.scale.z, 0.05f);
            }
            scene.SetModified(true);
        }
        return;
    }

    // --- выбор ---
    if (isMousePressed) {
        float closest = 1e9f;
        int hitBrush = -1, hitEntity = -1;
        uint32_t hitObj = 0;

        for (size_t i = 0; i < doc.GetWorldBrushes().size(); ++i) {
            float dist = 0.0f;
            if (core::RayIntersectsBox(ray, doc.GetWorldBrushes()[i].bounds, &dist) && dist < closest) {
                closest = dist; hitBrush = (int)i; hitEntity = -1; hitObj = 0;
            }
        }
        for (size_t i = 0; i < doc.GetEntities().size(); ++i) {
            BoundingBox box{core::Vec3Sub(doc.GetEntities()[i].origin, {0.35f, 0.35f, 0.35f}), core::Vec3Add(doc.GetEntities()[i].origin, {0.35f, 0.35f, 0.35f})};
            float dist = 0.0f;
            if (core::RayIntersectsBox(ray, box, &dist) && dist < closest) {
                closest = dist; hitEntity = (int)i; hitBrush = -1; hitObj = 0;
            }
        }
        for (const auto& up : scene.GetGameObjects()) {
            const auto& o = *up;
            BoundingBox box = PickBounds(o, scene.WorldPosition(o));
            float dist = 0.0f;
            // объекты сцены приоритетнее брашей при равной дальности
            if (core::RayIntersectsBox(ray, box, &dist) && dist < closest + 0.3f) {
                closest = dist; hitObj = o.id; hitBrush = -1; hitEntity = -1;
            }
        }

        if (hitObj) {
            doc.DeselectAll();
            scene.Select(hitObj);
        } else if (hitBrush >= 0) {
            scene.Deselect();
            doc.SelectBrush(hitBrush);
        } else if (hitEntity >= 0) {
            scene.Deselect();
            doc.SelectEntity(hitEntity);
        } else {
            scene.Deselect();
            doc.DeselectAll();
        }
    }
}

}
