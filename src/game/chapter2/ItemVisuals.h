#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace game::chapter2 {

struct ItemPart {
    Mesh mesh{};
    Material material{};
    Matrix local{};
};

struct ItemVisual {
    std::vector<ItemPart> parts;
    float inspectDistance = 0.32f;
};

class ItemVisualLibrary {
public:
    void Init(Shader shader, Texture2D keyTagTexture);
    void Shutdown();
    const ItemVisual& Get(const std::string& model, Color tint);
    void Draw(const ItemVisual& visual, Matrix world, const Shader* overrideShader = nullptr) const;

private:
    ItemVisual Build(const std::string& model, Color tint);
    void AddPart(ItemVisual& visual, Mesh mesh, Color color, Matrix local, Texture2D texture);

    Shader m_shader{};
    Texture2D m_keyTag{};
    std::unordered_map<std::string, ItemVisual> m_cache;
};

}
