#include "game/systems/SanitySystem.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace game::systems {

void SanitySystem::Init(const std::string& profileSavePath) {
    m_profilePath = profileSavePath;
    LoadProfile();
}

void SanitySystem::ResetForShift(engine::math::Vec2 startPos) {
    m_gameOver = false;
    m_selectedBtn = 0;
    m_hasCheckpoint = false;
    m_checkpointPos = startPos;
    m_currentStress = 0.0f;
    m_hallucinationTimer = 14.0f;
    m_clockGlitchTimer = 0.0f;
}

void SanitySystem::Update(engine::core::Engine& eng, float dt, engine::math::Vec2 playerPos) {
    int sanity = eng.State().GetSanity();
    float targetStress = std::clamp((100.0f - static_cast<float>(sanity)) / 100.0f, 0.0f, 1.0f);
    m_currentStress += (targetStress - m_currentStress) * std::min(1.0f, dt * 3.5f);
    eng.Renderer().SetSanityStress(m_currentStress);

    if (m_clockGlitchTimer > 0.0f) {
        m_clockGlitchTimer = std::max(0.0f, m_clockGlitchTimer - dt);
    }

    if (!m_hasCheckpoint && sanity > 0 && eng.Time().GetShiftElapsedMinutes() >= 230.0f) {
        m_hasCheckpoint = true;
        m_checkpointState = eng.State();
        m_checkpointHour = eng.Time().GetHours24();
        m_checkpointMin = eng.Time().GetMinutes();
        m_checkpointPos = playerPos;
        eng.State().SetFlag("checkpoint_0200_saved");
    }

    if (!m_gameOver && sanity <= 0 && !eng.Dialogue().IsActive()) {
        m_gameOver = true;
        m_selectedBtn = 0;
        eng.Renderer().TriggerFlash(Color{168, 36, 36, 190}, 0.55f);
        eng.Renderer().TriggerShake(4.0f, 0.5f);
        eng.Audio().PlayScreamerStinger("sfx_sc_07_clock_chord.wav");
        return;
    }

    if (!m_gameOver && sanity < 70 && !eng.Dialogue().IsActive()) {
        m_hallucinationTimer -= dt;
        if (m_hallucinationTimer <= 0.0f) {
            m_hallucinationTimer = (sanity < 40) ? 8.5f : 15.0f;
            m_clockGlitchTimer = 1.6f;
            if (sanity < 40) {

                eng.Audio().PlaySfx("audio/sfx/sfx_heartbeat_dread.wav", 0.90f);
                int r = std::rand() % 3;
                if (r == 0) eng.Audio().PlaySfx("audio/sfx/sfx_window_bang.wav", 0.85f);
                else if (r == 1) eng.Audio().PlaySfx("audio/sfx/sfx_heavy_knock.wav", 0.85f);
                else eng.Audio().PlaySfx("audio/sfx/sfx_cat_hiss.wav", 0.75f);
                eng.Renderer().TriggerShake(2.5f, 0.35f);
                eng.Renderer().TriggerFlash(Color{26, 36, 62, 110}, 0.25f);
            } else {
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.65f);
            }
        }
    }
}

GameOverCommand SanitySystem::UpdateGameOverInput(engine::core::Engine& eng) {
    if (!m_gameOver) return GameOverCommand::None;

    constexpr int COUNT = 3;
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
        m_selectedBtn = (m_selectedBtn - 1 + COUNT) % COUNT;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.45f);
    }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
        m_selectedBtn = (m_selectedBtn + 1) % COUNT;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.45f);
    }

    bool activate = (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE));
    engine::math::Vec2 mpos = eng.Renderer().GetVirtualMousePos();

    for (int i = 0; i < COUNT; ++i) {
        engine::math::Rect r = {360.0f, 350.0f + i * 64.0f, 560.0f, 52.0f};
        if (r.Contains(mpos)) {
            m_selectedBtn = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) activate = true;
        }
    }

    if (activate) {
        eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.6f);
        m_gameOver = false;
        if (m_selectedBtn == 0) return m_hasCheckpoint ? GameOverCommand::RestoreCheckpoint : GameOverCommand::RestartNight;
        if (m_selectedBtn == 1) return GameOverCommand::RestartNight;
        return GameOverCommand::ReturnToMainMenu;
    }

    return GameOverCommand::None;
}

void SanitySystem::RestoreCheckpointTo(engine::core::Engine& eng, engine::math::Vec2& outPlayerPos) {
    eng.State() = m_checkpointState;
    eng.State().SetSanity(std::max(65, m_checkpointState.GetSanity()));
    eng.Time().SetTimeHHMM(m_checkpointHour, m_checkpointMin);
    outPlayerPos = m_checkpointPos;
    m_gameOver = false;
    m_currentStress = 0.25f;
    eng.Renderer().SetSanityStress(m_currentStress);
    eng.Renderer().TriggerFlash(Color{244, 184, 96, 130}, 0.35f);
}

void SanitySystem::RenderGameOverModal(engine::core::Engine& eng, int currentNight) const {
    if (!m_gameOver) return;

    Font font = eng.Assets().GetMainFont();
    DrawRectangle(0, 0, 1280, 720, Color{10, 8, 16, 232});
    DrawRectangle(310, 140, 660, 440, Color{36, 16, 18, 250});
    DrawRectangleLinesEx(Rectangle{310, 140, 660, 440}, 3.0f, Color{214, 69, 69, 255});

    engine::graphics::TextLayout::DrawCentered(
        font, "РАССУДОК ИСЧЕРПАН (0%) — КАФЕ ПОГЛОТИЛО ВАС",
        640.0f, 174.0f, 25.0f, Color{214, 69, 69, 255}, 1.0f);

    std::string desc =
        "Лампы «Тёплого угла» гаснут одна за другой. На часах застыло 03:33, а ваше собственное отражение в зеркале улыбается и занимает место за кассой.\n"
        "Соблюдайте правила на стене, проверяйте отражения гостей и не позволяйте рассудку упасть до нуля.";
    engine::graphics::TextLayout::DrawWrapped(
        font, desc, {345.0f, 224.0f, 590.0f, 110.0f}, 19.5f, 1.0f, Color{232, 227, 217, 245});

    const char* btn0 = m_hasCheckpoint
        ? "Продолжить с чекпоинта (около 02:00 ночи, Рассудок 65%)"
        : "Попробовать снова с начала этой ночи (22:00)";
    const char* labels[3] = {
        btn0,
        (currentNight == 1) ? "Перезапустить Ночь 1 (22:00)" :
        (currentNight == 2) ? "Перезапустить Ночь 2 (22:00)" :
        (currentNight == 3) ? "Перезапустить Ночь 3 (22:00)" :
        (currentNight == 4) ? "Перезапустить Ночь 4 (22:00)" :
                              "Перезапустить Ночь 5 (22:00)",
        "Выйти в главное меню"
    };

    for (int i = 0; i < 3; ++i) {
        bool sel = (m_selectedBtn == i);
        float bx = 360.0f;
        float by = 350.0f + i * 64.0f;
        DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 560, 52,
                      sel ? Color{168, 36, 36, 255} : Color{90, 32, 30, 235});
        DrawRectangleLinesEx(Rectangle{bx, by, 560, 52}, sel ? 2.0f : 1.0f, Color{244, 184, 96, 255});
        DrawTextEx(font, labels[i], Vector2{bx + 20.0f, by + 15.0f}, 20.0f, 1.0f,
                   sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 235});
    }
}

std::string SanitySystem::FormatHudClock(const engine::core::Engine& eng) const {
    if (m_clockGlitchTimer > 0.0f || (eng.State().GetSanity() < 35 && (static_cast<int>(GetTime() * 4.0) % 7 == 0))) {
        return "ЧАСЫ: 25:61";
    }
    return "ЧАСЫ: " + eng.Time().FormatHHMM();
}

void SanitySystem::MarkNightCompleted(int nightNumber) {
    if (nightNumber >= 1 && nightNumber <= 5) {
        m_nightCompleted[nightNumber] = true;
        if (nightNumber == 5) m_endlessUnlocked = true;
        SaveProfile();
    }
}

bool SanitySystem::IsNightCompleted(int nightNumber) const {
    if (nightNumber >= 1 && nightNumber <= 5) {
        return m_nightCompleted[nightNumber];
    }
    return false;
}

void SanitySystem::LoadProfile() {
    if (m_profilePath.empty()) return;
    std::ifstream in(m_profilePath);
    if (!in.is_open()) return;
    try {
        nlohmann::json j;
        in >> j;
        for (int i = 1; i <= 5; ++i) {
            std::string key = "night" + std::to_string(i) + "_completed";
            m_nightCompleted[i] = j.value(key, false);
        }
        m_endlessUnlocked = j.value("endless_unlocked", false);
    } catch (...) {}
}

void SanitySystem::SaveProfile() const {
    if (m_profilePath.empty()) return;
    nlohmann::json j = {
        {"night1_completed", m_nightCompleted[1]},
        {"night2_completed", m_nightCompleted[2]},
        {"night3_completed", m_nightCompleted[3]},
        {"night4_completed", m_nightCompleted[4]},
        {"night5_completed", m_nightCompleted[5]},
        {"endless_unlocked", m_endlessUnlocked}
    };
    std::ofstream out(m_profilePath);
    if (out.is_open()) {
        out << j.dump(4);
    }
}

}
