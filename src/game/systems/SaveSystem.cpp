#include "game/systems/SaveSystem.h"
#include "engine/core/PathUtils.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace game::systems {

bool SaveSystem::Save(const SaveProfile& profile) {
    try {
        std::string path = engine::core::GetUserSavePath();
        nlohmann::json j;
        j["currentNight"] = profile.currentNight;
        j["hour"] = profile.hour;
        j["minute"] = profile.minute;
        j["elapsedMinutes"] = profile.elapsedMinutes;
        j["sanity"] = profile.sanity;
        j["heldItem"] = profile.heldItem;
        j["heldItemName"] = profile.heldItemName;
        j["playerX"] = profile.playerX;
        j["playerY"] = profile.playerY;
        j["playerFacing"] = profile.playerFacing;
        j["maxUnlockedNight"] = profile.maxUnlockedNight;
        j["flags"] = profile.flags;

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveSystem::Load(SaveProfile& outProfile) {
    try {
        std::string path = engine::core::GetUserSavePath();
        if (!std::filesystem::exists(path)) return false;
        std::ifstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j;
        file >> j;

        outProfile.currentNight = j.value("currentNight", 1);
        outProfile.hour = j.value("hour", 22);
        outProfile.minute = j.value("minute", 0);
        outProfile.elapsedMinutes = j.value("elapsedMinutes", 0.0f);
        outProfile.sanity = j.value("sanity", 100);
        outProfile.heldItem = j.value("heldItem", "");
        outProfile.heldItemName = j.value("heldItemName", "");
        outProfile.playerX = j.value("playerX", 112.0f);
        outProfile.playerY = j.value("playerY", 46.0f);
        outProfile.playerFacing = j.value("playerFacing", 0);
        outProfile.maxUnlockedNight = j.value("maxUnlockedNight", 1);
        outProfile.flags = j.value("flags", std::vector<std::string>{});
        outProfile.valid = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveSystem::HasSave() {
    SaveProfile sp;
    return Load(sp) && sp.valid;
}

SaveProfile SaveSystem::GetQuickInfo() {
    SaveProfile sp;
    Load(sp);
    return sp;
}

void SaveSystem::DeleteSave() {
    std::error_code ec;
    std::filesystem::remove(engine::core::GetUserSavePath(), ec);
}

}
