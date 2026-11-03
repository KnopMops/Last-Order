#include "game/chapter2/MotelLevel.h"
#include "engine/3d/LightBaker.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <type_traits>

namespace game::chapter2 {

using engine::render3d::BakeLight;
using engine::render3d::BakeSample;
using engine::render3d::LightBaker;
using engine::world3d::BrushPolygon;
using engine::world3d::MapBrush;
using engine::world3d::MapEntity;
using engine::world3d::QuakeMap;

namespace {

bool IsInvisibleTexture(const std::string& name) {
    return name == "clip" || name == "trigger" || name == "nodraw" || name == "skip" || name == "origin";
}

bool IsTriggerBrush(const MapBrush& brush) {
    for (const auto& f : brush.faces) {
        if (f.texture != "trigger") return false;
    }
    return true;
}

bool IsEmissiveTexture(const std::string& name) {
    return name.find("sign") != std::string::npos || name.find("neon") != std::string::npos;
}

Vector3 ParseColor(const std::string& text, Vector3 fallback) {
    std::istringstream ss(text);
    Vector3 c = fallback;
    if (!(ss >> c.x >> c.y >> c.z)) return fallback;
    if (c.x > 1.0f || c.y > 1.0f || c.z > 1.0f) c = Vector3Scale(c, 1.0f / 255.0f);
    return c;
}

float QuakeYawToWorld(float angleDeg) {
    return (90.0f - angleDeg) * DEG2RAD;
}

struct MeshBuilder {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned char> colors;
    std::vector<float> tangents;

    void Add(Vector3 p, Vector3 n, Vector2 uv, const BakeSample& s) {
        positions.insert(positions.end(), {p.x, p.y, p.z});
        normals.insert(normals.end(), {n.x, n.y, n.z});
        texcoords.insert(texcoords.end(), {uv.x, uv.y});
        auto pack = [](float v) { return static_cast<unsigned char>(std::clamp(v * 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f); };
        colors.insert(colors.end(), {pack(s.base.x), pack(s.base.y), pack(s.base.z),
                                     static_cast<unsigned char>(std::clamp(s.ao, 0.0f, 1.0f) * 255.0f)});
        tangents.insert(tangents.end(), {s.power.x, s.power.y, s.power.z, 1.0f});
    }

    int VertexCount() const { return static_cast<int>(positions.size() / 3); }

    Mesh Build() const {
        Mesh mesh{};
        mesh.vertexCount = VertexCount();
        mesh.triangleCount = mesh.vertexCount / 3;
        auto copy = [](const auto& src) {
            using T = typename std::decay_t<decltype(src)>::value_type;
            T* dst = static_cast<T*>(MemAlloc(static_cast<unsigned int>(src.size() * sizeof(T))));
            std::copy(src.begin(), src.end(), dst);
            return dst;
        };
        mesh.vertices = copy(positions);
        mesh.normals = copy(normals);
        mesh.texcoords = copy(texcoords);
        mesh.colors = copy(colors);
        mesh.tangents = copy(tangents);
        UploadMesh(&mesh, false);
        return mesh;
    }
};

float Axis(Vector3 v, int axis) {
    return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
}

void SplitPolygon(const std::vector<Vector3>& poly, int axis, float value,
                  std::vector<Vector3>& below, std::vector<Vector3>& above) {
    below.clear();
    above.clear();
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        Vector3 a = poly[i];
        Vector3 b = poly[(i + 1) % n];
        float da = Axis(a, axis) - value;
        float db = Axis(b, axis) - value;
        if (da <= 0.0f) below.push_back(a);
        if (da >= 0.0f) above.push_back(a);
        if ((da < 0.0f && db > 0.0f) || (da > 0.0f && db < 0.0f)) {
            float t = da / (da - db);
            Vector3 p = Vector3Lerp(a, b, t);
            below.push_back(p);
            above.push_back(p);
        }
    }
    if (below.size() < 3) below.clear();
    if (above.size() < 3) above.clear();
}

std::vector<std::vector<Vector3>> Subdivide(const std::vector<Vector3>& poly, Vector3 normal) {
    float area = 0.0f;
    for (size_t i = 1; i + 1 < poly.size(); ++i) {
        area += 0.5f * Vector3Length(Vector3CrossProduct(Vector3Subtract(poly[i], poly[0]), Vector3Subtract(poly[i + 1], poly[0])));
    }
    float cell = area > 160.0f ? 2.0f : 1.0f;
    int dropAxis = 0;
    float best = std::abs(normal.x);
    if (std::abs(normal.y) > best) { best = std::abs(normal.y); dropAxis = 1; }
    if (std::abs(normal.z) > best) dropAxis = 2;

    std::vector<std::vector<Vector3>> pieces{poly};
    for (int axis = 0; axis < 3; ++axis) {
        if (axis == dropAxis) continue;
        std::vector<std::vector<Vector3>> next;
        for (const auto& piece : pieces) {
            float mn = 1e9f, mx = -1e9f;
            for (const Vector3& p : piece) {
                mn = std::min(mn, Axis(p, axis));
                mx = std::max(mx, Axis(p, axis));
            }
            std::vector<Vector3> current = piece;
            int k0 = static_cast<int>(std::floor(mn / cell)) + 1;
            int k1 = static_cast<int>(std::ceil(mx / cell)) - 1;
            std::vector<Vector3> below, above;
            for (int k = k0; k <= k1 && !current.empty(); ++k) {
                float v = static_cast<float>(k) * cell;
                if (v <= mn + 0.01f || v >= mx - 0.01f) continue;
                SplitPolygon(current, axis, v, below, above);
                if (!below.empty()) next.push_back(below);
                current = above;
            }
            if (!current.empty()) next.push_back(current);
        }
        pieces = std::move(next);
    }
    return pieces;
}

void AppendPolygon(std::map<std::string, MeshBuilder>& builders, const BrushPolygon& poly, Vector2 texSize,
                   LightBaker& baker, Vector3 offset) {
    MeshBuilder& builder = builders[poly.texture];
    for (const auto& piece : Subdivide(poly.points, poly.normal)) {
        for (size_t i = 1; i + 1 < piece.size(); ++i) {
            const Vector3 tri[3] = {piece[0], piece[i], piece[i + 1]};
            for (const Vector3& p : tri) {
                Vector2 texel = poly.TexelUV(p);
                Vector2 uv{texel.x / texSize.x, texel.y / texSize.y};
                BakeSample s = baker.Sample(p, poly.normal);
                builder.Add(Vector3Subtract(p, offset), poly.normal, uv, s);
            }
        }
    }
}

BoundingBox MergeBounds(const BoundingBox& a, const BoundingBox& b) {
    return BoundingBox{
        {std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z)},
        {std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z)},
    };
}

BoundingBox EntityBounds(const MapEntity& e) {
    BoundingBox box{{1e9f, 1e9f, 1e9f}, {-1e9f, -1e9f, -1e9f}};
    for (const auto& brush : e.brushes) box = MergeBounds(box, QuakeMap::BuildBounds(brush));
    return box;
}

Vector3 BoundsCenter(const BoundingBox& b) {
    return Vector3Scale(Vector3Add(b.min, b.max), 0.5f);
}

}

Texture2D MotelLevel::ResolveTexture(engine::core::Engine& eng, const std::string& name) {
    auto it = m_textureCache.find(name);
    if (it != m_textureCache.end()) return it->second;
    std::string rel = "sprites/3d/" + name + ".png";
    Texture2D tex{};
    if (FileExists((eng.Assets().GetBaseDir() + "/" + rel).c_str())) {
        tex = eng.Assets().GetTexture(rel);
    }
    if (tex.id == 0) tex = eng.Assets().GetTexture("sprites/3d/motel_concrete.png");
    if (tex.id != 0) {
        SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    }
    m_textureCache[name] = tex;
    return tex;
}

bool MotelLevel::Load(engine::core::Engine& eng, engine::physics3d::Physics3D& physics,
                      const engine::render3d::Lighting3D& lighting, const std::string& mapPath) {
    Unload();
    QuakeMap map;
    if (!map.Load(mapPath)) {
        TraceLog(LOG_ERROR, "CH2: failed to load map %s: %s", mapPath.c_str(), map.GetError().c_str());
        return false;
    }

    Shader lit = lighting.LitShader();
    Shader mirrorShader = lighting.MirrorShader();

    if (const MapEntity* world = map.FindFirst("worldspawn")) {
        m_scriptPath = world->Get("script", "data/chapter2/act1.json");
        m_env.fogColor = ParseColor(world->Get("fog_color"), m_env.fogColor);
        m_env.ambientSky = ParseColor(world->Get("ambient_sky"), m_env.ambientSky);
        m_env.ambientGround = ParseColor(world->Get("ambient_ground"), m_env.ambientGround);
        m_env.skyLight = ParseColor(world->Get("sky_light"), m_env.skyLight);
        m_env.fogStart = world->GetFloat("fog_start", m_env.fogStart);
        m_env.fogEnd = world->GetFloat("fog_end", m_env.fogEnd);
    }
    if (const MapEntity* start = map.FindFirst("info_player_start")) {
        m_playerStart = start->GetOrigin();
        m_playerYaw = QuakeYawToWorld(start->GetFloat("angle", 0.0f));
    }

    struct PendingStatic {
        const MapBrush* brush;
        BodyHandle body;
    };
    std::vector<PendingStatic> staticBrushes;
    struct PendingMirror {
        const MapEntity* entity;
    };
    std::vector<PendingMirror> mirrors;

    auto addStaticBrush = [&](const MapBrush& brush, bool solid) {
        BodyHandle body = INVALID_BODY;
        if (solid && !IsTriggerBrush(brush)) {
            body = physics.AddStaticHull(QuakeMap::BuildVertices(brush));
            if (body != INVALID_BODY) {
                float bestUp = -2.0f;
                std::string surface;
                for (const auto& poly : QuakeMap::BuildPolygons(brush)) {
                    if (poly.normal.y > bestUp) {
                        bestUp = poly.normal.y;
                        surface = poly.texture;
                    }
                }
                m_surfaces[body] = surface;
            }
        }
        staticBrushes.push_back({&brush, body});
        return body;
    };

    for (const MapEntity& e : map.Entities()) {
        const std::string& cls = e.ClassName();
        if (cls == "worldspawn") {
            for (const auto& brush : e.brushes) addStaticBrush(brush, true);
        } else if (cls == "func_interact") {
            InteractObject obj;
            obj.event = e.Get("event");
            obj.label = e.Get("label", "Осмотреть");
            obj.bounds = EntityBounds(e);
            obj.center = BoundsCenter(obj.bounds);
            bool solid = e.GetInt("solid", 1) != 0;
            for (const auto& brush : e.brushes) {
                BodyHandle body = addStaticBrush(brush, solid);
                if (body != INVALID_BODY) obj.bodies.push_back(body);
            }
            m_interacts.push_back(obj);
        } else if (cls == "func_mirror") {
            for (const auto& brush : e.brushes) addStaticBrush(brush, true);
            mirrors.push_back({&e});
        } else if (cls == "trigger_event") {
            TriggerObject t;
            t.event = e.Get("event");
            t.once = e.GetInt("once", 1) != 0;
            t.bounds = EntityBounds(e);
            m_triggers.push_back(t);
        } else if (cls == "light") {
            LightObject l;
            l.position = e.GetOrigin();
            l.color = ParseColor(e.Get("_color", "1 0.85 0.6"), Vector3{1.0f, 0.85f, 0.6f});
            l.radius = e.GetFloat("radius", 5.0f);
            l.intensity = e.GetFloat("intensity", 1.0f);
            l.group = e.Get("group");
            l.style = e.Get("style", "steady");
            l.dynamic = e.GetInt("dynamic", 0) != 0;
            l.phase = static_cast<float>(m_lights.size()) * 1.37f;
            m_lights.push_back(l);
        } else if (cls == "item_pickup") {
            ItemObject item;
            item.item = e.Get("item");
            item.position = e.GetOrigin();
            item.yaw = QuakeYawToWorld(e.GetFloat("angle", 0.0f));
            item.requireFlag = e.Get("require_flag");
            m_items.push_back(item);
        } else if (cls == "hide_spot") {
            HideSpot h;
            h.eye = e.GetOrigin();
            h.yaw = QuakeYawToWorld(e.GetFloat("angle", 0.0f));
            h.label = e.Get("label", "Укрытие");
            m_hideSpots.push_back(h);
        } else if (cls == "sound_emitter") {
            SoundEmitter s;
            s.position = e.GetOrigin();
            s.sound = e.Get("sound");
            s.interval = std::max(0.3f, e.GetFloat("interval", 10.0f));
            s.volume = e.GetFloat("volume", 0.6f);
            s.radius = e.GetFloat("radius", 12.0f);
            s.flag = e.Get("flag");
            s.hideFlag = e.Get("hide_flag");
            s.timer = s.interval * 0.3f;
            m_sounds.push_back(s);
        } else if (cls == "mirror_phantom") {
            Phantom p;
            p.position = e.GetOrigin();
            p.yaw = QuakeYawToWorld(e.GetFloat("angle", 0.0f));
            p.flag = e.Get("flag");
            p.hideFlag = e.Get("hide_flag");
            m_phantoms.push_back(p);
        }
    }
    physics.FinalizeStatic();

    std::vector<BakeLight> bakeLights;
    for (const LightObject& l : m_lights) {
        if (l.dynamic) continue;
        bakeLights.push_back(BakeLight{l.position, Vector3Scale(l.color, l.intensity), l.radius, l.group == "power"});
    }
    LightBaker baker(
        [&physics](Vector3 origin, Vector3 dir, float maxDistance) {
            return physics.CastStaticDistance(origin, dir, maxDistance);
        },
        bakeLights, m_env.skyLight);

    auto texSizeOf = [&](const std::string& name) {
        Texture2D tex = ResolveTexture(eng, name);
        return Vector2{static_cast<float>(std::max(1, tex.width)), static_cast<float>(std::max(1, tex.height))};
    };

    auto finishBatches = [&](std::map<std::string, MeshBuilder>& builders, std::vector<RenderBatch>& out) {
        for (auto& [name, builder] : builders) {
            if (builder.VertexCount() == 0) continue;
            RenderBatch batch;
            batch.mesh = builder.Build();
            batch.material = LoadMaterialDefault();
            batch.material.shader = lit;
            batch.material.maps[MATERIAL_MAP_DIFFUSE].texture = ResolveTexture(eng, name);
            batch.emissive = IsEmissiveTexture(name);
            m_vertexCount += static_cast<size_t>(builder.VertexCount());
            out.push_back(batch);
        }
    };

    std::map<std::string, MeshBuilder> staticBuilders;
    for (const PendingStatic& ps : staticBrushes) {
        for (const auto& poly : QuakeMap::BuildPolygons(*ps.brush)) {
            if (IsInvisibleTexture(poly.texture) || poly.texture == "motel_mirror_glass") continue;
            AppendPolygon(staticBuilders, poly, texSizeOf(poly.texture), baker, Vector3{0.0f, 0.0f, 0.0f});
        }
    }
    finishBatches(staticBuilders, m_static);

    for (const PendingMirror& pm : mirrors) {
        for (const auto& brush : pm.entity->brushes) {
            for (const auto& poly : QuakeMap::BuildPolygons(brush)) {
                if (poly.texture != "motel_mirror_glass") continue;
                std::map<std::string, MeshBuilder> one;
                AppendPolygon(one, poly, texSizeOf(poly.texture), baker, Vector3{0.0f, 0.0f, 0.0f});
                MirrorObject mirror;
                mirror.surface.mesh = one[poly.texture].Build();
                mirror.surface.material = LoadMaterialDefault();
                mirror.surface.material.shader = mirrorShader;
                mirror.surface.material.maps[MATERIAL_MAP_SPECULAR].texture = ResolveTexture(eng, poly.texture);
                Vector3 c{0.0f, 0.0f, 0.0f};
                for (const Vector3& p : poly.points) c = Vector3Add(c, p);
                mirror.center = Vector3Scale(c, 1.0f / static_cast<float>(poly.points.size()));
                mirror.normal = poly.normal;
                mirror.planeDistance = Vector3DotProduct(poly.normal, mirror.center);
                mirror.lookEvent = pm.entity->Get("look_event");
                m_mirrors.push_back(mirror);
            }
        }
    }

    for (const MapEntity& e : map.Entities()) {
        const std::string& cls = e.ClassName();
        if (cls != "func_door" && cls != "prop_physics") continue;
        std::vector<Vector3> points;
        for (const auto& brush : e.brushes) {
            auto v = QuakeMap::BuildVertices(brush);
            points.insert(points.end(), v.begin(), v.end());
        }
        if (points.size() < 4) continue;
        BoundingBox bounds = EntityBounds(e);

        if (cls == "func_door") {
            DoorObject door;
            door.name = e.Get("targetname");
            door.label = e.Get("label", "Дверь");
            door.lockItem = e.Get("lock");
            door.lockedEvent = e.Get("locked_event");
            door.unlockEvent = e.Get("unlock_event");
            door.locked = !door.lockItem.empty();
            door.bothWays = e.GetInt("both_ways", 1) != 0;
            door.openAngle = e.GetFloat("open_angle", 100.0f) * DEG2RAD;
            Vector3 hq = e.GetVector("hinge");
            door.restCenter = BoundsCenter(bounds);
            door.hinge = Vector3{hq.x / QuakeMap::UNITS_PER_METER, door.restCenter.y, -hq.y / QuakeMap::UNITS_PER_METER};
            float a = std::abs(door.openAngle);
            float minA = door.bothWays ? -a : std::min(0.0f, door.openAngle);
            float maxA = door.bothWays ? a : std::max(0.0f, door.openAngle);
            Vector3 origin{};
            door.body = physics.AddDoor(points, door.hinge, 22.0f, minA, maxA, origin);
            if (door.locked) physics.SetDoorLimits(door.body, 0.0f, 0.0f);
            std::map<std::string, MeshBuilder> builders;
            for (const auto& brush : e.brushes) {
                for (const auto& poly : QuakeMap::BuildPolygons(brush)) {
                    if (IsInvisibleTexture(poly.texture)) continue;
                    AppendPolygon(builders, poly, texSizeOf(poly.texture), baker, origin);
                }
            }
            finishBatches(builders, door.batches);
            m_doors.push_back(std::move(door));
        } else {
            PropObject prop;
            prop.label = e.Get("label", "Предмет");
            prop.mass = e.GetFloat("mass", 10.0f);
            Vector3 origin{};
            prop.body = physics.AddDynamicHull(points, prop.mass, origin);
            std::map<std::string, MeshBuilder> builders;
            for (const auto& brush : e.brushes) {
                for (const auto& poly : QuakeMap::BuildPolygons(brush)) {
                    if (IsInvisibleTexture(poly.texture)) continue;
                    AppendPolygon(builders, poly, texSizeOf(poly.texture), baker, origin);
                }
            }
            finishBatches(builders, prop.batches);
            m_props.push_back(std::move(prop));
        }
    }

    for (ItemObject& item : m_items) {
        BakeSample s = baker.Sample(Vector3Add(item.position, Vector3{0.0f, 0.05f, 0.0f}), Vector3{0.0f, 1.0f, 0.0f});
        item.bakedLight = s.base;
        item.bakedPower = s.power;
    }

    TraceLog(LOG_INFO, "CH2: level built: %zu vertices, %zu bake samples, %zu doors, %zu props",
             m_vertexCount, baker.CacheSize(), m_doors.size(), m_props.size());
    return true;
}

void MotelLevel::Unload() {
    auto release = [](std::vector<RenderBatch>& batches) {
        for (RenderBatch& b : batches) {
            UnloadMesh(b.mesh);
            if (b.material.maps != nullptr) MemFree(b.material.maps);
        }
        batches.clear();
    };
    release(m_static);
    for (DoorObject& d : m_doors) release(d.batches);
    for (PropObject& p : m_props) release(p.batches);
    for (MirrorObject& m : m_mirrors) {
        UnloadMesh(m.surface.mesh);
        if (m.surface.material.maps != nullptr) MemFree(m.surface.material.maps);
    }
    m_doors.clear();
    m_props.clear();
    m_mirrors.clear();
    m_interacts.clear();
    m_triggers.clear();
    m_items.clear();
    m_hideSpots.clear();
    m_lights.clear();
    m_sounds.clear();
    m_phantoms.clear();
    m_surfaces.clear();
    m_textureCache.clear();
    m_vertexCount = 0;
}

std::string MotelLevel::SurfaceForBody(BodyHandle body) const {
    auto it = m_surfaces.find(body);
    return it != m_surfaces.end() ? it->second : std::string{};
}

int MotelLevel::FindDoorByBody(BodyHandle body) const {
    for (size_t i = 0; i < m_doors.size(); ++i) {
        if (m_doors[i].body == body) return static_cast<int>(i);
    }
    return -1;
}

int MotelLevel::FindDoorByName(const std::string& name) const {
    for (size_t i = 0; i < m_doors.size(); ++i) {
        if (m_doors[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

int MotelLevel::FindPropByBody(BodyHandle body) const {
    for (size_t i = 0; i < m_props.size(); ++i) {
        if (m_props[i].body == body) return static_cast<int>(i);
    }
    return -1;
}

int MotelLevel::FindInteractByBody(BodyHandle body) const {
    for (size_t i = 0; i < m_interacts.size(); ++i) {
        for (BodyHandle b : m_interacts[i].bodies) {
            if (b == body) return static_cast<int>(i);
        }
    }
    return -1;
}

}
