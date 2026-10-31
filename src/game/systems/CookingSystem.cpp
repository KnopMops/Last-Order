#include "game/systems/CookingSystem.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game::systems {

const char* CookingSystem::GetItemDisplayName(const std::string& itemId) {
    if (itemId == "coffee_sugar") return "Чёрный кофе с сахаром";
    if (itemId == "coffee_black") return "Крепкий эспрессо без сахара";
    if (itemId == "tea_sugar")    return "Чай с сахаром";
    if (itemId == "tea_nosugar")  return "Пустой чай без сахара";
    if (itemId == "cherry_pie")   return "Тёплый вишнёвый пирог";
    if (itemId == "cold_meat")    return "Сырое мясо из морозилки";
    if (itemId == "trash_bag")    return "Пакет с мусором";
    return itemId.c_str();
}

void CookingSystem::Reset() {
    m_active = false;
    m_readyToPickup = false;
    m_itemId.clear();
    m_itemName.clear();
    m_timer = 0.0f;
    m_servedDishes.clear();
}

void CookingSystem::StartPreparation(engine::core::Engine& eng, const std::string& itemId, float durationSec) {
    if (itemId == "trash_bag" || itemId == "cold_meat") {
        eng.State().GiveItem(itemId, GetItemDisplayName(itemId));
        return;
    }

    m_active = true;
    m_readyToPickup = false;
    m_itemId = itemId;
    m_itemName = GetItemDisplayName(itemId);
    m_duration = std::max(0.4f, durationSec);
    m_timer = 0.0f;

    if (itemId == "cherry_pie") {
        m_stationPos = {502.0f, 36.0f};
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.7f);
    } else {
        m_stationPos = {304.0f, 84.0f};
        eng.Audio().PlaySfx("audio/sfx/sfx_coffee_pour.wav", 0.75f);
    }
}

void CookingSystem::PlaceDishOnTable(const std::string& tableId, const std::string& itemId) {
    engine::math::Vec2 pos = {172.0f, 132.0f};
    if (tableId == "booth_1")      pos = {172.0f, 132.0f};
    else if (tableId == "booth_2") pos = {172.0f, 212.0f};
    else if (tableId == "booth_3") pos = {284.0f, 132.0f};
    else if (tableId == "booth_4") pos = {284.0f, 212.0f};
    else if (tableId == "booth_5") pos = {392.0f, 132.0f};
    else if (tableId == "booth_6") pos = {392.0f, 212.0f};

    for (auto& d : m_servedDishes) {
        if (d.tableId == tableId) {
            d.itemId = itemId;
            return;
        }
    }
    m_servedDishes.push_back({tableId, itemId, pos});
}

void CookingSystem::Update(engine::core::Engine& eng, float dt, engine::math::Vec2 playerCenter) {
    m_steamAnim += dt * 4.5f;

    if (!m_active) return;

    if (!m_readyToPickup) {
        m_timer += dt;
        if (m_timer >= m_duration) {
            m_timer = m_duration;
            m_readyToPickup = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.85f);
        }
    }

    if (m_readyToPickup) {
        float dist = (playerCenter - m_stationPos).Length();
        if (dist <= 75.0f || IsKeyPressed(KEY_E)) {
            eng.State().GiveItem(m_itemId, m_itemName);
            m_active = false;
            m_readyToPickup = false;
        }
    }
}

void CookingSystem::RenderWorld(engine::core::Engine& , engine::math::Vec2 camOffset) const {
    for (const auto& d : m_servedDishes) {
        int px = static_cast<int>(d.worldPos.x - camOffset.x);
        int py = static_cast<int>(d.worldPos.y - camOffset.y);
        if (px < -16 || px > 336 || py < -16 || py > 196) continue;

        if (d.itemId == "cherry_pie") {
            DrawRectangle(px - 3, py - 2, 7, 4, Color{232, 227, 217, 255});
            DrawRectangle(px - 2, py - 2, 5, 3, Color{168, 36, 36, 255});
        } else {
            DrawRectangle(px - 2, py - 3, 5, 5, Color{232, 227, 217, 255});
            Color liquid = (d.itemId == "coffee_sugar" || d.itemId == "coffee_black")
                               ? Color{107, 51, 34, 255}
                               : Color{196, 122, 59, 255};
            DrawRectangle(px - 1, py - 2, 3, 2, liquid);
        }
        int sy = py - 5 - (static_cast<int>(m_steamAnim) % 3);
        DrawPixel(px, sy, Color{232, 227, 217, 150});
    }

    if (m_active) {
        int sx = static_cast<int>(m_stationPos.x - camOffset.x);
        int sy = static_cast<int>(m_stationPos.y - camOffset.y);
        if (sx >= -16 && sx <= 336 && sy >= -16 && sy <= 196) {
            Color glow = m_readyToPickup ? Color{120, 214, 110, 220} : Color{244, 184, 96, 220};
            DrawCircleLines(sx, sy, 8.0f + std::sin(m_steamAnim * 2.0f) * 1.5f, glow);

            for (int i = 0; i < 3; ++i) {
                int pY = sy - 6 - ((static_cast<int>(m_steamAnim * 3.0f) + i * 3) % 9);
                int pX = sx - 2 + ((i + static_cast<int>(m_steamAnim)) % 5);
                DrawPixel(pX, pY, Color{232, 227, 217, 195});
            }
        }
    }
}

void CookingSystem::RenderUI(engine::core::Engine& eng, engine::math::Vec2 camOffset) const {
    if (!m_active) return;

    Font font = eng.Assets().GetMainFont();
    float uiX = (m_stationPos.x - camOffset.x) * 4.0f - 135.0f;
    float uiY = (m_stationPos.y - camOffset.y) * 4.0f - 84.0f;
    uiX = std::clamp(uiX, 20.0f, 980.0f);
    uiY = std::clamp(uiY, 96.0f, 600.0f);

    DrawRectangle(static_cast<int>(uiX), static_cast<int>(uiY), 270, 56, Color{10, 13, 26, 240});
    Color borderCol = m_readyToPickup ? Color{120, 214, 110, 255} : Color{244, 184, 96, 255};
    DrawRectangleLinesEx(Rectangle{uiX, uiY, 270.0f, 56.0f}, 2.0f, borderCol);

    float ratio = std::clamp(m_timer / std::max(0.1f, m_duration), 0.0f, 1.0f);
    std::string label = m_readyToPickup
        ? ("ГОТОВО: " + m_itemName)
        : ("Готовится: " + m_itemName);
    DrawTextEx(font, label.c_str(), Vector2{uiX + 10.0f, uiY + 7.0f}, 16.5f, 1.0f, borderCol);

    DrawRectangle(static_cast<int>(uiX + 10.0f), static_cast<int>(uiY + 32.0f), 250, 14, Color{42, 23, 21, 255});
    DrawRectangle(static_cast<int>(uiX + 10.0f), static_cast<int>(uiY + 32.0f),
                  static_cast<int>(250.0f * ratio), 14, borderCol);
}

}
