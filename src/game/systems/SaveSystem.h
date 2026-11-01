#pragma once
#include <string>
#include <vector>

namespace game::systems {

struct SaveProfile {
    int currentNight = 1;
    int hour = 22;
    int minute = 0;
    float elapsedMinutes = 0.0f;
    int sanity = 100;
    std::string heldItem;
    std::string heldItemName;
    float playerX = 112.0f;
    float playerY = 46.0f;
    int playerFacing = 0;
    int maxUnlockedNight = 1;
    std::vector<std::string> flags;
    bool valid = false;
};

class SaveSystem {
public:
    static bool Save(const SaveProfile& profile);
    static bool Load(SaveProfile& outProfile);
    static bool HasSave();
    static SaveProfile GetQuickInfo();
    static void DeleteSave();
};

}
