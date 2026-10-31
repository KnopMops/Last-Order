#include "game/systems/EndlessDirector.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

namespace game::systems {

static const char* ALL_SCREAMERS[14] = {
    "sc_night1_window",
    "sc_night2_toilet",
    "sc_night3_basement",
    "sc_night4_fridge",
    "sc_night5_window_hands",
    "sc_night6_mirror_hand",
    "sc_night7_guest_mouth",
    "sc_night8_tv_demon",
    "sc_night9_shadow_reaper",
    "sc_night10_bloody_mirror",
    "sc_night11_ceiling_crawler",
    "sc_night12_corridor_rush",
    "sc_night13_grinning_face",
    "sc_night14_skull_glitch"
};

void EndlessDirector::Init(const std::string& profilePath) {
    m_profilePath = profilePath;
    LoadRecords();
    Reset();
}

void EndlessDirector::LoadRecords() {
    if (m_profilePath.empty()) return;
    std::ifstream in(m_profilePath);
    if (!in.is_open()) return;
    try {
        nlohmann::json j;
        in >> j;
        m_highScore = j.value("endless_high_score", 0);
        m_bestTime = j.value("endless_best_time", 0.0f);
    } catch (...) {}
}

void EndlessDirector::SaveRecords() {
    if (m_profilePath.empty()) return;
    nlohmann::json j;
    {
        std::ifstream in(m_profilePath);
        if (in.is_open()) {
            try { in >> j; } catch (...) {}
        }
    }
    j["endless_high_score"] = m_highScore;
    j["endless_best_time"] = m_bestTime;
    std::ofstream out(m_profilePath);
    if (out.is_open()) {
        out << j.dump(4);
    }
}

void EndlessDirector::Reset() {
    m_elapsedSeconds = 0.0f;
    m_score = 0;
    m_tension = 22.0f;
    m_screamerCooldown = 10.0f;
    m_blackoutTimer = 42.0f;
    m_ambientScareTimer = 7.0f;
    m_blackoutActive = false;
    m_patronSpawnTimer = 4.0f;
    m_pendingScreamer = false;
    m_pendingBlackout = false;
    m_gameOver = false;
    m_lastStatusMsg.clear();
    m_statusTimer = 0.0f;

    m_patrons[0] = {"p1", "Стол №1", "trig_booth1", "sprites/characters/spr_kurt.png",  {56.0f,  68.0f},  EndlessPatronState::Empty, "", 0.0f};
    m_patrons[1] = {"p2", "Стол №2", "trig_booth2", "sprites/characters/spr_valya.png", {56.0f,  110.0f}, EndlessPatronState::Empty, "", 0.0f};
    m_patrons[2] = {"p3", "Стол №3", "trig_booth3", "sprites/characters/spr_guest.png", {116.0f, 68.0f},  EndlessPatronState::Empty, "", 0.0f};
    m_patrons[3] = {"p4", "Стол №4", "trig_booth4", "sprites/characters/spr_vera.png",  {116.0f, 110.0f}, EndlessPatronState::Empty, "", 0.0f};
    m_patrons[4] = {"p5", "Стол №5", "trig_booth5", "sprites/characters/spr_kurt.png",  {176.0f, 68.0f},  EndlessPatronState::Empty, "", 0.0f};
    m_patrons[5] = {"p6", "Стол №6", "trig_table6", "sprites/characters/spr_guest.png", {236.0f, 110.0f}, EndlessPatronState::Empty, "", 0.0f};
    m_patrons[6] = {"p7", "Стол №7", "trig_booth7", "sprites/characters/spr_vera.png",  {236.0f, 68.0f},  EndlessPatronState::Empty, "", 0.0f};
    m_patrons[7] = {"p8", "Стол №8", "trig_booth8", "sprites/characters/spr_guest.png", {176.0f, 110.0f}, EndlessPatronState::Empty, "", 0.0f};
}

void EndlessDirector::Update(engine::core::Engine& eng, float dt, engine::math::Vec2 , bool hasLight) {
    if (m_gameOver) return;

    m_elapsedSeconds += dt;
    if (m_statusTimer > 0.0f) m_statusTimer -= dt;

    static float secAccum = 0.0f;
    secAccum += dt;
    if (secAccum >= 1.0f) {
        secAccum -= 1.0f;
        m_score += 8;
    }

    float difficultyScale = 1.0f + (m_elapsedSeconds / 75.0f);
    float tensionRate = (m_blackoutActive ? 4.8f : 2.1f) * difficultyScale;
    if (!hasLight && m_blackoutActive) tensionRate += 3.8f;

    m_tension = std::clamp(m_tension + tensionRate * dt, 0.0f, 100.0f);

    if (m_screamerCooldown > 0.0f) {
        m_screamerCooldown -= dt;
    }

    m_ambientScareTimer -= dt;
    if (m_ambientScareTimer <= 0.0f) {
        m_ambientScareTimer = std::max(4.5f, 11.0f - difficultyScale * 2.0f);
        int r = std::rand() % 4;
        if (r == 0) eng.Audio().PlaySfx("audio/sfx/sfx_phantom_steps.wav", 0.95f);
        else if (r == 1) eng.Audio().PlaySfx("audio/sfx/sfx_window_bang.wav", 0.90f);
        else if (r == 2) eng.Audio().PlaySfx("audio/sfx/sfx_heavy_knock.wav", 0.90f);
        else eng.Audio().PlaySfx("audio/sfx/sfx_whisper_vera.wav", 0.90f);
        eng.Renderer().TriggerShake(3.6f, 0.40f);
        eng.Renderer().TriggerFlash(Color{185, 20, 28, 120}, 0.28f);
    }

    if (m_tension >= 68.0f && m_screamerCooldown <= 0.0f) {
        m_tension = 24.0f;
        m_screamerCooldown = std::max(8.0f, 16.0f - difficultyScale * 1.5f);
        m_pendingScreamer = true;
        int idx = std::rand() % 14;
        m_pendingScreamerId = ALL_SCREAMERS[idx];
        eng.State().AddSanity(-14);
    }

    if (!m_blackoutActive) {
        m_blackoutTimer -= dt;
        if (m_blackoutTimer <= 0.0f) {
            m_blackoutActive = true;
            m_pendingBlackout = true;
            m_blackoutTimer = std::max(30.0f, 55.0f - difficultyScale * 4.0f);
            eng.Audio().PlaySfx("audio/sfx/sfx_blackout_burst.wav", 0.95f);
            m_lastStatusMsg = "ВЫБИЛО ПРОБКИ! Почините электрощиток!";
            m_statusTimer = 5.0f;
        }
    }

    UpdatePatrons(eng, dt);

    if (eng.State().GetSanity() <= 0) {
        m_gameOver = true;
        if (m_score > m_highScore) m_highScore = m_score;
        if (m_elapsedSeconds > m_bestTime) m_bestTime = m_elapsedSeconds;
        SaveRecords();
    }
}

void EndlessDirector::UpdatePatrons(engine::core::Engine& eng, float dt) {
    m_patronSpawnTimer -= dt;
    if (m_patronSpawnTimer <= 0.0f) {
        m_patronSpawnTimer = std::max(5.0f, 11.0f - (m_elapsedSeconds / 60.0f));

        for (int attempt = 0; attempt < 8; ++attempt) {
            int i = (std::rand() + attempt) % 8;
            if (m_patrons[i].state == EndlessPatronState::Empty) {
                bool isMimic = (std::rand() % 100) < 45;
                if (isMimic) {
                    m_patrons[i].state = EndlessPatronState::AnomalyMimic;
                    m_patrons[i].requestedItem.clear();
                    m_patrons[i].patienceTimer = 36.0f;
                } else {
                    m_patrons[i].state = EndlessPatronState::RegularOrder;
                    m_patrons[i].requestedItem = (std::rand() % 2 == 0) ? "coffee_sugar" : "cherry_pie";
                    m_patrons[i].patienceTimer = 52.0f;
                }
                eng.Audio().PlaySfx("audio/sfx/sfx_door_bell.wav", 0.75f);
                break;
            }
        }
    }

    for (int i = 0; i < 8; ++i) {
        if (m_patrons[i].state != EndlessPatronState::Empty) {
            m_patrons[i].patienceTimer -= dt;
            if (m_patrons[i].patienceTimer <= 0.0f) {
                m_tension = std::min(100.0f, m_tension + 28.0f);
                eng.State().AddSanity(-12);
                m_patrons[i].state = EndlessPatronState::Empty;
                eng.Audio().PlaySfx("audio/sfx/sfx_heavy_knock.wav", 0.85f);
            }
        }
    }
}

void EndlessDirector::SyncGuestNpcs(std::vector<world::GuestNpc>& outGuests) const {
    outGuests.clear();
    for (int i = 0; i < 8; ++i) {
        if (m_patrons[i].state != EndlessPatronState::Empty) {
            bool isMimic = (m_patrons[i].state == EndlessPatronState::AnomalyMimic);
            outGuests.push_back(world::GuestNpc{
                m_patrons[i].id,
                m_patrons[i].name,
                m_patrons[i].spritePath,
                m_patrons[i].triggerId,
                "",
                m_patrons[i].position,
                0,
                true,
                isMimic,
                !isMimic
            });
        }
    }
}

void EndlessDirector::InteractWithBooth(engine::core::Engine& eng, const std::string& triggerId, bool flashlightOn) {
    for (int i = 0; i < 8; ++i) {
        if (m_patrons[i].triggerId == triggerId && m_patrons[i].state != EndlessPatronState::Empty) {
            if (m_patrons[i].state == EndlessPatronState::AnomalyMimic) {
                if (flashlightOn) {
                    m_patrons[i].state = EndlessPatronState::Empty;
                    m_score += 350;
                    m_tension = std::max(0.0f, m_tension - 30.0f);
                    eng.State().AddSanity(15);
                    eng.Renderer().TriggerFlash(Color{244, 184, 96, 200}, 0.35f);
                    eng.Renderer().TriggerShake(3.5f, 0.4f);
                    eng.Audio().PlaySfx("audio/sfx/sfx_window_bang.wav", 0.85f);
                    m_lastStatusMsg = "Двойник изгнан! (+350 очков)";
                    m_statusTimer = 3.5f;
                } else {
                    m_pendingScreamer = true;
                    m_pendingScreamerId = ALL_SCREAMERS[std::rand() % 14];
                    m_patrons[i].state = EndlessPatronState::Empty;
                    eng.State().AddSanity(-20);
                }
            } else {
                std::string held = eng.State().GetHeldItem();
                if (held == m_patrons[i].requestedItem) {
                    m_patrons[i].state = EndlessPatronState::Empty;
                    eng.State().ClearHeldItem();
                    m_score += 250;
                    m_tension = std::max(0.0f, m_tension - 22.0f);
                    eng.State().AddSanity(12);
                    eng.Audio().PlaySfx("audio/sfx/sfx_cash_register.wav", 0.85f);
                    m_lastStatusMsg = "Заказ выполнен! (+250 очков)";
                    m_statusTimer = 3.5f;
                } else {
                    const char* reqName = (m_patrons[i].requestedItem == "coffee_sugar") ? "Кофе" : "Вишнёвый пирог";
                    m_lastStatusMsg = m_patrons[i].name + ": " + reqName;
                    m_statusTimer = 4.0f;
                    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
                }
            }
            return;
        }
    }
}

bool EndlessDirector::ShouldTriggerScreamer(std::string& outScreamerId) {
    if (m_pendingScreamer) {
        m_pendingScreamer = false;
        outScreamerId = m_pendingScreamerId;
        return true;
    }
    return false;
}

bool EndlessDirector::ShouldTriggerBlackout() {
    if (m_pendingBlackout) {
        m_pendingBlackout = false;
        return true;
    }
    return false;
}

void EndlessDirector::RestorePower() {
    m_blackoutActive = false;
    m_tension = std::max(0.0f, m_tension - 30.0f);
    m_lastStatusMsg = "Питание восстановлено!";
    m_statusTimer = 3.0f;
}

void EndlessDirector::OnSanityRestored(int amount) {
    m_tension = std::max(0.0f, m_tension - static_cast<float>(amount));
}

void EndlessDirector::RenderUI(engine::core::Engine& eng) const {
    Font font = eng.Assets().GetMainFont();

    if (m_gameOver) {
        DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 235});
        DrawRectangle(320, 180, 640, 360, Color{42, 23, 21, 250});
        DrawRectangleLinesEx(Rectangle{320, 180, 640, 360}, 3.0f, Color{214, 69, 69, 255});

        engine::graphics::TextLayout::DrawCentered(font, "СВОБОДНЫЙ РЕЖИМ: СМЕНА ОКОНЧЕНА", 640.0f, 220.0f, 26.0f, Color{214, 69, 69, 255}, 1.0f);

        int mins = static_cast<int>(m_elapsedSeconds) / 60;
        int secs = static_cast<int>(m_elapsedSeconds) % 60;
        char statBuf[256];
        std::snprintf(statBuf, sizeof(statBuf), "ВРЕМЯ: %02d:%02d | СЧЁТ: %d", mins, secs, m_score);
        engine::graphics::TextLayout::DrawCentered(font, statBuf, 640.0f, 275.0f, 22.0f, Color{244, 184, 96, 255}, 1.0f);

        int bestM = static_cast<int>(m_bestTime) / 60;
        int bestS = static_cast<int>(m_bestTime) % 60;
        char recBuf[256];
        std::snprintf(recBuf, sizeof(recBuf), "РЕКОРД: %02d:%02d | ЛУЧШИЙ СЧЁТ: %d", bestM, bestS, m_highScore);
        engine::graphics::TextLayout::DrawCentered(font, recBuf, 640.0f, 320.0f, 19.0f, Color{196, 122, 59, 255}, 1.0f);

        DrawRectangle(440, 420, 400, 50, Color{107, 51, 34, 255});
        DrawRectangleLinesEx(Rectangle{440, 420, 400, 50}, 2.0f, Color{244, 184, 96, 255});
        engine::graphics::TextLayout::DrawCentered(font, "Повторить [R] | Меню [ESC]", 640.0f, 434.0f, 19.0f, Color{244, 184, 96, 255}, 1.0f);
        return;
    }

    DrawRectangle(0, 0, 1280, 48, Color{10, 13, 26, 235});
    DrawLine(0, 48, 1280, 48, Color{196, 122, 59, 220});

    int mins = static_cast<int>(m_elapsedSeconds) / 60;
    int secs = static_cast<int>(m_elapsedSeconds) % 60;
    char timeBuf[128];
    std::snprintf(timeBuf, sizeof(timeBuf), "СВОБОДНЫЙ РЕЖИМ | %02d:%02d | СЧЁТ: %d (РЕКОРД: %d)", mins, secs, m_score, m_highScore);
    DrawTextEx(font, timeBuf, Vector2{24, 14}, 19.0f, 1.0f, Color{244, 184, 96, 255});

    DrawTextEx(font, "УГРОЗА:", Vector2{820, 15}, 18.0f, 1.0f, Color{214, 69, 69, 240});
    DrawRectangle(905, 15, 140, 18, Color{42, 23, 21, 240});
    DrawRectangle(907, 17, static_cast<int>(136.0f * (m_tension / 100.0f)), 14,
                  (m_tension > 68.0f) ? Color{214, 69, 69, 255} : Color{244, 184, 96, 230});
    DrawRectangleLines(905, 15, 140, 18, Color{196, 122, 59, 255});

    int sanity = eng.State().GetSanity();
    char sanBuf[32];
    std::snprintf(sanBuf, sizeof(sanBuf), "РАССУДОК: %d%%", sanity);
    DrawTextEx(font, sanBuf, Vector2{1070, 14}, 19.0f, 1.0f, (sanity > 45) ? Color{232, 227, 217, 255} : Color{214, 69, 69, 255});

    if (m_statusTimer > 0.0f && !m_lastStatusMsg.empty()) {
        DrawRectangle(320, 58, 640, 36, Color{42, 23, 21, 235});
        DrawRectangleLines(320, 58, 640, 36, Color{244, 184, 96, 220});
        engine::graphics::TextLayout::DrawCentered(font, m_lastStatusMsg, 640.0f, 67.0f, 18.0f, Color{244, 184, 96, 255});
    }
}

}
