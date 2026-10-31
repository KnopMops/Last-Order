#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace engine::core {

struct EngineAction {
    std::string type;
    std::string target;
    int intValue = 0;
};

class StateStore {
public:
    void Reset() {
        m_flags.clear();
        m_counters.clear();
        m_items.clear();
        m_heldItem.clear();
        m_heldItemName.clear();
        m_sanity = 100;
    }

    void SetFlag(const std::string& flag) { m_flags.insert(flag); }
    void ClearFlag(const std::string& flag) { m_flags.erase(flag); }
    bool HasFlag(const std::string& flag) const { return m_flags.find(flag) != m_flags.end(); }

    void SetCounter(const std::string& key, int val) { m_counters[key] = val; }
    void AddCounter(const std::string& key, int delta) { m_counters[key] += delta; }
    int GetCounter(const std::string& key) const {
        auto it = m_counters.find(key);
        return (it != m_counters.end()) ? it->second : 0;
    }

    void GiveItem(const std::string& item, const std::string& displayName) {
        m_items[item] = displayName;
        m_heldItem = item;
        m_heldItemName = displayName;
    }

    void ClearHeldItem() {
        m_heldItem.clear();
        m_heldItemName.clear();
    }

    bool HasItem(const std::string& item) const { return m_items.find(item) != m_items.end(); }
    const std::string& GetHeldItem() const { return m_heldItem; }
    const std::string& GetHeldItemName() const { return m_heldItemName; }

    int GetSanity() const { return m_sanity; }
    void SetSanity(int val) { m_sanity = std::clamp(val, 0, 100); }
    void AddSanity(int delta) { SetSanity(m_sanity + delta); }

    const std::unordered_set<std::string>& GetAllFlags() const { return m_flags; }

private:
    std::unordered_set<std::string> m_flags;
    std::unordered_map<std::string, int> m_counters;
    std::unordered_map<std::string, std::string> m_items;
    std::string m_heldItem;
    std::string m_heldItemName;
    int m_sanity = 100;
};

}
