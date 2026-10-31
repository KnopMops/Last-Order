#include "game/systems/TutorialSystem.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <cmath>
#include <cstdio>

namespace game::systems {

void TutorialSystem::Reset(int nightNumber, bool enableTutorial) {
    m_nightNumber = nightNumber;
    m_step = (nightNumber == 1 && enableTutorial) ? TutorialStep::Step1_Movement : TutorialStep::Completed;
    m_walkedDistance = 0.0f;
}

void TutorialSystem::Update(engine::core::Engine& eng, engine::math::Vec2 , float distMovedThisFrame,
                            bool , bool ) {
    if (m_step == TutorialStep::Completed) return;

    if (IsKeyPressed(KEY_BACKSPACE)) {
        m_step = TutorialStep::Completed;
        eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.6f);
        return;
    }

    const auto& st = eng.State();
    if (st.HasFlag("kurt_served")) {
        m_step = TutorialStep::Completed;
        return;
    }

    switch (m_step) {
        case TutorialStep::Step1_Movement:
            m_walkedDistance += distMovedThisFrame;
            if (m_walkedDistance >= 28.0f) {
                m_step = TutorialStep::Step2_ReadNote;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step2_ReadNote:
            if (st.HasFlag("read_mikhail_note") && !eng.Dialogue().IsActive()) {
                m_step = TutorialStep::Step3_ReadRules;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step3_ReadRules:
            if (st.HasFlag("read_wall_rules") && !eng.Dialogue().IsActive()) {
                m_step = TutorialStep::Step4_TalkToKurt;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step4_TalkToKurt:
            if (st.GetHeldItem() == "coffee_sugar") {
                m_step = TutorialStep::Step6_ServeKurt;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            } else if (eng.Dialogue().IsActive() && eng.Dialogue().GetActiveNode() &&
                       eng.Dialogue().GetActiveNode()->id == "kurt_talk") {
                m_step = TutorialStep::Step5_BrewCoffee;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
            }
            break;
        case TutorialStep::Step5_BrewCoffee:
            if (st.GetHeldItem() == "coffee_sugar") {
                m_step = TutorialStep::Step6_ServeKurt;
                eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.65f);
            }
            break;
        case TutorialStep::Step6_ServeKurt:
            if (st.HasFlag("kurt_served") && !eng.Dialogue().IsActive()) {
                m_step = TutorialStep::Completed;
                eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.80f);
            }
            break;
        case TutorialStep::Completed:
            break;
    }
}

void TutorialSystem::RenderWorldGuides(engine::core::Engine& , engine::math::Vec2 playerCenter) const {
    if (!IsTutorialActive()) return;

    engine::math::Vec2 target = {0.0f, 0.0f};
    bool hasTarget = true;
    switch (m_step) {
        case TutorialStep::Step2_ReadNote:   target = {95.0f,  44.0f}; break;
        case TutorialStep::Step3_ReadRules:  target = {117.0f, 18.0f}; break;
        case TutorialStep::Step4_TalkToKurt: target = {78.0f,  86.0f}; break;
        case TutorialStep::Step5_BrewCoffee: target = {148.0f, 44.0f}; break;
        case TutorialStep::Step6_ServeKurt:  target = {78.0f,  86.0f}; break;
        default: hasTarget = false; break;
    }

    if (!hasTarget) return;

    float t = static_cast<float>(GetTime());
    float r = 9.0f + std::sin(t * 6.0f) * 2.5f;
    DrawCircleLines(static_cast<int>(target.x), static_cast<int>(target.y), r, Color{244, 184, 96, 230});
    DrawCircleLines(static_cast<int>(target.x), static_cast<int>(target.y), r - 2.0f, Color{255, 220, 120, 180});

    engine::math::Vec2 toT = target - playerCenter;
    float dist = toT.Length();
    if (dist > 18.0f) {
        engine::math::Vec2 dir = toT / dist;
        engine::math::Vec2 p1 = playerCenter + dir * 12.0f;
        engine::math::Vec2 p2 = playerCenter + dir * std::min(dist - 10.0f, 28.0f);
        DrawLineEx(Vector2{p1.x, p1.y}, Vector2{p2.x, p2.y}, 1.5f, Color{244, 184, 96, 210});
        DrawCircle(static_cast<int>(p2.x), static_cast<int>(p2.y), 2.0f, Color{255, 225, 130, 240});
    }
}

void TutorialSystem::RenderUIOverlay(engine::core::Engine& eng) const {
    if (!IsTutorialActive()) return;

    Font font = eng.Assets().GetMainFont();
    const char* msg = "";
    switch (m_step) {
        case TutorialStep::Step1_Movement:
            msg = "ОБУЧЕНИЕ (1/6): Пройдитесь по залу кафе [W, A, S, D]";
            break;
        case TutorialStep::Step2_ReadNote:
            msg = "ОБУЧЕНИЕ (2/6): Подойдите к Кассе на стойке и прочитайте записку [E]";
            break;
        case TutorialStep::Step3_ReadRules:
            msg = "ОБУЧЕНИЕ (3/6): Изучите Устав смены [R] или плакат на стене [E]";
            break;
        case TutorialStep::Step4_TalkToKurt:
            msg = "ОБУЧЕНИЕ (4/6): Подойдите к Дяде Курту (Стол №1) и примите заказ [E]";
            break;
        case TutorialStep::Step5_BrewCoffee:
            msg = "ОБУЧЕНИЕ (5/6): Сварите «Чёрный кофе с сахаром» у Кофемашины на стойке [E]";
            break;
        case TutorialStep::Step6_ServeKurt:
            msg = "ОБУЧЕНИЕ (6/6): Подайте готовый кофе Дяде Курту за Столом №1 [E]";
            break;
        default:
            return;
    }

    DrawRectangle(250, 12, 760, 40, Color{10, 13, 26, 240});
    DrawRectangleLinesEx(Rectangle{250, 12, 760, 40}, 1.5f, Color{244, 184, 96, 255});
    engine::graphics::TextLayout::DrawCentered(font, msg, 630.0f, 22.0f, 17.5f, Color{244, 184, 96, 255});
}

}
