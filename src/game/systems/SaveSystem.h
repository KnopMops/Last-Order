#pragma once
#include <string>
#include <vector>

namespace game::systems {

struct SaveProfile {
    int chapter = 1;
    int currentNight = 1;
    int hour = 22;
    int minute = 0;
    float elapsedMinutes = 0.0f;
    int sanity = 100;
    std::string heldItem;
    std::string heldItemName;
    float playerX = 112.0f;
    float playerY = 48.0f;
    int playerFacing = 0;
    int maxUnlockedNight = 1;
    std::vector<std::string> flags;
    std::string slotTitle = "Сохранение";
    std::string timestamp;
    bool valid = false;
};

class SaveSystem {
public:
    static constexpr int NUM_SLOTS = 3;

    static bool SaveSlot(int slotIndex, const SaveProfile& profile);
    static bool LoadSlot(int slotIndex, SaveProfile& outProfile);
    static bool HasSlot(int slotIndex);
    static SaveProfile GetSlotInfo(int slotIndex);
    static void DeleteSlot(int slotIndex);

    static bool Save(const SaveProfile& profile);
    static bool Load(SaveProfile& outProfile);
    static bool HasSave();
    static SaveProfile GetQuickInfo();
    static void DeleteSave();

    static int GetMaxUnlockedNight();
    static void UpdateMaxUnlockedNight(int night);

private:
    static std::string GetSlotPath(int slotIndex);
};

}
