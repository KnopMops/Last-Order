#include "game/chapter2/Chapter2Script.h"
#include <algorithm>
#include <fstream>

namespace game::chapter2 {

using nlohmann::json;

bool Chapter2Script::Load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        TraceLog(LOG_ERROR, "CH2: cannot open script %s", path.c_str());
        return false;
    }
    json root = json::parse(file, nullptr, false);
    if (root.is_discarded()) {
        TraceLog(LOG_ERROR, "CH2: invalid script json %s", path.c_str());
        return false;
    }

    m_title = root.value("title", std::string{});
    m_startEvent = root.value("start_event", std::string{});
    if (root.contains("ambient")) {
        m_ambientPath = root["ambient"].value("path", std::string{});
        m_ambientVolume = root["ambient"].value("volume", 0.6f);
    }
    m_sounds.clear();
    if (root.contains("sounds")) {
        for (auto it = root["sounds"].begin(); it != root["sounds"].end(); ++it) {
            m_sounds[it.key()] = it.value().get<std::string>();
        }
    }
    m_items.clear();
    if (root.contains("items")) {
        for (auto it = root["items"].begin(); it != root["items"].end(); ++it) {
            const json& j = it.value();
            ItemDefinition def;
            def.id = it.key();
            def.name = j.value("name", it.key());
            def.model = j.value("model", std::string("box"));
            def.inspectTitle = j.value("inspect_title", def.name);
            def.inspectText = j.value("inspect_text", std::string{});
            def.event = j.value("event", std::string{});
            def.consumable = j.value("consumable", false);
            if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
                def.color = Color{j["color"][0].get<unsigned char>(), j["color"][1].get<unsigned char>(),
                                  j["color"][2].get<unsigned char>(), 255};
            }
            m_items[def.id] = def;
        }
    }
    m_events = root.contains("events") ? root["events"] : json::object();
    Reset();
    return true;
}

void Chapter2Script::Reset() {
    m_flags.clear();
    m_counters.clear();
    m_inventory.clear();
    m_depth = 0;
}

bool Chapter2Script::HasItem(const std::string& item) const {
    return std::find(m_inventory.begin(), m_inventory.end(), item) != m_inventory.end();
}

void Chapter2Script::GiveItem(const std::string& item) {
    if (!HasItem(item)) m_inventory.push_back(item);
}

void Chapter2Script::TakeItem(const std::string& item) {
    m_inventory.erase(std::remove(m_inventory.begin(), m_inventory.end(), item), m_inventory.end());
}

const ItemDefinition* Chapter2Script::FindItem(const std::string& id) const {
    auto it = m_items.find(id);
    return it != m_items.end() ? &it->second : nullptr;
}

std::string Chapter2Script::Sound(const std::string& key, const std::string& fallback) const {
    auto it = m_sounds.find(key);
    return it != m_sounds.end() ? it->second : fallback;
}

bool Chapter2Script::Matches(const json& when) const {
    if (!when.is_object()) return true;
    if (when.contains("flag") && !HasFlag(when["flag"].get<std::string>())) return false;
    if (when.contains("not_flag") && HasFlag(when["not_flag"].get<std::string>())) return false;
    if (when.contains("item") && !HasItem(when["item"].get<std::string>())) return false;
    if (when.contains("not_item") && HasItem(when["not_item"].get<std::string>())) return false;
    if (when.contains("counter")) {
        const json& c = when["counter"];
        auto it = m_counters.find(c.value("name", std::string{}));
        int value = it != m_counters.end() ? it->second : 0;
        if (c.contains("eq") && value != c["eq"].get<int>()) return false;
        if (c.contains("gte") && value < c["gte"].get<int>()) return false;
        if (c.contains("lt") && value >= c["lt"].get<int>()) return false;
    }
    return true;
}

void Chapter2Script::RunEvent(const std::string& name, IScriptHost& host, std::optional<Vector3> source) {
    if (name.empty() || !m_events.contains(name) || m_depth > 8) return;
    ++m_depth;
    const json& rules = m_events[name];
    for (const json& rule : rules) {
        if (!Matches(rule.contains("when") ? rule["when"] : json())) continue;
        if (rule.contains("do")) {
            for (const json& action : rule["do"]) Execute(action, host, source);
        }
        if (!rule.value("continue", false)) break;
    }
    --m_depth;
}

void Chapter2Script::Execute(const json& a, IScriptHost& host, std::optional<Vector3> source) {
    std::string type = a.value("type", std::string{});
    if (type == "message") {
        host.ShowMessage(a.value("text", std::string{}), a.value("time", 4.0f));
    } else if (type == "objective") {
        host.SetObjective(a.value("text", std::string{}));
    } else if (type == "sfx") {
        std::optional<Vector3> at = source;
        if (a.contains("at") && a["at"].is_array() && a["at"].size() >= 3) {
            at = Vector3{a["at"][0].get<float>(), a["at"][1].get<float>(), a["at"][2].get<float>()};
        } else if (a.contains("at") && a["at"].is_string() && a["at"].get<std::string>() == "player") {
            at.reset();
        }
        host.PlaySound(a.value("path", std::string{}), a.value("volume", 0.8f), at);
    } else if (type == "set_flag") {
        m_flags.insert(a.value("name", std::string{}));
    } else if (type == "clear_flag") {
        m_flags.erase(a.value("name", std::string{}));
    } else if (type == "give_item") {
        GiveItem(a.value("item", std::string{}));
    } else if (type == "take_item") {
        TakeItem(a.value("item", std::string{}));
    } else if (type == "counter_add") {
        m_counters[a.value("name", std::string{})] += a.value("value", 1);
    } else if (type == "flicker") {
        host.Flicker(a.value("time", 1.5f));
    } else if (type == "power") {
        host.SetPower(a.value("on", true));
    } else if (type == "unlock") {
        host.UnlockDoor(a.value("target", std::string{}));
    } else if (type == "open_door") {
        host.OpenDoor(a.value("target", std::string{}));
    } else if (type == "close_door") {
        host.CloseDoor(a.value("target", std::string{}));
    } else if (type == "shake") {
        host.Shake(a.value("amount", 3.0f), a.value("time", 0.5f));
    } else if (type == "sanity") {
        host.AddSanity(a.value("delta", 0.0f));
    } else if (type == "battery") {
        host.AddBattery(a.value("amount", 30.0f));
    } else if (type == "ambient") {
        host.SetAmbient(a.value("path", std::string{}), a.value("volume", 0.6f));
    } else if (type == "end_act") {
        host.EndAct(a.value("text", std::string{}));
    } else if (type == "event") {
        RunEvent(a.value("name", std::string{}), host, source);
    }
}

}
