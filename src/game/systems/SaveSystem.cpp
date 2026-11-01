#include "game/systems/SaveSystem.h"
#include "engine/core/PathUtils.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace game::systems {

static std::string GetCurrentTimestampStr() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%d.%m.%Y %H:%M");
    return ss.str();
}

std::string SaveSystem::GetSlotPath(int slotIndex) {
    std::string basePath = engine::core::GetUserSavePath();
    std::filesystem::path dir = std::filesystem::path(basePath).parent_path();
    if (!std::filesystem::exists(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }
    if (slotIndex == 0) return (dir / "save_slot_0_quick.json").string();
    if (slotIndex == 1) return (dir / "save_slot_1_shift.json").string();
    return (dir / "save_slot_2_key.json").string();
}

bool SaveSystem::SaveSlot(int slotIndex, const SaveProfile& profile) {
    try {
        std::string path = GetSlotPath(slotIndex);
        nlohmann::json j;
        j["chapter"] = profile.chapter;
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
        j["maxUnlockedNight"] = std::max(profile.maxUnlockedNight, GetMaxUnlockedNight());
        j["flags"] = profile.flags;
        j["slotTitle"] = profile.slotTitle.empty() ? ("Слот " + std::to_string(slotIndex + 1)) : profile.slotTitle;
        j["timestamp"] = profile.timestamp.empty() ? GetCurrentTimestampStr() : profile.timestamp;

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);

        if (slotIndex == 0) {
            std::ofstream fDef(engine::core::GetUserSavePath());
            if (fDef.is_open()) fDef << j.dump(2);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveSystem::LoadSlot(int slotIndex, SaveProfile& outProfile) {
    try {
        std::string path = GetSlotPath(slotIndex);
        if (!std::filesystem::exists(path)) {
            if (slotIndex == 0) {
                path = engine::core::GetUserSavePath();
                if (!std::filesystem::exists(path)) return false;
            } else {
                return false;
            }
        }
        std::ifstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j;
        file >> j;

        outProfile.chapter = j.value("chapter", 1);
        outProfile.currentNight = j.value("currentNight", 1);
        outProfile.hour = j.value("hour", 22);
        outProfile.minute = j.value("minute", 0);
        outProfile.elapsedMinutes = j.value("elapsedMinutes", 0.0f);
        outProfile.sanity = j.value("sanity", 100);
        outProfile.heldItem = j.value("heldItem", "");
        outProfile.heldItemName = j.value("heldItemName", "");
        outProfile.playerX = j.value("playerX", 112.0f);
        outProfile.playerY = j.value("playerY", 48.0f);
        outProfile.playerFacing = j.value("playerFacing", 0);
        outProfile.maxUnlockedNight = j.value("maxUnlockedNight", 1);
        outProfile.flags = j.value("flags", std::vector<std::string>{});
        outProfile.slotTitle = j.value("slotTitle", "Слот " + std::to_string(slotIndex + 1));
        outProfile.timestamp = j.value("timestamp", "");
        outProfile.valid = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveSystem::HasSlot(int slotIndex) {
    SaveProfile sp;
    return LoadSlot(slotIndex, sp) && sp.valid;
}

SaveProfile SaveSystem::GetSlotInfo(int slotIndex) {
    SaveProfile sp;
    LoadSlot(slotIndex, sp);
    return sp;
}

void SaveSystem::DeleteSlot(int slotIndex) {
    std::error_code ec;
    std::filesystem::remove(GetSlotPath(slotIndex), ec);
    if (slotIndex == 0) {
        std::filesystem::remove(engine::core::GetUserSavePath(), ec);
    }
}

bool SaveSystem::Save(const SaveProfile& profile) {
    return SaveSlot(0, profile);
}

bool SaveSystem::Load(SaveProfile& outProfile) {
    if (LoadSlot(0, outProfile) && outProfile.valid) return true;
    if (LoadSlot(1, outProfile) && outProfile.valid) return true;
    if (LoadSlot(2, outProfile) && outProfile.valid) return true;
    return false;
}

bool SaveSystem::HasSave() {
    return HasSlot(0) || HasSlot(1) || HasSlot(2);
}

SaveProfile SaveSystem::GetQuickInfo() {
    SaveProfile sp;
    if (LoadSlot(0, sp) && sp.valid) return sp;
    if (LoadSlot(1, sp) && sp.valid) return sp;
    if (LoadSlot(2, sp) && sp.valid) return sp;
    return sp;
}

void SaveSystem::DeleteSave() {
    DeleteSlot(0);
    DeleteSlot(1);
    DeleteSlot(2);
}

int SaveSystem::GetMaxUnlockedNight() {
    int maxN = 1;
    for (int i = 0; i < NUM_SLOTS; ++i) {
        SaveProfile sp = GetSlotInfo(i);
        if (sp.valid) {
            maxN = std::max(maxN, sp.maxUnlockedNight);
            maxN = std::max(maxN, sp.currentNight);
        }
    }
    return maxN;
}

void SaveSystem::UpdateMaxUnlockedNight(int night) {
    if (night < 1) return;
    for (int i = 0; i < NUM_SLOTS; ++i) {
        SaveProfile sp;
        if (LoadSlot(i, sp) && sp.valid) {
            if (night > sp.maxUnlockedNight) {
                sp.maxUnlockedNight = night;
                SaveSlot(i, sp);
            }
        }
    }
}

}
