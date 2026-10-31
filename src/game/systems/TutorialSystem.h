#pragma once
#include "engine/core/Engine.h"
#include "engine/math/Math2D.h"
#include <string>

namespace game::systems {

enum class TutorialStep {
    Step1_Movement = 0,
    Step2_Flashlight,
    Step3_Visor3D,
    Step4_ReadNote,
    Step5_ReadRules,
    Completed
};

class TutorialSystem {
public:
    void Reset(int nightNumber, bool enableTutorial);
    void Update(engine::core::Engine& eng, engine::math::Vec2 playerCenter, float distMovedThisFrame,
                bool toggledFlashlight, bool toggledVisor);

    void RenderWorldGuides(engine::core::Engine& eng, engine::math::Vec2 playerCenter) const;
    void RenderUIOverlay(engine::core::Engine& eng) const;

    bool IsTutorialActive() const { return m_nightNumber == 1 && m_step != TutorialStep::Completed; }
    int GetNightNumber() const { return m_nightNumber; }

private:
    int m_nightNumber = 1;
    TutorialStep m_step = TutorialStep::Completed;
    float m_walkedDistance = 0.0f;
};

}
