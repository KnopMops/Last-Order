#include "game/chapter3/HouseLevel.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace game::chapter3 {

namespace {

constexpr float B_FLOOR = -3.0f;
constexpr float F1 = 0.0f;
constexpr float F2 = 3.2f;
constexpr float F3 = 6.4f;
constexpr float ROOF = 9.0f;
constexpr float SLAB = 0.25f;
constexpr float DOOR_H = 2.1f;

const std::string T_WALLPAPER = "ch3/house_wallpaper_peeled";
const std::string T_PARQUET = "ch3/house_floor_parquet";
const std::string T_PLANKS = "ch3/house_wood_planks";
const std::string T_BRICK = "ch3/house_basement_brick";
const std::string T_ATTIC = "ch3/house_attic_dust";
const std::string T_GARAGE = "ch3/house_garage_wall";
const std::string T_DOOR = "ch3/house_door_interior";
const std::string T_WORKBENCH = "ch3/house_workbench";
const std::string T_CEILING = "motel_ceiling";
const std::string T_TILE = "motel_tile";
const std::string T_FABRIC = "motel_fabric";
const std::string T_METAL = "motel_metal";
const std::string T_CONCRETE = "motel_concrete";
const std::string T_GRASS = "motel_grass";
const std::string T_GRAVEL = "motel_gravel";
const std::string T_ROOF = "motel_roof";
const std::string T_WINDOW = "motel_window";
const std::string T_CARPET = "motel_carpet";
const std::string T_TRIM = "motel_trim";
const std::string T_PORCELAIN = "motel_porcelain";
const std::string T_RADIO = "motel_radio";
const std::string T_EXTERIOR = "motel_wall_wood";
const std::string T_WHITE = "@white";

const Color WP_HALL{214, 204, 188, 255};
const Color WP_LIVING{214, 204, 188, 255};
const Color WP_BEDROOM{214, 204, 188, 255};
const Color WP_GUEST{214, 204, 188, 255};
const Color WP_DINING{214, 204, 188, 255};
const Color DARK_WOOD{120, 86, 60, 255};
const Color MID_WOOD{170, 128, 92, 255};

struct MeshBuilder {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned char> colors;
    std::vector<float> tangents;

    void Add(Vector3 p, Vector3 n, Vector2 uv, Vector3 light, float ao) {
        positions.insert(positions.end(), {p.x, p.y, p.z});
        normals.insert(normals.end(), {n.x, n.y, n.z});
        texcoords.insert(texcoords.end(), {uv.x, uv.y});
        auto pack = [](float v) { return static_cast<unsigned char>(std::clamp(v * 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f); };
        colors.insert(colors.end(), {pack(light.x), pack(light.y), pack(light.z),
                                     static_cast<unsigned char>(std::clamp(ao, 0.0f, 1.0f) * 255.0f)});
        tangents.insert(tangents.end(), {0.0f, 0.0f, 0.0f, 1.0f});
    }

    Mesh Build() const {
        Mesh mesh{};
        mesh.vertexCount = static_cast<int>(positions.size() / 3);
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

float Axis(Vector3 v, int a) {
    return a == 0 ? v.x : (a == 1 ? v.y : v.z);
}

void SetAxis(Vector3& v, int a, float value) {
    if (a == 0) v.x = value;
    else if (a == 1) v.y = value;
    else v.z = value;
}

bool PointInBox(Vector3 p, const HouseBox& b) {
    return p.x > b.min.x && p.x < b.max.x && p.y > b.min.y && p.y < b.max.y && p.z > b.min.z && p.z < b.max.z;
}

bool BoxesOverlap(Vector3 amin, Vector3 amax, Vector3 bmin, Vector3 bmax) {
    return amin.x <= bmax.x && amax.x >= bmin.x && amin.y <= bmax.y && amax.y >= bmin.y && amin.z <= bmax.z && amax.z >= bmin.z;
}

bool SegmentHitsBox(Vector3 a, Vector3 d, const HouseBox& b) {
    float t0 = 0.0f;
    float t1 = 1.0f;
    for (int i = 0; i < 3; ++i) {
        float o = Axis(a, i);
        float dir = Axis(d, i);
        float mn = Axis(b.min, i) + 0.005f;
        float mx = Axis(b.max, i) - 0.005f;
        if (std::abs(dir) < 1e-6f) {
            if (o <= mn || o >= mx) return false;
            continue;
        }
        float inv = 1.0f / dir;
        float ta = (mn - o) * inv;
        float tb = (mx - o) * inv;
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    return t1 > 0.0f && t0 < 0.999f;
}

std::string ColorKey(Color c) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
    return buf;
}

}

void HouseLevel::Box(Vector3 mn, Vector3 mx, const std::string& side, const std::string& top, const std::string& bottom,
                     Color tint, float uv) {
    HouseBox b;
    b.min = mn;
    b.max = mx;
    b.side = side;
    b.top = top;
    b.bottom = bottom;
    b.tint = tint;
    b.uvScale = uv;
    m_boxes.push_back(b);
}

void HouseLevel::Solid(Vector3 mn, Vector3 mx, const std::string& tex, Color tint, float uv) {
    Box(mn, mx, tex, tex, tex, tint, uv);
}

void HouseLevel::Fitted(Vector3 mn, Vector3 mx, const std::string& face, const std::string& rest, Color tint) {
    HouseBox b;
    b.min = mn;
    b.max = mx;
    b.side = face;
    b.top = rest;
    b.bottom = rest;
    b.tint = tint;
    b.fitUV = true;
    m_boxes.push_back(b);
}

void HouseLevel::Deco(Vector3 mn, Vector3 mx, const std::string& tex, Color tint, float emissive, const std::string& tag) {
    HouseBox b;
    b.min = mn;
    b.max = mx;
    b.side = tex;
    b.top = tex;
    b.bottom = tex;
    b.tint = tint;
    b.emissive = emissive;
    b.solid = false;
    b.occluder = false;
    b.uvScale = 1.0f;
    b.tag = tag;
    m_boxes.push_back(b);
}

void HouseLevel::WallX(float x, float z0, float z1, float y0, float y1, const std::string& texNeg, const std::string& texPos,
                       const std::vector<Vector3>& openings, Color tintNeg, Color tintPos) {
    std::vector<Vector3> ops = openings;
    std::sort(ops.begin(), ops.end(), [](const Vector3& a, const Vector3& b) { return a.x < b.x; });
    auto piece = [&](float za, float zb, float ya, float yb) {
        if (zb - za < 0.01f || yb - ya < 0.01f) return;
        Box({x - 0.1f, ya, za}, {x, yb, zb}, texNeg, texNeg, texNeg, tintNeg);
        Box({x, ya, za}, {x + 0.1f, yb, zb}, texPos, texPos, texPos, tintPos);
    };
    float cursor = z0;
    for (const Vector3& o : ops) {
        piece(cursor, o.x, y0, y1);
        piece(o.x, o.y, y0 + o.z, y1);
        Deco({x - 0.13f, y0, o.x - 0.07f}, {x + 0.13f, y0 + o.z + 0.07f, o.x}, T_TRIM, DARK_WOOD, 0.0f);
        Deco({x - 0.13f, y0, o.y}, {x + 0.13f, y0 + o.z + 0.07f, o.y + 0.07f}, T_TRIM, DARK_WOOD, 0.0f);
        Deco({x - 0.13f, y0 + o.z, o.x - 0.07f}, {x + 0.13f, y0 + o.z + 0.07f, o.y + 0.07f}, T_TRIM, DARK_WOOD, 0.0f);
        cursor = o.y;
    }
    piece(cursor, z1, y0, y1);
}

void HouseLevel::WallZ(float z, float x0, float x1, float y0, float y1, const std::string& texNeg, const std::string& texPos,
                       const std::vector<Vector3>& openings, Color tintNeg, Color tintPos) {
    std::vector<Vector3> ops = openings;
    std::sort(ops.begin(), ops.end(), [](const Vector3& a, const Vector3& b) { return a.x < b.x; });
    auto piece = [&](float xa, float xb, float ya, float yb) {
        if (xb - xa < 0.01f || yb - ya < 0.01f) return;
        Box({xa, ya, z - 0.1f}, {xb, yb, z}, texNeg, texNeg, texNeg, tintNeg);
        Box({xa, ya, z}, {xb, yb, z + 0.1f}, texPos, texPos, texPos, tintPos);
    };
    float cursor = x0;
    for (const Vector3& o : ops) {
        piece(cursor, o.x, y0, y1);
        piece(o.x, o.y, y0 + o.z, y1);
        Deco({o.x - 0.07f, y0, z - 0.13f}, {o.x, y0 + o.z + 0.07f, z + 0.13f}, T_TRIM, DARK_WOOD, 0.0f);
        Deco({o.y, y0, z - 0.13f}, {o.y + 0.07f, y0 + o.z + 0.07f, z + 0.13f}, T_TRIM, DARK_WOOD, 0.0f);
        Deco({o.x - 0.07f, y0 + o.z, z - 0.13f}, {o.y + 0.07f, y0 + o.z + 0.07f, z + 0.13f}, T_TRIM, DARK_WOOD, 0.0f);
        cursor = o.y;
    }
    piece(cursor, x1, y0, y1);
}

void HouseLevel::Slab(float x0, float x1, float z0, float z1, float yTop, float thick, const std::string& top,
                      const std::string& bottom, Color tint) {
    Box({x0, yTop - thick, z0}, {x1, yTop, z1}, bottom, top, bottom, tint, top == T_PARQUET ? 1.2f : 1.5f);
}

void HouseLevel::Stairs(float x0, float x1, float zStart, float dir, float yBottom, float yTop, const std::string& tex) {
    int steps = static_cast<int>(std::round((yTop - yBottom) / 0.2f));
    float rise = (yTop - yBottom) / static_cast<float>(steps);
    float run = 0.28f;
    for (int i = 0; i < steps; ++i) {
        float za = zStart + dir * run * static_cast<float>(i);
        float zb = zStart + dir * run * static_cast<float>(i + 1);
        float top = yBottom + rise * static_cast<float>(i + 1);
        Box({x0, yBottom, std::min(za, zb)}, {x1, top, std::max(za, zb)}, T_PLANKS, T_PLANKS, T_PLANKS, MID_WOOD, 1.0f);
    }
}

void HouseLevel::Railing(Vector3 mn, Vector3 mx) {
    HouseBox col;
    col.min = mn;
    col.max = {mx.x, mn.y + 1.05f, mx.z};
    col.occluder = false;
    m_boxes.push_back(col);
    bool alongZ = (mx.z - mn.z) > (mx.x - mn.x);
    float cx = (mn.x + mx.x) * 0.5f;
    float cz = (mn.z + mx.z) * 0.5f;
    if (alongZ) {
        Deco({cx - 0.05f, mn.y + 0.95f, mn.z}, {cx + 0.05f, mn.y + 1.05f, mx.z}, T_PLANKS, DARK_WOOD, 0.0f);
        for (float z = mn.z + 0.05f; z < mx.z; z += 0.45f) {
            Deco({cx - 0.025f, mn.y, z - 0.025f}, {cx + 0.025f, mn.y + 0.95f, z + 0.025f}, T_PLANKS, DARK_WOOD, 0.0f);
        }
    } else {
        Deco({mn.x, mn.y + 0.95f, cz - 0.05f}, {mx.x, mn.y + 1.05f, cz + 0.05f}, T_PLANKS, DARK_WOOD, 0.0f);
        for (float x = mn.x + 0.05f; x < mx.x; x += 0.45f) {
            Deco({x - 0.025f, mn.y, cz - 0.025f}, {x + 0.025f, mn.y + 0.95f, cz + 0.025f}, T_PLANKS, DARK_WOOD, 0.0f);
        }
    }
}

void HouseLevel::Lamp(Vector3 pos, Vector3 color, float radius, float intensity) {
    m_lamps.push_back({pos, color, radius, intensity});
    Deco({pos.x - 0.015f, pos.y + 0.08f, pos.z - 0.015f}, {pos.x + 0.015f, pos.y + 0.36f, pos.z + 0.015f}, T_WHITE, Color{30, 30, 30, 255}, 0.0f);
    Deco({pos.x - 0.07f, pos.y - 0.07f, pos.z - 0.07f}, {pos.x + 0.07f, pos.y + 0.08f, pos.z + 0.07f}, T_WHITE,
         Color{static_cast<unsigned char>(255.0f * std::min(1.0f, color.x)), static_cast<unsigned char>(255.0f * std::min(1.0f, color.y)),
               static_cast<unsigned char>(255.0f * std::min(1.0f, color.z)), 255},
         1.0f, "lamp" + std::to_string(m_lamps.size() - 1));
}

void HouseLevel::Window(Vector3 mn, Vector3 mx) {
    HouseBox b;
    b.min = mn;
    b.max = mx;
    b.side = T_WINDOW;
    b.top = T_TRIM;
    b.bottom = T_TRIM;
    b.tint = Color{150, 160, 190, 255};
    b.emissive = 0.18f;
    b.solid = false;
    b.occluder = false;
    b.fitUV = true;
    m_boxes.push_back(b);
}

void HouseLevel::BuildShell() {
    std::vector<Vector3> none;

    WallX(-6.1f, -6.2f, 6.2f, B_FLOOR - 0.3f, F1, T_CONCRETE, T_BRICK, none);
    WallX(6.1f, -6.2f, 6.2f, B_FLOOR - 0.3f, F1, T_BRICK, T_CONCRETE, none);
    WallZ(-6.1f, -6.0f, 6.0f, B_FLOOR - 0.3f, F1, T_CONCRETE, T_BRICK, none);
    WallZ(6.1f, -6.0f, 6.0f, B_FLOOR - 0.3f, F1, T_BRICK, T_CONCRETE, none);

    WallX(-6.1f, -6.2f, 1.5f, F1, F2, T_GARAGE, T_PLANKS, {{-1.0f, -0.1f, 1.15f}});
    WallX(-6.1f, 1.5f, 3.6f, F1, F2, T_GARAGE, T_WALLPAPER, {{2.2f, 3.2f, DOOR_H}}, WHITE, WP_LIVING);
    WallX(-6.1f, 3.6f, 6.2f, F1, F2, T_EXTERIOR, T_WALLPAPER, none, WHITE, WP_LIVING);
    WallX(6.1f, -6.2f, 1.0f, F1, F2, T_TILE, T_EXTERIOR, none);
    WallX(6.1f, 1.0f, 6.2f, F1, F2, T_WALLPAPER, T_EXTERIOR, none, WP_DINING);
    WallZ(-6.1f, -6.0f, -1.5f, F1, F2, T_EXTERIOR, T_PLANKS, none);
    WallZ(-6.1f, -1.5f, 1.5f, F1, F2, T_EXTERIOR, T_WALLPAPER, none, WHITE, WP_HALL);
    WallZ(-6.1f, 1.5f, 6.0f, F1, F2, T_EXTERIOR, T_TILE, none);
    WallZ(6.1f, -6.0f, -1.5f, F1, F2, T_WALLPAPER, T_EXTERIOR, none, WP_LIVING);
    WallZ(6.1f, -1.5f, 1.5f, F1, F2, T_WALLPAPER, T_EXTERIOR, {{-0.6f, 0.6f, 2.2f}}, WP_HALL);
    WallZ(6.1f, 1.5f, 6.0f, F1, F2, T_WALLPAPER, T_EXTERIOR, none, WP_DINING);

    WallX(-6.1f, -6.2f, 1.5f, F2, F3, T_EXTERIOR, T_PLANKS, none);
    WallX(-6.1f, 1.5f, 6.2f, F2, F3, T_EXTERIOR, T_WALLPAPER, none, WHITE, WP_BEDROOM);
    WallX(6.1f, -6.2f, 1.0f, F2, F3, T_TILE, T_EXTERIOR, none);
    WallX(6.1f, 1.0f, 6.2f, F2, F3, T_WALLPAPER, T_EXTERIOR, none, WP_GUEST);
    WallZ(-6.1f, -6.0f, -1.5f, F2, F3, T_EXTERIOR, T_PLANKS, none);
    WallZ(-6.1f, -1.5f, 1.5f, F2, F3, T_EXTERIOR, T_WALLPAPER, none, WHITE, WP_HALL);
    WallZ(-6.1f, 1.5f, 6.0f, F2, F3, T_EXTERIOR, T_TILE, none);
    WallZ(6.1f, -6.0f, -1.5f, F2, F3, T_WALLPAPER, T_EXTERIOR, none, WP_BEDROOM);
    WallZ(6.1f, -1.5f, 1.5f, F2, F3, T_WALLPAPER, T_EXTERIOR, none, WP_HALL);
    WallZ(6.1f, 1.5f, 6.0f, F2, F3, T_WALLPAPER, T_EXTERIOR, none, WP_GUEST);

    WallX(-6.1f, -6.2f, 6.2f, F3, ROOF, T_EXTERIOR, T_ATTIC, none);
    WallX(6.1f, -6.2f, 6.2f, F3, ROOF, T_ATTIC, T_EXTERIOR, none);
    WallZ(-6.1f, -6.0f, 6.0f, F3, ROOF, T_EXTERIOR, T_ATTIC, none);
    WallZ(6.1f, -6.0f, 6.0f, F3, ROOF, T_ATTIC, T_EXTERIOR, none);
    Box({-6.5f, ROOF, -6.5f}, {6.5f, ROOF + 0.3f, 6.5f}, T_ROOF, T_ROOF, T_ATTIC, WHITE, 2.0f);

    Window({-4.6f, 0.9f, 5.97f}, {-3.2f, 2.2f, 6.23f});
    Window({-6.23f, 0.9f, 4.3f}, {-5.97f, 2.2f, 5.5f});
    Window({3.2f, 0.9f, 5.97f}, {4.6f, 2.2f, 6.23f});
    Window({5.97f, 0.9f, 2.4f}, {6.23f, 2.2f, 3.6f});
    Window({5.97f, 1.0f, -3.0f}, {6.23f, 2.1f, -1.8f});
    Window({-4.6f, 4.1f, 5.97f}, {-3.2f, 5.4f, 6.23f});
    Window({3.2f, 4.3f, 5.97f}, {4.6f, 5.4f, 6.23f});
    Window({5.97f, 4.1f, 2.9f}, {6.23f, 5.4f, 3.9f});
    Window({5.97f, 4.4f, -3.0f}, {6.23f, 5.4f, -2.0f});
    Window({-0.7f, 7.2f, 5.97f}, {0.7f, 8.2f, 6.23f});
    Window({-0.7f, 7.2f, -6.23f}, {0.7f, 8.2f, -5.97f});
}

void HouseLevel::BuildBasement() {
    Slab(-6.0f, 6.0f, -6.0f, 6.0f, B_FLOOR, 0.3f, T_CONCRETE, T_CONCRETE);

    WallZ(1.5f, 1.6f, 6.0f, B_FLOOR, F1 - SLAB, T_BRICK, T_BRICK, {{3.0f, 4.2f, DOOR_H}});
    WallX(1.5f, 1.4f, 6.0f, B_FLOOR, F1 - SLAB, T_BRICK, T_BRICK, {});

    Solid({-2.15f, B_FLOOR, -0.15f}, {-1.85f, F1 - SLAB, 0.15f}, T_BRICK, WHITE, 1.0f);
    Solid({-0.15f, B_FLOOR, -2.65f}, {0.15f, F1 - SLAB, -2.35f}, T_BRICK, WHITE, 1.0f);

    Stairs(-6.0f, -4.8f, -4.2f, 1.0f, B_FLOOR, F1, T_PLANKS);

    Fitted({-2.5f, B_FLOOR + 0.8f, -5.95f}, {-0.5f, B_FLOOR + 0.9f, -5.2f}, T_PLANKS, T_WORKBENCH, MID_WOOD);
    Solid({-2.45f, B_FLOOR, -5.9f}, {-2.3f, B_FLOOR + 0.8f, -5.25f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({-0.7f, B_FLOOR, -5.9f}, {-0.55f, B_FLOOR + 0.8f, -5.25f}, T_PLANKS, DARK_WOOD, 1.0f);
    Fitted({-2.4f, B_FLOOR + 0.9f, -6.0f}, {-0.6f, B_FLOOR + 1.9f, -5.93f}, T_WORKBENCH, T_PLANKS, WHITE);
    Solid({-0.3f, B_FLOOR, -5.95f}, {0.4f, B_FLOOR + 0.8f, -5.4f}, T_PLANKS, DARK_WOOD, 1.0f);

    Solid({5.5f, B_FLOOR, -3.2f}, {5.95f, B_FLOOR + 0.05f, -0.8f}, T_PLANKS, DARK_WOOD, 1.0f);
    for (int i = 0; i < 4; ++i) {
        float y = B_FLOOR + 0.45f + 0.5f * static_cast<float>(i);
        Solid({5.5f, y, -3.2f}, {5.95f, y + 0.05f, -0.8f}, T_PLANKS, DARK_WOOD, 1.0f);
        for (int j = 0; j < 5; ++j) {
            float z = -3.0f + 0.45f * static_cast<float>(j);
            Deco({5.6f, y + 0.05f, z}, {5.78f, y + 0.27f, z + 0.18f}, T_PORCELAIN, Color{120, 150, 110, 255}, 0.0f);
        }
    }
    Solid({5.5f, B_FLOOR, -3.25f}, {5.95f, B_FLOOR + 2.1f, -3.2f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({5.5f, B_FLOOR, -0.8f}, {5.95f, B_FLOOR + 2.1f, -0.75f}, T_PLANKS, DARK_WOOD, 1.0f);

    Solid({2.65f, B_FLOOR, -5.5f}, {3.35f, B_FLOOR + 0.9f, -4.8f}, T_PLANKS, MID_WOOD, 0.8f);
    Solid({3.5f, B_FLOOR, -5.6f}, {4.2f, B_FLOOR + 0.9f, -4.9f}, T_PLANKS, DARK_WOOD, 0.8f);

    Solid({2.0f, B_FLOOR, 4.6f}, {3.2f, B_FLOOR + 2.0f, 5.9f}, T_METAL, Color{150, 120, 100, 255}, 1.0f);
    Deco({2.5f, B_FLOOR + 2.0f, 5.1f}, {2.7f, F1 - SLAB, 5.3f}, T_METAL, Color{110, 100, 90, 255}, 0.0f);
    Deco({3.2f, B_FLOOR + 1.5f, 5.1f}, {5.95f, B_FLOOR + 1.65f, 5.25f}, T_METAL, Color{140, 110, 90, 255}, 0.0f);
    Solid({5.75f, B_FLOOR, 3.0f}, {5.98f, B_FLOOR + 1.2f, 4.4f}, T_METAL, Color{70, 75, 80, 255}, 0.5f);
    Deco({5.7f, B_FLOOR + 1.3f, 3.6f}, {5.95f, B_FLOOR + 1.55f, 3.85f}, T_METAL, Color{170, 40, 30, 255}, 0.0f);

    Solid({-5.95f, B_FLOOR, 2.0f}, {-5.3f, B_FLOOR + 2.0f, 3.2f}, T_PLANKS, DARK_WOOD, 1.0f);
    Deco({-5.31f, B_FLOOR + 0.1f, 2.05f}, {-5.28f, B_FLOOR + 1.9f, 3.15f}, T_DOOR, Color{140, 110, 90, 255}, 0.0f);

    Railing({-4.84f, F1, -4.2f}, {-4.76f, F1, -0.3f});
    Railing({-6.0f, F1, -4.24f}, {-4.8f, F1, -4.16f});

    Lamp({-2.0f, F1 - SLAB - 0.45f, -3.0f}, {1.0f, 0.75f, 0.45f}, 7.5f, 0.6f);
    Lamp({3.8f, F1 - SLAB - 0.45f, 3.6f}, {0.75f, 0.9f, 0.6f}, 5.5f, 0.55f);
    Lamp({-3.0f, F1 - SLAB - 0.45f, 3.0f}, {1.0f, 0.7f, 0.4f}, 5.0f, 0.35f);
}

void HouseLevel::BuildFirstFloor() {
    float ceil = F2 - SLAB;

    Slab(-6.0f, -1.5f, 1.5f, 6.0f, F1, SLAB, T_PARQUET, T_PLANKS);
    Slab(-4.8f, -1.5f, -6.0f, 1.5f, F1, SLAB, T_PLANKS, T_PLANKS);
    Slab(-6.0f, -4.8f, -6.0f, -4.2f, F1, SLAB, T_PLANKS, T_PLANKS);
    Slab(-6.0f, -4.8f, 0.0f, 1.5f, F1, SLAB, T_PLANKS, T_PLANKS);
    Slab(-1.5f, 1.5f, -6.0f, 6.0f, F1, SLAB, T_PARQUET, T_PLANKS);
    Slab(1.5f, 6.0f, -6.0f, 1.0f, F1, SLAB, T_TILE, T_PLANKS);
    Slab(1.5f, 6.0f, 1.0f, 6.0f, F1, SLAB, T_PARQUET, T_PLANKS);

    WallX(-1.5f, -6.0f, 1.5f, F1, ceil, T_PLANKS, T_WALLPAPER, {{-3.0f, -2.0f, DOOR_H}}, WHITE, WP_HALL);
    WallX(-1.5f, 1.5f, 6.0f, F1, ceil, T_WALLPAPER, T_WALLPAPER, {{3.0f, 4.0f, DOOR_H}}, WP_LIVING, WP_HALL);
    WallX(1.5f, -6.0f, 1.0f, F1, ceil, T_WALLPAPER, T_TILE, {{-0.3f, 0.7f, DOOR_H}}, WP_HALL, WHITE);
    WallX(1.5f, 1.0f, 6.0f, F1, ceil, T_WALLPAPER, T_WALLPAPER, {{3.0f, 4.0f, DOOR_H}}, WP_HALL, WP_DINING);
    WallZ(1.5f, -6.0f, -1.6f, F1, ceil, T_PLANKS, T_WALLPAPER, {{-5.6f, -4.7f, 1.2f}}, WHITE, WP_LIVING);
    WallZ(1.0f, 1.6f, 6.0f, F1, ceil, T_TILE, T_WALLPAPER, {{3.5f, 4.5f, 2.3f}}, WHITE, WP_DINING);

    Stairs(0.3f, 1.4f, -5.0f, 1.0f, F1, F2, T_PLANKS);

    Fitted({-0.6f, F1, 5.98f}, {0.6f, F1 + 2.2f, 6.18f}, T_DOOR, T_PLANKS, Color{120, 92, 70, 255});
    m_boxes.back().tag = "front_door";
    for (int i = 0; i < 3; ++i) {
        float y = 0.55f + 0.6f * static_cast<float>(i);
        Deco({-0.85f, y, 5.86f}, {0.85f, y + 0.2f, 5.96f}, T_PLANKS, MID_WOOD, 0.0f, "plank" + std::to_string(i));
    }
    Deco({0.15f, 1.0f, 5.82f}, {0.32f, 1.2f, 5.9f}, T_METAL, Color{150, 140, 90, 255}, 0.0f, "padlock");
    Deco({-0.62f, 1.9f, 5.9f}, {0.62f, 1.93f, 5.94f}, T_WHITE, Color{190, 30, 25, 255}, 0.0f, "alarm");
    Deco({0.8f, 1.25f, 5.92f}, {1.05f, 1.55f, 5.98f}, T_METAL, Color{60, 70, 60, 255}, 0.0f);
    Deco({0.86f, 1.45f, 5.91f}, {0.99f, 1.5f, 5.92f}, T_WHITE, Color{80, 255, 120, 255}, 1.0f, "keypad_led");

    Solid({-1.45f, F1, -1.4f}, {-1.0f, F1 + 0.85f, -0.2f}, T_PLANKS, DARK_WOOD, 1.0f);
    Deco({-1.4f, F1 + 1.5f, 4.6f}, {-1.37f, F1 + 2.2f, 5.4f}, "motel_mirror_glass", Color{110, 115, 120, 255}, 0.05f);

    Deco({-5.0f, F1, 2.7f}, {-2.2f, F1 + 0.02f, 5.4f}, T_CARPET, Color{150, 70, 60, 255}, 0.0f);
    Box({-5.95f, F1, 3.7f}, {-5.1f, F1 + 0.45f, 5.7f}, T_FABRIC, T_FABRIC, T_FABRIC, Color{110, 80, 70, 255}, 0.8f);
    Box({-5.95f, F1 + 0.45f, 3.7f}, {-5.75f, F1 + 0.95f, 5.7f}, T_FABRIC, T_FABRIC, T_FABRIC, Color{100, 72, 64, 255}, 0.8f);
    Box({-5.95f, F1, 3.5f}, {-5.1f, F1 + 0.65f, 3.7f}, T_FABRIC, T_FABRIC, T_FABRIC, Color{95, 70, 60, 255}, 0.8f);
    Box({-5.95f, F1, 5.7f}, {-5.1f, F1 + 0.65f, 5.9f}, T_FABRIC, T_FABRIC, T_FABRIC, Color{95, 70, 60, 255}, 0.8f);
    Solid({-3.7f, F1, 5.45f}, {-2.5f, F1 + 0.55f, 5.95f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({-3.5f, F1 + 0.55f, 5.5f}, {-2.7f, F1 + 1.15f, 5.9f}, T_METAL, Color{70, 60, 50, 255}, 1.0f);
    Deco({-3.42f, F1 + 0.62f, 5.48f}, {-2.78f, F1 + 1.08f, 5.5f}, T_WHITE, Color{40, 55, 50, 255}, 0.25f);
    Solid({-4.2f, F1, 1.6f}, {-2.8f, F1 + 1.2f, 2.2f}, T_BRICK, Color{170, 140, 130, 255}, 1.0f);
    Deco({-3.85f, F1 + 0.1f, 2.19f}, {-3.15f, F1 + 0.75f, 2.21f}, T_WHITE, Color{20, 15, 12, 255}, 0.0f);
    Solid({-4.35f, F1 + 1.2f, 1.6f}, {-2.65f, F1 + 1.3f, 2.3f}, T_PLANKS, DARK_WOOD, 1.0f);
    Fitted({-2.35f, F1, 5.3f}, {-1.65f, F1 + 2.1f, 5.95f}, T_DOOR, T_PLANKS, Color{150, 115, 85, 255});
    Solid({-3.4f, F1, 3.6f}, {-2.6f, F1 + 0.45f, 4.4f}, T_PLANKS, MID_WOOD, 1.0f);

    Solid({-4.5f, F1, -5.95f}, {-2.0f, F1 + 0.05f, -5.5f}, T_PLANKS, DARK_WOOD, 1.0f);
    for (int i = 0; i < 4; ++i) {
        float y = F1 + 0.5f * static_cast<float>(i + 1);
        Solid({-4.5f, y, -5.95f}, {-2.0f, y + 0.05f, -5.5f}, T_PLANKS, DARK_WOOD, 1.0f);
        for (int j = 0; j < 5; ++j) {
            float x = -4.3f + 0.48f * static_cast<float>(j);
            Deco({x, y + 0.05f, -5.85f}, {x + 0.2f, y + 0.3f, -5.65f}, T_PORCELAIN, Color{170, 140, 90, 255}, 0.0f);
        }
    }

    Solid({3.0f, F1 + 0.72f, 2.8f}, {4.6f, F1 + 0.78f, 4.4f}, T_PLANKS, MID_WOOD, 1.0f);
    for (float lx : {3.05f, 4.48f}) {
        for (float lz : {2.85f, 4.28f}) {
            Solid({lx, F1, lz}, {lx + 0.07f, F1 + 0.72f, lz + 0.07f}, T_PLANKS, DARK_WOOD, 1.0f);
        }
    }
    for (float cz : {2.3f, 4.6f}) {
        Solid({3.5f, F1, cz}, {4.0f, F1 + 0.45f, cz + 0.45f}, T_PLANKS, DARK_WOOD, 1.0f);
        Solid({3.5f, F1 + 0.45f, (cz < 3.0f ? cz : cz + 0.4f)}, {4.0f, F1 + 1.0f, (cz < 3.0f ? cz + 0.05f : cz + 0.45f)}, T_PLANKS, DARK_WOOD, 1.0f);
    }
    Solid({5.55f, F1, 4.4f}, {5.95f, F1 + 1.9f, 5.8f}, T_PLANKS, DARK_WOOD, 1.0f);
    Deco({5.53f, F1 + 0.9f, 4.5f}, {5.56f, F1 + 1.8f, 5.7f}, T_WINDOW, Color{120, 110, 100, 255}, 0.05f);

    Solid({2.6f, F1, -5.95f}, {5.95f, F1 + 0.9f, -5.35f}, T_PLANKS, Color{200, 190, 170, 255}, 1.0f);
    Solid({2.6f, F1 + 0.9f, -5.95f}, {5.95f, F1 + 0.95f, -5.3f}, T_TILE, Color{230, 230, 225, 255}, 0.6f);
    Solid({1.6f, F1, -5.95f}, {2.4f, F1 + 0.92f, -5.3f}, T_METAL, Color{220, 215, 205, 255}, 1.0f);
    Deco({1.7f, F1 + 0.15f, -5.31f}, {2.3f, F1 + 0.65f, -5.29f}, T_WHITE, Color{25, 25, 25, 255}, 0.0f);
    Solid({2.6f, F1 + 1.5f, -5.95f}, {5.9f, F1 + 2.2f, -5.6f}, T_PLANKS, Color{200, 190, 170, 255}, 1.0f);
    Solid({5.3f, F1, -4.2f}, {5.95f, F1 + 1.8f, -3.4f}, T_METAL, Color{235, 235, 228, 255}, 1.0f);
    Solid({3.0f, F1 + 0.72f, -2.5f}, {4.2f, F1 + 0.78f, -1.5f}, T_PLANKS, Color{210, 200, 185, 255}, 1.0f);
    for (float lx : {3.05f, 4.08f}) {
        for (float lz : {-2.45f, -1.62f}) {
            Solid({lx, F1, lz}, {lx + 0.07f, F1 + 0.72f, lz + 0.07f}, T_METAL, Color{150, 150, 150, 255}, 1.0f);
        }
    }

    Lamp({0.0f, ceil - 0.4f, 3.8f}, {1.0f, 0.78f, 0.5f}, 6.0f, 0.75f);
    Lamp({-0.6f, ceil - 0.4f, -3.2f}, {1.0f, 0.75f, 0.45f}, 5.0f, 0.45f);
    Lamp({-3.8f, ceil - 0.4f, 3.8f}, {1.0f, 0.8f, 0.55f}, 6.5f, 0.85f);
    Lamp({-3.2f, ceil - 0.4f, -2.0f}, {1.0f, 0.72f, 0.42f}, 5.0f, 0.4f);
    Lamp({3.8f, ceil - 0.4f, 3.5f}, {1.0f, 0.8f, 0.55f}, 6.0f, 0.8f);
    Lamp({3.8f, ceil - 0.4f, -2.5f}, {0.82f, 0.9f, 1.0f}, 6.0f, 0.75f);
}

void HouseLevel::BuildSecondFloor() {
    float ceil = F3 - SLAB;

    Slab(-6.0f, -1.5f, 1.5f, 6.0f, F2, SLAB, T_PARQUET, T_CEILING);
    Slab(-6.0f, -1.5f, -6.0f, 1.5f, F2, SLAB, T_PLANKS, T_CEILING);
    Slab(-1.5f, 0.3f, -6.0f, 6.0f, F2, SLAB, T_PARQUET, T_CEILING);
    Slab(0.3f, 1.5f, -6.0f, -5.0f, F2, SLAB, T_PARQUET, T_CEILING);
    Slab(0.3f, 1.5f, -0.52f, 6.0f, F2, SLAB, T_PARQUET, T_CEILING);
    Slab(1.5f, 6.0f, -6.0f, 1.0f, F2, SLAB, T_TILE, T_CEILING);
    Slab(1.5f, 6.0f, 1.0f, 6.0f, F2, SLAB, T_PARQUET, T_CEILING);

    WallX(-1.5f, -6.0f, 1.5f, F2, ceil, T_PLANKS, T_WALLPAPER, {{-3.0f, -2.0f, DOOR_H}}, WHITE, WP_HALL);
    WallX(-1.5f, 1.5f, 6.0f, F2, ceil, T_WALLPAPER, T_WALLPAPER, {{3.0f, 4.0f, DOOR_H}}, WP_BEDROOM, WP_HALL);
    WallX(1.5f, -6.0f, 1.0f, F2, ceil, T_WALLPAPER, T_TILE, {{-0.3f, 0.7f, DOOR_H}}, WP_HALL, WHITE);
    WallX(1.5f, 1.0f, 6.0f, F2, ceil, T_WALLPAPER, T_WALLPAPER, {{3.0f, 4.0f, DOOR_H}}, WP_HALL, WP_GUEST);
    WallZ(1.5f, -6.0f, -1.6f, F2, ceil, T_PLANKS, T_WALLPAPER, {{-3.9f, -3.0f, 1.2f}}, WHITE, WP_BEDROOM);
    WallZ(1.0f, 1.6f, 6.0f, F2, ceil, T_TILE, T_WALLPAPER, {}, WHITE, WP_GUEST);

    Railing({0.26f, F2, -5.0f}, {0.34f, F2, -0.7f});
    Railing({0.3f, F2, -5.04f}, {1.4f, F2, -4.96f});

    Stairs(-6.0f, -4.8f, -4.6f, 1.0f, F2, F3, T_PLANKS);

    Box({-5.0f, F2 + 0.3f, 3.3f}, {-3.4f, F2 + 0.35f, 5.4f}, T_METAL, T_METAL, T_METAL, Color{90, 95, 100, 255}, 0.5f);
    Box({-4.95f, F2 + 0.35f, 3.35f}, {-3.45f, F2 + 0.55f, 5.3f}, T_FABRIC, T_FABRIC, T_FABRIC, Color{170, 160, 140, 255}, 0.8f);
    Deco({-4.7f, F2 + 0.55f, 4.75f}, {-3.7f, F2 + 0.67f, 5.25f}, T_FABRIC, Color{200, 195, 185, 255}, 0.0f);
    Box({-5.0f, F2, 5.3f}, {-3.4f, F2 + 1.0f, 5.45f}, T_METAL, T_METAL, T_METAL, Color{80, 85, 90, 255}, 0.5f);
    Box({-5.0f, F2, 3.25f}, {-3.4f, F2 + 0.6f, 3.35f}, T_METAL, T_METAL, T_METAL, Color{80, 85, 90, 255}, 0.5f);
    for (float lx : {-5.0f, -3.45f}) {
        Solid({lx, F2, 3.3f}, {lx + 0.05f, F2 + 0.3f, 3.35f}, T_METAL, Color{80, 85, 90, 255}, 1.0f);
        Solid({lx, F2, 5.25f}, {lx + 0.05f, F2 + 0.3f, 5.3f}, T_METAL, Color{80, 85, 90, 255}, 1.0f);
    }
    Solid({-5.95f, F2, 4.6f}, {-5.4f, F2 + 0.6f, 5.2f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({-2.2f, F2, 1.8f}, {-1.6f, F2 + 0.7f, 2.5f}, T_PLANKS, MID_WOOD, 1.0f);
    Fitted({-5.95f, F2, 1.7f}, {-5.3f, F2 + 2.1f, 3.0f}, T_DOOR, T_PLANKS, Color{150, 115, 85, 255});
    Solid({-2.7f, F2, 5.4f}, {-1.75f, F2 + 0.9f, 5.95f}, T_PLANKS, MID_WOOD, 1.0f);
    for (int i = 0; i < 5; ++i) {
        float z = 1.9f + 0.12f * static_cast<float>(i);
        Deco({-1.62f, F2 + 1.2f + 0.02f * static_cast<float>(i), z}, {-1.6f, F2 + 1.6f, z + 0.03f}, T_WHITE, Color{50, 30, 25, 255}, 0.0f);
    }
    Deco({-4.6f, F2 + 0.005f, 2.0f}, {-2.6f, F2 + 0.02f, 3.1f}, T_CARPET, Color{110, 60, 70, 255}, 0.0f);

    Solid({-1.4f, F2, -5.95f}, {-0.85f, F2 + 2.0f, -4.9f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({-1.4f, F2, 0.0f}, {-1.0f, F2 + 0.8f, 1.2f}, T_PLANKS, MID_WOOD, 1.0f);
    Deco({-0.9f, F2 + 0.005f, 1.5f}, {0.9f, F2 + 0.02f, 5.5f}, T_CARPET, Color{90, 60, 50, 255}, 0.0f);

    Solid({1.7f, F2, 1.2f}, {2.6f, F2 + 0.45f, 2.9f}, T_FABRIC, Color{120, 130, 150, 255}, 0.8f);
    Solid({1.7f, F2, 1.15f}, {2.6f, F2 + 0.8f, 1.25f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({3.0f, F2 + 0.72f, 5.35f}, {4.4f, F2 + 0.78f, 5.95f}, T_PLANKS, MID_WOOD, 1.0f);
    Solid({3.0f, F2, 5.35f}, {3.5f, F2 + 0.72f, 5.95f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({4.32f, F2, 5.4f}, {4.4f, F2 + 0.72f, 5.9f}, T_PLANKS, DARK_WOOD, 1.0f);
    Deco({3.7f, F2 + 0.78f, 5.5f}, {4.05f, F2 + 1.1f, 5.8f}, T_RADIO, WHITE, 0.0f);
    Solid({5.6f, F2, 1.3f}, {5.95f, F2 + 2.0f, 2.6f}, T_PLANKS, DARK_WOOD, 1.0f);
    for (int i = 0; i < 4; ++i) {
        float y = F2 + 0.4f + 0.45f * static_cast<float>(i);
        Deco({5.62f, y, 1.35f}, {5.9f, y + 0.32f, 2.55f}, T_FABRIC, Color{static_cast<unsigned char>(110 + i * 25), 70, static_cast<unsigned char>(60 + i * 20), 255}, 0.0f);
    }
    Solid({1.8f, F2, 5.35f}, {2.4f, F2 + 0.4f, 5.95f}, T_PLANKS, Color{190, 160, 110, 255}, 0.6f);
    Fitted({5.3f, F2, 4.1f}, {5.95f, F2 + 2.1f, 5.2f}, T_DOOR, T_PLANKS, Color{140, 105, 80, 255});

    Box({1.7f, F2, -5.9f}, {3.4f, F2 + 0.6f, -5.0f}, T_PORCELAIN, T_PORCELAIN, T_PORCELAIN, Color{230, 230, 222, 255}, 1.0f);
    Deco({1.8f, F2 + 0.6f, -5.8f}, {3.3f, F2 + 0.61f, -5.1f}, T_WHITE, Color{60, 70, 60, 255}, 0.0f);
    Solid({5.45f, F2, -1.0f}, {5.95f, F2 + 0.85f, -0.1f}, T_PLANKS, Color{220, 215, 205, 255}, 1.0f);
    Box({5.5f, F2 + 0.85f, -0.95f}, {5.95f, F2 + 0.95f, -0.15f}, T_PORCELAIN, T_PORCELAIN, T_PORCELAIN, Color{235, 235, 230, 255}, 1.0f);
    Solid({5.8f, F2 + 1.2f, -0.95f}, {5.95f, F2 + 2.0f, -0.15f}, T_PLANKS, Color{220, 215, 205, 255}, 1.0f);
    Deco({5.78f, F2 + 1.28f, -0.88f}, {5.8f, F2 + 1.92f, -0.22f}, "motel_mirror_glass", Color{170, 180, 185, 255}, 0.1f);
    Box({5.4f, F2, -3.5f}, {5.95f, F2 + 0.45f, -2.9f}, T_PORCELAIN, T_PORCELAIN, T_PORCELAIN, Color{235, 235, 230, 255}, 1.0f);
    Box({5.75f, F2 + 0.45f, -3.45f}, {5.95f, F2 + 0.95f, -2.95f}, T_PORCELAIN, T_PORCELAIN, T_PORCELAIN, Color{235, 235, 230, 255}, 1.0f);

    Solid({-4.0f, F2, -5.95f}, {-2.0f, F2 + 0.6f, -5.3f}, T_PLANKS, Color{160, 130, 90, 255}, 0.8f);
    Solid({-3.4f, F2 + 0.6f, -5.9f}, {-2.4f, F2 + 1.1f, -5.4f}, T_PLANKS, Color{180, 150, 100, 255}, 0.8f);
    Solid({-2.6f, F2, 0.3f}, {-1.65f, F2 + 0.7f, 1.3f}, T_PLANKS, Color{170, 140, 95, 255}, 0.8f);
    Railing({-4.84f, F3, -4.6f}, {-4.76f, F3, -0.4f});
    Railing({-6.0f, F3, -4.64f}, {-4.8f, F3, -4.56f});

    Lamp({0.0f, ceil - 0.4f, 3.0f}, {1.0f, 0.78f, 0.5f}, 6.0f, 0.7f);
    Lamp({-0.6f, ceil - 0.4f, -3.5f}, {1.0f, 0.74f, 0.45f}, 5.0f, 0.45f);
    Lamp({-3.8f, ceil - 0.4f, 3.6f}, {1.0f, 0.76f, 0.5f}, 6.0f, 0.7f);
    Lamp({3.8f, ceil - 0.4f, 3.5f}, {1.0f, 0.8f, 0.55f}, 6.0f, 0.75f);
    Lamp({3.8f, ceil - 0.4f, -2.5f}, {0.88f, 0.93f, 1.0f}, 5.5f, 0.7f);
    Lamp({-3.0f, ceil - 0.4f, -2.5f}, {1.0f, 0.7f, 0.4f}, 5.0f, 0.4f);
}

void HouseLevel::BuildAttic() {
    Slab(-4.8f, 6.0f, -6.0f, 6.0f, F3, SLAB, T_ATTIC, T_CEILING);
    Slab(-6.0f, -4.8f, -6.0f, -4.6f, F3, SLAB, T_ATTIC, T_CEILING);
    Slab(-6.0f, -4.8f, -0.12f, 6.0f, F3, SLAB, T_ATTIC, T_CEILING);

    for (float z = -5.4f; z < 6.0f; z += 1.6f) {
        Deco({-6.0f, ROOF - 0.45f, z}, {6.0f, ROOF - 0.25f, z + 0.16f}, T_PLANKS, DARK_WOOD, 0.0f);
    }
    Deco({-0.1f, ROOF - 0.7f, -6.0f}, {0.1f, ROOF - 0.45f, 6.0f}, T_PLANKS, DARK_WOOD, 0.0f);
    Solid({-0.1f, F3, -2.1f}, {0.1f, ROOF - 0.45f, -1.9f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({-0.1f, F3, 2.4f}, {0.1f, ROOF - 0.45f, 2.6f}, T_PLANKS, DARK_WOOD, 1.0f);

    Solid({1.8f, F3 + 0.7f, -5.95f}, {4.2f, F3 + 0.75f, -5.2f}, T_PLANKS, MID_WOOD, 1.0f);
    Solid({1.85f, F3, -5.9f}, {2.0f, F3 + 0.7f, -5.25f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({4.0f, F3, -5.9f}, {4.15f, F3 + 0.7f, -5.25f}, T_PLANKS, DARK_WOOD, 1.0f);
    Fitted({2.2f, F3 + 0.75f, -5.9f}, {3.8f, F3 + 1.4f, -5.4f}, T_RADIO, T_METAL, WHITE);
    Deco({2.95f, F3 + 1.4f, -5.7f}, {3.0f, ROOF, -5.65f}, T_METAL, Color{120, 120, 120, 255}, 0.0f);
    Deco({2.4f, F3 + 1.25f, -5.39f}, {2.5f, F3 + 1.32f, -5.38f}, T_WHITE, Color{255, 80, 40, 255}, 1.0f, "radio_led");
    Solid({4.4f, F3, -5.95f}, {5.2f, F3 + 0.8f, -5.3f}, T_PLANKS, DARK_WOOD, 1.0f);

    Solid({-3.6f, F3, 3.5f}, {-2.8f, F3 + 0.6f, 4.3f}, T_PLANKS, Color{150, 120, 80, 255}, 0.6f);
    Solid({2.0f, F3, 4.7f}, {3.2f, F3 + 0.55f, 5.4f}, T_PLANKS, Color{110, 70, 45, 255}, 0.6f);
    Solid({-5.6f, F3, 4.4f}, {-4.6f, F3 + 0.5f, 5.6f}, T_PLANKS, Color{150, 120, 80, 255}, 0.6f);
    Solid({-5.5f, F3 + 0.5f, 4.6f}, {-4.8f, F3 + 0.9f, 5.3f}, T_PLANKS, Color{140, 110, 75, 255}, 0.6f);
    Deco({4.6f, F3, 2.0f}, {5.8f, F3 + 1.8f, 2.06f}, T_FABRIC, Color{200, 195, 185, 255}, 0.0f);

    Lamp({3.0f, ROOF - 0.9f, -4.0f}, {1.0f, 0.75f, 0.45f}, 6.5f, 0.7f);
    Lamp({-2.0f, ROOF - 0.9f, 2.5f}, {1.0f, 0.7f, 0.4f}, 6.0f, 0.35f);
}

void HouseLevel::BuildGarage() {
    Slab(-13.0f, -6.2f, -6.0f, 3.5f, F1, 0.4f, T_CONCRETE, T_CONCRETE);
    WallX(-13.1f, -6.2f, 3.7f, F1, 3.0f, T_EXTERIOR, T_GARAGE, {});
    WallZ(-6.1f, -13.2f, -6.2f, F1, 3.0f, T_EXTERIOR, T_GARAGE, {{-11.0f, -8.0f, 2.6f}});
    WallZ(3.6f, -13.2f, -6.2f, F1, 3.0f, T_GARAGE, T_EXTERIOR, {});
    Box({-13.4f, 3.0f, -6.4f}, {-6.0f, 3.25f, 3.9f}, T_CONCRETE, T_ROOF, T_CONCRETE, WHITE, 2.0f);

    HouseBox gate;
    gate.min = {-11.0f, F1, -6.15f};
    gate.max = {-8.0f, 2.6f, -6.05f};
    gate.side = T_PLANKS;
    gate.top = T_PLANKS;
    gate.bottom = T_PLANKS;
    gate.tint = Color{140, 110, 80, 255};
    gate.uvScale = 1.2f;
    gate.tag = "gate";
    m_boxes.push_back(gate);
    Deco({-11.0f, 1.2f, -6.0f}, {-8.0f, 1.35f, -5.94f}, T_PLANKS, DARK_WOOD, 0.0f, "gate");

    Color body{196, 186, 150, 255};
    Box({-10.4f, 0.32f, -4.6f}, {-8.6f, 0.95f, -0.6f}, T_METAL, T_METAL, T_METAL, body, 1.2f);
    Box({-10.3f, 0.95f, -3.3f}, {-8.7f, 1.45f, -1.2f}, T_METAL, T_METAL, T_METAL, body, 1.2f);
    Deco({-10.32f, 1.0f, -3.25f}, {-8.68f, 1.4f, -1.25f}, T_WINDOW, Color{60, 70, 80, 255}, 0.05f);
    Deco({-10.25f, 1.0f, -3.36f}, {-8.75f, 1.38f, -3.3f}, T_WINDOW, Color{60, 70, 80, 255}, 0.05f);
    Deco({-10.25f, 0.55f, -4.63f}, {-9.95f, 0.75f, -4.6f}, T_WHITE, Color{230, 225, 190, 255}, 0.4f);
    Deco({-9.05f, 0.55f, -4.63f}, {-8.75f, 0.75f, -4.6f}, T_WHITE, Color{230, 225, 190, 255}, 0.4f);
    Deco({-10.3f, 0.45f, -0.6f}, {-10.0f, 0.62f, -0.57f}, T_WHITE, Color{160, 30, 25, 255}, 0.3f);
    Deco({-9.0f, 0.45f, -0.6f}, {-8.7f, 0.62f, -0.57f}, T_WHITE, Color{160, 30, 25, 255}, 0.3f);
    for (float wx : {-10.5f, -8.7f}) {
        for (float wz : {-4.1f, -1.4f}) {
            Box({wx, F1, wz - 0.3f}, {wx + 0.2f, 0.6f, wz + 0.3f}, T_METAL, T_METAL, T_METAL, Color{30, 30, 32, 255}, 1.0f);
        }
    }

    Solid({-12.95f, F1 + 0.85f, -2.2f}, {-12.3f, F1 + 0.9f, 1.2f}, T_PLANKS, MID_WOOD, 1.0f);
    Solid({-12.95f, F1, -2.2f}, {-12.3f, F1 + 0.85f, -2.05f}, T_PLANKS, DARK_WOOD, 1.0f);
    Solid({-12.95f, F1, 1.05f}, {-12.3f, F1 + 0.85f, 1.2f}, T_PLANKS, DARK_WOOD, 1.0f);
    Fitted({-13.0f, F1 + 0.9f, -2.2f}, {-12.95f, F1 + 2.0f, 1.2f}, T_WORKBENCH, T_PLANKS, Color{180, 170, 160, 255});
    Solid({-12.95f, F1, 1.6f}, {-12.3f, F1 + 1.6f, 2.8f}, T_METAL, Color{90, 110, 95, 255}, 1.0f);
    Solid({-7.4f, F1, -5.9f}, {-6.3f, F1 + 0.5f, -5.3f}, T_METAL, Color{150, 40, 30, 255}, 1.0f);
    Solid({-12.8f, F1, -5.8f}, {-12.1f, F1 + 0.9f, -5.1f}, T_METAL, Color{70, 90, 70, 255}, 0.8f);

    Lamp({-9.5f, 2.55f, -1.2f}, {0.85f, 0.92f, 1.0f}, 9.0f, 1.3f);
    Lamp({-7.6f, 2.55f, 2.2f}, {1.0f, 0.8f, 0.55f}, 5.5f, 0.8f);
    Lamp({-11.8f, 2.55f, 1.8f}, {1.0f, 0.78f, 0.5f}, 5.5f, 0.6f);
}

void HouseLevel::BuildExterior() {
    float g = -0.3f;
    auto ground = [&](float x0, float x1, float z0, float z1) {
        HouseBox b;
        b.min = {x0, g - 0.3f, z0};
        b.max = {x1, g, z1};
        b.side = T_GRASS;
        b.top = T_GRASS;
        b.bottom = T_GRASS;
        b.uvScale = 3.0f;
        b.cell = 6.0f;
        b.tint = Color{120, 135, 110, 255};
        m_boxes.push_back(b);
    };
    ground(-60.0f, 60.0f, 6.25f, 60.0f);
    ground(-60.0f, 60.0f, -60.0f, -6.25f);
    ground(-60.0f, -13.2f, -6.25f, 6.25f);
    ground(6.25f, 60.0f, -6.25f, 6.25f);
    ground(-13.2f, -6.25f, 3.7f, 6.25f);

    Box({-1.6f, g, 6.25f}, {1.6f, 0.0f, 7.6f}, T_PLANKS, T_PLANKS, T_PLANKS, MID_WOOD, 1.0f);
    Box({-1.6f, 0.0f, 7.4f}, {-1.45f, 2.6f, 7.55f}, T_PLANKS, T_PLANKS, T_PLANKS, DARK_WOOD, 1.0f);
    Box({1.45f, 0.0f, 7.4f}, {1.6f, 2.6f, 7.55f}, T_PLANKS, T_PLANKS, T_PLANKS, DARK_WOOD, 1.0f);
    Box({-1.8f, 2.6f, 6.2f}, {1.8f, 2.75f, 7.8f}, T_ROOF, T_ROOF, T_PLANKS, WHITE, 1.5f);
    HouseBox path;
    path.min = {-0.9f, g, 7.6f};
    path.max = {0.9f, g + 0.03f, 45.0f};
    path.side = T_GRAVEL;
    path.top = T_GRAVEL;
    path.bottom = T_GRAVEL;
    path.cell = 6.0f;
    path.solid = false;
    m_boxes.push_back(path);
    HouseBox drive = path;
    drive.min = {-11.0f, g, -45.0f};
    drive.max = {-8.0f, g + 0.03f, -6.25f};
    m_boxes.push_back(drive);

    Lamp({0.0f, 2.3f, 6.7f}, {1.0f, 0.78f, 0.5f}, 5.5f, 0.6f);

    unsigned int seed = 86u;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    m_trees.clear();
    for (int i = 0; i < 140; ++i) {
        float ang = rnd() * 2.0f * PI;
        float rad = 12.0f + rnd() * 38.0f;
        Vector3 p{std::cos(ang) * rad - 3.0f, g, std::sin(ang) * rad};
        if (std::abs(p.x) < 3.5f && p.z > 6.0f) continue;
        if (p.x > -13.0f && p.x < -6.0f && p.z < -6.0f) continue;
        if (p.x > -15.0f && p.x < 8.0f && p.z > -8.0f && p.z < 8.0f) continue;
        m_trees.push_back({p, 0.8f + rnd() * 0.6f});
    }
}

Texture2D HouseLevel::ResolveTexture(engine::core::Engine& eng, const std::string& name) {
    auto it = m_textures.find(name);
    if (it != m_textures.end()) return it->second;
    Texture2D tex{};
    if (name == T_WHITE) {
        tex.id = rlGetTextureIdDefault();
        tex.width = 1;
        tex.height = 1;
        tex.mipmaps = 1;
        tex.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    } else {
        std::string rel = "sprites/3d/" + name + ".png";
        if (FileExists((eng.Assets().GetBaseDir() + "/" + rel).c_str())) tex = eng.Assets().GetTexture(rel);
        if (tex.id == 0) tex = eng.Assets().GetTexture("sprites/3d/motel_concrete.png");
        if (tex.id != 0) {
            if (tex.mipmaps <= 1) GenTextureMipmaps(&tex);
            SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
            SetTextureFilter(tex, tex.mipmaps > 1 ? TEXTURE_FILTER_TRILINEAR : TEXTURE_FILTER_BILINEAR);
        }
    }
    m_textures[name] = tex;
    return tex;
}

bool HouseLevel::SegmentBlocked(Vector3 a, Vector3 b, const std::vector<int>& candidates) const {
    Vector3 d = Vector3Subtract(b, a);
    for (int idx : candidates) {
        if (SegmentHitsBox(a, d, m_boxes[idx])) return true;
    }
    return false;
}

void HouseLevel::GenerateMeshes(engine::core::Engine& eng, Shader lit) {
    std::vector<std::vector<int>> lampOccluders(m_lamps.size());
    for (size_t li = 0; li < m_lamps.size(); ++li) {
        const HouseLamp& l = m_lamps[li];
        Vector3 lmin = Vector3SubtractValue(l.position, l.radius);
        Vector3 lmax = Vector3AddValue(l.position, l.radius);
        for (size_t bi = 0; bi < m_boxes.size(); ++bi) {
            const HouseBox& b = m_boxes[bi];
            if (!b.occluder || b.side.empty()) continue;
            if (BoxesOverlap(lmin, lmax, b.min, b.max)) lampOccluders[li].push_back(static_cast<int>(bi));
        }
    }

    struct BatchKey {
        std::string tex;
        Color tint;
        float emissive;
        std::string tag;
    };
    std::map<std::string, std::pair<BatchKey, MeshBuilder>> builders;

    auto faceRank = [&](size_t bi, int a, int s, int u, int v) {
        const HouseBox& b = m_boxes[bi];
        float c = s > 0 ? Axis(b.max, a) : Axis(b.min, a);
        const float eps = 0.002f;
        int rank = 0;
        for (size_t oj = 0; oj < bi; ++oj) {
            const HouseBox& o = m_boxes[oj];
            if (!o.solid || o.side.empty()) continue;
            float same = s > 0 ? Axis(o.max, a) : Axis(o.min, a);
            if (std::abs(same - c) >= eps) continue;
            if (Axis(o.min, u) < Axis(b.max, u) - eps && Axis(o.max, u) > Axis(b.min, u) + eps &&
                Axis(o.min, v) < Axis(b.max, v) - eps && Axis(o.max, v) > Axis(b.min, v) + eps) {
                ++rank;
            }
        }
        return std::min(rank, 4);
    };

    auto faceHidden = [&](size_t bi, int a, int s, int u, int v) {
        const HouseBox& b = m_boxes[bi];
        float c = s > 0 ? Axis(b.max, a) : Axis(b.min, a);
        const float eps = 0.002f;
        for (size_t oj = 0; oj < m_boxes.size(); ++oj) {
            if (oj == bi) continue;
            const HouseBox& o = m_boxes[oj];
            if (!o.solid || o.side.empty()) continue;
            float opposite = s > 0 ? Axis(o.min, a) : Axis(o.max, a);
            float same = s > 0 ? Axis(o.max, a) : Axis(o.min, a);
            bool touching = std::abs(opposite - c) < eps;
            bool duplicate = oj < bi && std::abs(same - c) < eps;
            if (!touching && !duplicate) continue;
            if (Axis(o.min, u) <= Axis(b.min, u) + eps && Axis(o.max, u) >= Axis(b.max, u) - eps &&
                Axis(o.min, v) <= Axis(b.min, v) + eps && Axis(o.max, v) >= Axis(b.max, v) - eps) {
                return true;
            }
        }
        return false;
    };

    for (size_t bi = 0; bi < m_boxes.size(); ++bi) {
        const HouseBox& box = m_boxes[bi];
        if (box.side.empty()) continue;
        Vector3 size = Vector3Subtract(box.max, box.min);

        std::vector<int> aoCandidates;
        Vector3 emin = Vector3SubtractValue(box.min, 0.45f);
        Vector3 emax = Vector3AddValue(box.max, 0.45f);
        if (box.emissive <= 0.0f && box.cell < 2.0f) {
            for (size_t oi = 0; oi < m_boxes.size(); ++oi) {
                if (oi == bi) continue;
                const HouseBox& o = m_boxes[oi];
                if (!o.solid && !o.occluder) continue;
                if (BoxesOverlap(emin, emax, o.min, o.max)) aoCandidates.push_back(static_cast<int>(oi));
            }
        }

        for (int a = 0; a < 3; ++a) {
            for (int s = -1; s <= 1; s += 2) {
                const std::string& tex = (a == 1) ? (s > 0 ? box.top : box.bottom) : box.side;
                if (tex.empty()) continue;
                int u = (a + 1) % 3;
                int v = (a + 2) % 3;
                if (box.solid && box.emissive <= 0.0f && faceHidden(bi, a, s, u, v)) continue;
                float zOffset = box.solid ? 0.003f * static_cast<float>(faceRank(bi, a, s, u, v)) : 0.0f;
                float su = Axis(size, u);
                float sv = Axis(size, v);
                if (su < 0.001f || sv < 0.001f) continue;
                int nu = std::max(1, static_cast<int>(std::ceil(su / box.cell)));
                int nv = std::max(1, static_cast<int>(std::ceil(sv / box.cell)));
                Vector3 n{0.0f, 0.0f, 0.0f};
                SetAxis(n, a, static_cast<float>(s));

                std::string key = tex + "|" + ColorKey(box.tint) + "|" + std::to_string(box.emissive) + "|" + box.tag;
                auto& entry = builders[key];
                entry.first = BatchKey{tex, box.tint, box.emissive, box.tag};
                MeshBuilder& mb = entry.second;

                bool flip = (a == 0 && s > 0) || (a == 2 && s < 0);
                auto vertexAt = [&](int i, int j) {
                    Vector3 p{};
                    SetAxis(p, a, (s > 0 ? Axis(box.max, a) : Axis(box.min, a)) + static_cast<float>(s) * zOffset);
                    SetAxis(p, u, Axis(box.min, u) + su * static_cast<float>(i) / static_cast<float>(nu));
                    SetAxis(p, v, Axis(box.min, v) + sv * static_cast<float>(j) / static_cast<float>(nv));
                    return p;
                };
                auto uvAt = [&](Vector3 p) {
                    Vector2 uv{};
                    if (box.fitUV) {
                        if (a == 0) uv = {(p.z - box.min.z) / size.z, 1.0f - (p.y - box.min.y) / size.y};
                        else if (a == 1) uv = {(p.x - box.min.x) / size.x, (p.z - box.min.z) / size.z};
                        else uv = {(p.x - box.min.x) / size.x, 1.0f - (p.y - box.min.y) / size.y};
                        if (flip) uv.x = 1.0f - uv.x;
                    } else {
                        if (a == 0) uv = {p.z, -p.y};
                        else if (a == 1) uv = {p.x, p.z};
                        else uv = {p.x, -p.y};
                        if (flip) uv.x = -uv.x;
                        uv.x /= box.uvScale;
                        uv.y /= box.uvScale;
                    }
                    return uv;
                };
                auto lightAt = [&](Vector3 p, float& ao) {
                    Vector3 acc{0.0f, 0.0f, 0.0f};
                    ao = 1.0f;
                    if (box.emissive > 0.0f) return acc;
                    Vector3 sp = Vector3Add(p, Vector3Scale(n, 0.04f));
                    for (size_t li = 0; li < m_lamps.size(); ++li) {
                        const HouseLamp& l = m_lamps[li];
                        Vector3 d = Vector3Subtract(l.position, sp);
                        float dist = Vector3Length(d);
                        if (dist >= l.radius || dist < 0.0001f) continue;
                        float ndl = Vector3DotProduct(n, Vector3Scale(d, 1.0f / dist));
                        if (ndl <= -0.05f) continue;
                        float k = 1.0f - (dist * dist) / (l.radius * l.radius);
                        float att = k * k;
                        if (SegmentBlocked(sp, Vector3Subtract(l.position, Vector3Scale(d, 0.12f / dist)), lampOccluders[li])) continue;
                        float shade = (0.2f + 0.8f * std::max(ndl, 0.0f)) * att * l.intensity;
                        acc = Vector3Add(acc, Vector3Scale(l.color, shade));
                    }
                    if (!aoCandidates.empty()) {
                        int hits = 0;
                        for (int dir = 0; dir < 4; ++dir) {
                            Vector3 probe = Vector3Add(p, Vector3Scale(n, 0.06f));
                            int ax = dir < 2 ? u : v;
                            SetAxis(probe, ax, Axis(probe, ax) + ((dir % 2 == 0) ? 0.3f : -0.3f));
                            for (int oi : aoCandidates) {
                                if (PointInBox(probe, m_boxes[oi])) {
                                    hits++;
                                    break;
                                }
                            }
                        }
                        ao = std::clamp(1.0f - 0.2f * static_cast<float>(hits), 0.45f, 1.0f);
                    }
                    return acc;
                };

                std::vector<Vector3> gridPos((nu + 1) * (nv + 1));
                std::vector<Vector3> gridLight((nu + 1) * (nv + 1));
                std::vector<float> gridAo((nu + 1) * (nv + 1));
                for (int j = 0; j <= nv; ++j) {
                    for (int i = 0; i <= nu; ++i) {
                        int idx = j * (nu + 1) + i;
                        gridPos[idx] = vertexAt(i, j);
                        gridLight[idx] = lightAt(gridPos[idx], gridAo[idx]);
                    }
                }
                auto emit = [&](int i, int j) {
                    int idx = j * (nu + 1) + i;
                    mb.Add(gridPos[idx], n, uvAt(gridPos[idx]), gridLight[idx], gridAo[idx]);
                };
                for (int j = 0; j < nv; ++j) {
                    for (int i = 0; i < nu; ++i) {
                        if (s > 0) {
                            emit(i, j);
                            emit(i + 1, j);
                            emit(i + 1, j + 1);
                            emit(i, j);
                            emit(i + 1, j + 1);
                            emit(i, j + 1);
                        } else {
                            emit(i, j);
                            emit(i + 1, j + 1);
                            emit(i + 1, j);
                            emit(i, j);
                            emit(i, j + 1);
                            emit(i + 1, j + 1);
                        }
                    }
                }
            }
        }
    }

    for (auto& [key, entry] : builders) {
        if (entry.second.positions.empty()) continue;
        HouseBatch batch;
        batch.mesh = entry.second.Build();
        batch.material = LoadMaterialDefault();
        batch.material.shader = lit;
        batch.material.maps[MATERIAL_MAP_DIFFUSE].texture = ResolveTexture(eng, entry.first.tex);
        batch.material.maps[MATERIAL_MAP_DIFFUSE].color = entry.first.tint;
        batch.emissive = entry.first.emissive;
        batch.tag = entry.first.tag;
        batch.texture = entry.first.tex;
        m_batches.push_back(batch);
    }

    m_trunk = GenMeshCylinder(0.18f, 7.0f, 7);
    m_crown = GenMeshCone(1.7f, 3.6f, 8);
    m_treeMeshes = true;
    m_barkMat = LoadMaterialDefault();
    m_barkMat.shader = lit;
    m_barkMat.maps[MATERIAL_MAP_DIFFUSE].texture = ResolveTexture(eng, "motel_bark");
    m_pineMat = LoadMaterialDefault();
    m_pineMat.shader = lit;
    m_pineMat.maps[MATERIAL_MAP_DIFFUSE].texture = ResolveTexture(eng, "motel_pine");
    m_pineMat.maps[MATERIAL_MAP_DIFFUSE].color = Color{150, 175, 150, 255};
}

void HouseLevel::Build(engine::core::Engine& eng, engine::physics3d::Physics3D& physics, Shader lit) {
    Unload();
    BuildShell();
    BuildBasement();
    BuildFirstFloor();
    BuildSecondFloor();
    BuildAttic();
    BuildGarage();
    BuildExterior();

    m_playerStart = {-3.0f, F2 + 0.05f, 3.2f};
    m_playerYaw = 0.81f;

    for (const HouseBox& b : m_boxes) {
        if (!b.solid) continue;
        std::vector<Vector3> pts;
        for (int i = 0; i < 8; ++i) {
            pts.push_back({(i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y, (i & 4) ? b.max.z : b.min.z});
        }
        physics.AddStaticHull(pts);
    }
    physics.FinalizeStatic();

    GenerateMeshes(eng, lit);
}

void HouseLevel::Unload() {
    for (HouseBatch& b : m_batches) {
        UnloadMesh(b.mesh);
        if (b.material.maps != nullptr) MemFree(b.material.maps);
    }
    m_batches.clear();
    if (m_treeMeshes) {
        UnloadMesh(m_trunk);
        UnloadMesh(m_crown);
        if (m_barkMat.maps != nullptr) MemFree(m_barkMat.maps);
        if (m_pineMat.maps != nullptr) MemFree(m_pineMat.maps);
        m_barkMat = Material{};
        m_pineMat = Material{};
        m_treeMeshes = false;
    }
    m_boxes.clear();
    m_lamps.clear();
    m_trees.clear();
    m_textures.clear();
}

bool HouseLevel::IsConcreteFloor(Vector3 feet) const {
    return feet.y < -1.5f || feet.x < -6.0f;
}

}
