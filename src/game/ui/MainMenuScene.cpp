#include "game/ui/MainMenuScene.h"
#include "game/GameVersion.h"
#include "game/systems/SaveSystem.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace game::ui {

void MainMenuScene::Init(engine::core::Engine& eng) {
    m_state = MenuState::Main;
    m_selectedBtn = 0;
    m_animMode = 0;
    m_animCycleTimer = 0.0f;
    m_lightningTimer = 0.0f;

    m_rain.clear();
    for (int i = 0; i < 55; ++i) {
        m_rain.push_back({
            static_cast<float>(std::rand() % 350),
            static_cast<float>(std::rand() % 190),
            120.0f + static_cast<float>(std::rand() % 80),
            7.0f + static_cast<float>(std::rand() % 8)
        });
    }

    m_ripples.clear();
    for (int i = 0; i < 7; ++i) {
        m_ripples.push_back({
            16.0f + static_cast<float>(std::rand() % 288),
            126.0f + static_cast<float>(std::rand() % 48),
            static_cast<float>(std::rand() % 8),
            6.0f + static_cast<float>(std::rand() % 6)
        });
    }

    m_steam.clear();
    for (int i = 0; i < 12; ++i) {
        m_steam.push_back({
            212.0f + static_cast<float>((std::rand() % 6) - 3),
            114.0f,
            static_cast<float>(i) * 0.18f,
            2.2f,
            static_cast<float>((std::rand() % 10) - 5) * 0.1f
        });
    }

    eng.Audio().PlayAmbient("audio/bgm/amb_cafe_calm_lp.ogg", 0.55f);
}

void MainMenuScene::SetupMenuLighting(engine::core::Engine& eng, float t) {
    auto& light = eng.Lighting();
    light.ClearLights();

    if (m_animMode == 0) {
        bool flash = (m_lightningTimer > 7.15f && m_lightningTimer < 7.35f);
        Color amb = flash ? Color{110, 130, 168, 255} : Color{18, 22, 38, 255};
        light.SetAmbientColor(amb);

        float flicker = 1.0f + std::sin(t * 12.0f) * 0.08f;
        light.AddPointLight({"menu_neon_sign",  {160.0f, 24.0f},  135.0f, Color{244, 184, 96, 255}, 1.25f * flicker, 3.0f, 0.05f, true});
        light.AddPointLight({"menu_cafe_window",{210.0f, 88.0f},  105.0f, Color{216, 114, 52, 255}, 1.10f, 2.0f, 0.04f, true});
        light.AddPointLight({"menu_car_head_L", {64.0f,  142.0f}, 60.0f,  Color{240, 230, 190, 255},0.85f, 1.5f, 0.02f, true});
        light.AddPointLight({"menu_car_head_R", {78.0f,  142.0f}, 60.0f,  Color{240, 230, 190, 255},0.85f, 1.5f, 0.02f, true});
    } else {
        light.SetAmbientColor(Color{32, 24, 28, 255});
        float swingX = 196.0f + std::sin(t * 2.2f) * 34.0f;
        float swingY = 48.0f  + std::abs(std::cos(t * 2.2f)) * 6.0f;
        light.AddPointLight({"menu_pendulum",    {swingX, swingY}, 115.0f, Color{244, 184, 96, 255}, 1.2f, 4.0f, 0.04f, true});
        light.AddPointLight({"menu_mirror_glow", {156.0f, 58.0f},  62.0f,  Color{168, 36, 36, 255},  0.75f,9.0f, 0.25f, true});
    }
}

MenuCommand MainMenuScene::Update(engine::core::Engine& eng, float dt) {
    float t = eng.Time().GetTotalRealTime();

    for (auto& r : m_rain) {
        r.y += r.speed * dt;
        r.x -= r.speed * 0.22f * dt;
        if (r.y > 185.0f) {
            r.y = -10.0f;
            r.x = static_cast<float>(std::rand() % 350);
        }
    }

    for (auto& rp : m_ripples) {
        rp.radius += dt * 9.0f;
        if (rp.radius > rp.maxRadius) {
            rp.radius = 0.0f;
            rp.x = 16.0f + static_cast<float>(std::rand() % 288);
            rp.y = 126.0f + static_cast<float>(std::rand() % 48);
        }
    }

    for (auto& sp : m_steam) {
        sp.age += dt;
        sp.y -= dt * 14.0f;
        sp.x += std::sin(t * 3.0f + sp.age * 2.0f) * dt * 5.0f;
        if (sp.age >= sp.maxAge) {
            sp.age = 0.0f;
            sp.x = 212.0f + static_cast<float>((std::rand() % 6) - 3);
            sp.y = 114.0f;
        }
    }

    m_lightningTimer += dt;
    if (m_lightningTimer > 7.5f) m_lightningTimer = 0.0f;

    if (m_autoCycleAnims) {
        m_animCycleTimer += dt;
        if (m_animCycleTimer >= 12.0f) {
            m_animCycleTimer = 0.0f;
            m_animMode = 1 - m_animMode;
        }
    }

    if (IsKeyPressed(KEY_TAB)) {
        m_animMode = 1 - m_animMode;
        m_autoCycleAnims = false;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.6f);
    }

    SetupMenuLighting(eng, t);

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (m_state == MenuState::NightSelect) {
            m_state = MenuState::ChapterSelect;
            m_selectedBtn = 0;
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
            return MenuCommand::None;
        }
        if (m_state == MenuState::ChapterSelect || m_state == MenuState::SaveSelect) {
            m_state = MenuState::Main;
            m_selectedBtn = 0;
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
            return MenuCommand::None;
        }
        if (m_state == MenuState::Main) {
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
            return MenuCommand::QuitGame;
        }
    }

#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    constexpr bool isDebug = true;
#else
    constexpr bool isDebug = false;
#endif

    int maxUnlocked = systems::SaveSystem::GetMaxUnlockedNight();

    int btnCount = 5;
    if (m_state == MenuState::ChapterSelect) btnCount = 6;
    else if (m_state == MenuState::NightSelect) btnCount = 7;
    else if (m_state == MenuState::SaveSelect) btnCount = 4;

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
        m_selectedBtn = (m_selectedBtn - 1 + btnCount) % btnCount;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.5f);
    }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
        m_selectedBtn = (m_selectedBtn + 1) % btnCount;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.5f);
    }

    bool triggerSelected = (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE));
    engine::math::Vec2 mpos = eng.Renderer().GetVirtualMousePos();

    float startY = 220.0f;
    float stepY = 64.0f;
    float btnH = 52.0f;
    float btnW = 460.0f;

    if (m_state == MenuState::ChapterSelect) {
        startY = 224.0f;
        stepY = 62.0f;
        btnH = 52.0f;
    } else if (m_state == MenuState::NightSelect) {
        startY = 224.0f;
        stepY = 58.0f;
        btnH = 48.0f;
    } else if (m_state == MenuState::SaveSelect) {
        startY = 224.0f;
        stepY = 100.0f;
        btnH = 86.0f;
        btnW = 540.0f;
    }

    for (int i = 0; i < btnCount; ++i) {
        float by = (m_state == MenuState::SaveSelect && i == 3) ? (startY + 3 * stepY + 10.0f) : (startY + i * stepY);
        float bh = (m_state == MenuState::SaveSelect && i == 3) ? 50.0f : btnH;
        engine::math::Rect btnRect = {58.0f, by, btnW, bh};
        if (btnRect.Contains(mpos)) {
            if (m_selectedBtn != i) {
                m_selectedBtn = i;
                eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                triggerSelected = true;
            }
        }
    }

    if (triggerSelected) {
        eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.65f);

        if (m_state == MenuState::Main) {
            switch (m_selectedBtn) {
                case 0:
                    m_state = MenuState::ChapterSelect;
                    m_selectedBtn = 0;
                    return MenuCommand::None;
                case 1:
                    m_state = MenuState::SaveSelect;
                    m_selectedBtn = 0;
                    return MenuCommand::None;
                case 2:
                    return MenuCommand::StartEndlessMode;
                case 3:
                    return MenuCommand::OpenSettings;
                case 4:
                    return MenuCommand::QuitGame;
            }
        } else if (m_state == MenuState::ChapterSelect) {
            switch (m_selectedBtn) {
                case 0:
                    m_state = MenuState::NightSelect;
                    m_selectedBtn = 0;
                    return MenuCommand::None;
                case 1:
                    return MenuCommand::StartChapter2_Motel3D;
                case 2:
                    return MenuCommand::StartChapter3_House3D;
                case 3:
                    return MenuCommand::StartChapter35;
                case 4:
                    return MenuCommand::StartChapter4;
                case 5:
                    m_state = MenuState::Main;
                    m_selectedBtn = 0;
                    return MenuCommand::None;
            }
        } else if (m_state == MenuState::NightSelect) {
            switch (m_selectedBtn) {
                case 0: return MenuCommand::StartNight1_IntroTutorial;
                case 1: return MenuCommand::StartNight1_NoTutorial;
                case 2:
                    if (isDebug || maxUnlocked >= 2) return MenuCommand::StartNight2_Independent;
                    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                    return MenuCommand::None;
                case 3:
                    if (isDebug || maxUnlocked >= 3) return MenuCommand::StartNight3_Pipes;
                    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                    return MenuCommand::None;
                case 4:
                    if (isDebug || maxUnlocked >= 4) return MenuCommand::StartNight4_Phone;
                    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                    return MenuCommand::None;
                case 5:
                    if (isDebug || maxUnlocked >= 5) return MenuCommand::StartNight5_Finale;
                    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                    return MenuCommand::None;
                case 6:
                    m_state = MenuState::ChapterSelect;
                    m_selectedBtn = 0;
                    return MenuCommand::None;
            }
        } else if (m_state == MenuState::SaveSelect) {
            switch (m_selectedBtn) {
                case 0:
                    if (systems::SaveSystem::HasSlot(0)) return MenuCommand::LoadSlot0;
                    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                    return MenuCommand::None;
                case 1:
                    if (systems::SaveSystem::HasSlot(1)) return MenuCommand::LoadSlot1;
                    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                    return MenuCommand::None;
                case 2:
                    if (systems::SaveSystem::HasSlot(2)) return MenuCommand::LoadSlot2;
                    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
                    return MenuCommand::None;
                case 3:
                    m_state = MenuState::Main;
                    m_selectedBtn = 0;
                    return MenuCommand::None;
            }
        }
    }

    return MenuCommand::None;
}

void MainMenuScene::RenderWorld(engine::core::Engine& eng) {
    float t = eng.Time().GetTotalRealTime();
    if (m_animMode == 0) {
        RenderAnim1_HighwayRainAndNeon(eng, t);
    } else {
        RenderAnim2_SwingingLampAndMirror(eng, t);
    }
}

void MainMenuScene::RenderAnim1_HighwayRainAndNeon(engine::core::Engine& eng, float t) {
    auto& assets = eng.Assets();
    Texture2D tsExt = assets.GetTexture("sprites/tilesets/ts_exterior_highway.png");
    Texture2D lera  = assets.GetTexture("sprites/characters/spr_lera.png");
    Texture2D cat   = assets.GetTexture("sprites/characters/spr_cat_ugolyok.png");
    Texture2D vera  = assets.GetTexture("sprites/characters/spr_vera.png");

    DrawRectangle(0, 0, 320, 48, Color{10, 13, 26, 255});
    for (int tx = 0; tx < 320; tx += 12) {
        int treeH = 14 + ((tx * 7) % 12);
        DrawRectangle(tx, 48 - treeH, 10, treeH, Color{18, 25, 44, 255});
    }

    for (int ty = 3; ty < 12; ++ty) {
        for (int tx = 0; tx < 20; ++tx) {
            Rectangle src = {static_cast<float>((tx % 4) * 16), static_cast<float>((ty % 4) * 16), 16.0f, 16.0f};
            DrawTextureRec(tsExt, src, Vector2{static_cast<float>(tx * 16), static_cast<float>(ty * 16)}, WHITE);
        }
    }

    DrawRectangle(0, 126, 320, 54, Color{22, 24, 30, 255});
    for (int lx = 0; lx < 320; lx += 40) {
        int dashOff = (static_cast<int>(t * 40.0f)) % 40;
        DrawRectangle(lx - dashOff, 150, 22, 3, Color{244, 184, 96, 210});
    }

    DrawRectangle(140, 26, 170, 72, Color{28, 18, 22, 255});
    DrawRectangleLines(140, 26, 170, 72, Color{196, 122, 59, 255});
    DrawRectangle(180, 42, 60, 36, Color{244, 184, 96, 220});
    DrawRectangleLines(180, 42, 60, 36, Color{107, 51, 34, 255});

    DrawTextureRec(cat, Rectangle{0, 0, 16, 16}, Vector2{200, 58}, WHITE);

    int leraFrame = (static_cast<int>(t * 2.5f)) % 4;
    DrawTextureRec(lera, Rectangle{static_cast<float>(leraFrame * 32), 0, 32, 32}, Vector2{242, 46}, WHITE);

    if (static_cast<int>(t * 0.35f) % 2 == 1) {
        DrawTextureRec(vera, Rectangle{0, 0, 32, 32}, Vector2{150, 46}, Color{255, 255, 255, 140});
    }

    for (const auto& sp : m_steam) {
        float alpha = (1.0f - sp.age / sp.maxAge) * 0.6f;
        DrawCircle(static_cast<int>(sp.x), static_cast<int>(sp.y), 2.5f + sp.age * 2.0f,
                   ColorAlpha(Color{232, 227, 217, 255}, alpha));
    }

    for (const auto& rp : m_ripples) {
        DrawCircleLines(static_cast<int>(rp.x), static_cast<int>(rp.y), rp.radius,
                        ColorAlpha(Color{143, 169, 196, 255}, 1.0f - rp.radius / rp.maxRadius));
    }

    for (const auto& r : m_rain) {
        DrawLine(static_cast<int>(r.x), static_cast<int>(r.y),
                 static_cast<int>(r.x - 2.0f), static_cast<int>(r.y + r.len),
                 Color{143, 169, 196, 180});
    }
}

void MainMenuScene::RenderAnim2_SwingingLampAndMirror(engine::core::Engine& eng, float t) {
    auto& assets = eng.Assets();
    Texture2D lera = assets.GetTexture("sprites/characters/spr_lera.png");
    Texture2D cat  = assets.GetTexture("sprites/characters/spr_cat_ugolyok.png");

    DrawRectangle(0, 0, 320, 180, Color{16, 12, 14, 255});
    for (int y = 0; y < 180; y += 16) {
        DrawLine(0, y, 320, y, Color{26, 18, 20, 255});
    }

    DrawRectangle(130, 22, 52, 68, Color{20, 28, 38, 255});
    DrawRectangleLines(130, 22, 52, 68, Color{196, 122, 59, 255});

    float redFlicker = (std::sin(t * 8.0f) > 0.6f) ? 0.35f : 0.08f;
    DrawRectangle(134, 26, 44, 60, ColorAlpha(Color{214, 69, 69, 255}, redFlicker));

    DrawRectangle(70, 94, 180, 22, Color{107, 51, 34, 255});
    DrawRectangle(70, 92, 180, 3,  Color{196, 122, 59, 255});

    DrawTextureRec(cat, Rectangle{16, 0, 16, 16}, Vector2{90, 80}, WHITE);

    int lFrame = (static_cast<int>(t * 2.0f)) % 4;
    DrawTextureRec(lera, Rectangle{static_cast<float>(lFrame * 32), 64, 32, 32}, Vector2{170, 72}, WHITE);

    float swingX = 196.0f + std::sin(t * 2.2f) * 34.0f;
    float swingY = 48.0f  + std::abs(std::cos(t * 2.2f)) * 6.0f;
    DrawLine(196, 0, static_cast<int>(swingX), static_cast<int>(swingY), Color{80, 70, 60, 255});
    DrawCircle(static_cast<int>(swingX), static_cast<int>(swingY), 4.5f, Color{244, 184, 96, 255});
}

void MainMenuScene::RenderUI(engine::core::Engine& eng) const {
    Font font = eng.Assets().GetMainFont();

    DrawTextEx(font, "LAST ORDER", Vector2{58, 88}, 54.0f, 2.0f, Color{244, 184, 96, 255});
    DrawTextEx(font, "ПОСЛЕДНИЙ ЗАКАЗ", Vector2{58, 146}, 24.0f, 2.0f, Color{214, 69, 69, 255});

    DrawRectangle(58, 180, 540, 3, Color{168, 36, 36, 255});

#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    constexpr bool isDebug = true;
#else
    constexpr bool isDebug = false;
#endif

    int maxUnlocked = systems::SaveSystem::GetMaxUnlockedNight();

    if (m_state == MenuState::Main) {
        const char* mainButtons[5] = {
            "Играть",
            "Выбрать сохранение",
            "Свободный режим",
            "Настройки",
            "Выход из игры"
        };

        for (int i = 0; i < 5; ++i) {
            bool sel = (m_selectedBtn == i);
            float bx = 58.0f;
            float by = 216.0f + i * 64.0f;
            float bw = 460.0f;
            float bh = 52.0f;

            bool isQuitBtn = (i == 4);
            Color bgCol = sel ? (isQuitBtn ? Color{130, 36, 36, 245} : Color{107, 51, 34, 245}) : (isQuitBtn ? Color{52, 20, 20, 215} : Color{42, 23, 21, 215});
            Color borderCol = sel ? (isQuitBtn ? Color{255, 96, 96, 255} : Color{244, 184, 96, 255}) : (isQuitBtn ? Color{196, 70, 70, 160} : Color{196, 122, 59, 160});

            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh), bgCol);
            DrawRectangleLinesEx(Rectangle{bx, by, bw, bh}, sel ? 2.5f : 1.0f, borderCol);

            if (sel) {
                DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 8, static_cast<int>(bh), isQuitBtn ? Color{255, 96, 96, 255} : Color{244, 184, 96, 255});
            }

            Color txtCol = sel ? (isQuitBtn ? Color{255, 230, 230, 255} : Color{244, 184, 96, 255}) : (isQuitBtn ? Color{232, 190, 190, 230} : Color{232, 227, 217, 230});
            DrawTextEx(font, mainButtons[i], Vector2{bx + 24.0f, by + 14.0f}, 22.0f, 1.0f, txtCol);
        }
        DrawTextEx(font, "[ESC] Выход из игры    [ENTER / ЛКМ] Выбрать", Vector2{58.0f, 544.0f}, 17.0f, 1.0f, Color{196, 170, 150, 180});
    } else if (m_state == MenuState::ChapterSelect) {
        DrawTextEx(font, "ВЫБОР ГЛАВЫ", Vector2{58, 184}, 21.0f, 1.0f, Color{244, 184, 96, 255});

        const char* chTitles[6] = {
            "Глава 1: Кафе «Тёплый угол» [5 Ночей]",
            "Глава 2: Мотель «Северное сияние» [3D]",
            "Глава 3: Дом смотрителя «Побег» [3D Horror]",
            "Глава 3.5: Разъезд 86 [3D]",
            "Глава 4: Дом на сопке — Финал [RE7 Horror]",
            "Назад в главное меню"
        };

        for (int i = 0; i < 6; ++i) {
            bool sel = (m_selectedBtn == i);
            float bx = 58.0f;
            float by = 224.0f + i * 62.0f;
            float bw = 500.0f;
            float bh = 52.0f;

            bool locked = false;
            Color bgCol = locked ? Color{26, 18, 18, 180} : (sel ? Color{107, 51, 34, 245} : Color{42, 23, 21, 215});
            Color borderCol = locked ? Color{110, 80, 70, 120} : (sel ? Color{244, 184, 96, 255} : Color{196, 122, 59, 160});

            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh), bgCol);
            DrawRectangleLinesEx(Rectangle{bx, by, bw, bh}, sel ? 2.5f : 1.0f, borderCol);

            if (sel && !locked) {
                DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 8, static_cast<int>(bh), Color{244, 184, 96, 255});
            }

            Color txtCol = locked ? Color{140, 120, 120, 180} : (sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 230});
            DrawTextEx(font, chTitles[i], Vector2{bx + 22.0f, by + 14.0f}, 20.0f, 1.0f, txtCol);
        }
    } else if (m_state == MenuState::NightSelect) {
        DrawTextEx(font, isDebug ? "ГЛАВА 1: ВЫБОР СМЕНЫ [РЕЖИМ РАЗРАБОТЧИКА: ВСЕ НОЧИ ОТКРЫТЫ]" : "ГЛАВА 1: ВЫБОР СМЕНЫ",
                   Vector2{58, 192}, 19.5f, 1.0f, isDebug ? Color{110, 255, 140, 255} : Color{244, 184, 96, 255});

        std::string nightLabels[7];
        nightLabels[0] = "Ночь 1: Первые гости (С обучением)";
        nightLabels[1] = "Ночь 1: Первые гости (Без обучения)";

        bool u2 = (isDebug || maxUnlocked >= 2);
        bool u3 = (isDebug || maxUnlocked >= 3);
        bool u4 = (isDebug || maxUnlocked >= 4);
        bool u5 = (isDebug || maxUnlocked >= 5);

        nightLabels[2] = u2 ? "Ночь 2: Авария электросети" : "Ночь 2: [🔒 Заблокировано]";
        nightLabels[3] = u3 ? "Ночь 3: Голоса в трубах"     : "Ночь 3: [🔒 Заблокировано]";
        nightLabels[4] = u4 ? "Ночь 4: Полночный звонок"    : "Ночь 4: [🔒 Заблокировано]";
        nightLabels[5] = u5 ? "Ночь 5: Финал Разлома"       : "Ночь 5: [🔒 Заблокировано]";
        nightLabels[6] = "Назад к выбору главы";

        bool lockedFlags[7] = {false, false, !u2, !u3, !u4, !u5, false};

        for (int i = 0; i < 7; ++i) {
            bool sel = (m_selectedBtn == i);
            float bx = 58.0f;
            float by = 224.0f + i * 58.0f;
            float bw = 500.0f;
            float bh = 48.0f;

            bool locked = lockedFlags[i];
            Color bgCol = locked ? Color{26, 18, 18, 180} : (sel ? Color{107, 51, 34, 245} : Color{42, 23, 21, 215});
            Color borderCol = locked ? Color{90, 70, 70, 100} : (sel ? Color{244, 184, 96, 255} : Color{196, 122, 59, 160});

            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh), bgCol);
            DrawRectangleLinesEx(Rectangle{bx, by, bw, bh}, sel ? 2.0f : 1.0f, borderCol);

            if (sel && !locked) {
                DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 8, static_cast<int>(bh), Color{244, 184, 96, 255});
            }

            Color txtCol = locked ? Color{140, 115, 115, 160} : (sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 225});
            DrawTextEx(font, nightLabels[i].c_str(), Vector2{bx + 20.0f, by + 13.0f}, 19.0f, 1.0f, txtCol);
        }
    } else if (m_state == MenuState::SaveSelect) {
        DrawTextEx(font, "ВЫБОР СОХРАНЕНИЯ", Vector2{58, 192}, 21.0f, 1.0f, Color{244, 184, 96, 255});

        for (int i = 0; i < 3; ++i) {
            bool sel = (m_selectedBtn == i);
            float bx = 58.0f;
            float by = 224.0f + i * 100.0f;
            float bw = 540.0f;
            float bh = 86.0f;

            bool hasSlot = systems::SaveSystem::HasSlot(i);
            systems::SaveProfile sp = systems::SaveSystem::GetSlotInfo(i);

            Color bgCol = sel ? Color{107, 51, 34, 245} : Color{42, 23, 21, 215};
            Color borderCol = sel ? Color{244, 184, 96, 255} : Color{196, 122, 59, 160};

            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh), bgCol);
            DrawRectangleLinesEx(Rectangle{bx, by, bw, bh}, sel ? 2.5f : 1.0f, borderCol);

            if (sel) {
                DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 8, static_cast<int>(bh), Color{244, 184, 96, 255});
            }

            if (hasSlot && sp.valid) {
                std::string header = sp.slotTitle;
                if (!sp.timestamp.empty()) header += "  |  " + sp.timestamp;
                DrawTextEx(font, header.c_str(), Vector2{bx + 20.0f, by + 14.0f}, 19.5f, 1.0f,
                           sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 240});

                char sub[128];
                std::snprintf(sub, sizeof(sub), "Гл.%d  Ночь %d (%02d:%02d)   Рассудок: %d%%",
                              sp.chapter, sp.currentNight, sp.hour, sp.minute, sp.sanity);
                DrawTextEx(font, sub, Vector2{bx + 20.0f, by + 46.0f}, 17.5f, 1.0f, Color{196, 178, 156, 220});
            } else {
                std::string emptyTitle = (i == 0) ? "[Слот 1] Быстрое сохранение [F5] (Пусто)" :
                                         (i == 1) ? "[Слот 2] Авто: Начало смены (Пусто)" :
                                                    "[Слот 3] Авто: Ключевой момент (Пусто)";
                DrawTextEx(font, emptyTitle.c_str(), Vector2{bx + 20.0f, by + 18.0f}, 19.0f, 1.0f, Color{150, 130, 130, 180});
                DrawTextEx(font, "Нет данных сохранения", Vector2{bx + 20.0f, by + 48.0f}, 16.5f, 1.0f, Color{110, 95, 95, 160});
            }
        }

        bool backSel = (m_selectedBtn == 3);
        float bby = 534.0f;
        DrawRectangle(58, static_cast<int>(bby), 540, 50, backSel ? Color{107, 51, 34, 245} : Color{42, 23, 21, 215});
        DrawRectangleLinesEx(Rectangle{58.0f, bby, 540.0f, 50.0f}, backSel ? 2.5f : 1.0f, backSel ? Color{244, 184, 96, 255} : Color{196, 122, 59, 160});
        if (backSel) DrawRectangle(58, static_cast<int>(bby), 8, 50, Color{244, 184, 96, 255});
        DrawTextEx(font, "Назад в главное меню", Vector2{78, bby + 14.0f}, 21.0f, 1.0f,
                   backSel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 230});
    }

    DrawRectangle(0, 686, 1280, 34, Color{10, 13, 26, 238});
    DrawTextEx(font, "[W/S, ENTER / ЛКМ] Выбор | [ESC] Назад | [TAB] Сменить сцену | [F11] Экран",
               Vector2{24, 694}, 17.5f, 1.0f, Color{188, 178, 162, 230});
    std::string verText = game::GetVersionBuildString();
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    verText += " [DEBUG]";
#endif
    Vector2 verSize = MeasureTextEx(font, verText.c_str(), 17.5f, 1.0f);
    DrawTextEx(font, verText.c_str(), Vector2{1280.0f - verSize.x - 24.0f, 694.0f}, 17.5f, 1.0f, Color{143, 169, 196, 220});
}

}
