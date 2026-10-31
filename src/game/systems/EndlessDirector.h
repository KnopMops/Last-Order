#pragma once
#include "engine/core/Engine.h"
#include "engine/math/Math2D.h"
#include "game/world/CafeWorld.h"
#include <string>
#include <vector>

namespace game::systems {

enum class EndlessPatronState {
    Empty = 0,
    RegularOrder,
    AnomalyMimic
};

struct EndlessPatron {
    std::string id;
    std::string name;
    std::string triggerId;
    std::string spritePath;
    engine::math::Vec2 position;
    EndlessPatronState state = EndlessPatronState::Empty;
    std::string requestedItem;
    float patienceTimer = 60.0f;
};

class EndlessDirector {
public:
    void Init(const std::string& profilePath);
    void Reset();
    void Update(engine::core::Engine& eng, float dt, engine::math::Vec2 playerPos, bool hasLight);
    void RenderUI(engine::core::Engine& eng) const;

    bool ShouldTriggerScreamer(std::string& outScreamerId);
    bool ShouldTriggerBlackout();
    void RestorePower();

    bool IsBlackout() const { return m_blackoutActive; }
    float GetTension() const { return m_tension; }
    int GetScore() const { return m_score; }
    int GetHighScore() const { return m_highScore; }
    float GetElapsedSeconds() const { return m_elapsedSeconds; }
    bool IsGameOver() const { return m_gameOver; }

    void InteractWithBooth(engine::core::Engine& eng, const std::string& triggerId, bool flashlightOn);
    void OnSanityRestored(int amount);
    void SyncGuestNpcs(std::vector<world::GuestNpc>& outGuests) const;

private:
    void LoadRecords();
    void SaveRecords();
    void UpdatePatrons(engine::core::Engine& eng, float dt);

    std::string m_profilePath;
    float m_elapsedSeconds = 0.0f;
    int m_score = 0;
    int m_highScore = 0;
    float m_bestTime = 0.0f;

    float m_tension = 20.0f;
    float m_screamerCooldown = 12.0f;
    float m_blackoutTimer = 45.0f;
    float m_ambientScareTimer = 8.0f;
    bool m_blackoutActive = false;

    float m_patronSpawnTimer = 6.0f;
    EndlessPatron m_patrons[6];

    bool m_pendingScreamer = false;
    std::string m_pendingScreamerId;
    bool m_pendingBlackout = false;
    bool m_gameOver = false;
    std::string m_lastStatusMsg;
    float m_statusTimer = 0.0f;
};

}
