#pragma once
#include "engine/core/Engine.h"
#include "engine/math/Math2D.h"
#include <string>
#include <vector>

namespace game::systems {

struct TableDish {
    std::string tableId;
    std::string itemId;
    engine::math::Vec2 worldPos;
};

class CookingSystem {
public:
    void Reset();
    void StartPreparation(engine::core::Engine& eng, const std::string& itemId, float durationSec = 2.0f);
    void PlaceDishOnTable(const std::string& tableId, const std::string& itemId);
    void RemoveDishFromTable(const std::string& tableId);

    void Update(engine::core::Engine& eng, float dt, engine::math::Vec2 playerCenter);
    void RenderWorld(engine::core::Engine& eng, engine::math::Vec2 camOffset) const;
    void RenderUI(engine::core::Engine& eng, engine::math::Vec2 camOffset) const;

    bool IsActive() const { return m_active; }
    static const char* GetItemDisplayName(const std::string& itemId);

private:
    bool m_active = false;
    bool m_readyToPickup = false;
    std::string m_itemId;
    std::string m_itemName;
    engine::math::Vec2 m_stationPos = {148.0f, 44.0f};
    float m_timer = 0.0f;
    float m_duration = 2.0f;
    float m_steamAnim = 0.0f;

    std::vector<TableDish> m_servedDishes;
};

}
