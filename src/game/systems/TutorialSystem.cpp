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
                m_step = TutorialStep::Step4_ReadNote;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step4_ReadNote:
            if (st.HasFlag("read_mikhail_note") && !eng.Dialogue().IsActive()) {
                m_step = TutorialStep::Step5_ReadRules;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step5_ReadRules:
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
            msg = "ОБУЧЕНИЕ (1/5): Пройдитесь по залу [W, A, S, D]   ([Backspace] — скрыть)";
            break;
        case TutorialStep::Step2_Flashlight:
            msg = "ОБУЧЕНИЕ (2/5): Нажмите [F], чтобы включить фонарик";
            break;
        case TutorialStep::Step3_Visor3D:
            msg = "ОБУЧЕНИЕ (3/5): Нажмите [V], чтобы проверить 3D-Визор «ОКО-86» (показывает ауру двойников)";
            break;
        case TutorialStep::Step4_ReadNote:
            msg = "ОБУЧЕНИЕ (4/5): Подойдите к Кассе на стойке и нажмите [E]";
            break;
        case TutorialStep::Step5_ReadRules:
            msg = "ОБУЧЕНИЕ (5/5): Изучите Правила смены [R] или на стене [E]";
            break;
        default:
            return;
    }

    DrawRectangle(340, 14, 640, 42, Color{10, 13, 26, 235});
    DrawRectangleLinesEx(Rectangle{340, 14, 640, 42}, 1.5f, Color{244, 184, 96, 255});
    DrawTextEx(font, msg, Vector2{356, 25}, 18.0f, 1.0f, Color{244, 184, 96, 255});
}

}
