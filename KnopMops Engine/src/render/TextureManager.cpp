#include "render/TextureManager.h"
#include "core/Log.h"
#include <filesystem>

namespace knopmops::render {

namespace fs = std::filesystem;

void TextureManager::Init(const std::string& assetsPath) {
    m_assetsPath = assetsPath;
    GenerateDefaultTextures();

    if (!fs::exists(m_assetsPath)) {
        core::Log::Warn("Texture directory not found: " + m_assetsPath);
        return;
    }

    for (const auto& entry : fs::directory_iterator(m_assetsPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            std::string stem = entry.path().stem().string();
            std::string fullPath = entry.path().string();

            Texture2D tex = LoadTexture(fullPath.c_str());
            if (tex.id != 0) {
                SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
                SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
                m_cache[stem] = tex;
                m_items.push_back(TextureItem{stem, fullPath, tex});
            }
        }
    }

    core::Log::Info("Loaded " + std::to_string(m_items.size()) + " textures from " + m_assetsPath);
}

void TextureManager::Shutdown() {
    for (auto& item : m_items) {
        if (item.texture.id != 0) UnloadTexture(item.texture);
    }
    m_items.clear();
    m_cache.clear();

    if (m_defaultGrid.id != 0) UnloadTexture(m_defaultGrid);
    if (m_missingTexture.id != 0) UnloadTexture(m_missingTexture);
}

Texture2D TextureManager::GetTexture(const std::string& name) {
    if (name.empty()) return m_defaultGrid;
    auto it = m_cache.find(name);
    if (it != m_cache.end()) return it->second;

    std::string path = m_assetsPath + "/" + name + ".png";
    if (FileExists(path.c_str())) {
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id != 0) {
            SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            m_cache[name] = tex;
            m_items.push_back(TextureItem{name, path, tex});
            return tex;
        }
    }

    return m_missingTexture.id != 0 ? m_missingTexture : m_defaultGrid;
}

void TextureManager::GenerateDefaultTextures() {
    Image imgGrid = GenImageChecked(128, 128, 32, 32, Color{60, 65, 75, 255}, Color{85, 90, 105, 255});
    m_defaultGrid = LoadTextureFromImage(imgGrid);
    UnloadImage(imgGrid);
    SetTextureWrap(m_defaultGrid, TEXTURE_WRAP_REPEAT);

    Image imgMissing = GenImageChecked(64, 64, 32, 32, Color{220, 40, 220, 255}, Color{20, 20, 20, 255});
    m_missingTexture = LoadTextureFromImage(imgMissing);
    UnloadImage(imgMissing);
    SetTextureWrap(m_missingTexture, TEXTURE_WRAP_REPEAT);
}

}

