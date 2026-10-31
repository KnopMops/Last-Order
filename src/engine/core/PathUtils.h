#pragma once
#include <cstdlib>
#include <filesystem>
#include <string>

namespace engine::core {

inline std::filesystem::path GetUserStorageDir() {
    std::error_code ec;
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0] != '\0') {
        std::filesystem::path p = std::filesystem::path(appdata) / "LastOrder";
        std::filesystem::create_directories(p, ec);
        if (!ec) return p;
    }

    return std::filesystem::current_path(ec);
}

inline std::string GetUserSettingsPath() {
    return (GetUserStorageDir() / "settings.json").string();
}

inline std::string GetUserSavePath() {
    return (GetUserStorageDir() / "save_profile.json").string();
}

}
