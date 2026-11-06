#include "render/TextureManager.h"
#include "core/Log.h"
#include <filesystem>

namespace knopmops::render {

namespace fs = std::filesystem;

void TextureManager::Init(const std::string& assetsPath) {
    m_assetsPath = assetsPath;
    GenerateDefaultTextures();
    AddDirectory(assetsPath);
}

void TextureManager::AddDirectory(const std::string& dir) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        core::Log::Warn("Texture directory not found: " + dir);
        return;
    }

    size_t before = m_items.size();
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file() || entry.path().extension() != ".png") continue;
        std::string stem = entry.path().stem().string();
        if (m_cache.count(stem)) continue;
        std::string fullPath = entry.path().string();

        Texture2D tex = LoadTexture(fullPath.c_str());
        if (tex.id != 0) {
            SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            m_cache[stem] = tex;
            m_items.push_back(TextureItem{stem, fullPath, tex});
        }
    }
    core::Log::Info("Loaded " + std::to_string(m_items.size() - before) + " textures from " + dir);
}

void TextureManager::Shutdown() {
    for (auto& item : m_items) {
        if (item.texture.id != 0) UnloadTexture(item.texture);
    }
    m_items.clear();
    m_cache.clear();

    if (m_defaultGrid.id != 0) UnloadTexture(m_defaultGrid);
    if (m_missingTexture.id != 0) UnloadTexture(m_missingTexture);
    m_defaultGrid = Texture2D{};
    m_missingTexture = Texture2D{};
}

Texture2D TextureManager::GetTexture(const std::string& name) {
    if (name.empty()) return m_defaultGrid;
    auto it = m_cache.find(name);
    if (it != m_cache.end()) return it->second;
    return m_missingTexture.id != 0 ? m_missingTexture : m_defaultGrid;
}

std::string TextureManager::FindFile(const std::string& name) const {
    for (const auto& item : m_items) {
        if (item.name == name) return item.filePath;
    }
    return "";
}

void TextureManager::GenerateDefaultTextures() {
    if (m_defaultGrid.id != 0) return;
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
