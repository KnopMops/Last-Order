#include "game/chapter2/ItemVisuals.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>

namespace game::chapter2 {

namespace {

Texture2D WhiteTexture() {
    Texture2D t{};
    t.id = rlGetTextureIdDefault();
    t.width = 1;
    t.height = 1;
    t.mipmaps = 1;
    t.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return t;
}

Color Scale(Color c, float k) {
    auto s = [k](unsigned char v) { return static_cast<unsigned char>(std::min(255.0f, static_cast<float>(v) * k)); };
    return Color{s(c.r), s(c.g), s(c.b), 255};
}

}

void ItemVisualLibrary::Init(Shader shader, Texture2D keyTagTexture) {
    Shutdown();
    m_shader = shader;
    m_keyTag = keyTagTexture;
}

void ItemVisualLibrary::Shutdown() {
    for (auto& [key, visual] : m_cache) {
        for (ItemPart& p : visual.parts) {
            UnloadMesh(p.mesh);
            if (p.material.maps != nullptr) MemFree(p.material.maps);
        }
    }
    m_cache.clear();
}

void ItemVisualLibrary::AddPart(ItemVisual& visual, Mesh mesh, Color color, Matrix local, Texture2D texture) {
    ItemPart part;
    part.mesh = mesh;
    part.material = LoadMaterialDefault();
    part.material.shader = m_shader;
    part.material.maps[MATERIAL_MAP_DIFFUSE].texture = texture.id != 0 ? texture : WhiteTexture();
    part.material.maps[MATERIAL_MAP_DIFFUSE].color = color;
    part.local = local;
    visual.parts.push_back(part);
}

const ItemVisual& ItemVisualLibrary::Get(const std::string& model, Color tint) {
    std::string key = model + "#" + std::to_string(tint.r) + "," + std::to_string(tint.g) + "," + std::to_string(tint.b);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) return it->second;
    auto inserted = m_cache.emplace(key, Build(model, tint));
    return inserted.first->second;
}

ItemVisual ItemVisualLibrary::Build(const std::string& model, Color tint) {
    ItemVisual v;
    Texture2D white = WhiteTexture();
    Color brass{196, 160, 70, 255};
    Color steel{150, 156, 162, 255};

    if (model == "key") {
        AddPart(v, GenMeshCube(0.05f, 0.068f, 0.007f), tint, MatrixTranslate(0.0f, -0.045f, 0.0f), m_keyTag);
        AddPart(v, GenMeshTorus(0.25f, 0.024f, 6, 10), brass, MatrixMultiply(MatrixRotateX(PI * 0.5f), MatrixTranslate(0.0f, 0.004f, 0.0f)), white);
        AddPart(v, GenMeshCube(0.009f, 0.065f, 0.004f), brass, MatrixTranslate(0.0f, 0.046f, 0.0f), white);
        AddPart(v, GenMeshCube(0.008f, 0.006f, 0.004f), brass, MatrixTranslate(0.008f, 0.06f, 0.0f), white);
        AddPart(v, GenMeshCube(0.006f, 0.006f, 0.004f), brass, MatrixTranslate(0.007f, 0.07f, 0.0f), white);
        v.inspectDistance = 0.26f;
    } else if (model == "fuse") {
        AddPart(v, GenMeshCylinder(0.011f, 0.05f, 10), Scale(tint, 0.95f), MatrixTranslate(0.0f, -0.025f, 0.0f), white);
        AddPart(v, GenMeshCylinder(0.0125f, 0.012f, 10), steel, MatrixTranslate(0.0f, -0.034f, 0.0f), white);
        AddPart(v, GenMeshCylinder(0.0125f, 0.012f, 10), steel, MatrixTranslate(0.0f, 0.022f, 0.0f), white);
        AddPart(v, GenMeshCube(0.004f, 0.03f, 0.024f), Color{210, 60, 40, 255}, MatrixTranslate(0.0f, -0.003f, 0.0f), white);
        v.inspectDistance = 0.2f;
    } else if (model == "battery") {
        AddPart(v, GenMeshCylinder(0.017f, 0.058f, 12), tint, MatrixTranslate(0.0f, -0.029f, 0.0f), white);
        AddPart(v, GenMeshCylinder(0.0172f, 0.008f, 12), Color{30, 30, 34, 255}, MatrixTranslate(0.0f, 0.022f, 0.0f), white);
        AddPart(v, GenMeshCylinder(0.006f, 0.005f, 8), steel, MatrixTranslate(0.0f, 0.029f, 0.0f), white);
        v.inspectDistance = 0.22f;
    } else if (model == "cassette") {
        AddPart(v, GenMeshCube(0.1f, 0.064f, 0.012f), tint, MatrixIdentity(), white);
        AddPart(v, GenMeshCube(0.07f, 0.024f, 0.013f), Color{220, 200, 150, 255}, MatrixTranslate(0.0f, 0.012f, 0.0f), white);
        AddPart(v, GenMeshCylinder(0.008f, 0.014f, 8), Color{240, 240, 240, 255},
                MatrixMultiply(MatrixTranslate(0.0f, -0.007f, 0.0f), MatrixMultiply(MatrixRotateX(PI * 0.5f), MatrixTranslate(-0.02f, -0.008f, 0.0f))), white);
        AddPart(v, GenMeshCylinder(0.008f, 0.014f, 8), Color{240, 240, 240, 255},
                MatrixMultiply(MatrixTranslate(0.0f, -0.007f, 0.0f), MatrixMultiply(MatrixRotateX(PI * 0.5f), MatrixTranslate(0.02f, -0.008f, 0.0f))), white);
        v.inspectDistance = 0.3f;
    } else {
        AddPart(v, GenMeshCube(0.06f, 0.06f, 0.06f), tint, MatrixIdentity(), white);
    }
    return v;
}

void ItemVisualLibrary::Draw(const ItemVisual& visual, Matrix world, const Shader* overrideShader) const {
    for (const ItemPart& p : visual.parts) {
        Material m = p.material;
        if (overrideShader != nullptr) m.shader = *overrideShader;
        DrawMesh(p.mesh, m, MatrixMultiply(p.local, world));
    }
}

}
