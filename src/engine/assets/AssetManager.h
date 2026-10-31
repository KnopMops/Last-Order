#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>

namespace engine::assets {

class AssetManager {
public:
    void Init(const std::string& baseAssetDir);
    void Shutdown();

    Texture2D GetTexture(const std::string& relativePath);
    Sound GetSound(const std::string& relativePath);
    Font GetMainFont() const { return m_mainFont; }

    const std::string& GetBaseDir() const { return m_baseDir; }

private:
    std::string m_baseDir;
    std::unordered_map<std::string, Texture2D> m_textures;
    std::unordered_map<std::string, Sound> m_sounds;
    Font m_mainFont{};
};

}
