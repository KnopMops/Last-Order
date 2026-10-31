#include "game/systems/TutorialSystem.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <cstdio>

namespace game::systems {

void TutorialSystem::Reset(int nightNumber, bool enableTutorial) {
    m_nightNumber = nightNumber;
    m_step = (nightNumber == 1 && enableTutorial) ? TutorialStep::Step1_Movement : TutorialStep::Completed;
    m_walkedDistance = 0.0f;
}

void TutorialSystem::Update(engine::core::Engine& eng, engine::math::Vec2 , float distMovedThisFrame,
                            bool toggledFlashlight, bool toggledVisor) {
    if (m_step == TutorialStep::Completed) return;

    if (IsKeyPressed(KEY_BACKSPACE)) {
        m_step = TutorialStep::Completed;
        eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.6f);
        return;
    }

    const auto& st = eng.State();
    switch (m_step) {
        case TutorialStep::Step1_Movement:
            m_walkedDistance += distMovedThisFrame;
            if (m_walkedDistance >= 36.0f) {
                m_step = TutorialStep::Step2_Flashlight;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step2_Flashlight:
            if (toggledFlashlight) {
                m_step = TutorialStep::Step3_Visor3D;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step3_Visor3D:
            if (toggledVisor) {
                m_step = TutorialStep::Step4_ShootAnomaly3D;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step4_ShootAnomaly3D:
            if (st.HasFlag("visor_first_hit") || st.HasFlag("anomalies_cleared_3d")) {
                m_step = TutorialStep::Step5_ReadNote;
                eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.65f);
            }
            break;
        case TutorialStep::Step5_ReadNote:
            if (st.HasFlag("read_mikhail_note") && !eng.Dialogue().IsActive()) {
                m_step = TutorialStep::Step6_ReadRules;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step6_ReadRules:
            if (st.HasFlag("read_wall_rules") && !eng.Dialogue().IsActive()) {
                m_step = TutorialStep::Completed;
                eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.75f);
            }
            break;
        case TutorialStep::Completed:
            break;
    }
}

void TutorialSystem::RenderWorldGuides(engine::core::Engine& , engine::math::Vec2 ) const {
}

void TutorialSystem::RenderUIOverlay(engine::core::Engine& eng) const {
    if (!IsTutorialActive()) return;

    Font font = eng.Assets().GetMainFont();
    const char* msg = "";
    switch (m_step) {
        case TutorialStep::Step1_Movement:
            msg = "ОБУЧЕНИЕ (1/6): Пройдитесь по залу [W, A, S, D]   ([Backspace] — пропустить)";
            break;
        case TutorialStep::Step2_Flashlight:
            msg = "ОБУЧЕНИЕ (2/6): Нажмите [F], чтобы включить тактический фонарик";
            break;
        case TutorialStep::Step3_Visor3D:
            msg = "ОБУЧЕНИЕ (3/6): Нажмите [V], чтобы включить 3D-Сканер и Шутер «ОКО-86»";
            break;
        case TutorialStep::Step4_ShootAnomaly3D:
            msg = "ОБУЧЕНИЕ (4/6): В 3D-режиме наведите прицел на аномалию и выстрелите [ЛКМ / ПРОБЕЛ] ([X] — волна)";
            break;
        case TutorialStep::Step5_ReadNote:
            msg = "ОБУЧЕНИЕ (5/6): Выйдите в 2D [V], подойдите к Кассе на стойке и нажмите [E]";
            break;
        case TutorialStep::Step6_ReadRules:
            msg = "ОБУЧЕНИЕ (6/6): Откройте Устав смены [R] или Тетрадь задач [TAB]";
            break;
        default:
            return;
    }

    DrawRectangle(250, 12, 780, 40, Color{10, 13, 26, 240});
    DrawRectangleLinesEx(Rectangle{250, 12, 780, 40}, 1.5f, Color{244, 184, 96, 255});
    engine::graphics::TextLayout::DrawCentered(font, msg, 640.0f, 22.0f, 17.5f, Color{244, 184, 96, 255});
}

}
