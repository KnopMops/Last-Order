#pragma once
#include "engine/core/PathUtils.h"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>

namespace engine::hd {

inline std::filesystem::path ShaderCacheDir() {
    std::filesystem::path dir = engine::core::GetUserStorageDir() / "shader_cache";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

inline std::string ShaderCacheFile(const std::string& key) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ull;
    }
    uint64_t h2 = 14695981039346656037ull ^ key.size();
    for (size_t i = 0; i < key.size(); i += 3) {
        h2 ^= static_cast<unsigned char>(key[i]);
        h2 *= 1099511628211ull;
    }
    char name[64];
    std::snprintf(name, sizeof(name), "%016llx%016llx.bin", static_cast<unsigned long long>(h), static_cast<unsigned long long>(h2));
    return (ShaderCacheDir() / name).string();
}

inline bool ShaderCacheLoad(const std::string& key, std::vector<uint8_t>& out) {
    std::ifstream f(ShaderCacheFile(key), std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !out.empty();
}

inline void ShaderCacheStore(const std::string& key, const std::vector<uint8_t>& data) {
    std::ofstream f(ShaderCacheFile(key), std::ios::binary);
    if (f) f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

}
