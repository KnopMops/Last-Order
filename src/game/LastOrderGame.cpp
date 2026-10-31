#include "game/LastOrderGame.h"
#include "engine/core/PathUtils.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {

void LastOrderGame::OnInit(engine::core::Engine& eng) {
    const std::string base = eng.Assets().GetBaseDir();
    eng.Dialogue().LoadFromJsonFile(base + "/data/dialogs/night1.json");
    eng.Dialogue().LoadFromJsonFile(base + "/data/dialogs/night2.json");
    eng.Dialogue().LoadFromJsonFile(base + "/data/dialogs/night3.json");
    eng.Dialogue().LoadFromJsonFile(base + "/data/dialogs/night4.json");
    eng.Dialogue().LoadFromJsonFile(base + "/data/dialogs/night5.json");

    m_sanity.Init(engine::core::GetUserSavePath());
    m_endlessDirector.Init(engine::core::GetUserSavePath());
    eng.Audio().PlayAmbient("audio/bgm/amb_cafe_calm_lp.ogg", 0.70f);
    m_mainMenu.Init(eng);
    m_screen = GameScreen::MainMenu;
}

void LastOrderGame::StartEndlessShift(engine::core::Engine& eng) {
    m_currentNight = 0;
    eng.Dialogue().CloseDialogue();
    eng.State().Reset();
    eng.Time().Reset(23, 0, 1.80f);

    m_playerBody.position = {280.0f, 112.0f};
    m_playerBody.colliderOffset = {9.0f, 18.0f};
    m_playerBody.colliderSize = {14.0f, 11.0f};
    m_playerBody.velocity = {0.0f, 0.0f};
    m_playerFacing = 0;
    m_blackoutMode = false;
    m_lightsFlickering = false;
    m_visor3DActive = false;
    m_visorBattery = 100.0f;
    eng.Renderer().SetSkip2DLightmap(false);

    m_cooking.Reset();
    m_sanity.ResetForShift(m_playerBody.position);
    m_inspectionUI.Reset();
    m_endlessDirector.Reset();

    m_world.BuildCollidersAndTriggers(eng, 2);
    m_world.BuildLighting(eng, 2);

    eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.85f);
    m_guests.clear();

    m_tutorial.Reset(1, false);
    m_screen = GameScreen::EndlessMode;
}

void LastOrderGame::StartMinigame(engine::core::Engine& eng, systems::MinigameType type) {
    eng.Dialogue().CloseDialogue();
    m_visor3DActive = false;
    m_chaseMinigame.Start(type);
    bool is3D = (type != systems::MinigameType::Night1_RadioTuner2D);
    eng.Renderer().SetSkip2DLightmap(is3D);
    eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.90f);
    m_screen = GameScreen::NightmareChase;
}

void LastOrderGame::CompleteActiveMinigame(engine::core::Engine& eng) {
    eng.Renderer().SetSkip2DLightmap(false);
    eng.State().AddSanity(20);
    systems::MinigameType t = m_chaseMinigame.GetType();
    if (t == systems::MinigameType::Night1_RadioTuner2D) {
        eng.State().SetFlag("heard_radio_0214");
    } else if (t == systems::MinigameType::Night2_BasementFuse3D) {
        eng.State().SetFlag("n2_power_restored");
        m_lightsFlickering = false;
        m_blackoutMode = false;
        eng.Lighting().SetAmbientColor(Color{112, 96, 92, 255});
        eng.Lighting().SetAllPointLightsFlicker(2.0f, 0.03f);
    } else if (t == systems::MinigameType::Night3_FreezerMaze3D) {
        eng.State().SetFlag("n3_fridge_checked");
        eng.State().SetFlag("n3_basement_checked");
    } else if (t == systems::MinigameType::Night4_MirrorHunt3D) {
        eng.State().SetFlag("n4_jukebox_cleared");
        m_lightsFlickering = false;
        m_blackoutMode = false;
        eng.Lighting().SetAmbientColor(Color{112, 96, 92, 255});
        eng.Lighting().SetAllPointLightsFlicker(2.0f, 0.03f);
    } else if (t == systems::MinigameType::Night5_HighwayChase3D) {
        eng.State().SetFlag("n5_relic_collected");
        eng.State().SetFlag("n3_mirror_key_found");
    }
    m_world.BuildLighting(eng, std::max(1, m_currentNight));
    m_screen = (m_currentNight == 0) ? GameScreen::EndlessMode : GameScreen::PlayingShift;
}

void LastOrderGame::StartNewShift(engine::core::Engine& eng, int nightNumber, bool withTutorial) {
    m_currentNight = nightNumber;
    eng.Dialogue().CloseDialogue();
    eng.State().Reset();

    eng.Time().Reset(22, 0, 1.80f);

    m_playerBody.position = {280.0f, 112.0f};
    m_playerBody.colliderOffset = {9.0f, 18.0f};
    m_playerBody.colliderSize = {14.0f, 11.0f};
    m_playerBody.velocity = {0.0f, 0.0f};
    m_playerFacing = 0;
    m_blackoutMode = false;
    m_lightsFlickering = false;
    m_visor3DActive = false;
    m_visorBattery = 100.0f;
    eng.Renderer().SetSkip2DLightmap(false);

    m_cooking.Reset();
    m_sanity.ResetForShift(m_playerBody.position);
    m_inspectionUI.Reset();

    m_world.BuildCollidersAndTriggers(eng, nightNumber);
    m_world.BuildLighting(eng, nightNumber);

    if (nightNumber == 1) {
        eng.Audio().PlayAmbient("audio/bgm/amb_cafe_calm_lp.ogg", 0.70f);
        m_guests = {
            {"kurt",   "Дядя Курт",       "sprites/characters/spr_kurt.png",  "trig_booth1", "kurt_talk",   {156.0f, 118.0f}, 15,  false, false, true},
            {"katya",  "Студентка Катя",  "sprites/characters/spr_vera.png",  "trig_booth3", "katya_talk",  {268.0f, 118.0f}, 60,  false, false, true},
            {"valya",  "Тётя Валя",       "sprites/characters/spr_valya.png", "trig_booth2", "valya_talk",  {156.0f, 198.0f}, 115, false, false, true},
            {"hatman", "Человек в шляпе", "sprites/characters/spr_guest.png", "trig_booth5", "hatman_talk", {376.0f, 118.0f}, 180, false, true,  false}
        };
    } else if (nightNumber == 2) {
        eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.75f);
        m_guests = {
            {"arkhip",    "Механик Архип",       "sprites/characters/spr_kurt.png",  "trig_booth3", "arkhip_talk",         {268.0f, 118.0f}, 20,  false, false, true},
            {"valya_n2",  "Тётя Валя",           "sprites/characters/spr_valya.png", "trig_booth2", "n2_valya_talk",       {156.0f, 198.0f}, 75,  false, false, true},
            {"fake_kurt", "Дядя Курт (?)",       "sprites/characters/spr_kurt.png",  "trig_booth1", "n2_fake_kurt_talk",   {156.0f, 118.0f}, 160, false, true,  false},
            {"guest_t6",  "Фигура за Столом №6", "sprites/characters/spr_guest.png", "trig_table6", "n2_guest_table6_talk",{376.0f, 198.0f}, 270, false, true,  false}
        };
    } else if (nightNumber == 3) {
        eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.85f);
        m_guests = {
            {"gleb",     "Следователь Глеб",  "sprites/characters/spr_kurt.png",  "trig_booth4", "gleb_talk",            {268.0f, 198.0f}, 20,  false, false, true},
            {"katya_n3", "Попутчица (?)",     "sprites/characters/spr_vera.png",  "trig_booth5", "katya_mimic_talk",     {376.0f, 118.0f}, 110, false, true,  false},
            {"boss_n3",  "Сущность Стола №6", "sprites/characters/spr_guest.png", "trig_table6", "n3_guest_table6_talk", {376.0f, 198.0f}, 260, false, true,  false}
        };
    } else if (nightNumber == 4) {
        eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.90f);
        m_guests = {
            {"officer_n4",   "Офицер Борис",   "sprites/characters/spr_guest.png", "trig_booth3", "n4_officer_boris_talk",{268.0f, 118.0f}, 25,  false, false, true},
            {"arkhip_mimic", "Архип (?)",      "sprites/characters/spr_kurt.png",  "trig_booth2", "katya_mimic_talk",     {156.0f, 198.0f}, 115, false, true,  false},
            {"fake_kurt4",   "Двойник Курта",  "sprites/characters/spr_kurt.png",  "trig_booth1", "n2_fake_kurt_talk",   {156.0f, 118.0f}, 190, false, true,  false}
        };
    } else {
        eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.95f);
        m_guests = {
            {"vera_spirit", "Силуэт Веры",        "sprites/characters/spr_vera.png",  "trig_booth4", "vera_spirit_talk",   {268.0f, 198.0f}, 15,  false, false, true},
            {"gleb_n5",     "Следователь Глеб",   "sprites/characters/spr_kurt.png",  "trig_booth1", "gleb_talk",          {156.0f, 118.0f}, 85,  false, false, true},
            {"boss_n5",     "Хозяин Перекрёстка", "sprites/characters/spr_guest.png", "trig_table6", "n5_boss_table6_talk",{376.0f, 198.0f}, 210, false, true,  false}
        };
    }

    m_tutorial.Reset(nightNumber, withTutorial);
    m_screen = GameScreen::PlayingShift;

    if (nightNumber == 1)      eng.Dialogue().StartDialogue("intro_briefing_night1");
    else if (nightNumber == 2) eng.Dialogue().StartDialogue("n2_intro_diary");
    else if (nightNumber == 3) eng.Dialogue().StartDialogue("n3_intro_diary");
    else if (nightNumber == 4) eng.Dialogue().StartDialogue("n4_intro_diary");
    else                       eng.Dialogue().StartDialogue("n5_intro_diary");
}

void LastOrderGame::OnFixedUpdate(engine::core::Engine& eng, float fixedDt) {
    if (m_screen == GameScreen::NightmareChase) {
        m_chaseMinigame.Update(eng, fixedDt);
        return;
    }

    if (m_settingsUI.IsOpen() || m_sanity.IsGameOver() ||
        m_screen == GameScreen::MainMenu || m_screen == GameScreen::Paused) {
        eng.Time().SetPaused(true);
        eng.Time().Tick(fixedDt);
        return;
    }

    auto movePlayer = [&](float dt) {
        engine::math::Vec2 moveDir = {0.0f, 0.0f};
        if (m_visor3DActive) {
            float rotSpeed = 2.4f;
            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_Q))  m_visorYaw -= rotSpeed * dt;
            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_E)) m_visorYaw += rotSpeed * dt;
            Vector2 md = GetMouseDelta();
            m_visorYaw += md.x * 0.004f;
            m_visorPitch = std::clamp(m_visorPitch - md.y * 0.003f, -0.45f, 0.45f);

            float fwdX = std::cos(m_visorYaw);
            float fwdY = std::sin(m_visorYaw);
            float rightX = -fwdY;
            float rightY = fwdX;

            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))   { moveDir.x += fwdX; moveDir.y += fwdY; }
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) { moveDir.x -= fwdX; moveDir.y -= fwdY; }
            if (IsKeyDown(KEY_D)) { moveDir.x += rightX; moveDir.y += rightY; }
            if (IsKeyDown(KEY_A)) { moveDir.x -= rightX; moveDir.y -= rightY; }
        } else {
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) { moveDir.x += 1.0f; m_playerFacing = 3; }
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  { moveDir.x -= 1.0f; m_playerFacing = 2; }
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  { moveDir.y += 1.0f; m_playerFacing = 0; }
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    { moveDir.y -= 1.0f; m_playerFacing = 1; }
        }

        m_isMoving = (moveDir.LengthSq() > 0.01f);
        constexpr float MOVE_SPEED = 78.0f;
        m_playerBody.velocity = moveDir.Normalized() * MOVE_SPEED;

        engine::math::Vec2 prevPos = m_playerBody.position;
        if (m_debugNoclip) {
            m_playerBody.position += m_playerBody.velocity * dt;
        } else {
            eng.Physics().MoveAndSlide(m_playerBody, dt);
        }
        m_distMovedThisFrame += (m_playerBody.position - prevPos).Length();

        auto& fl = eng.Lighting().GetFlashlight();
        fl.position = m_playerBody.Center();
        if (m_visor3DActive) {
            fl.directionDeg = m_visorYaw * (180.0f / 3.14159265f);
        } else {
            const float dirAngles[4] = {90.0f, 270.0f, 180.0f, 0.0f};
            fl.directionDeg = dirAngles[m_playerFacing];
        }

        m_nearestTrigger = eng.Physics().FindNearestTrigger(m_playerBody.Center(), 28.0f);
    };

    if (m_screen == GameScreen::EndlessMode) {
        eng.Time().SetPaused(false);
        eng.Time().Tick(fixedDt);
        movePlayer(fixedDt);
        m_endlessDirector.SyncGuestNpcs(m_guests);
        return;
    }

    bool pauseShiftClock = eng.Dialogue().IsActive() || m_inspectionUI.IsAnyModalOpen();
    eng.Time().SetPaused(pauseShiftClock);
    eng.Time().Tick(fixedDt);

    if (eng.Dialogue().IsActive() || m_inspectionUI.IsAnyModalOpen()) {
        m_playerBody.velocity = {0.0f, 0.0f};
        m_isMoving = false;
        return;
    }

    movePlayer(fixedDt);

    float elapsedMins = eng.Time().GetShiftElapsedMinutes();

    for (auto& g : m_guests) {
        if ((g.id == "fake_kurt" || g.id == "fake_kurt4") && eng.State().HasFlag("n2_fake_kurt_resolved")) continue;
        if ((g.id == "katya_n3" || g.id == "arkhip_mimic") && eng.State().HasFlag("n3_mimic_banished")) continue;
        if (!g.spawned && elapsedMins >= static_cast<float>(g.spawnMinuteOffset)) {
            g.spawned = true;
            eng.Physics().SetTriggerEnabled(g.triggerId, true);
            eng.Audio().PlaySfx("audio/sfx/sfx_door_bell.wav", 0.85f);
        }
    }

    if (m_currentNight == 1) {
        if (elapsedMins >= 170.0f && !eng.State().HasFlag("radio_0214_ringing")) {
            eng.State().SetFlag("radio_0214_ringing");
            eng.Audio().PlaySfx("audio/sfx/sfx_radio_static.wav", 0.75f);
        }

        if (elapsedMins >= 300.0f && !eng.State().HasFlag("event_0300_triggered")) {
            eng.State().SetFlag("event_0300_triggered");
            m_lightsFlickering = true;
            eng.Lighting().SetAmbientColor(Color{26, 30, 52, 255});
            eng.Lighting().SetAllPointLightsFlicker(28.0f, 0.85f);
            eng.Audio().PlaySfx("audio/sfx/sfx_cat_hiss.wav", 0.85f);
            eng.Dialogue().StartDialogue("event_light_flicker_0300");
        }

        bool allN1Done = eng.State().HasFlag("kurt_served") &&
                         eng.State().HasFlag("valya_served") &&
                         (eng.State().HasFlag("hatman_served_safe") || eng.State().HasFlag("hatman_served_anomaly")) &&
                         eng.State().HasFlag("trash_taken_out");
        if ((elapsedMins >= 360.0f || allN1Done) && !eng.State().HasFlag("shift_0400_ready")) {
            eng.State().SetFlag("shift_0400_ready");
            eng.Time().SetTimeHHMM(4, 0);
            eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.85f);
        }
    } else if (m_currentNight == 2) {
        if (elapsedMins >= 140.0f && !eng.State().HasFlag("n2_blackout_active") && !eng.State().HasFlag("n2_power_restored")) {
            eng.State().SetFlag("n2_blackout_active");
            m_blackoutMode = true;
            m_lightsFlickering = true;
            eng.Lighting().SetAmbientColor(Color{10, 14, 28, 255});
            eng.Lighting().SetAllPointLightsFlicker(22.0f, 0.95f);
            eng.Lighting().GetFlashlight().enabled = true;
            eng.Renderer().TriggerFlash(Color{214, 69, 69, 140}, 0.35f);
            eng.Renderer().TriggerShake(3.0f, 0.35f);
            eng.Audio().PlaySfx("audio/sfx/sfx_blackout_burst.wav", 0.90f);
        }

        bool allN2Done = eng.State().HasFlag("n2_valya_served") &&
                         eng.State().HasFlag("n2_power_restored") &&
                         eng.State().HasFlag("n2_fake_kurt_resolved") &&
                         eng.State().HasFlag("n2_guest_survived");
        if ((elapsedMins >= 360.0f || allN2Done) && !eng.State().HasFlag("n2_shift_0400_ready")) {
            eng.State().SetFlag("n2_shift_0400_ready");
            eng.Time().SetTimeHHMM(4, 0);
            eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.85f);
        }
    } else if (m_currentNight == 3) {
        if (eng.State().HasFlag("n3_mirror_key_found")) {
            for (auto& g : m_guests) {
                if (g.id == "boss_n3" && !g.spawned) {
                    g.spawned = true;
                    eng.Physics().SetTriggerEnabled(g.triggerId, true);
                    eng.Audio().PlaySfx("audio/sfx/sfx_door_bell.wav", 0.85f);
                }
            }
        }
        if ((elapsedMins >= 360.0f || eng.State().HasFlag("n3_boss_resolved")) && !eng.State().HasFlag("n3_shift_0400_ready")) {
            eng.State().SetFlag("n3_shift_0400_ready");
            eng.Time().SetTimeHHMM(4, 0);
            eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.85f);
        }
    } else if (m_currentNight == 4) {
        bool allN4Done = eng.State().HasFlag("n4_phone_answered") &&
                         eng.State().HasFlag("n4_officer_served") &&
                         eng.State().HasFlag("n4_jukebox_cleared");
        if ((elapsedMins >= 360.0f || allN4Done) && !eng.State().HasFlag("n4_shift_0400_ready")) {
            eng.State().SetFlag("n4_shift_0400_ready");
            eng.Time().SetTimeHHMM(4, 0);
            eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.85f);
        }
    } else if (m_currentNight == 5) {
        if (eng.State().HasFlag("n5_relic_collected")) {
            for (auto& g : m_guests) {
                if (g.id == "boss_n5" && !g.spawned) {
                    g.spawned = true;
                    eng.Physics().SetTriggerEnabled(g.triggerId, true);
                    eng.Audio().PlaySfx("audio/sfx/sfx_door_bell.wav", 0.85f);
                }
            }
        }
        if ((elapsedMins >= 360.0f || eng.State().HasFlag("n5_boss_banished")) && !eng.State().HasFlag("n5_shift_0400_ready")) {
            eng.State().SetFlag("n5_shift_0400_ready");
            eng.Time().SetTimeHHMM(4, 0);
            eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.85f);
        }
    }
}

void LastOrderGame::OnFrameUpdate(engine::core::Engine& eng, float dt) {
    if (IsKeyPressed(KEY_F3) || IsKeyPressed(KEY_F4)) {
        m_debugMode = !m_debugMode;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
    }

    if (m_debugMode && !eng.Dialogue().IsActive()) {
        if (IsKeyPressed(KEY_N)) m_debugNoclip = !m_debugNoclip;
        if (IsKeyPressed(KEY_ONE))   StartNewShift(eng, 1, false);
        if (IsKeyPressed(KEY_TWO))   StartNewShift(eng, 2, false);
        if (IsKeyPressed(KEY_THREE)) StartNewShift(eng, 3, false);
        if (IsKeyPressed(KEY_FOUR))  StartNewShift(eng, 4, false);
        if (IsKeyPressed(KEY_FIVE))  StartNewShift(eng, 5, false);
        if (IsKeyPressed(KEY_F5)) StartMinigame(eng, systems::MinigameType::Night1_RadioTuner2D);
        if (IsKeyPressed(KEY_F6)) StartMinigame(eng, systems::MinigameType::Night2_BasementFuse3D);
        if (IsKeyPressed(KEY_F7)) StartMinigame(eng, systems::MinigameType::Night3_FreezerMaze3D);
        if (IsKeyPressed(KEY_F9)) StartMinigame(eng, systems::MinigameType::Night4_MirrorHunt3D);
        if (IsKeyPressed(KEY_F10)) StartMinigame(eng, systems::MinigameType::Night5_HighwayChase3D);
    }

    if (m_settingsUI.IsOpen()) {
        m_settingsUI.Update(eng);
        return;
    }

    if (m_screen == GameScreen::MainMenu) {
        eng.Renderer().SetSanityStress(0.0f);
        eng.Renderer().SetSkip2DLightmap(false);
        ui::MenuCommand cmd = m_mainMenu.Update(eng, dt);
        switch (cmd) {
            case ui::MenuCommand::StartNight1_IntroTutorial:
                StartNewShift(eng, 1, true);
                break;
            case ui::MenuCommand::StartNight1_NoTutorial:
                StartNewShift(eng, 1, false);
                break;
            case ui::MenuCommand::StartNight2_Independent:
                StartNewShift(eng, 2, false);
                break;
            case ui::MenuCommand::StartNight3_Pipes:
                StartNewShift(eng, 3, false);
                break;
            case ui::MenuCommand::StartNight4_Phone:
                StartNewShift(eng, 4, false);
                break;
            case ui::MenuCommand::StartNight5_Finale:
                StartNewShift(eng, 5, false);
                break;
            case ui::MenuCommand::StartEndlessMode:
                StartEndlessShift(eng);
                break;
            case ui::MenuCommand::OpenSettings:
                m_settingsUI.Open();
                break;
            case ui::MenuCommand::QuitGame:
                eng.RequestQuit();
                break;
            default:
                break;
        }
        return;
    }

    if (m_screen == GameScreen::NightmareChase) {
        if (m_chaseMinigame.GetState() == systems::ChaseState::Victory) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E)) {
                CompleteActiveMinigame(eng);
            }
        } else if (m_chaseMinigame.GetState() == systems::ChaseState::Caught) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                eng.Renderer().SetSkip2DLightmap(false);
                m_screen = (m_currentNight == 0) ? GameScreen::EndlessMode : GameScreen::PlayingShift;
            }
        }
        return;
    }

    if (IsKeyPressed(KEY_V) && !eng.Dialogue().IsActive() && !m_inspectionUI.IsAnyModalOpen()) {
        if (!m_visor3DActive && m_visorBattery > 5.0f) {
            m_visor3DActive = true;
            m_toggledVisorThisFrame = true;
            const float facingRad[4] = {1.5708f, -1.5708f, 3.14159f, 0.0f};
            m_visorYaw = facingRad[m_playerFacing];
            m_visorPitch = 0.0f;
            eng.Renderer().SetSkip2DLightmap(true);
            eng.Audio().PlaySfx("audio/sfx/sfx_visor_toggle.wav", 0.85f);
        } else if (m_visor3DActive) {
            m_visor3DActive = false;
            eng.Renderer().SetSkip2DLightmap(false);
            eng.Audio().PlaySfx("audio/sfx/sfx_visor_toggle.wav", 0.70f);
        }
    }

    if (m_visor3DActive) {
        m_visorBattery = std::max(0.0f, m_visorBattery - dt * 6.5f);
        if (m_visorBattery <= 0.0f) {
            m_visor3DActive = false;
            eng.Renderer().SetSkip2DLightmap(false);
            eng.Audio().PlaySfx("audio/sfx/sfx_visor_toggle.wav", 0.70f);
        }
    } else {
        m_visorBattery = std::min(100.0f, m_visorBattery + dt * 9.0f);
    }

    if (m_screen == GameScreen::EndlessMode) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            m_screen = GameScreen::Paused;
            m_pauseSelectedBtn = 0;
            return;
        }

        m_endlessDirector.Update(eng, dt, m_playerBody.position, eng.Lighting().GetFlashlight().enabled);

        std::string scId;
        if (m_endlessDirector.ShouldTriggerScreamer(scId)) {
            TriggerScreamer(eng, scId);
        }
        if (m_endlessDirector.ShouldTriggerBlackout()) {
            m_blackoutMode = true;
            eng.Lighting().SetAmbientColor(Color{14, 18, 36, 255});
            eng.Lighting().SetAllPointLightsFlicker(18.0f, 0.92f);
        }

        if (m_endlessDirector.IsGameOver()) {
            if (IsKeyPressed(KEY_R)) {
                StartEndlessShift(eng);
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                m_mainMenu.Init(eng);
                m_screen = GameScreen::MainMenu;
            }
            return;
        }

        if (m_isMoving) {
            m_walkAnimTimer += dt * 7.5f;
            m_footstepTimer += dt;
            if (m_footstepTimer >= 0.34f) {
                m_footstepTimer = 0.0f;
                int stepIdx = (static_cast<int>(m_walkAnimTimer) % 4) + 1;
                char path[64];
                std::snprintf(path, sizeof(path), "audio/sfx/sfx_footstep_wood_0%d.wav", stepIdx);
                eng.Audio().PlaySfx(path, 0.38f);
            }
        } else {
            m_walkAnimTimer = 0.0f;
            m_footstepTimer = 0.25f;
        }

        if (IsKeyPressed(KEY_F)) {
            auto& fl = eng.Lighting().GetFlashlight();
            fl.enabled = !fl.enabled;
            eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.7f);
        }

        if (m_nearestTrigger.has_value() && IsKeyPressed(KEY_E)) {
            std::string tid = m_nearestTrigger->id;
            if (tid == "trig_breaker") {
                m_blackoutMode = false;
                m_endlessDirector.RestorePower();
                eng.Lighting().SetAmbientColor(Color{135, 112, 98, 255});
                eng.Lighting().SetAllPointLightsFlicker(2.0f, 0.03f);
                eng.Renderer().TriggerFlash(Color{244, 184, 96, 120}, 0.25f);
                eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.8f);
            } else if (tid == "trig_coffee") {
                eng.State().GiveItem("coffee_sugar", "Чёрный кофе с сахаром");
                m_endlessDirector.OnSanityRestored(10);
                eng.State().AddSanity(10);
                eng.Audio().PlaySfx("audio/sfx/sfx_coffee_sip.wav", 0.85f);
            } else if (tid == "trig_oven") {
                eng.State().GiveItem("cherry_pie", "Тёплый вишнёвый пирог");
                m_endlessDirector.OnSanityRestored(10);
                eng.State().AddSanity(10);
                eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.85f);
            } else if (tid == "trig_cat") {
                m_endlessDirector.OnSanityRestored(12);
                eng.State().AddSanity(12);
                eng.Audio().PlaySfx("audio/sfx/sfx_cat_purr.wav", 0.85f);
            } else {
                m_endlessDirector.InteractWithBooth(eng, tid, eng.Lighting().GetFlashlight().enabled);
            }
        }

        if (IsKeyPressed(KEY_F8) || IsKeyPressed(KEY_G)) {
            const char* scIds[] = {
                "sc_night1_window", "sc_night2_toilet", "sc_night3_basement",
                "sc_night4_fridge", "sc_night5_window_hands", "sc_night6_mirror_hand",
                "sc_night7_guest_mouth", "sc_night8_tv_demon", "sc_night9_shadow_reaper",
                "sc_night10_bloody_mirror", "sc_night11_ceiling_crawler", "sc_night12_corridor_rush",
                "sc_night13_grinning_face", "sc_night14_skull_glitch"
            };
            TriggerScreamer(eng, scIds[m_demoScreamerIdx % 14]);
            m_demoScreamerIdx++;
        }

        m_cooking.Update(eng, dt, m_playerBody.Center());
        return;
    }

    if (m_screen == GameScreen::Paused) {
        UpdatePauseMenu(eng);
        return;
    }

    m_sanity.Update(eng, dt, m_playerBody.position);
    if (m_sanity.IsGameOver()) {
        systems::GameOverCommand goCmd = m_sanity.UpdateGameOverInput(eng);
        if (goCmd == systems::GameOverCommand::RestoreCheckpoint) {
            m_sanity.RestoreCheckpointTo(eng, m_playerBody.position);
        } else if (goCmd == systems::GameOverCommand::RestartNight) {
            StartNewShift(eng, m_currentNight, false);
        } else if (goCmd == systems::GameOverCommand::ReturnToMainMenu) {
            m_mainMenu.Init(eng);
            m_screen = GameScreen::MainMenu;
        }
        return;
    }

    m_cooking.Update(eng, dt, m_playerBody.Center());

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (m_inspectionUI.IsAnyModalOpen()) {
            m_inspectionUI.CloseAll();
        } else if (m_visor3DActive) {
            m_visor3DActive = false;
            eng.Renderer().SetSkip2DLightmap(false);
        } else if (!eng.Dialogue().IsActive()) {
            m_screen = GameScreen::Paused;
            m_pauseSelectedBtn = 0;
            eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.5f);
            return;
        }
    }

    if (m_isMoving) {
        m_walkAnimTimer += dt * 7.5f;
        m_footstepTimer += dt;
        if (m_footstepTimer >= 0.34f) {
            m_footstepTimer = 0.0f;
            int stepIdx = (static_cast<int>(m_walkAnimTimer) % 4) + 1;
            char path[64];
            std::snprintf(path, sizeof(path), "audio/sfx/sfx_footstep_wood_0%d.wav", stepIdx);
            eng.Audio().PlaySfx(path, 0.38f);
        }
    } else {
        m_walkAnimTimer = 0.0f;
        m_footstepTimer = 0.25f;
    }

    if (!eng.Dialogue().IsActive()) {
        if (IsKeyPressed(KEY_TAB)) {
            m_inspectionUI.ToggleNotebook(eng);
        }
        if (IsKeyPressed(KEY_R)) {
            m_inspectionUI.ToggleRulesSheet(eng);
            eng.State().SetFlag("read_wall_rules");
        }
    }

    if (!eng.Dialogue().IsActive() && !m_inspectionUI.IsAnyModalOpen() && IsKeyPressed(KEY_E)) {
        if (m_nearestTrigger.has_value() && !m_nearestTrigger->dialogueId.empty()) {
            eng.Dialogue().StartDialogue(m_nearestTrigger->dialogueId);
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.45f);
        }
    }

    if (IsKeyPressed(KEY_F)) {
        auto& fl = eng.Lighting().GetFlashlight();
        fl.enabled = !fl.enabled;
        m_toggledFlashlightThisFrame = true;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.7f);
    }

    if (IsKeyPressed(KEY_L)) {
        m_blackoutMode = !m_blackoutMode;
        if (m_blackoutMode) {
            eng.Lighting().SetAmbientColor(Color{14, 18, 36, 255});
            eng.Lighting().SetAllPointLightsFlicker(18.0f, 0.92f);
        } else {
            eng.Lighting().SetAmbientColor(Color{135, 112, 98, 255});
            eng.Lighting().SetAllPointLightsFlicker(2.0f, 0.03f);
        }
    }

    if (IsKeyPressed(KEY_F2)) {
        int h = eng.Time().GetHours24();
        int m = eng.Time().GetMinutes() + 30;
        eng.Time().SetTimeHHMM((h + m / 60) % 24, m % 60);
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.7f);
    }

    if ((IsKeyPressed(KEY_G) || IsKeyPressed(KEY_F8)) && !eng.Dialogue().IsActive()) {
        const char* scIds[] = {
            "sc_night1_window", "sc_night2_toilet", "sc_night3_basement",
            "sc_night4_fridge", "sc_night5_window_hands", "sc_night6_mirror_hand",
            "sc_night7_guest_mouth", "sc_night8_tv_demon", "sc_night9_shadow_reaper",
            "sc_night10_bloody_mirror", "sc_night11_ceiling_crawler", "sc_night12_corridor_rush",
            "sc_night13_grinning_face", "sc_night14_skull_glitch"
        };
        TriggerScreamer(eng, scIds[m_demoScreamerIdx % 14]);
        m_demoScreamerIdx++;
    }

    m_tutorial.Update(eng, m_playerBody.Center(), m_distMovedThisFrame,
                      m_toggledFlashlightThisFrame, m_toggledVisorThisFrame);

    m_distMovedThisFrame = 0.0f;
    m_toggledFlashlightThisFrame = false;
    m_toggledVisorThisFrame = false;
}

ui::MirrorWorldState LastOrderGame::BuildMirrorState(const engine::core::Engine& eng) const {
    ui::MirrorWorldState ms{};
    ms.eyelessLera = eng.State().HasFlag("event_0300_triggered") || m_blackoutMode || eng.State().GetSanity() < 45;
    ms.showVeraSilhouette = (m_currentNight >= 2) || eng.State().HasFlag("event_0300_triggered");

    for (const auto& g : m_guests) {
        if (!g.spawned) continue;
        if (g.triggerId == "trig_booth1" || g.triggerId == "trig_booth2") {
            ms.guestAtTable2 = true;
            ms.table2HasReflection = g.hasReflection;
        } else if (g.triggerId == "trig_booth3" || g.triggerId == "trig_booth5") {
            ms.guestAtTable1 = true;
            ms.table1HasReflection = g.hasReflection;
        } else if (g.triggerId == "trig_table6") {
            ms.guestAtTable6 = true;
        }
    }
    return ms;
}

void LastOrderGame::UpdatePauseMenu(engine::core::Engine& eng) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        m_screen = (m_currentNight == 0) ? GameScreen::EndlessMode : GameScreen::PlayingShift;
        return;
    }

    constexpr int COUNT = 8;
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
        m_pauseSelectedBtn = (m_pauseSelectedBtn - 1 + COUNT) % COUNT;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.45f);
    }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
        m_pauseSelectedBtn = (m_pauseSelectedBtn + 1) % COUNT;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.45f);
    }

    bool activate = (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE));
    engine::math::Vec2 mpos = eng.Renderer().GetVirtualMousePos();

    for (int i = 0; i < COUNT; ++i) {
        engine::math::Rect r = {380.0f, 175.0f + i * 48.0f, 520.0f, 42.0f};
        if (r.Contains(mpos)) {
            m_pauseSelectedBtn = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) activate = true;
        }
    }

    if (activate) {
        eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.55f);
        switch (m_pauseSelectedBtn) {
            case 0: m_screen = (m_currentNight == 0) ? GameScreen::EndlessMode : GameScreen::PlayingShift; break;
            case 1: m_settingsUI.Open(); break;
            case 2: StartNewShift(eng, 1, false); break;
            case 3: StartNewShift(eng, 2, false); break;
            case 4: StartNewShift(eng, 3, false); break;
            case 5: StartNewShift(eng, 4, false); break;
            case 6: StartNewShift(eng, 5, false); break;
            case 7:
                m_mainMenu.Init(eng);
                m_screen = GameScreen::MainMenu;
                break;
        }
    }
}

void LastOrderGame::RenderPauseMenu(engine::core::Engine& eng) const {
    Font font = eng.Assets().GetMainFont();
    DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 212});
    DrawRectangle(340, 95, 600, 525, Color{42, 23, 21, 248});
    DrawRectangleLinesEx(Rectangle{340, 95, 600, 525}, 3.0f, Color{244, 184, 96, 255});

    std::string title =
        (m_currentNight == 0) ? "ПАУЗА — СВОБОДНЫЙ РЕЖИМ" :
        (m_currentNight == 1) ? "ПАУЗА — НОЧЬ 1" :
        (m_currentNight == 2) ? "ПАУЗА — НОЧЬ 2" :
        (m_currentNight == 3) ? "ПАУЗА — НОЧЬ 3" :
        (m_currentNight == 4) ? "ПАУЗА — НОЧЬ 4" :
                                "ПАУЗА — НОЧЬ 5";
    engine::graphics::TextLayout::DrawCentered(font, title, 640.0f, 125.0f, 24.0f, Color{244, 184, 96, 255}, 1.0f);

    const char* labels[8] = {
        "Продолжить смену (ESC)",
        "Настройки",
        "Перезапустить Ночь 1",
        "Перезапустить Ночь 2",
        "Перезапустить Ночь 3",
        "Перезапустить Ночь 4",
        "Перезапустить Ночь 5",
        "Выйти в главное меню"
    };

    for (int i = 0; i < 8; ++i) {
        bool sel = (m_pauseSelectedBtn == i);
        float bx = 380.0f;
        float by = 175.0f + i * 48.0f;
        DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 520, 42,
                      sel ? Color{150, 82, 46, 255} : Color{107, 51, 34, 235});
        DrawRectangleLinesEx(Rectangle{bx, by, 520, 42}, sel ? 2.0f : 1.0f, Color{244, 184, 96, 255});
        DrawTextEx(font, labels[i], Vector2{bx + 18.0f, by + 11.0f}, 18.0f, 1.0f,
                   sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 235});
    }
}

void LastOrderGame::OnAction(engine::core::Engine& eng, const engine::core::EngineAction& action) {
    if (action.type == "give_item") {
        m_cooking.StartPreparation(eng, action.target, 1.6f);
    } else if (action.type == "set_flag") {
        if (action.target == "kurt_served" || action.target == "n3_gleb_served") {
            m_cooking.PlaceDishOnTable("booth_1", "coffee_sugar");
        } else if (action.target == "katya_served") {
            m_cooking.PlaceDishOnTable("booth_3", "tea_sugar");
        } else if (action.target == "n2_arkhip_served" || action.target == "n4_officer_served") {
            m_cooking.PlaceDishOnTable("booth_3", "coffee_black");
        } else if (action.target == "valya_served" || action.target == "n2_valya_served") {
            m_cooking.PlaceDishOnTable("booth_2", "cherry_pie");
        } else if (action.target == "hatman_served_safe" || action.target == "hatman_served_anomaly") {
            m_cooking.PlaceDishOnTable("booth_5", "tea_sugar");
        } else if (action.target == "night1_completed") {
            m_sanity.MarkNightCompleted(1);
        } else if (action.target == "night2_completed") {
            m_sanity.MarkNightCompleted(2);
        } else if (action.target == "night3_completed") {
            m_sanity.MarkNightCompleted(3);
        } else if (action.target == "night4_completed") {
            m_sanity.MarkNightCompleted(4);
        } else if (action.target == "night5_completed") {
            m_sanity.MarkNightCompleted(5);
        }
    } else if (action.type == "screamer") {
        TriggerScreamer(eng, action.target);
    } else if (action.type == "event") {
        if (action.target == "stabilize_lights") {
            m_lightsFlickering = false;
            m_blackoutMode = false;
            eng.Lighting().SetAmbientColor((m_currentNight == 1) ? Color{135, 112, 98, 255} : Color{112, 96, 92, 255});
            eng.Lighting().SetAllPointLightsFlicker(2.0f, 0.03f);
            eng.Renderer().TriggerFlash(Color{244, 184, 96, 120}, 0.25f);
        } else if (action.target == "flash_dark") {
            eng.Renderer().TriggerFlash(Color{10, 13, 26, 255}, 0.65f);
            eng.Renderer().TriggerShake(3.5f, 0.45f);
        } else if (action.target == "banish_fake_kurt") {
            eng.Renderer().TriggerFlash(Color{168, 36, 36, 240}, 0.65f);
            eng.Renderer().TriggerShake(4.2f, 0.55f);
            for (auto& g : m_guests) {
                if (g.anomalous) {
                    g.spawned = false;
                    eng.Physics().SetTriggerEnabled(g.triggerId, false);
                }
            }
        } else if (action.target == "start_minigame_n1") {
            StartMinigame(eng, systems::MinigameType::Night1_RadioTuner2D);
        } else if (action.target == "start_minigame_n2") {
            StartMinigame(eng, systems::MinigameType::Night2_BasementFuse3D);
        } else if (action.target == "start_minigame_n3") {
            StartMinigame(eng, systems::MinigameType::Night3_FreezerMaze3D);
        } else if (action.target == "start_minigame_n4") {
            StartMinigame(eng, systems::MinigameType::Night4_MirrorHunt3D);
        } else if (action.target == "start_minigame_n5") {
            StartMinigame(eng, systems::MinigameType::Night5_HighwayChase3D);
        } else if (action.target == "start_night_2") {
            StartNewShift(eng, 2, false);
        } else if (action.target == "start_night_3") {
            StartNewShift(eng, 3, false);
        } else if (action.target == "start_night_4") {
            StartNewShift(eng, 4, false);
        } else if (action.target == "start_night_5") {
            StartNewShift(eng, 5, false);
        } else if (action.target == "return_main_menu") {
            m_mainMenu.Init(eng);
            m_screen = GameScreen::MainMenu;
        }
    }
}

void LastOrderGame::TriggerScreamer(engine::core::Engine& eng, const std::string& screamerId) {
    std::string texPath = "sprites/screamers/" + screamerId + ".png";
    Texture2D tex = eng.Assets().GetTexture(texPath);
    eng.Renderer().TriggerScreamerOverlay(tex, 1.50f, true);
    eng.Renderer().TriggerShake(5.5f, 1.25f);
    eng.Time().PauseFor(1.1f);

    if (screamerId == "sc_night1_window")                 eng.Audio().PlayScreamerStinger("sfx_sc_01_window_gasp.wav");
    else if (screamerId == "sc_night2_toilet")            eng.Audio().PlayScreamerStinger("sfx_sc_02_whisper_hinge.wav");
    else if (screamerId == "sc_night3_basement")          eng.Audio().PlayScreamerStinger("sfx_sc_03_string_screech.wav");
    else if (screamerId == "sc_night4_fridge")            eng.Audio().PlayScreamerStinger("sfx_sc_04_meat_drop.wav");
    else if (screamerId == "sc_night5_window_hands")      eng.Audio().PlayScreamerStinger("sfx_sc_05_glass_pound.wav");
    else if (screamerId == "sc_night6_mirror_hand")       eng.Audio().PlayScreamerStinger("sfx_sc_06_mirror_crack.wav");
    else if (screamerId == "sc_night7_guest_mouth")       eng.Audio().PlayScreamerStinger("sfx_sc_07_clock_chord.wav");
    else if (screamerId == "sc_night8_tv_demon")          eng.Audio().PlayScreamerStinger("sfx_sc_08_tv_static_shriek.wav");
    else if (screamerId == "sc_night9_shadow_reaper")     eng.Audio().PlayScreamerStinger("sfx_sc_09_shadow_roar.wav");
    else if (screamerId == "sc_night10_bloody_mirror")    eng.Audio().PlayScreamerStinger("sfx_sc_10_mirror_agony.wav");
    else if (screamerId == "sc_night11_ceiling_crawler")  eng.Audio().PlayScreamerStinger("sfx_sc_11_crawler_snap.wav");
    else if (screamerId == "sc_night12_corridor_rush")    eng.Audio().PlayScreamerStinger("sfx_sc_12_chase_breath.wav");
    else if (screamerId == "sc_night13_grinning_face")    eng.Audio().PlayScreamerStinger("sfx_sc_13_blood_curdle.wav");
    else if (screamerId == "sc_night14_skull_glitch")     eng.Audio().PlayScreamerStinger("sfx_sc_14_skull_glitch.wav");
    else                                                  eng.Audio().PlayScreamerStinger("sfx_sc_07_clock_chord.wav");
}

void LastOrderGame::OnRenderWorld(engine::core::Engine& eng) {
    if (m_screen == GameScreen::MainMenu) {
        m_mainMenu.RenderWorld(eng);
        return;
    }
    if (m_screen == GameScreen::NightmareChase) {
        m_chaseMinigame.RenderWorld(eng);
        return;
    }

    int nightToRender = (m_screen == GameScreen::EndlessMode) ? 2 : m_currentNight;
    if (m_visor3DActive) {
        m_world.Render3DVisor(eng, nightToRender, m_playerBody.Center(),
                              m_visorYaw, m_visorPitch, m_guests);
    } else {
        m_world.Render(eng, nightToRender,
                       m_playerBody, m_playerFacing, m_isMoving, m_walkAnimTimer,
                       m_lightsFlickering, m_blackoutMode, m_debugMode, m_nearestTrigger, m_guests);
        m_cooking.RenderWorld(eng, m_world.ComputeCameraOffset(m_playerBody.Center()));
    }
}

void LastOrderGame::RenderDebugOverlay(engine::core::Engine& eng) const {
    if (!m_debugMode) return;
    Font font = eng.Assets().GetMainFont();

    DrawRectangle(14, 108, 415, 212, Color{10, 18, 12, 235});
    DrawRectangleLinesEx(Rectangle{14, 108, 415, 212}, 2.0f, Color{80, 240, 120, 255});

    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "DEBUG MODE [F3] | FPS: %d | Ночь: %d\n"
                  "Позиция: (%.1f, %.1f) | Камера: (%.1f, %.1f)\n"
                  "Коллизии: ВКЛ | Noclip [N]: %s | 3D-Визор [V]: %s\n"
                  "Триггер: %s\n"
                  "[1..5] Сменить Ночь | [F2] +30 мин | [L] Блэкаут\n"
                  "[F5..F7, F9, F10] Запуск 2D/3D Мини-игр 1..5\n"
                  "[F8 / G] Тест 14 скримеров (1.5 сек MAX ЗВУК)",
                  GetFPS(), m_currentNight,
                  m_playerBody.Center().x, m_playerBody.Center().y,
                  m_world.ComputeCameraOffset(m_playerBody.Center()).x, m_world.ComputeCameraOffset(m_playerBody.Center()).y,
                  m_debugNoclip ? "ВКЛ" : "ВЫКЛ",
                  m_visor3DActive ? "3D ВКЛ" : "2D",
                  m_nearestTrigger.has_value() ? m_nearestTrigger->id.c_str() : "нет");

    DrawTextEx(font, buf, Vector2{24, 116}, 16.5f, 1.0f, Color{110, 255, 140, 255});
}

void LastOrderGame::OnRenderUI(engine::core::Engine& eng) {
    if (m_screen == GameScreen::MainMenu) {
        m_mainMenu.RenderUI(eng);
        m_settingsUI.Render(eng);
        return;
    }
    if (m_screen == GameScreen::NightmareChase) {
        m_chaseMinigame.RenderUI(eng);
        m_settingsUI.Render(eng);
        return;
    }

    Font font = eng.Assets().GetMainFont();

    if (m_visor3DActive) {
        DrawRectangleLinesEx(Rectangle{12, 12, 1256, 696}, 4.0f, Color{214, 69, 69, 200});
        DrawCircleLines(640, 360, 18.0f, Color{214, 69, 69, 180});
        DrawLine(620, 360, 660, 360, Color{214, 69, 69, 180});
        DrawLine(640, 340, 640, 380, Color{214, 69, 69, 180});
        char vBuf[128];
        std::snprintf(vBuf, sizeof(vBuf), "3D-ВИЗОР «ОКО-86» АКТИВЕН (ЗАРЯД: %d%%) — [Q/E/Мышь] Поворот | [V] Выключить",
                      static_cast<int>(m_visorBattery));
        DrawRectangle(340, 62, 600, 34, Color{24, 8, 12, 230});
        DrawRectangleLines(340, 62, 600, 34, Color{214, 69, 69, 255});
        engine::graphics::TextLayout::DrawCentered(font, vBuf, 640.0f, 70.0f, 17.5f, Color{255, 110, 110, 255});
    }

    if (m_screen == GameScreen::EndlessMode) {
        m_endlessDirector.RenderUI(eng);
        m_cooking.RenderUI(eng, m_world.ComputeCameraOffset(m_playerBody.Center()));
        RenderDebugOverlay(eng);
        m_settingsUI.Render(eng);
        return;
    }

    m_tutorial.RenderUIOverlay(eng);
    m_cooking.RenderUI(eng, m_world.ComputeCameraOffset(m_playerBody.Center()));

    DrawRectangle(1018, 12, 248, 86, Color{10, 13, 26, 238});
    DrawRectangleLinesEx(Rectangle{1018, 12, 248, 86}, 2.0f, Color{196, 122, 59, 255});

    const char* nightTitle =
        (m_currentNight == 1) ? "НОЧЬ 1: СМЕНА" :
        (m_currentNight == 2) ? "НОЧЬ 2: ДОЖДЬ" :
        (m_currentNight == 3) ? "НОЧЬ 3: ТРУБЫ" :
        (m_currentNight == 4) ? "НОЧЬ 4: ЗВОНОК" :
                                "НОЧЬ 5: ФИНАЛ";
    DrawTextEx(font, nightTitle, Vector2{1032, 18}, 16.5f, 1.0f, Color{143, 169, 196, 255});

    std::string timeStr = m_sanity.FormatHudClock(eng);
    DrawTextEx(font, timeStr.c_str(), Vector2{1032, 40}, 24.0f, 1.0f, Color{244, 184, 96, 255});

    int san = eng.State().GetSanity();
    char sanBuf[64];
    std::snprintf(sanBuf, sizeof(sanBuf), "Рассудок: %d%%", san);
    Color sanCol = (san < 40) ? Color{214, 69, 69, 255} : Color{232, 227, 217, 255};
    DrawTextEx(font, sanBuf, Vector2{1032, 68}, 19.0f, 1.0f, sanCol);

    DrawRectangle(14, 12, 310, 86, Color{10, 13, 26, 238});
    DrawRectangleLinesEx(Rectangle{14, 12, 310, 86}, 2.0f, Color{196, 122, 59, 255});
    std::string held = eng.State().GetHeldItemName();
    std::string trayText = "В руках: " + (held.empty() ? std::string("Пусто") : held);
    DrawTextEx(font, trayText.c_str(), Vector2{28, 22}, 19.0f, 1.0f, Color{244, 184, 96, 255});

    char visorSub[96];
    std::snprintf(visorSub, sizeof(visorSub), "[V] 3D-Визор (%d%%) | [TAB] Задачи | [R] Устав", static_cast<int>(m_visorBattery));
    DrawTextEx(font, visorSub, Vector2{28, 56}, 15.5f, 1.0f, Color{188, 178, 162, 255});

    if (!eng.Dialogue().IsActive() && !m_inspectionUI.IsAnyModalOpen() && !m_sanity.IsGameOver() && m_nearestTrigger.has_value()) {
        std::string prompt = "[E] " + m_nearestTrigger->label;
        Vector2 sz = MeasureTextEx(font, prompt.c_str(), 22.0f, 1.0f);
        float px = (1280.0f - sz.x) * 0.5f;
        DrawRectangle(static_cast<int>(px - 18), 618, static_cast<int>(sz.x + 36), 40, Color{42, 23, 21, 245});
        DrawRectangleLinesEx(Rectangle{px - 18, 618, sz.x + 36, 40}, 2.0f, Color{244, 184, 96, 255});
        DrawTextEx(font, prompt.c_str(), Vector2{px, 627}, 22.0f, 1.0f, Color{244, 184, 96, 255});
    }

    m_inspectionUI.Render(eng, m_currentNight, BuildMirrorState(eng));
    RenderDebugOverlay(eng);

    const char* controls = "WASD: Ходьба | E: Действие | F: Фонарик | V: 3D-Визор «ОКО-86» | TAB: Тетрадь | R: Устав | F3: Debug | ESC: Пауза";
    DrawRectangle(0, 688, 1280, 32, Color{10, 13, 26, 235});
    DrawTextEx(font, controls, Vector2{18, 694}, 16.5f, 1.0f, Color{188, 178, 162, 235});

    if (m_sanity.IsGameOver()) {
        m_sanity.RenderGameOverModal(eng, m_currentNight);
    } else if (m_screen == GameScreen::Paused) {
        RenderPauseMenu(eng);
    }
    m_settingsUI.Render(eng);
}

void LastOrderGame::OnShutdown(engine::core::Engine& ) {}

}
