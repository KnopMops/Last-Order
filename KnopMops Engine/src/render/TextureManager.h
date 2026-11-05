#pragma once

#include "raylib.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace knopmops::render {

struct TextureItem {
    std::string name;
    std::string filePath;
    Texture2D texture{};
};

class TextureManager {
public:
    static TextureManager& Instance() {
        static TextureManager s_instance;
        return s_instance;
    }

    void Init(const std::string& assetsPath = "assets/textures");
    void Shutdown();
    // Дополнительная папка с текстурами (например, textures/ внутри проекта главы).
    void AddDirectory(const std::string& dir);

    Texture2D GetTexture(const std::string& name);
    const std::vector<TextureItem>& GetAllTextures() const { return m_items; }
    // Путь к исходному PNG (для экспорта главы) или пусто.
    std::string FindFile(const std::string& name) const;

    Texture2D GetDefaultGridTexture() const { return m_defaultGrid; }

private:
    std::string m_assetsPath;
    std::vector<TextureItem> m_items;
    std::unordered_map<std::string, Texture2D> m_cache;
    Texture2D m_defaultGrid{};
    Texture2D m_missingTexture{};

    void GenerateDefaultTextures();
};

}
