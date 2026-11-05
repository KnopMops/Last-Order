#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace game::chapter3 {

enum class ItemCategory {
    Tool,
    Key,
    WeaponPart,
    CarPart,
    Consumable,
    Clue
};

struct ItemDef {
    std::string id;
    std::string name;
    std::string description;
    ItemCategory category = ItemCategory::Tool;
    std::string iconPath;
};

struct SearchSlot {
    std::string id;
    std::string name;
    std::string zone;
    Vector3 position{};
    std::string containedItem;
    bool searched = false;
    bool locked = false;
    std::string requiredTool;
    bool open = false;
};

class ItemRandomizer {
public:
    void Init();
    void Generate(unsigned int seed);
    const SearchSlot* GetSlot(const std::string& slotId) const;
    SearchSlot* GetSlotMut(const std::string& slotId);
    const std::vector<SearchSlot>& GetAllSlots() const { return m_slots; }
    std::vector<SearchSlot>& GetAllSlotsMut() { return m_slots; }
    const ItemDef* GetItemDef(const std::string& itemId) const;
    std::string GetDoorKeypadCode() const { return m_keypadCode; }

private:
    std::vector<SearchSlot> m_slots;
    std::unordered_map<std::string, ItemDef> m_registry;
    std::string m_keypadCode = "7492";

    void RegisterItem(const std::string& id, const std::string& name, const std::string& desc, ItemCategory cat, const std::string& icon);
    void SetupSlots();
};

}

