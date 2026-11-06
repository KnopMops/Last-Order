#pragma once
#include "engine/core/Engine.h"
#include <vector>

namespace game::ui {

enum class MenuCommand {
    None,
    ContinueSaveGame,
    LoadSlot0,
    LoadSlot1,
    LoadSlot2,
    StartNight1_IntroTutorial,
    StartNight1_NoTutorial,
    StartNight2_Independent,
    StartNight3_Pipes,
    StartNight4_Phone,
    StartNight5_Finale,
    StartEndlessMode,
    StartChapter2_Motel3D,
    StartChapter3_House3D,
    StartChapter35,
    StartChapter4,
    OpenSettings,
    QuitGame
};

enum class MenuState {
    Main,
    ChapterSelect,
    NightSelect,
    SaveSelect
};

struct RainDrop {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 140.0f;
    float len = 10.0f;
};

struct PuddleRipple {
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    float maxRadius = 8.0f;
};

struct SteamPuff {
    float x = 0.0f;
    float y = 0.0f;
    float age = 0.0f;
    float maxAge = 2.0f;
    float drift = 0.0f;
};

class MainMenuScene {
public:
    void Init(engine::core::Engine& eng);
    MenuCommand Update(engine::core::Engine& eng, float dt);
    void RenderWorld(engine::core::Engine& eng);
    void RenderUI(engine::core::Engine& eng) const;

private:
    void SetupMenuLighting(engine::core::Engine& eng, float t);
    void RenderAnim1_HighwayRainAndNeon(engine::core::Engine& eng, float t);
    void RenderAnim2_SwingingLampAndMirror(engine::core::Engine& eng, float t);

    MenuState m_state = MenuState::Main;
    int m_selectedBtn = 0;
    int m_animMode = 0;
    bool m_autoCycleAnims = true;
    float m_animCycleTimer = 0.0f;
    float m_lightningTimer = 0.0f;

    std::vector<RainDrop> m_rain;
    std::vector<PuddleRipple> m_ripples;
    std::vector<SteamPuff> m_steam;
};

}
