#include "engine/assets/AssetManager.h"
#include <vector>

namespace engine::assets {

void AssetManager::Init(const std::string& baseAssetDir) {
    m_baseDir = baseAssetDir;

    std::vector<int> codepoints;
    for (int cp = 0x0020; cp <= 0x007E; ++cp) codepoints.push_back(cp);
    for (int cp = 0x00A0; cp <= 0x00FF; ++cp) codepoints.push_back(cp);
    for (int cp = 0x0400; cp <= 0x04FF; ++cp) codepoints.push_back(cp);
    for (int cp = 0x2010; cp <= 0x203A; ++cp) codepoints.push_back(cp);
    for (int cp = 0x2190; cp <= 0x2199; ++cp) codepoints.push_back(cp);
    for (int cp = 0x25A0; cp <= 0x25FF; ++cp) codepoints.push_back(cp);

    const int extras[] = {0x2116, 0x2713, 0x2714, 0x2715, 0x2716};
    for (int cp : extras) codepoints.push_back(cp);

    std::string fontPath = m_baseDir + "/fonts/font_main.ttf";
    m_mainFont = LoadFontEx(fontPath.c_str(), 26, codepoints.data(), static_cast<int>(codepoints.size()));
    if (m_mainFont.texture.id != 0) {
        SetTextureFilter(m_mainFont.texture, TEXTURE_FILTER_BILINEAR);
    } else {
        m_mainFont = GetFontDefault();
    }
}

void AssetManager::Shutdown() {
    for (auto& [k, snd] : m_sounds) {
        if (snd.stream.buffer != nullptr) UnloadSound(snd);
    }
    m_sounds.clear();

    for (auto& [k, tex] : m_textures) {
        if (tex.id != 0) UnloadTexture(tex);
    }
    m_textures.clear();

    if (m_mainFont.texture.id != 0 && m_mainFont.texture.id != GetFontDefault().texture.id) {
        UnloadFont(m_mainFont);
    }
}

Texture2D AssetManager::GetTexture(const std::string& relativePath) {
    auto it = m_textures.find(relativePath);
    if (it != m_textures.end()) return it->second;

    std::string fullPath = m_baseDir + "/" + relativePath;
    Texture2D tex = LoadTexture(fullPath.c_str());
    if (tex.id != 0) {
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    }
    m_textures[relativePath] = tex;
    return tex;
}

Sound AssetManager::GetSound(const std::string& relativePath) {
    auto it = m_sounds.find(relativePath);
    if (it != m_sounds.end()) return it->second;

    std::string fullPath = m_baseDir + "/" + relativePath;
    Sound snd = LoadSound(fullPath.c_str());
    m_sounds[relativePath] = snd;
    return snd;
}

}
