#pragma once
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::chapter2 {

struct ItemDefinition {
    std::string id;
    std::string name;
    std::string model = "box";
    Color color{200, 200, 200, 255};
    std::string inspectTitle;
    std::string inspectText;
    std::string event;
    bool consumable = false;
};

class IScriptHost {
public:
    virtual ~IScriptHost() = default;
    virtual void ShowMessage(const std::string& text, float seconds) = 0;
    virtual void SetObjective(const std::string& text) = 0;
    virtual void PlaySound(const std::string& path, float volume, std::optional<Vector3> at) = 0;
    virtual void Flicker(float seconds) = 0;
    virtual void SetPower(bool on) = 0;
    virtual void UnlockDoor(const std::string& name) = 0;
    virtual void OpenDoor(const std::string& name) = 0;
    virtual void CloseDoor(const std::string& name) = 0;
    virtual void Shake(float amount, float seconds) = 0;
    virtual void AddSanity(float delta) = 0;
    virtual void AddBattery(float amount) = 0;
    virtual void SetAmbient(const std::string& path, float volume) = 0;
    virtual void TriggerScreamer(const std::string& texture, float duration, float shake) = 0;
    virtual void EndAct(const std::string& text) = 0;
    virtual void StartHighwayChase() {}
};

class Chapter2Script {
public:
    bool Load(const std::string& path);
    void Reset();

    void RunEvent(const std::string& name, IScriptHost& host, std::optional<Vector3> source = std::nullopt);

    bool HasFlag(const std::string& flag) const { return m_flags.count(flag) > 0; }
    void SetFlag(const std::string& flag) { m_flags.insert(flag); }
    bool HasItem(const std::string& item) const;
    void GiveItem(const std::string& item);
    void TakeItem(const std::string& item);
    const std::vector<std::string>& Inventory() const { return m_inventory; }

    const ItemDefinition* FindItem(const std::string& id) const;
    std::string Sound(const std::string& key, const std::string& fallback) const;
    const std::string& Title() const { return m_title; }
    const std::string& StartEvent() const { return m_startEvent; }
    const std::string& AmbientPath() const { return m_ambientPath; }
    float AmbientVolume() const { return m_ambientVolume; }

private:
    bool Matches(const nlohmann::json& when) const;
    void Execute(const nlohmann::json& action, IScriptHost& host, std::optional<Vector3> source);

    nlohmann::json m_events;
    std::unordered_map<std::string, ItemDefinition> m_items;
    std::unordered_map<std::string, std::string> m_sounds;
    std::set<std::string> m_flags;
    std::unordered_map<std::string, int> m_counters;
    std::vector<std::string> m_inventory;
    std::string m_title;
    std::string m_startEvent;
    std::string m_ambientPath;
    float m_ambientVolume = 0.6f;
    int m_depth = 0;
};

}
