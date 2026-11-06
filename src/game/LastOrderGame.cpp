#include "game/systems/SaveSystem.h"
#include "game/LastOrderGame.h"
#include "engine/core/PathUtils.h"
#include "game/chapter3/Ch3Graphics.h"
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include "engine/core/PathUtils.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace game {

static const char* SCREAMER_IDS[14] = {
    "sc_night1_window", "sc_night2_toilet", "sc_night3_basement",
    "sc_night4_fridge", "sc_night5_window_hands", "sc_night6_mirror_hand",
    "sc_night7_guest_mouth", "sc_night8_tv_demon", "sc_night9_shadow_reaper",
    "sc_night10_bloody_mirror", "sc_night11_ceiling_crawler", "sc_night12_corridor_rush",
    "sc_night13_grinning_face", "sc_night14_skull_glitch"
};

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

void LastOrderGame::SetVisor3DActive(engine::core::Engine& eng, bool active) {
    if (m_visor3DActive == active) return;
    m_visor3DActive = active;
    eng.Renderer().SetSkip2DLightmap(active);
    if (active) {
        const float facingRad[4] = {1.5708f, -1.5708f, 3.14159f, 0.0f};
        m_visorYaw = facingRad[std::clamp(m_playerFacing, 0, 3)];
        m_visorPitch = 0.0f;
        DisableCursor();
        eng.Audio().PlaySfx("audio/sfx/sfx_visor_toggle.wav", 0.85f);
    } else {
        if (IsCursorHidden()) EnableCursor();
        eng.Audio().PlaySfx("audio/sfx/sfx_visor_toggle.wav", 0.70f);

        float normYaw = std::fmod(m_visorYaw, 6.2831853f);
        if (normYaw < 0.0f) normYaw += 6.2831853f;
        if (normYaw >= 5.4978f || normYaw < 0.7854f)       m_playerFacing = 3;
        else if (normYaw >= 0.7854f && normYaw < 2.3562f)  m_playerFacing = 0;
        else if (normYaw >= 2.3562f && normYaw < 3.9270f)  m_playerFacing = 2;
        else                                               m_playerFacing = 1;

        m_playerBody.velocity = {0.0f, 0.0f};
        m_isMoving = false;
        m_playerBody.position.x = std::clamp(m_playerBody.position.x, 12.0f, 296.0f);
        m_playerBody.position.y = std::clamp(m_playerBody.position.y, 4.0f, 156.0f);
        m_nearestTrigger = eng.Physics().FindNearestTrigger(m_playerBody.Center(), 26.0f);
    }
}

void LastOrderGame::SaveCurrentGame(engine::core::Engine& eng, int slotIndex) {
    systems::SaveProfile sp{};
    sp.chapter = m_currentChapter;
    sp.currentNight = m_currentNight;
    sp.hour = eng.Time().GetHours24();
    sp.minute = eng.Time().GetMinutes();
    sp.elapsedMinutes = eng.Time().GetShiftElapsedMinutes();
    sp.sanity = eng.State().GetSanity();
    sp.heldItem = eng.State().GetHeldItem();
    sp.heldItemName = eng.State().GetHeldItemName();
    sp.playerX = m_playerBody.position.x;
    sp.playerY = m_playerBody.position.y;
    sp.playerFacing = m_playerFacing;
    sp.maxUnlockedNight = std::max(m_maxUnlockedNight, m_currentNight);
    for (const auto& f : eng.State().GetAllFlags()) {
        sp.flags.push_back(f);
    }
    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", sp.hour, sp.minute);
    if (slotIndex == 0) {
        sp.slotTitle = "Быстрое [F5]: Ночь " + std::to_string(m_currentNight) + ", " + timeBuf;
    } else if (slotIndex == 1) {
        sp.slotTitle = "Авто: Начало Ночи " + std::to_string(m_currentNight);
    } else {
        sp.slotTitle = "Авто: Ночь " + std::to_string(m_currentNight) + ", " + timeBuf;
    }
    sp.valid = true;

    if (systems::SaveSystem::SaveSlot(slotIndex, sp)) {
        if (slotIndex == 0) m_saveBannerText = "ИГРА СОХРАНЕНА [СЛОТ 1 — БЫСТРОЕ]";
        else if (slotIndex == 1) m_saveBannerText = "АВТОСОХРАНЕНИЕ СМЕНЫ [СЛОТ 2]";
        else m_saveBannerText = "АВТОСОХРАНЕНИЕ [СЛОТ 3]";
        m_saveBannerTimer = 2.8f;
        eng.Audio().PlaySfx("audio/sfx/sfx_cash_register.wav", 0.85f);
    }
}

void LastOrderGame::SaveKeyMoment(engine::core::Engine& eng, const std::string& momentTitle) {
    systems::SaveProfile sp{};
    sp.chapter = m_currentChapter;
    sp.currentNight = m_currentNight;
    sp.hour = eng.Time().GetHours24();
    sp.minute = eng.Time().GetMinutes();
    sp.elapsedMinutes = eng.Time().GetShiftElapsedMinutes();
    sp.sanity = eng.State().GetSanity();
    sp.heldItem = eng.State().GetHeldItem();
    sp.heldItemName = eng.State().GetHeldItemName();
    sp.playerX = m_playerBody.position.x;
    sp.playerY = m_playerBody.position.y;
    sp.playerFacing = m_playerFacing;
    sp.maxUnlockedNight = std::max(m_maxUnlockedNight, m_currentNight);
    for (const auto& f : eng.State().GetAllFlags()) {
        sp.flags.push_back(f);
    }
    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", sp.hour, sp.minute);
    sp.slotTitle = "Ключ: " + momentTitle + " (" + timeBuf + ")";
    sp.valid = true;

    systems::SaveSystem::SaveSlot(2, sp);
    m_saveBannerText = "СОХРАНЁН МОМЕНТ: " + momentTitle;
    m_saveBannerTimer = 2.8f;
    eng.Audio().PlaySfx("audio/sfx/sfx_cash_register.wav", 0.75f);
}

void LastOrderGame::LoadSavedGame(engine::core::Engine& eng, int slotIndex) {
    systems::SaveProfile sp{};
    bool loaded = (slotIndex >= 0) ? systems::SaveSystem::LoadSlot(slotIndex, sp) : systems::SaveSystem::Load(sp);
    if (!loaded || !sp.valid) {
        StartNewShift(eng, 1, false);
        return;
    }

    m_currentChapter = sp.chapter;
    StartNewShift(eng, sp.currentNight, false);
    eng.Time().Reset(sp.hour, sp.minute, 1.80f);
    m_playerBody.position = {sp.playerX, sp.playerY};
    m_playerFacing = sp.playerFacing;
    m_maxUnlockedNight = std::max(m_maxUnlockedNight, sp.maxUnlockedNight);

    eng.State().Reset();
    eng.State().SetSanity(sp.sanity);
    if (!sp.heldItem.empty()) {
        eng.State().GiveItem(sp.heldItem, sp.heldItemName);
    }
    for (const auto& f : sp.flags) {
        eng.State().SetFlag(f);
    }

    if (eng.State().HasFlag("jukebox_playing")) {
        eng.Audio().PlayAmbient("audio/bgm/mus_jukebox_retro.wav", 0.85f);
    }

    m_screen = (sp.currentNight == 0) ? GameScreen::EndlessMode : GameScreen::PlayingShift;
    m_saveBannerText = "ЗАГРУЖЕНО: " + sp.slotTitle;
    m_saveBannerTimer = 2.8f;
    eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.85f);
}

void LastOrderGame::SpawnInitial3DAnomalies(int nightNumber) {
    m_anomalies3D.clear();
    m_nextAnomalyId = 1;
    m_anomalySpawnTimer = 55.0f;
    m_scannerTargetText.clear();

    if (nightNumber <= 1) {
        return;
    }

    m_anomalies3D.push_back({m_nextAnomalyId++, 0, "Кровавый Разлом", {34.0f,  98.0f},  1.15f, 2, 2, 0.0f, true});
    m_anomalies3D.push_back({m_nextAnomalyId++, 1, "Теневой Сталкер", {282.0f, 146.0f}, 1.05f, 2 + nightNumber / 3, 2 + nightNumber / 3, 0.0f, true});
}

void LastOrderGame::Spawn3DShooterWave(engine::core::Engine& eng, int count) {
    static const engine::math::Vec2 spawnSpots[8] = {
        {32.0f,  46.0f},
        {32.0f,  146.0f},
        {108.0f, 104.0f},
        {188.0f, 104.0f},
        {268.0f, 104.0f},
        {284.0f, 62.0f},
        {284.0f, 148.0f},
        {156.0f, 154.0f}
    };

    m_anomalies3D.erase(
        std::remove_if(m_anomalies3D.begin(), m_anomalies3D.end(),
                       [](const world::VisorAnomaly3D& a) { return !a.active; }),
        m_anomalies3D.end());
    if (static_cast<int>(m_anomalies3D.size()) >= 6) return;

    for (int i = 0; i < count; ++i) {
        const auto& sp = spawnSpots[(std::rand() + i) % 8];
        int type = (i % 3 == 0) ? 1 : ((i % 3 == 1) ? 2 : 0);
        const char* name = (type == 1) ? "Теневой Сталкер" : ((type == 2) ? "Череп-Фантом" : "Кровавый Разлом");
        int hp = (type == 1) ? 3 : 2;
        m_anomalies3D.push_back({
            m_nextAnomalyId++,
            type,
            name,
            {sp.x + static_cast<float>((std::rand() % 13) - 6), sp.y + static_cast<float>((std::rand() % 13) - 6)},
            (type == 1) ? 1.05f : 1.20f,
            hp,
            hp,
            0.0f,
            true
        });
    }
    m_visorKillBanner = "ВОЛНА ТЕНЕЙ В ЗАЛЕ!";
    m_visorKillBannerTimer = 2.6f;
    eng.Audio().PlaySfx("audio/sfx/sfx_phantom_steps.wav", 0.85f);
}

void LastOrderGame::StartEndlessShift(engine::core::Engine& eng) {
    m_currentNight = 0;
    eng.Dialogue().CloseDialogue();
    eng.State().Reset();
    eng.State().ClearFlag("shift_sanity_restored");
    m_endlessSanityCooldownTimer = 0.0f;
    eng.Time().Reset(23, 0, 1.80f);

    m_playerBody.position = {112.0f, 48.0f};
    m_playerBody.colliderOffset = {9.0f, 18.0f};
    m_playerBody.colliderSize = {14.0f, 11.0f};
    m_playerBody.velocity = {0.0f, 0.0f};
    m_playerFacing = 0;
    m_blackoutMode = false;
    m_lightsFlickering = false;
    SetVisor3DActive(eng, false);

    m_cooking.Reset();
    m_sanity.ResetForShift(m_playerBody.position);
    m_inspectionUI.Reset();
    m_endlessDirector.Reset();

    m_world.BuildCollidersAndTriggers(eng, 2);
    m_world.BuildLighting(eng, 2);
    SpawnInitial3DAnomalies(3);

    eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.85f);
    m_guests.clear();

    m_tutorial.Reset(1, false);
    m_screen = GameScreen::EndlessMode;
}

void LastOrderGame::StartMinigame(engine::core::Engine& eng, systems::MinigameType type) {
    eng.Dialogue().CloseDialogue();
    SetVisor3DActive(eng, false);
    m_chaseMinigame.Start(type);
    bool is3D = (type != systems::MinigameType::Night1_RadioTuner2D);
    eng.Renderer().SetSkip2DLightmap(is3D);
    eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.90f);
    m_screen = GameScreen::NightmareChase;
}

void LastOrderGame::CompleteActiveMinigame(engine::core::Engine& eng) {
    eng.Renderer().SetSkip2DLightmap(false);
    eng.State().AddSanity(10);
    systems::MinigameType t = m_chaseMinigame.GetType();
    if (t == systems::MinigameType::Night1_RadioTuner2D) {
        eng.State().SetFlag("heard_radio_0214");
        eng.Physics().UpdateTriggerDialogue("trig_radio", "radio_tuned", "Радиоприёмник [94.5 FM]");
        SaveKeyMoment(eng, "Радио 94.5 FM настроено");
        if (m_currentNight == 3) {
            eng.State().SetFlag("n3_radio_tuned_mikhail");
            eng.State().ClearFlag("n3_radio_urgent_tune");
            m_lightsFlickering = false;
            eng.Dialogue().StartDialogue("n3_mikhail_radio_instructions");
        }
    } else if (t == systems::MinigameType::Night2_BasementFuse3D) {
        eng.State().SetFlag("n2_power_restored");
        m_lightsFlickering = false;
        m_blackoutMode = false;
        eng.Lighting().SetAmbientColor(Color{112, 96, 92, 255});
        eng.Lighting().SetAllPointLightsFlicker(2.0f, 0.03f);
        SaveKeyMoment(eng, "Электросеть восстановлена");
    } else if (t == systems::MinigameType::Night3_FreezerMaze3D) {
        eng.State().SetFlag("n3_fridge_checked");
        eng.State().SetFlag("n3_basement_checked");
        if (eng.State().HasFlag("n3_mirror_key_found")) {
            eng.State().SetFlag("n3_artifacts_ready");
        }
        SaveKeyMoment(eng, "Чек 1986 года найден");
    } else if (t == systems::MinigameType::Night4_MirrorHunt3D) {
        eng.State().SetFlag("n4_jukebox_cleared");
        m_lightsFlickering = false;
        m_blackoutMode = false;
        eng.Lighting().SetAmbientColor(Color{112, 96, 92, 255});
        eng.Lighting().SetAllPointLightsFlicker(2.0f, 0.03f);
        SaveKeyMoment(eng, "Зазеркалье очищено");
    } else if (t == systems::MinigameType::Night5_HighwayChase3D) {
        eng.State().SetFlag("n5_relic_collected");
        eng.State().SetFlag("n3_mirror_key_found");
        eng.State().SetFlag("chapter2_completed");
        SaveKeyMoment(eng, "Погоня на шоссе пройдена");
        eng.Renderer().SetSkip2DLightmap(false);
        m_screen = GameScreen::MainMenu;
        m_mainMenu.Init(eng);
        EnableCursor();
        return;
    } else if (t == systems::MinigameType::Night_JukeboxTuner2D) {
        eng.State().SetFlag("jukebox_playing");
        eng.Audio().PlayAmbient("audio/bgm/mus_jukebox_retro.wav", 0.85f);
        eng.State().AddSanity(12);
        eng.Renderer().TriggerFlash(Color{244, 184, 96, 200}, 0.35f);
        eng.Audio().PlaySfx("audio/sfx/sfx_cash_register.wav", 0.85f);
        m_saveBannerText = "ПЛАСТИНКА ИГРАЕТ! РАССУДОК +12%";
        m_saveBannerTimer = 3.0f;
        SaveKeyMoment(eng, "Мелодия-86 запущена");
    }
    m_world.BuildLighting(eng, std::max(1, m_currentNight));
    m_screen = (m_currentNight == 0) ? GameScreen::EndlessMode : GameScreen::PlayingShift;
}

void LastOrderGame::StartNewShift(engine::core::Engine& eng, int nightNumber, bool withTutorial) {
    m_currentNight = nightNumber;
    eng.Dialogue().CloseDialogue();
    eng.State().Reset();
    eng.State().ClearFlag("shift_sanity_restored");

    eng.Time().Reset(22, 0, 1.80f);

    m_playerBody.position = {112.0f, 48.0f};
    m_playerBody.colliderOffset = {9.0f, 18.0f};
    m_playerBody.colliderSize = {14.0f, 11.0f};
    m_playerBody.velocity = {0.0f, 0.0f};
    m_playerFacing = 0;
    m_blackoutMode = false;
    m_lightsFlickering = false;
    m_ambientScreamerTimer = 42.0f;
    SetVisor3DActive(eng, false);

    m_cooking.Reset();
    m_sanity.ResetForShift(m_playerBody.position);
    m_inspectionUI.Reset();

    m_world.BuildCollidersAndTriggers(eng, nightNumber);
    m_world.BuildLighting(eng, nightNumber);
    SpawnInitial3DAnomalies(nightNumber);

    if (nightNumber == 1) {
        eng.Audio().PlayAmbient("audio/bgm/amb_cafe_calm_lp.ogg", 0.70f);
        m_guests = {
            {"kurt",     "Дядя Курт",          "sprites/characters/spr_kurt.png",  "trig_booth1", "kurt_talk",     {64.0f,  68.0f},  0,   false, false, true, 0, "kurt_served",        false, -1.0f},
            {"katya",    "Студентка Катя",     "sprites/characters/spr_vera.png",  "trig_booth3", "katya_talk",    {144.0f, 68.0f},  25,  false, false, true, 0, "katya_served",       false, -1.0f},
            {"valya",    "Тётя Валя",          "sprites/characters/spr_valya.png", "trig_booth2", "valya_talk",    {64.0f,  110.0f}, 55,  false, false, true, 0, "valya_served",       false, -1.0f},
            {"danila",   "Водитель Данила",    "sprites/characters/spr_kurt.png",  "trig_booth4", "danila_talk",   {144.0f, 110.0f}, 95,  false, false, true, 0, "danila_served",      false, -1.0f},
            {"hatman",   "Человек в шляпе",    "sprites/characters/spr_guest.png", "trig_booth5", "hatman_talk",   {224.0f, 68.0f},  140, false, false, true, 0, "hatman_served_safe", false, -1.0f},
            {"bride_n1", "Невеста с обочины",  "sprites/characters/spr_vera.png",  "trig_booth1", "bride_n1_talk", {64.0f,  68.0f},  185, false, false, true, 0, "bride_n1_served",       false, -1.0f}
        };
    } else if (nightNumber == 2) {
        eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.75f);
        m_guests = {
            {"arkhip",      "Механик Архип",       "sprites/characters/spr_kurt.png",  "trig_booth3", "arkhip_talk",         {144.0f, 68.0f},  5,   false, false, true, 0, "n2_arkhip_served",      false, -1.0f},
            {"valya_n2",    "Тётя Валя",           "sprites/characters/spr_valya.png", "trig_booth2", "n2_valya_talk",       {64.0f,  110.0f}, 40,  false, false, true, 0, "n2_valya_served",       false, -1.0f},
            {"semen_n2",    "Радист Семён",        "sprites/characters/spr_guest.png", "trig_booth4", "semen_n2_talk",       {144.0f, 110.0f}, 80,  false, false, true, 0, "semen_served",          false, -1.0f},
            {"fake_kurt",   "Дядя Курт (?)",       "sprites/characters/spr_kurt.png",  "trig_booth1", "n2_fake_kurt_talk",   {64.0f,  68.0f},  125, false, true,  false,0, "n2_fake_kurt_resolved", false, -1.0f},
            {"wet_stalker", "Мокрый Путник",       "sprites/characters/spr_guest.png", "trig_booth5", "wet_stalker_talk",    {224.0f, 68.0f},  170, false, true,  false,0, "n2_fake_kurt_resolved", false, -1.0f},
            {"guest_t6",    "Фигура за Столом №6", "sprites/characters/spr_guest.png", "trig_table6", "n2_guest_table6_talk",{224.0f, 110.0f}, 215, false, true,  false,0, "n2_guest_survived",     false, -1.0f}
        };
    } else if (nightNumber == 3) {
        eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.85f);
        m_guests = {
            {"boss_n3",     "Клиент за Столом №6", "sprites/characters/spr_guest.png", "trig_table6", "n3_guest_table6_talk", {224.0f, 110.0f}, 0,   false, false, true, 0, "n3_boss_resolved",  false, -1.0f},
            {"gleb",        "Следователь Глеб",   "sprites/characters/spr_kurt.png",  "trig_booth4", "gleb_talk",            {144.0f, 110.0f}, 15,  false, false, true, 0, "n3_gleb_served",    false, -1.0f},
            {"korsakov_n3", "Доктор Корсаков",    "sprites/characters/spr_guest.png", "trig_booth2", "korsakov_n3_talk",     {64.0f,  110.0f}, 55,  false, false, true, 0, "korsakov_served",   false, -1.0f},
            {"katya_n3",    "Попутчица (?)",      "sprites/characters/spr_vera.png",  "trig_booth5", "katya_mimic_talk",     {224.0f, 68.0f},  100, false, true,  false,0, "n3_mimic_banished", false, -1.0f},
            {"faceless_n3", "Официантка 1986",    "sprites/characters/spr_lera.png",  "trig_booth1", "faceless_n3_talk",     {64.0f,  68.0f},  150, false, true,  false,0, "n3_mimic_banished", false, -1.0f}
        };
    } else if (nightNumber == 4) {
        eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.90f);
        m_guests = {
            {"officer_n4",   "Офицер Борис",      "sprites/characters/spr_guest.png", "trig_booth3", "n4_officer_boris_talk",{144.0f, 68.0f},  5,   false, false, true, 0, "n4_officer_served",     false, -1.0f},
            {"mikhail_n4",   "Тень Михаила",      "sprites/characters/spr_kurt.png",  "trig_booth4", "mikhail_n4_talk",      {144.0f, 110.0f}, 50,  false, false, true, 0, "mikhail_served",        false, -1.0f},
            {"arkhip_mimic", "Архип (?)",         "sprites/characters/spr_kurt.png",  "trig_booth2", "katya_mimic_talk",     {64.0f,  110.0f}, 95,  false, true,  false,0, "n3_mimic_banished",     false, -1.0f},
            {"fake_kurt4",   "Двойник Курта",     "sprites/characters/spr_kurt.png",  "trig_booth1", "n2_fake_kurt_talk",   {64.0f,  68.0f},  140, false, true,  false,0, "n2_fake_kurt_resolved", false, -1.0f},
            {"mirror_twin",  "Зеркальный Мимик",  "sprites/characters/spr_vera.png",  "trig_booth5", "mirror_twin_talk",     {224.0f, 68.0f},  185, false, true,  false,0, "n2_fake_kurt_resolved", false, -1.0f}
        };
    } else {
        eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.95f);
        m_guests = {
            {"vera_spirit",  "Силуэт Веры",         "sprites/characters/spr_vera.png",  "trig_booth4", "vera_spirit_talk",   {144.0f, 110.0f}, 5,   false, false, true, 0, "n5_relic_collected", false, -1.0f},
            {"gleb_n5",      "Следователь Глеб",    "sprites/characters/spr_kurt.png",  "trig_booth1", "gleb_talk",          {64.0f,  68.0f},  45,  false, false, true, 0, "n3_gleb_served",     false, -1.0f},
            {"kurt_n5",      "Дядя Курт",           "sprites/characters/spr_kurt.png",  "trig_booth3", "kurt_n5_talk",       {144.0f, 68.0f},  90,  false, false, true, 0, "kurt5_served",       false, -1.0f},
            {"harbinger_n5", "Вестник Перекрёстка", "sprites/characters/spr_guest.png", "trig_booth5", "harbinger_n5_talk",  {224.0f, 68.0f},  135, false, true,  false,0, "n2_fake_kurt_resolved", false, -1.0f},
            {"boss_n5",      "Хозяин Перекрёстка",  "sprites/characters/spr_guest.png", "trig_table6", "n5_boss_table6_talk",{224.0f, 110.0f}, 180, false, true,  false,0, "n5_boss_banished",   false, -1.0f}
        };
    }

    for (auto& g : m_guests) {
        if (g.spawnMinuteOffset == 0) {
            g.spawned = true;
            eng.Physics().SetTriggerEnabled(g.triggerId, true);
            eng.Physics().UpdateTriggerDialogue(g.triggerId, g.dialogueId, g.name);
        }
    }

    m_tutorial.Reset(nightNumber, withTutorial);
    m_screen = GameScreen::PlayingShift;
    SaveCurrentGame(eng, 1);
}

void LastOrderGame::UpdatePlayerMovement(engine::core::Engine& eng, float dt) {
    engine::math::Vec2 moveDir = {0.0f, 0.0f};

    if (m_visor3DActive) {
        float rotSpeed = 2.6f;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_Q))  m_visorYaw -= rotSpeed * dt;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_E)) m_visorYaw += rotSpeed * dt;
        Vector2 md = GetMouseDelta();
        m_visorYaw += md.x * 0.0038f;
        m_visorPitch = std::clamp(m_visorPitch - md.y * 0.0032f, -0.50f, 0.50f);

        float fwdX = std::cos(m_visorYaw);
        float fwdY = std::sin(m_visorYaw);
        float rightX = -fwdY;
        float rightY = fwdX;

        bool invertStrafe = (eng.State().GetSanity() <= 75);
        if (invertStrafe) {
            rightX = -rightX;
            rightY = -rightY;
        }

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
    float speed = m_visor3DActive ? 78.0f : 72.0f;
    m_playerBody.velocity = moveDir.Normalized() * speed;

    engine::math::Vec2 prevPos = m_playerBody.position;
    if (m_debugNoclip) {
        m_playerBody.position += m_playerBody.velocity * dt;
    } else {
        eng.Physics().MoveAndSlide(m_playerBody, dt);
    }
    m_playerBody.position.x = std::clamp(m_playerBody.position.x, 12.0f, 296.0f);
    m_playerBody.position.y = std::clamp(m_playerBody.position.y, 4.0f, 156.0f);
    m_distMovedThisFrame += (m_playerBody.position - prevPos).Length();

    auto& fl = eng.Lighting().GetFlashlight();
    fl.position = m_playerBody.Center();

    if (m_visor3DActive) {
        fl.directionDeg = m_visorYaw * (180.0f / 3.14159265f);
    } else {
        const float dirAngles[4] = {90.0f, 270.0f, 180.0f, 0.0f};
        fl.directionDeg = dirAngles[m_playerFacing];
    }

    m_nearestTrigger = eng.Physics().FindNearestTrigger(m_playerBody.Center(), 26.0f);
}

void LastOrderGame::Fire3DVisorWeapon(engine::core::Engine& eng) {
    if (m_gunCooldown > 0.0f) return;
    m_gunCooldown = 0.22f;
    m_gunRecoilTimer = 0.16f;
    eng.Audio().PlaySfx("audio/sfx/sfx_chase_flash_burn.wav", 0.85f);
    eng.Renderer().TriggerShake(1.8f, 0.12f);

    engine::math::Vec2 pCenter = m_playerBody.Center();
    engine::math::Vec2 lookDir = {std::cos(m_visorYaw), std::sin(m_visorYaw)};

    int bestAnomalyIdx = -1;
    float bestDist = 320.0f;

    for (int i = 0; i < static_cast<int>(m_anomalies3D.size()); ++i) {
        auto& a = m_anomalies3D[i];
        if (!a.active) continue;
        engine::math::Vec2 toA = a.pos - pCenter;
        float dist = toA.Length();
        if (dist < 6.0f || dist > bestDist) continue;
        engine::math::Vec2 dir = toA / dist;
        float dot = dir.x * lookDir.x + dir.y * lookDir.y;
        float hitCos = (dist < 45.0f) ? 0.80f : 0.91f;
        if (dot >= hitCos) {
            bestDist = dist;
            bestAnomalyIdx = i;
        }
    }

    if (bestAnomalyIdx >= 0) {
        auto& hitA = m_anomalies3D[bestAnomalyIdx];
        hitA.hp -= 1;
        hitA.hitFlash = 0.18f;
        m_hitMarkerTimer = 0.22f;
        eng.State().SetFlag("visor_first_hit");
        if (hitA.hp <= 0) {
            hitA.active = false;
            m_anomaliesKilled++;
            m_shooterScore += 150;
            eng.State().SetFlag("anomalies_cleared_3d");
            eng.State().AddSanity(6);
            m_endlessDirector.OnSanityRestored(10);
            eng.Audio().PlaySfx("audio/sfx/sfx_chase_relic_pickup.wav", 0.85f);
            m_visorKillBanner = "АНОМАЛИЯ УНИЧТОЖЕНА: " + hitA.label;
            m_visorKillBannerTimer = 2.4f;
        }
        return;
    }

    for (auto& g : m_guests) {
        if (!g.spawned || !g.anomalous) continue;
        engine::math::Vec2 gCenter = {g.pos.x + 16.0f, g.pos.y + 16.0f};
        engine::math::Vec2 toG = gCenter - pCenter;
        float dist = toG.Length();
        if (dist > 280.0f || dist < 4.0f) continue;
        engine::math::Vec2 dir = toG / dist;
        float dot = dir.x * lookDir.x + dir.y * lookDir.y;
        if (dot >= 0.90f) {
            bool isUnmasked = eng.State().HasFlag("n2_mirror_checked") || eng.State().HasFlag("mimic_unmasked_" + g.id);
            if (!isUnmasked) {
                m_visorKillBanner = "ОБЪЕКТ НЕ ОПОЗНАН КАК АНОМАЛИЯ: Проверьте зеркало!";
                m_visorKillBannerTimer = 2.6f;
                eng.Audio().PlaySfx("audio/sfx/sfx_radio_static.wav", 0.6f);
                return;
            }
            g.spawned = false;
            g.finished = true;
            eng.Physics().SetTriggerEnabled(g.triggerId, false);
            m_hitMarkerTimer = 0.30f;
            m_anomaliesKilled++;
            m_shooterScore += 300;
            eng.State().SetFlag("visor_first_hit");
            eng.State().SetFlag("anomalies_cleared_3d");
            eng.State().AddSanity(8);
            if (!g.completedFlag.empty()) eng.State().SetFlag(g.completedFlag);
            eng.Renderer().TriggerFlash(Color{244, 184, 96, 180}, 0.30f);
            eng.Audio().PlaySfx("audio/sfx/sfx_window_bang.wav", 0.85f);
            m_visorKillBanner = "ДВОЙНИК ИЗГНАН: " + g.name;
            m_visorKillBannerTimer = 2.6f;
            return;
        }
    }
}

void LastOrderGame::Update3DVisorShooter(engine::core::Engine& eng, float dt) {
    if (m_gunCooldown > 0.0f)    m_gunCooldown = std::max(0.0f, m_gunCooldown - dt);
    if (m_gunRecoilTimer > 0.0f) m_gunRecoilTimer = std::max(0.0f, m_gunRecoilTimer - dt);
    if (m_hitMarkerTimer > 0.0f) m_hitMarkerTimer = std::max(0.0f, m_hitMarkerTimer - dt);
    if (m_visorKillBannerTimer > 0.0f) m_visorKillBannerTimer = std::max(0.0f, m_visorKillBannerTimer - dt);

    engine::math::Vec2 pCenter = m_playerBody.Center();
    engine::math::Vec2 lookDir = {std::cos(m_visorYaw), std::sin(m_visorYaw)};

    m_scannerTargetText.clear();
    float bestScanDist = 300.0f;

    int activeCount = 0;
    for (auto& a : m_anomalies3D) {
        if (!a.active) continue;
        activeCount++;
        if (a.hitFlash > 0.0f) a.hitFlash = std::max(0.0f, a.hitFlash - dt);

        engine::math::Vec2 toPlayer = pCenter - a.pos;
        float dist = toPlayer.Length();
        if (a.type == 1 || a.type == 2) {
            float moveSpd = m_visor3DActive
                ? ((a.type == 2) ? 24.0f : 16.0f)
                : ((a.type == 2) ? 13.0f : 9.0f);
            if (dist > 13.0f) {
                a.pos += (toPlayer / dist) * moveSpd * dt;
            }
            if (dist <= 14.0f) {
                a.active = false;
                eng.State().AddSanity(-16);
                TriggerScreamer(eng, (a.type == 1) ? "sc_night9_shadow_reaper" : "sc_night14_skull_glitch");
                continue;
            }
        }

        if (m_visor3DActive && dist > 4.0f && dist < bestScanDist) {
            engine::math::Vec2 dirToA = (a.pos - pCenter) / dist;
            float dot = dirToA.x * lookDir.x + dirToA.y * lookDir.y;
            if (dot >= 0.91f) {
                bestScanDist = dist;
                m_scannerTargetText = a.label + " [" + std::to_string(a.hp) + "/" + std::to_string(a.maxHp) + "]";
                m_scannerTargetDist = dist * 0.05f;
            }
        }
    }

    if (m_visor3DActive && m_scannerTargetText.empty()) {
        for (const auto& g : m_guests) {
            if (!g.spawned) continue;
            engine::math::Vec2 gCenter = {g.pos.x + 16.0f, g.pos.y + 16.0f};
            float dist = (gCenter - pCenter).Length();
            if (dist > 4.0f && dist < 260.0f) {
                engine::math::Vec2 dirToG = (gCenter - pCenter) / dist;
                float dot = dirToG.x * lookDir.x + dirToG.y * lookDir.y;
                if (dot >= 0.91f) {
                    bool isUnmasked = eng.State().HasFlag("n2_mirror_checked") || eng.State().HasFlag("mimic_unmasked_" + g.id);
                    if (g.anomalous && isUnmasked) {
                        m_scannerTargetText = "ДВОЙНИК (МИМИК): " + g.name;
                    } else {
                        m_scannerTargetText = "ПОСЕТИТЕЛЬ: " + g.name;
                    }
                    m_scannerTargetDist = dist * 0.05f;
                    break;
                }
            }
        }
    }

    bool allowAutoAnomalies = (m_screen == GameScreen::EndlessMode) || (m_currentNight >= 2);
    if (allowAutoAnomalies && activeCount < 2) {
        m_anomalySpawnTimer -= dt;
        if (m_anomalySpawnTimer <= 0.0f) {
            m_anomalySpawnTimer = 55.0f;
            Spawn3DShooterWave(eng, 1);
        }
    }

    if (m_visor3DActive) {
        if (IsKeyPressed(KEY_X)) {
            Spawn3DShooterWave(eng, 4);
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_F)) {
            Fire3DVisorWeapon(eng);
        }
    }
}

void LastOrderGame::OnFixedUpdate(engine::core::Engine& eng, float fixedDt) {
    if (m_screen == GameScreen::NightmareChase) {
        m_chaseMinigame.Update(eng, fixedDt);
        return;
    }
    if (m_screen == GameScreen::Chapter2_Motel3D) {
        if (!m_settingsUI.IsOpen()) m_motel3D.FixedUpdate(eng, fixedDt);
        return;
    }
    if (m_screen == GameScreen::Chapter3_House3D) {
        if (!m_settingsUI.IsOpen()) m_house3D.FixedUpdate(eng, fixedDt);
        return;
    }
    if (m_screen == GameScreen::CustomChapter) return;

    if (m_settingsUI.IsOpen() || m_sanity.IsGameOver() ||
        m_screen == GameScreen::MainMenu || m_screen == GameScreen::Paused) {
        eng.Time().SetPaused(true);
        eng.Time().Tick(fixedDt);
        return;
    }

    if (m_screen == GameScreen::EndlessMode) {
        eng.Time().SetPaused(false);
        eng.Time().Tick(fixedDt);
        m_endlessDirector.SyncGuestNpcs(m_guests);
        return;
    }

    bool pauseShiftClock = eng.Dialogue().IsActive() || m_inspectionUI.IsAnyModalOpen();
    eng.Time().SetPaused(pauseShiftClock);
    eng.Time().Tick(fixedDt);

    for (auto& g : m_guests) {
        if (!g.spawned || g.finished) continue;
        bool isDone = (!g.completedFlag.empty() && eng.State().HasFlag(g.completedFlag));
        if (g.id == "hatman" && eng.State().HasFlag("hatman_served_anomaly")) isDone = true;
        if (isDone) {
            if (g.leaveTimer < 0.0f) {
                g.leaveTimer = 1.6f;
            } else if (!eng.Dialogue().IsActive()) {
                g.leaveTimer -= fixedDt;
                if (g.leaveTimer <= 0.0f) {
                    g.spawned = false;
                    g.finished = true;
                    eng.Physics().SetTriggerEnabled(g.triggerId, false);
                    m_cooking.RemoveDishFromTable(g.triggerId);
                    eng.Audio().PlaySfx("audio/sfx/sfx_door_bell.wav", 0.65f);
                }
            }
        }
    }

    if (eng.Dialogue().IsActive() || m_inspectionUI.IsAnyModalOpen()) {
        return;
    }

    if (m_currentNight >= 2) {
        m_ambientScreamerTimer -= fixedDt;
        if (m_ambientScreamerTimer <= 0.0f) {
            float nightFactor = static_cast<float>(m_currentNight);
            m_ambientScreamerTimer = std::max(38.0f, 65.0f - nightFactor * 4.0f);
            int roll = std::rand() % 100;
            if (roll < 38 + m_currentNight * 7) {
                TriggerScreamer(eng, SCREAMER_IDS[std::rand() % 14]);
                eng.State().AddSanity(-8);
            } else {
                eng.Audio().PlaySfx("audio/sfx/sfx_whisper_vera.wav", 0.90f);
                eng.Renderer().TriggerFlash(Color{185, 18, 28, 130}, 0.30f);
                eng.Renderer().TriggerShake(3.8f, 0.40f);
            }
        }
    }

    float elapsedMins = eng.Time().GetShiftElapsedMinutes();

    for (auto& g : m_guests) {
        if (g.spawned || g.finished) continue;
        if (!g.completedFlag.empty() && eng.State().HasFlag(g.completedFlag)) {
            g.finished = true;
            continue;
        }
        if (elapsedMins >= static_cast<float>(g.spawnMinuteOffset)) {
            bool tableBusy = false;
            for (const auto& other : m_guests) {
                if (other.spawned && other.triggerId == g.triggerId) {
                    tableBusy = true;
                    break;
                }
            }
            if (!tableBusy) {
                g.spawned = true;
                eng.Physics().SetTriggerEnabled(g.triggerId, true);
                eng.Physics().UpdateTriggerDialogue(g.triggerId, g.dialogueId, g.name);
                eng.Audio().PlaySfx("audio/sfx/sfx_door_bell.wav", 0.85f);
            }
        }
    }

    if (m_currentNight == 1) {
        if (elapsedMins >= 150.0f && !eng.State().HasFlag("radio_0214_ringing")) {
            eng.State().SetFlag("radio_0214_ringing");
            eng.Audio().PlaySfx("audio/sfx/sfx_radio_static.wav", 0.85f);
            eng.Physics().UpdateTriggerDialogue("trig_radio", "radio_0214", "Радиоприёмник [ПОМЕХИ!]");
        }

        if (elapsedMins >= 300.0f && !eng.State().HasFlag("event_0300_triggered")) {
            eng.State().SetFlag("event_0300_triggered");
            m_lightsFlickering = true;
            eng.Lighting().SetAmbientColor(Color{26, 30, 52, 255});
            eng.Lighting().SetAllPointLightsFlicker(28.0f, 0.85f);
            eng.Audio().PlaySfx("audio/sfx/sfx_cat_hiss.wav", 0.85f);
            if (!m_visor3DActive) {
                eng.Dialogue().StartDialogue("event_light_flicker_0300");
            }
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
        if (elapsedMins >= 130.0f && !eng.State().HasFlag("n2_blackout_active") && !eng.State().HasFlag("n2_power_restored")) {
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
                if (g.id == "boss_n3" && !g.spawned && !g.finished) {
                    g.spawned = true;
                    eng.Physics().SetTriggerEnabled(g.triggerId, true);
                    eng.Physics().UpdateTriggerDialogue(g.triggerId, g.dialogueId, g.name);
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
                if (g.id == "boss_n5" && !g.spawned && !g.finished) {
                    g.spawned = true;
                    eng.Physics().SetTriggerEnabled(g.triggerId, true);
                    eng.Physics().UpdateTriggerDialogue(g.triggerId, g.dialogueId, g.name);
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

namespace {

nlohmann::json LoadProgress() {
    nlohmann::json j = nlohmann::json::object();
    std::ifstream f(engine::core::GetUserStorageDir() / "chapter_progress.json");
    if (f.is_open()) {
        try { f >> j; } catch (...) { j = nlohmann::json::object(); }
    }
    if (!j.is_object()) j = nlohmann::json::object();
    return j;
}

void SaveProgress(const nlohmann::json& j) {
    std::ofstream f(engine::core::GetUserStorageDir() / "chapter_progress.json");
    if (f.is_open()) f << j.dump(2);
}

}

void LastOrderGame::BeginChapter35(engine::core::Engine& eng, const std::string& ch3Ending) {
    std::string ending = ch3Ending;
    if (ending.empty()) {
        auto j = LoadProgress();
        if (j.contains("chapter3") && j["chapter3"].contains("ending")) ending = j["chapter3"]["ending"].get<std::string>();
    }
    if (ending.empty()) ending = "front_door";

    std::map<std::string, knopmops::runtime::Var> vars;
    knopmops::runtime::Var v;
    v.type = knopmops::runtime::Var::Type::String;
    v.str = ending;
    vars["ch3_ending"] = v;

    m_screen = GameScreen::CustomChapter;
    eng.Renderer().SetHighRes3DMode(true);
    eng.Renderer().SetSkip2DLightmap(true);
    if (!m_custom.Init(eng, "chapter_3_5", vars)) EndChapter35(eng);
}

void LastOrderGame::EndChapter35(engine::core::Engine& eng) {
    bool completed = m_custom.Completed();
    auto vars = m_custom.FinalVars();
    m_custom.Shutdown(eng);
    if (completed) {
        eng.State().SetFlag("chapter35_completed");
        auto j = LoadProgress();
        j["chapter3_5"]["completed"] = true;
        auto it = vars.find("ch35_took_tapes");
        if (it != vars.end()) j["chapter3_5"]["took_tapes"] = it->second.boolean;
        SaveProgress(j);
        SaveKeyMoment(eng, "Глава 3.5: Разъезд 86 пройден");
        BeginChapter4(eng);
        return;
    }
    eng.Renderer().SetHighRes3DMode(false);
    eng.Renderer().SetSkip2DLightmap(false);
    m_screen = GameScreen::MainMenu;
    m_mainMenu.Init(eng);
    EnableCursor();
}

void LastOrderGame::BeginChapter4(engine::core::Engine& eng) {
    std::map<std::string, knopmops::runtime::Var> vars;
    m_screen = GameScreen::CustomChapter;
    eng.Renderer().SetHighRes3DMode(true);
    eng.Renderer().SetSkip2DLightmap(true);
    if (!m_custom.Init(eng, "chapter_4", vars)) EndChapter4(eng);
}

void LastOrderGame::EndChapter4(engine::core::Engine& eng) {
    bool completed = m_custom.Completed();
    m_custom.Shutdown(eng);
    if (completed) {
        eng.State().SetFlag("chapter4_completed");
        auto j = LoadProgress();
        j["chapter_4"]["completed"] = true;
        SaveProgress(j);
        SaveKeyMoment(eng, "Глава 4: Дом на сопке пройден");
    }
    eng.Renderer().SetHighRes3DMode(false);
    eng.Renderer().SetSkip2DLightmap(false);
    m_screen = GameScreen::MainMenu;
    m_mainMenu.Init(eng);
    EnableCursor();
}

void LastOrderGame::OnFrameUpdate(engine::core::Engine& eng, float dt) {
    if (m_pendingChapter4) {
        m_pendingChapter4 = false;
        BeginChapter4(eng);
        return;
    }
    if (m_pendingCh35) {
        m_pendingCh35 = false;
        BeginChapter35(eng, m_pendingCh35Ending);
        return;
    }
    if (m_pendingChapter2) {
        m_pendingChapter2 = false;
        m_screen = GameScreen::Chapter2_Motel3D;
        eng.Renderer().SetHighRes3DMode(true);
        eng.Renderer().SetSkip2DLightmap(true);
        m_motel3D.Init(eng, 1);
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
        if (!m_chapter2TourDir.empty()) m_motel3D.StartTour(m_chapter2TourDir);
#endif
        return;
    }
    if (m_pendingChapter3) {
        m_pendingChapter3 = false;
        m_screen = GameScreen::Chapter3_House3D;
        eng.Renderer().SetHighRes3DMode(true);
        eng.Renderer().SetSkip2DLightmap(true);
        m_house3D.Init(eng, m_chapter3Renderer);
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
        if (m_chapter3Tour) m_house3D.StartTour();
        if (m_chapter3CutsceneTour) m_house3D.StartCutsceneTour();
        if (m_chapter3Holdout) m_house3D.StartHoldoutTest();
#endif
        return;
    }
    if (m_currentNight == 3 && eng.State().HasFlag("n3_fridge_checked") && eng.State().HasFlag("n3_mirror_key_found")) {
        eng.State().SetFlag("n3_artifacts_ready");
    }

    if (m_saveBannerTimer > 0.0f) {
        m_saveBannerTimer = std::max(0.0f, m_saveBannerTimer - dt);
    }

    if (IsKeyPressed(KEY_F5) && !m_debugMode && !eng.Dialogue().IsActive() && !m_visor3DActive && m_screen == GameScreen::PlayingShift) {
        SaveCurrentGame(eng);
    }

#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
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
        if (IsKeyPressed(KEY_F5))  StartMinigame(eng, systems::MinigameType::Night1_RadioTuner2D);
        if (IsKeyPressed(KEY_F6))  StartMinigame(eng, systems::MinigameType::Night2_BasementFuse3D);
        if (IsKeyPressed(KEY_F7))  StartMinigame(eng, systems::MinigameType::Night3_FreezerMaze3D);
        if (IsKeyPressed(KEY_F9))  StartMinigame(eng, systems::MinigameType::Night4_MirrorHunt3D);
        if (IsKeyPressed(KEY_F10)) StartMinigame(eng, systems::MinigameType::Night5_HighwayChase3D);
    }
#endif

    if (m_settingsUI.IsOpen()) {
        if (IsCursorHidden()) EnableCursor();
        m_settingsUI.Update(eng);
        return;
    }

    if (m_screen == GameScreen::MainMenu) {
        if (IsCursorHidden()) EnableCursor();
        eng.Renderer().SetSanityStress(0.0f);
        eng.Renderer().SetSkip2DLightmap(false);
        ui::MenuCommand cmd = m_mainMenu.Update(eng, dt);
        switch (cmd) {
            case ui::MenuCommand::None:
                break;
            case ui::MenuCommand::ContinueSaveGame:
                LoadSavedGame(eng, -1);
                break;
            case ui::MenuCommand::LoadSlot0:
                LoadSavedGame(eng, 0);
                break;
            case ui::MenuCommand::LoadSlot1:
                LoadSavedGame(eng, 1);
                break;
            case ui::MenuCommand::LoadSlot2:
                LoadSavedGame(eng, 2);
                break;
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
            case ui::MenuCommand::StartChapter2_Motel3D:
                m_screen = GameScreen::Chapter2_Motel3D;
                eng.Renderer().SetHighRes3DMode(true);
                eng.Renderer().SetSkip2DLightmap(true);
                m_motel3D.Init(eng, 1);
                break;
            case ui::MenuCommand::StartChapter3_House3D:
                m_screen = GameScreen::Chapter3_House3D;
                eng.Renderer().SetHighRes3DMode(true);
                eng.Renderer().SetSkip2DLightmap(true);
                m_house3D.Init(eng);
                break;
            case ui::MenuCommand::StartChapter35:
                BeginChapter35(eng, "");
                break;
            case ui::MenuCommand::StartChapter4:
                BeginChapter4(eng);
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

    if (m_screen == GameScreen::Chapter2_Motel3D) {
        if (m_settingsUI.IsOpen()) {
            m_settingsUI.Update(eng);
            return;
        }
        m_motel3D.Update(eng, dt);
        if (m_motel3D.WantsHighwayChase()) {
            m_motel3D.Shutdown(eng);
            StartMinigame(eng, systems::MinigameType::Night5_HighwayChase3D);
            return;
        }
        if (m_motel3D.WantsChapterComplete()) {
            m_motel3D.Shutdown(eng);
            eng.State().SetFlag("chapter2_completed");
            SaveKeyMoment(eng, "Глава 2: сущность у поста ДПС уничтожена");
            eng.Renderer().SetHighRes3DMode(false);
            eng.Renderer().SetSkip2DLightmap(false);
            m_screen = GameScreen::MainMenu;
            m_mainMenu.Init(eng);
            EnableCursor();
            return;
        }
        if (m_motel3D.WantsReturnToMenu()) {
            m_motel3D.Shutdown(eng);
            eng.Renderer().SetHighRes3DMode(false);
            eng.Renderer().SetSkip2DLightmap(false);
            m_screen = GameScreen::MainMenu;
            m_mainMenu.Init(eng);
            EnableCursor();
        }
        return;
    }

    if (m_screen == GameScreen::CustomChapter) {
        if (m_settingsUI.IsOpen()) {
            m_settingsUI.Update(eng);
            if (m_settingsUI.WantsQuitToMenu()) {
                m_settingsUI.ClearQuitToMenu();
                if (m_custom.ChapterId() == "chapter_4") EndChapter4(eng);
                else EndChapter35(eng);
                return;
            }
            if (!m_settingsUI.IsOpen()) {
                DisableCursor();
                m_custom.ApplyGraphics(eng);
            }
            return;
        }
        if (IsKeyPressed(KEY_ESCAPE) && m_custom.CanOpenSettings()) {
            m_settingsUI.Open();
            EnableCursor();
            return;
        }
        m_custom.Update(eng, dt);
        if (m_custom.WantsReturnToMenu()) {
            if (m_custom.ChapterId() == "chapter_4") EndChapter4(eng);
            else EndChapter35(eng);
        }
        return;
    }

    if (m_screen == GameScreen::Chapter3_House3D) {
        if (m_settingsUI.IsOpen()) {
            m_settingsUI.Update(eng);
            return;
        }
        m_house3D.Update(eng, dt);
        if (m_house3D.WantsChapterComplete()) {
            std::string ending = m_house3D.EndingId();
            m_house3D.Shutdown(eng);
            eng.State().SetFlag("chapter3_completed");
            SaveKeyMoment(eng, "Глава 3: побег из дома смотрителя завершен");
            {
                auto j = LoadProgress();
                j["chapter3"]["ending"] = ending;
                j["chapter3"]["completed"] = true;
                SaveProgress(j);
            }
            BeginChapter35(eng, ending); // Глава 3.5 начинается сразу, вариант зависит от концовки
            return;
        }
        if (m_house3D.WantsReturnToMenu()) {
            m_house3D.Shutdown(eng);
            eng.Renderer().SetHighRes3DMode(false);
            eng.Renderer().SetSkip2DLightmap(false);
            m_screen = GameScreen::MainMenu;
            m_mainMenu.Init(eng);
            EnableCursor();
        }
        return;
    }

    if (m_screen == GameScreen::NightmareChase) {
        if (IsCursorHidden()) EnableCursor();
        if (m_chaseMinigame.GetState() == systems::ChaseState::Victory) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E)) {
                CompleteActiveMinigame(eng);
            }
        } else if (m_chaseMinigame.GetState() == systems::ChaseState::Caught) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                eng.Renderer().SetSkip2DLightmap(false);
                m_screen = (m_chaseMinigame.GetType() == systems::MinigameType::Night5_HighwayChase3D) ? GameScreen::MainMenu : ((m_currentNight == 0) ? GameScreen::EndlessMode : GameScreen::PlayingShift);
                if (m_screen == GameScreen::MainMenu) {
                    m_mainMenu.Init(eng);
                    EnableCursor();
                }
            }
        }
        return;
    }

    if (m_screen == GameScreen::Paused) {
        if (IsCursorHidden()) EnableCursor();
        UpdatePauseMenu(eng);
        return;
    }

    if (IsKeyPressed(KEY_I)) {
        if (eng.Dialogue().IsActive()) eng.Dialogue().CloseDialogue();
        m_inspectionUI.ToggleNotesGuide(eng);
    }

    if (IsKeyPressed(KEY_V)) {
        if (eng.Dialogue().IsActive()) eng.Dialogue().CloseDialogue();
        if (m_inspectionUI.IsAnyModalOpen()) m_inspectionUI.CloseAll();
        SetVisor3DActive(eng, !m_visor3DActive);
        if (m_visor3DActive) m_toggledVisorThisFrame = true;
    }

    if (!eng.Dialogue().IsActive() && !m_inspectionUI.IsAnyModalOpen() && !m_sanity.IsGameOver()) {
        UpdatePlayerMovement(eng, dt);
        Update3DVisorShooter(eng, dt);
    } else {
        m_playerBody.velocity = {0.0f, 0.0f};
        m_isMoving = false;
    }

    if (m_screen == GameScreen::EndlessMode) {
        if (IsKeyPressed(KEY_ESCAPE)) {

    if (m_visor3DActive) {
                SetVisor3DActive(eng, false);
            } else {
                m_screen = GameScreen::Paused;
                m_pauseSelectedBtn = 0;
            }
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
            if (IsCursorHidden()) EnableCursor();
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

        if (!m_visor3DActive && IsKeyPressed(KEY_F)) {
            auto& fl = eng.Lighting().GetFlashlight();
            fl.enabled = !fl.enabled;
            eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.7f);
        }

        if (!m_visor3DActive && m_nearestTrigger.has_value() && IsKeyPressed(KEY_E)) {
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
                if (m_endlessSanityCooldownTimer <= 0.0f) {
                    m_endlessSanityCooldownTimer = 90.0f;
                    m_endlessDirector.OnSanityRestored(10);
                    eng.State().AddSanity(10);
                    m_saveBannerText = "ГОРЯЧИЙ КОФЕ: РАССУДОК +10% (КУЛДАУН 90 СЕК)";
                    m_saveBannerTimer = 2.5f;
                } else {
                    m_saveBannerText = "КОФЕИН ЕЩЁ ДЕЙСТВУЕТ (ОСТАЛОСЬ: " + std::to_string(static_cast<int>(m_endlessSanityCooldownTimer)) + " СЕК)";
                    m_saveBannerTimer = 2.0f;
                }
                eng.Audio().PlaySfx("audio/sfx/sfx_coffee_sip.wav", 0.85f);
            } else if (tid == "trig_oven") {
                eng.State().GiveItem("cherry_pie", "Тёплый вишнёвый пирог");
                if (m_endlessSanityCooldownTimer <= 0.0f) {
                    m_endlessSanityCooldownTimer = 90.0f;
                    m_endlessDirector.OnSanityRestored(10);
                    eng.State().AddSanity(10);
                    m_saveBannerText = "ГОРЯЧАЯ ВЫПЕЧКА: РАССУДОК +10% (КУЛДАУН 90 СЕК)";
                    m_saveBannerTimer = 2.5f;
                } else {
                    m_saveBannerText = "ВЫ НЕ ГОЛОДНЫ (ОСТАЛОСЬ: " + std::to_string(static_cast<int>(m_endlessSanityCooldownTimer)) + " СЕК)";
                    m_saveBannerTimer = 2.0f;
                }
                eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 0.85f);
            } else if (tid == "trig_cat") {
                if (m_endlessSanityCooldownTimer <= 0.0f) {
                    m_endlessSanityCooldownTimer = 90.0f;
                    m_endlessDirector.OnSanityRestored(12);
                    eng.State().AddSanity(12);
                    m_saveBannerText = "УГОЛЁК МУРЛЫЧЕТ: РАССУДОК +12% (КУЛДАУН 90 СЕК)";
                    m_saveBannerTimer = 2.5f;
                } else {
                    m_saveBannerText = "КОТ ЗАДРЕМАЛ (КУЛДАУН: " + std::to_string(static_cast<int>(m_endlessSanityCooldownTimer)) + " СЕК)";
                    m_saveBannerTimer = 2.0f;
                }
                eng.Audio().PlaySfx("audio/sfx/sfx_cat_purr.wav", 0.85f);
            } else {
                m_endlessDirector.InteractWithBooth(eng, tid, eng.Lighting().GetFlashlight().enabled);
            }
        }

        if (IsKeyPressed(KEY_F8)) {
            TriggerScreamer(eng, SCREAMER_IDS[m_demoScreamerIdx % 14]);
            m_demoScreamerIdx++;
        }

        if (m_endlessSanityCooldownTimer > 0.0f) {
            m_endlessSanityCooldownTimer = std::max(0.0f, m_endlessSanityCooldownTimer - dt);
        }

        m_cooking.Update(eng, dt, m_playerBody.Center());
        return;
    }

    int activeThreats = 0;
    for (const auto& a : m_anomalies3D) {
        if (a.active) activeThreats++;
    }
    for (const auto& g : m_guests) {
        if (g.spawned && g.anomalous && !g.finished) activeThreats++;
    }

    systems::SanityDrainFactors drainFactors{};
    drainFactors.isBlackout = m_blackoutMode;
    drainFactors.isFlickering = m_lightsFlickering;
    drainFactors.isFlashlightOn = eng.Lighting().GetFlashlight().enabled;
    drainFactors.isVisor3D = m_visor3DActive;
    drainFactors.threatCount = activeThreats;

    m_sanity.Update(eng, dt, m_playerBody.position, drainFactors);
    if (m_sanity.IsGameOver()) {
        if (IsCursorHidden()) EnableCursor();
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
            SetVisor3DActive(eng, false);
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
            if (m_visor3DActive) SetVisor3DActive(eng, false);
            m_inspectionUI.ToggleNotebook(eng);
        }
        if (IsKeyPressed(KEY_R)) {
            if (m_visor3DActive) SetVisor3DActive(eng, false);
            m_inspectionUI.ToggleRulesSheet(eng);
            eng.State().SetFlag("read_wall_rules");
        }
        if (IsKeyPressed(KEY_G)) {
            if (m_visor3DActive) SetVisor3DActive(eng, false);
            m_inspectionUI.ToggleMirrorInspector(eng);
            eng.State().SetFlag("n2_mirror_checked");
            eng.State().SetFlag("mirror_examined");
            if (m_currentNight == 3 && eng.State().HasFlag("n3_path_listened_owner") && !eng.State().HasFlag("n3_mirror_key_found")) {
                eng.State().SetFlag("n3_mirror_key_found");
                eng.State().GiveItem("vera_key", "Серебряный ключ Веры");
                eng.Audio().PlaySfx("audio/sfx/sfx_cash_register.wav", 0.85f);
                m_saveBannerText = "СЕРЕБРЯНЫЙ КЛЮЧ ВЕРЫ ПОЛУЧЕН ИЗ ЗАЗЕРКАЛЬЯ!";
                m_saveBannerTimer = 3.5f;
                SaveKeyMoment(eng, "Ключ Веры получен");
                if (eng.State().HasFlag("n3_fridge_checked")) {
                    eng.State().SetFlag("n3_artifacts_ready");
                }
            }
        }
    }

    if (!m_visor3DActive && !eng.Dialogue().IsActive() && !m_inspectionUI.IsAnyModalOpen() && IsKeyPressed(KEY_E)) {
        if (m_nearestTrigger.has_value() && !m_nearestTrigger->dialogueId.empty()) {
            std::string tid = m_nearestTrigger->id;
            if (tid == "trig_window" || tid == "trig_window_south") {
                eng.State().SetFlag("n2_mirror_checked");
                eng.State().SetFlag("mirror_examined");
                if (m_currentNight == 3 && eng.State().HasFlag("n3_path_listened_owner") && !eng.State().HasFlag("n3_mirror_key_found")) {
                    eng.State().SetFlag("n3_mirror_key_found");
                    eng.State().GiveItem("vera_key", "Серебряный ключ Веры");
                    eng.Audio().PlaySfx("audio/sfx/sfx_cash_register.wav", 0.85f);
                    m_saveBannerText = "СЕРЕБРЯНЫЙ КЛЮЧ ВЕРЫ ПОЛУЧЕН ЧЕРЕЗ СТЕКЛО ОКНА!";
                    m_saveBannerTimer = 3.5f;
                    SaveKeyMoment(eng, "Ключ Веры получен");
                    if (eng.State().HasFlag("n3_fridge_checked")) {
                        eng.State().SetFlag("n3_artifacts_ready");
                    }
                }
            } else if (tid == "trig_breaker") {
                m_blackoutMode = false;
                eng.Lighting().SetAmbientColor(Color{135, 112, 98, 255});
                eng.Lighting().SetAllPointLightsFlicker(2.0f, 0.03f);
            }
            std::string dlgId = m_nearestTrigger->dialogueId;
            if (tid == "trig_table6" && m_currentNight == 3) {
                if (!eng.State().HasFlag("n3_radio_tuned_mikhail")) {
                    if (eng.State().HasFlag("n3_radio_urgent_tune")) {
                        dlgId = "n3_guest_table6_static";
                    } else {
                        dlgId = "n3_guest_table6_talk";
                    }
                } else if (eng.State().HasFlag("n3_path_defied_owner")) {
                    dlgId = "n3_table6_defied";
                } else if (eng.State().HasFlag("n3_artifacts_ready")) {
                    dlgId = "n3_table6_banish";
                } else {
                    dlgId = "n3_table6_waiting";
                }
            } else if (tid == "trig_radio" && m_currentNight == 3) {
                if (eng.State().HasFlag("n3_radio_urgent_tune") && !eng.State().HasFlag("n3_radio_tuned_mikhail")) {
                    dlgId = "n3_radio_tune_prompt";
                }
            }
            eng.Dialogue().StartDialogue(dlgId);
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.45f);
        }
    }

    if (!m_visor3DActive && IsKeyPressed(KEY_F)) {
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

#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    if (IsKeyPressed(KEY_F2)) {
        int h = eng.Time().GetHours24();
        int m = eng.Time().GetMinutes() + 30;
        eng.Time().SetTimeHHMM((h + m / 60) % 24, m % 60);
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.7f);
    }
#endif

    if (IsKeyPressed(KEY_F8) && !eng.Dialogue().IsActive()) {
        TriggerScreamer(eng, SCREAMER_IDS[m_demoScreamerIdx % 14]);
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

    constexpr int COUNT = 7;
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
        engine::math::Rect r = {340.0f, 160.0f + i * 58.0f, 600.0f, 48.0f};
        if (r.Contains(mpos)) {
            m_pauseSelectedBtn = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) activate = true;
        }
    }

    if (activate) {
        eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.55f);
        switch (m_pauseSelectedBtn) {
            case 0:
                m_screen = (m_currentNight == 0) ? GameScreen::EndlessMode : GameScreen::PlayingShift;
                break;
            case 1:
                SaveCurrentGame(eng, 0);
                break;
            case 2:
                if (systems::SaveSystem::HasSlot(0)) LoadSavedGame(eng, 0);
                else eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                break;
            case 3:
                if (systems::SaveSystem::HasSlot(1)) LoadSavedGame(eng, 1);
                else eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                break;
            case 4:
                if (systems::SaveSystem::HasSlot(2)) LoadSavedGame(eng, 2);
                else eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                break;
            case 5:
                m_settingsUI.Open();
                break;
            case 6:
                m_mainMenu.Init(eng);
                m_screen = GameScreen::MainMenu;
                break;
        }
    }
}

void LastOrderGame::RenderPauseMenu(engine::core::Engine& eng) const {
    Font font = eng.Assets().GetMainFont();
    DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 212});
    DrawRectangle(320, 90, 640, 540, Color{42, 23, 21, 248});
    DrawRectangleLinesEx(Rectangle{320, 90, 640, 540}, 3.0f, Color{244, 184, 96, 255});

    std::string title =
        (m_currentNight == 0) ? "ПАУЗА — СВОБОДНЫЙ РЕЖИМ" :
                                ("ПАУЗА — ГЛАВА " + std::to_string(m_currentChapter) + ": НОЧЬ " + std::to_string(m_currentNight));
    engine::graphics::TextLayout::DrawCentered(font, title, 640.0f, 118.0f, 24.0f, Color{244, 184, 96, 255}, 1.0f);

    systems::SaveProfile sp0 = systems::SaveSystem::GetSlotInfo(0);
    systems::SaveProfile sp1 = systems::SaveSystem::GetSlotInfo(1);
    systems::SaveProfile sp2 = systems::SaveSystem::GetSlotInfo(2);

    std::string labels[7];
    labels[0] = "Продолжить смену";
    labels[1] = "Быстрое сохранение [F5]";
    labels[2] = sp0.valid ? ("Загрузить Слот 1: " + sp0.slotTitle) : "Загрузить: [Слот 1 пуст]";
    labels[3] = sp1.valid ? ("Загрузить Слот 2: " + sp1.slotTitle) : "Загрузить: [Слот 2 пуст]";
    labels[4] = sp2.valid ? ("Загрузить Слот 3: " + sp2.slotTitle) : "Загрузить: [Слот 3 пуст]";
    labels[5] = "Настройки";
    labels[6] = "Выйти в главное меню";

    for (int i = 0; i < 7; ++i) {
        bool sel = (m_pauseSelectedBtn == i);
        float bx = 340.0f;
        float by = 160.0f + i * 58.0f;
        float bw = 600.0f;
        float bh = 48.0f;

        Color bg = sel ? Color{107, 51, 34, 255} : Color{28, 16, 14, 220};
        Color bdr = sel ? Color{244, 184, 96, 255} : Color{196, 122, 59, 170};
        DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh), bg);
        DrawRectangleLinesEx(Rectangle{bx, by, bw, bh}, sel ? 2.5f : 1.0f, bdr);

        if (sel) {
            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 8, static_cast<int>(bh), Color{244, 184, 96, 255});
        }

        DrawTextEx(font, labels[i].c_str(), Vector2{bx + 18.0f, by + 12.0f}, 19.5f, 1.0f,
                   sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 230});
    }
}

void LastOrderGame::OnAction(engine::core::Engine& eng, const engine::core::EngineAction& action) {
    if (action.type == "give_item") {
        m_cooking.StartPreparation(eng, action.target, 1.4f);
    } else if (action.type == "set_flag") {
        if (action.target == "kurt_served") {
            m_cooking.PlaceDishOnTable("trig_booth1", "coffee_sugar");
        } else if (action.target == "katya_served") {
            m_cooking.PlaceDishOnTable("trig_booth3", "tea_sugar");
        } else if (action.target == "n2_arkhip_served" || action.target == "n4_officer_served") {
            m_cooking.PlaceDishOnTable("trig_booth3", "coffee_black");
        } else if (action.target == "kurt5_served") {
            m_cooking.PlaceDishOnTable("trig_booth3", "coffee_sugar");
        } else if (action.target == "danila_served" || action.target == "semen_served" || action.target == "n3_gleb_served") {
            m_cooking.PlaceDishOnTable("trig_booth4", "coffee_sugar");
        } else if (action.target == "mikhail_served") {
            m_cooking.PlaceDishOnTable("trig_booth4", "cherry_pie");
        } else if (action.target == "korsakov_served") {
            m_cooking.PlaceDishOnTable("trig_booth2", "tea_sugar");
        } else if (action.target == "valya_served" || action.target == "n2_valya_served") {
            m_cooking.PlaceDishOnTable("trig_booth2", "cherry_pie");
        } else if (action.target == "hatman_served_safe" || action.target == "hatman_served_anomaly") {
            m_cooking.PlaceDishOnTable("trig_booth5", "tea_sugar");
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
                if (g.anomalous && g.spawned) {
                    g.spawned = false;
                    g.finished = true;
                    eng.Physics().SetTriggerEnabled(g.triggerId, false);
                    break;
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
        } else if (action.target == "start_minigame_jukebox") {
            StartMinigame(eng, systems::MinigameType::Night_JukeboxTuner2D);
        } else if (action.target == "start_night_2") {
            StartNewShift(eng, 2, false);
        } else if (action.target == "start_night_3") {
            StartNewShift(eng, 3, false);
        } else if (action.target == "start_night_4") {
            StartNewShift(eng, 4, false);
        } else if (action.target == "start_night_5") {
            StartNewShift(eng, 5, false);
        } else if (action.target == "key_moment_n3_choice_owner") {
            SaveKeyMoment(eng, "Ночь 3: Выбор Михаила (Хороший путь)");
        } else if (action.target == "key_moment_n3_choice_defy") {
            SaveKeyMoment(eng, "Ночь 3: Выбор Разлома (Плохой путь)");
        } else if (action.target == "n3_radio_alarm") {
            eng.State().SetFlag("n3_radio_urgent_tune");
            eng.Physics().UpdateTriggerDialogue("trig_radio", "n3_radio_tune_prompt", "⚠ Срочно настроить радио [Шипение эфира]");
            m_lightsFlickering = true;
            eng.Renderer().TriggerFlash(Color{80, 255, 120, 160}, 0.5f);
            eng.Audio().PlaySfx("audio/sfx/sfx_radio_static.wav", 0.95f);
            m_saveBannerText = "РАДИО НА БАРНОЙ СТОЙКЕ ПРИНИМАЕТ СИГНАЛ БЕДСТВИЯ!";
            m_saveBannerTimer = 4.0f;
        } else if (action.target == "n3_banish_mimic") {
            eng.Renderer().TriggerFlash(Color{255, 245, 200, 240}, 0.8f);
            eng.Audio().PlaySfx("audio/sfx/sfx_cash_register.wav", 0.95f);
            m_lightsFlickering = false;
            m_blackoutMode = false;
            eng.Lighting().SetAmbientColor(Color{112, 96, 92, 255});
            m_saveBannerText = "СУЩНОСТЬ СТОЛА №6 ИЗГНАНА! РАЗЛОМ ЗАПЕЧАТАН!";
            m_saveBannerTimer = 4.0f;
            SaveKeyMoment(eng, "Ночь 3: Разлом запечатан (Хорошая концовка)");
        } else if (action.target == "n3_rift_consume") {
            eng.Renderer().TriggerFlash(Color{180, 20, 25, 230}, 0.9f);
            eng.Audio().PlaySfx("audio/sfx/sfx_cat_hiss.wav", 0.95f);
            m_blackoutMode = true;
            m_lightsFlickering = true;
            m_saveBannerText = "РАЗЛОМ ПОГЛОТИЛ КАФЕ! ПЛОХАЯ КОНЦОВКА!";
            m_saveBannerTimer = 4.0f;
            SaveKeyMoment(eng, "Ночь 3: Поглощение Разломом (Плохая концовка)");
        } else if (action.target == "return_main_menu") {
            m_mainMenu.Init(eng);
            m_screen = GameScreen::MainMenu;
        } else if (action.target == "open_mirror_inspector") {
            m_inspectionUI.ToggleMirrorInspector(eng);
        }
    }
}

void LastOrderGame::TriggerScreamer(engine::core::Engine& eng, const std::string& screamerId) {
    std::string texPath = "sprites/screamers/" + screamerId + ".png";
    Texture2D tex = eng.Assets().GetTexture(texPath);
    eng.Renderer().TriggerScreamerOverlay(tex, 2.25f, true);
    eng.Renderer().TriggerShake(9.5f, 1.90f);
    eng.Renderer().TriggerFlash(Color{235, 10, 20, 240}, 0.55f);
    eng.Time().PauseFor(1.35f);

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

    eng.Audio().PlaySfx("audio/sfx/sfx_blackout_burst.wav", 1.0f);
    eng.Audio().PlaySfx("audio/sfx/sfx_heavy_knock.wav", 1.0f);
}

void LastOrderGame::OnRenderOffscreen(engine::core::Engine& eng) {
    if (m_screen == GameScreen::Chapter2_Motel3D) {
        m_motel3D.RenderOffscreen(eng);
    }
    if (m_screen == GameScreen::Chapter3_House3D) {
        m_house3D.RenderOffscreen(eng);
    }
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
    if (m_screen == GameScreen::Chapter2_Motel3D) {
        m_motel3D.Render3D(eng);
        return;
    }
    if (m_screen == GameScreen::CustomChapter) {
        m_custom.RenderWorld(eng);
        return;
    }
    if (m_screen == GameScreen::Chapter3_House3D) {
        m_house3D.Render3D(eng);
        return;
    }

    int nightToRender = (m_screen == GameScreen::EndlessMode) ? 2 : m_currentNight;

    if (m_visor3DActive) {
        m_world.Render3DVisor(eng, nightToRender, m_playerBody.Center(),
                              m_visorYaw, m_visorPitch, m_guests,
                              m_anomalies3D, m_walkAnimTimer, m_gunRecoilTimer);
    } else {
        m_world.Render(eng, nightToRender,
                       m_playerBody, m_playerFacing, m_isMoving, m_walkAnimTimer,
                       m_lightsFlickering, m_blackoutMode, m_debugMode, m_nearestTrigger,
                       m_guests, m_anomalies3D);
        m_cooking.RenderWorld(eng, m_world.ComputeCameraOffset(m_playerBody.Center()));
        m_tutorial.RenderWorldGuides(eng, m_playerBody.Center());
    }
}

void LastOrderGame::RenderDebugOverlay(engine::core::Engine& eng) const {
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    if (!m_debugMode) return;
    Font font = eng.Assets().GetMainFont();

    DrawRectangle(14, 108, 415, 212, Color{10, 18, 12, 235});
    DrawRectangleLinesEx(Rectangle{14, 108, 415, 212}, 2.0f, Color{80, 240, 120, 255});

    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "DEBUG [F3] | FPS: %d | Ночь: %d\n"
                  "Позиция: (%.1f, %.1f)\n"
                  "Noclip [N]: %s | Визор: %s\n"
                  "Триггер: %s (%s)\n"
                  "[1..5] Ночь | [F2] +30м | [L] Свет\n"
                  "[F5..F7, F9, F10] Мини-игры 1..5\n"
                  "[F8 / G] Скример",
                  GetFPS(), m_currentNight,
                  m_playerBody.Center().x, m_playerBody.Center().y,
                  m_debugNoclip ? "ВКЛ" : "ВЫКЛ",
                  m_visor3DActive ? "3D" : "2D",
                  m_nearestTrigger.has_value() ? m_nearestTrigger->id.c_str() : "нет",
                  m_nearestTrigger.has_value() ? m_nearestTrigger->dialogueId.c_str() : "-");

    DrawTextEx(font, buf, Vector2{24, 116}, 16.5f, 1.0f, Color{110, 255, 140, 255});
#else
    (void)eng;
#endif
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
    if (m_screen == GameScreen::Chapter2_Motel3D) {
        m_motel3D.RenderUI(eng);
        m_settingsUI.Render(eng);
        return;
    }
    if (m_screen == GameScreen::CustomChapter) {
        m_custom.RenderUI(eng);
        if (game::chapter3::Ch3Graphics::Get().showFps) {
            std::string fpsText = std::to_string(GetFPS()) + " FPS";
            DrawTextEx(eng.Assets().GetMainFont(), fpsText.c_str(), {16.0f, 12.0f}, 18.0f, 1.0f, Color{150, 255, 150, 230});
        }
        m_settingsUI.Render(eng);
        return;
    }
    if (m_screen == GameScreen::Chapter3_House3D) {
        m_house3D.RenderUI(eng);
        m_settingsUI.Render(eng);
        return;
    }

    Font font = eng.Assets().GetMainFont();

    if (m_saveBannerTimer > 0.0f && !m_saveBannerText.empty()) {
        DrawRectangle(420, 58, 440, 36, Color{42, 23, 21, 240});
        DrawRectangleLines(420, 58, 440, 36, Color{244, 184, 96, 255});
        engine::graphics::TextLayout::DrawCentered(font, m_saveBannerText, 640.0f, 66.0f, 20.0f, Color{244, 184, 96, 255});
    }

    int activeAnomalies = 0;
    for (const auto& a : m_anomalies3D) {
        if (a.active) activeAnomalies++;
    }

    if (m_visor3DActive) {
        DrawRectangleLinesEx(Rectangle{10, 10, 1260, 700}, 2.0f, Color{90, 245, 200, 180});
        DrawCircleLines(640, 360, 18.0f, Color{90, 245, 200, 200});
        DrawLine(620, 360, 660, 360, Color{90, 245, 200, 200});
        DrawLine(640, 340, 640, 380, Color{90, 245, 200, 200});

        if (eng.State().GetSanity() <= 75) {
            DrawRectangle(370, 674, 540, 30, Color{36, 12, 14, 230});
            DrawRectangleLines(370, 674, 540, 30, Color{245, 70, 70, 240});
            engine::graphics::TextLayout::DrawCentered(font, "⚠ ДЕЗОРИЕНТАЦИЯ [РАССУДОК <= 75%]: УПРАВЛЕНИЕ [A]/[D] ИНВЕРТИРОВАНО", 640.0f, 680.0f, 15.0f, Color{255, 120, 120, 255});
        }

        if (m_hitMarkerTimer > 0.0f) {
            DrawLineEx(Vector2{624, 344}, Vector2{656, 376}, 3.0f, Color{255, 55, 55, 255});
            DrawLineEx(Vector2{656, 344}, Vector2{624, 376}, 3.0f, Color{255, 55, 55, 255});
        }

        if (!m_scannerTargetText.empty()) {
            DrawRectangle(420, 396, 440, 32, Color{42, 12, 16, 225});
            DrawRectangleLines(420, 396, 440, 32, Color{255, 70, 70, 240});
            engine::graphics::TextLayout::DrawCentered(font, m_scannerTargetText, 640.0f, 403.0f, 18.0f, Color{255, 110, 110, 255});
        }

        if (m_visorKillBannerTimer > 0.0f && !m_visorKillBanner.empty()) {
            DrawRectangle(420, 64, 440, 34, Color{36, 20, 14, 235});
            DrawRectangleLines(420, 64, 440, 34, Color{244, 184, 96, 240});
            engine::graphics::TextLayout::DrawCentered(font, m_visorKillBanner, 640.0f, 72.0f, 18.0f, Color{244, 184, 96, 255});
        }
    }

    engine::math::Vec2 camOff = m_world.ComputeCameraOffset(m_playerBody.Center());
    if (m_screen == GameScreen::EndlessMode) {
        m_endlessDirector.RenderUI(eng);
        m_cooking.RenderUI(eng, camOff);
        RenderDebugOverlay(eng);
        m_settingsUI.Render(eng);
        return;
    }

    m_tutorial.RenderUIOverlay(eng);
    m_cooking.RenderUI(eng, camOff);

    DrawRectangle(1038, 12, 228, 84, Color{10, 13, 26, 232});
    Color panelBorder = (activeAnomalies > 0) ? Color{214, 45, 45, 255} : Color{196, 122, 59, 255};
    DrawRectangleLinesEx(Rectangle{1038, 12, 228, 84}, 2.0f, panelBorder);

    std::string chapterNight = "ГЛ." + std::to_string(m_currentChapter) + " НОЧЬ " + std::to_string(m_currentNight);
    DrawTextEx(font, chapterNight.c_str(), Vector2{1052, 18}, 16.5f, 1.0f, Color{143, 169, 196, 255});

    std::string timeStr = m_sanity.FormatHudClock(eng);
    DrawTextEx(font, timeStr.c_str(), Vector2{1052, 38}, 24.0f, 1.0f, Color{244, 184, 96, 255});

    int san = eng.State().GetSanity();
    char sanBuf[64];
    if (san <= 75) {
        std::snprintf(sanBuf, sizeof(sanBuf), "Рассудок: %d%% [⚠ Срыв]", san);
    } else {
        std::snprintf(sanBuf, sizeof(sanBuf), "Рассудок: %d%%", san);
    }
    Color sanCol = (san <= 40) ? Color{214, 45, 45, 255} : (san <= 75) ? Color{245, 160, 60, 255} : Color{232, 227, 217, 255};
    DrawTextEx(font, sanBuf, Vector2{1052, 66}, 18.5f, 1.0f, sanCol);

    std::string held = eng.State().GetHeldItemName();
    if (!held.empty()) {
        DrawRectangle(14, 12, 280, 44, Color{10, 13, 26, 232});
        DrawRectangleLinesEx(Rectangle{14, 12, 280, 44}, 2.0f, Color{196, 122, 59, 255});
        std::string trayText = "В руках: " + held;
        DrawTextEx(font, trayText.c_str(), Vector2{26, 24}, 18.5f, 1.0f, Color{244, 184, 96, 255});
    }

    if (!m_visor3DActive && !eng.Dialogue().IsActive() && !m_inspectionUI.IsAnyModalOpen() && !m_sanity.IsGameOver()) {
        DrawTextEx(font, "[G] Осмотр стекла  |  [I] Памятка  |  [TAB] Задачи  |  [F5] Сохранить", Vector2{24, 692}, 16.0f, 1.0f, Color{188, 178, 162, 220});
    }

    if (!m_visor3DActive && !eng.Dialogue().IsActive() && !m_inspectionUI.IsAnyModalOpen() && !m_sanity.IsGameOver() && m_nearestTrigger.has_value()) {
        std::string prompt = m_nearestTrigger->label;
        Vector2 sz = MeasureTextEx(font, prompt.c_str(), 20.0f, 1.0f);
        float px = (1280.0f - sz.x) * 0.5f;
        DrawRectangle(static_cast<int>(px - 16), 658, static_cast<int>(sz.x + 32), 36, Color{42, 23, 21, 240});
        DrawRectangleLinesEx(Rectangle{px - 16, 658, sz.x + 32, 36}, 1.5f, Color{244, 184, 96, 235});
        DrawTextEx(font, prompt.c_str(), Vector2{px, 666}, 20.0f, 1.0f, Color{244, 184, 96, 255});
    }

    m_inspectionUI.Render(eng, m_currentNight, BuildMirrorState(eng));
    RenderDebugOverlay(eng);

    if (m_sanity.IsGameOver()) {
        m_sanity.RenderGameOverModal(eng, m_currentNight);
    } else if (m_screen == GameScreen::Paused) {
        RenderPauseMenu(eng);
    }
    m_settingsUI.Render(eng);
}

void LastOrderGame::OnShutdown(engine::core::Engine& eng) {
    m_motel3D.Shutdown(eng);
    if (m_screen == GameScreen::CustomChapter) m_custom.Shutdown(eng);
    if (IsCursorHidden()) EnableCursor();
}

}