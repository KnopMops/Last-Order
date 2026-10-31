#include "game/ui/MainMenuScene.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace game::ui {

void MainMenuScene::Init(engine::core::Engine& eng) {
    m_selectedBtn = 0;
    m_inPlaySubMenu = false;
    m_animMode = 0;
    m_animCycleTimer = 0.0f;

    m_rain.clear();
    for (int i = 0; i < 48; ++i) {
        m_rain.push_back(RainDrop{
            static_cast<float>(std::rand() % 340),
            static_cast<float>(std::rand() % 180),
            135.0f + static_cast<float>(std::rand() % 65),
            8.0f + static_cast<float>(std::rand() % 8)
        });
    }

    m_ripples.clear();
    for (int i = 0; i < 10; ++i) {
        m_ripples.push_back(PuddleRipple{
            20.0f + static_cast<float>(std::rand() % 280),
            125.0f + static_cast<float>(std::rand() % 50),
            static_cast<float>(std::rand() % 7),
            6.0f + static_cast<float>(std::rand() % 5)
        });
    }

    m_steam.clear();
    for (int i = 0; i < 14; ++i) {
        m_steam.push_back(SteamPuff{
            212.0f + static_cast<float>((std::rand() % 9) - 4),
            116.0f - static_cast<float>(i * 3),
            static_cast<float>(i) * 0.15f,
            2.1f,
            static_cast<float>((std::rand() % 10) - 5) * 1.2f
        });
    }

    SetupMenuLighting(eng, 0.0f);
}

void MainMenuScene::SetupMenuLighting(engine::core::Engine& eng, float t) {
    auto& light = eng.Lighting();
    light.ClearLights();
    light.SetCameraOffset({0.0f, 0.0f});
    light.GetFlashlight().enabled = false;

    if (m_animMode == 0) {
        bool lightningFlash = (m_lightningTimer > 0.0f && m_lightningTimer < 0.22f);
        light.SetAmbientColor(lightningFlash ? Color{155, 175, 205, 255} : Color{38, 48, 74, 255});

        bool neonSpark = (static_cast<int>(t * 14.0f) % 23 == 0);
        light.AddPointLight({"menu_neon", {216.0f, 38.0f}, 85.0f, Color{214, 69, 69, 255},  neonSpark ? 0.35f : 1.15f, 12.0f, 0.08f, true});
        light.AddPointLight({"menu_win1", {186.0f, 88.0f}, 75.0f, Color{244, 184, 96, 255}, 1.05f, 2.0f, 0.03f, true});
        light.AddPointLight({"menu_win2", {254.0f, 88.0f}, 75.0f, Color{244, 184, 96, 255}, 0.95f, 2.5f, 0.03f, true});

        float carPhase = std::fmod(t * 0.32f, 1.0f);
        float carY = -30.0f + carPhase * 250.0f;
        light.AddPointLight({"menu_car",  {58.0f, carY},   72.0f, Color{216, 230, 242, 255}, 1.1f, 0.0f, 0.0f, true});
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

    if (m_inPlaySubMenu && IsKeyPressed(KEY_ESCAPE)) {
        m_inPlaySubMenu = false;
        m_selectedBtn = 0;
        eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
        return MenuCommand::None;
    }

    int btnCount = m_inPlaySubMenu ? 7 : 4;
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
    float startY = m_inPlaySubMenu ? 210.0f : 240.0f;
    float stepY  = m_inPlaySubMenu ? 56.0f  : 68.0f;
    float btnH   = m_inPlaySubMenu ? 46.0f  : 54.0f;

    for (int i = 0; i < btnCount; ++i) {
        engine::math::Rect btnRect = {58.0f, startY + i * stepY, 460.0f, btnH};
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
        if (!m_inPlaySubMenu) {
            switch (m_selectedBtn) {
                case 0:
                    m_inPlaySubMenu = true;
                    m_selectedBtn = 0;
                    return MenuCommand::None;
                case 1: return MenuCommand::StartEndlessMode;
                case 2: return MenuCommand::OpenSettings;
                case 3: return MenuCommand::QuitGame;
            }
        } else {
            switch (m_selectedBtn) {
                case 0: return MenuCommand::StartNight1_IntroTutorial;
                case 1: return MenuCommand::StartNight1_NoTutorial;
                case 2: return MenuCommand::StartNight2_Independent;
                case 3: return MenuCommand::StartNight3_Pipes;
                case 4: return MenuCommand::StartNight4_Phone;
                case 5: return MenuCommand::StartNight5_Finale;
                case 6:
                    m_inPlaySubMenu = false;
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

    DrawRectangle(132, 24, 174, 104, Color{42, 23, 21, 255});
    DrawRectangle(136, 28, 166, 96,  Color{107, 51, 34, 255});
    DrawRectangle(128, 20, 182, 10,  Color{196, 122, 59, 255});

    bool neonOn = (static_cast<int>(t * 14.0f) % 23 != 0);
    DrawRectangle(158, 6, 120, 16, Color{10, 13, 26, 255});
    DrawRectangleLines(158, 6, 120, 16, neonOn ? Color{214, 69, 69, 255} : Color{105, 18, 22, 255});
    for (int nx = 164; nx < 270; nx += 8) {
        Color tubeCol = neonOn ? Color{244, 184, 96, 255} : Color{72, 34, 24, 255};
        DrawRectangle(nx, 10, 5, 8, tubeCol);
    }

    for (int wx : {148, 222}) {
        DrawRectangle(wx - 2, 52, 64, 52, Color{42, 23, 21, 255});
        DrawRectangle(wx, 54, 60, 48, Color{244, 184, 96, 255});
        for (int sx = wx + 2; sx < wx + 58; sx += 4) {
            DrawRectangle(sx, 54, 2, 28, Color{216, 194, 162, 255});
        }
        DrawRectangle(wx, 82, 60, 20, Color{107, 51, 34, 255});
    }

    int leraStep = static_cast<int>(t * 3.5f) % 4;
    float leraX = 162.0f + std::sin(t * 1.2f) * 10.0f;
    DrawTextureRec(lera, Rectangle{static_cast<float>(leraStep * 32), 32.0f, 32.0f, 28.0f},
                   Vector2{std::round(leraX), 60.0f}, WHITE);

    int catFrame = static_cast<int>(t * 2.0f) % 2;
    DrawTextureRec(cat, Rectangle{static_cast<float>(catFrame * 16), 0.0f, 16.0f, 16.0f},
                   Vector2{258.0f, 70.0f}, WHITE);
    DrawRectangle(236, 77, 5, 5, Color{232, 227, 217, 255});
    int steamY = static_cast<int>(t * 8.0f) % 6;
    DrawRectangle(237, 74 - steamY, 2, 2, Color{232, 227, 217, 180});

    if (m_lightningTimer > 0.0f && m_lightningTimer < 0.35f) {
        DrawTextureRec(vera, Rectangle{0.0f, 0.0f, 32.0f, 32.0f}, Vector2{92.0f, 94.0f}, WHITE);
    }

    float carPhase = std::fmod(t * 0.32f, 1.0f);
    int carY = static_cast<int>(-30.0f + carPhase * 250.0f);
    DrawRectangle(44, carY, 16, 24, Color{27, 40, 69, 255});
    DrawRectangle(45, carY + 22, 4, 4, Color{216, 230, 242, 255});
    DrawRectangle(55, carY + 22, 4, 4, Color{216, 230, 242, 255});

    for (const auto& rp : m_ripples) {
        DrawCircleLines(static_cast<int>(rp.x), static_cast<int>(rp.y), rp.radius, Color{143, 169, 196, 140});
    }

    for (const auto& r : m_rain) {
        DrawLine(static_cast<int>(r.x), static_cast<int>(r.y),
                 static_cast<int>(r.x - r.len * 0.25f), static_cast<int>(r.y + r.len),
                 Color{143, 169, 196, 175});
    }
}

void MainMenuScene::RenderAnim2_SwingingLampAndMirror(engine::core::Engine& eng, float t) {
    auto& assets = eng.Assets();
    Texture2D tsCafe = assets.GetTexture("sprites/tilesets/ts_cafe_interior.png");
    Texture2D vera   = assets.GetTexture("sprites/characters/spr_vera.png");
    Texture2D guest  = assets.GetTexture("sprites/characters/spr_guest.png");
    Texture2D lera   = assets.GetTexture("sprites/characters/spr_lera.png");

    for (int ty = 0; ty < 12; ++ty) {
        for (int tx = 0; tx < 20; ++tx) {
            int rowSrc = (ty < 7) ? 8 : 12;
            Rectangle src = {static_cast<float>((tx % 8) * 16), static_cast<float>(rowSrc * 16), 16.0f, 16.0f};
            DrawTextureRec(tsCafe, src, Vector2{static_cast<float>(tx * 16), static_cast<float>(ty * 16)}, WHITE);
        }
    }

    DrawRectangle(128, 20, 72, 88, Color{42, 23, 21, 255});
    DrawRectangle(132, 24, 64, 80, Color{27, 40, 69, 255});
    DrawRectangleLines(130, 22, 68, 84, Color{196, 122, 59, 255});

    bool glitchPhase = (std::fmod(t, 4.5f) > 3.6f);
    if (!glitchPhase) {
        DrawTexturePro(lera, Rectangle{0.0f, 0.0f, 32.0f, 32.0f},
                       Rectangle{140.0f, 38.0f, 48.0f, 48.0f}, Vector2{0, 0}, 0.0f, Color{143, 169, 196, 190});
    } else {
        DrawRectangle(132, 24, 64, 80, Color{105, 18, 22, 180});
        DrawTexturePro(vera, Rectangle{0.0f, 32.0f, 32.0f, 32.0f},
                       Rectangle{134.0f, 36.0f, 44.0f, 44.0f}, Vector2{0, 0}, 0.0f, WHITE);
        DrawTexturePro(guest, Rectangle{64.0f, 0.0f, 32.0f, 32.0f},
                       Rectangle{158.0f, 30.0f, 40.0f, 40.0f}, Vector2{0, 0}, 0.0f, WHITE);
    }
    DrawLine(164, 56, 134, 28, Color{143, 169, 196, 210});
    DrawLine(164, 56, 192, 42, Color{143, 169, 196, 210});
    DrawLine(164, 56, 186, 98, Color{143, 169, 196, 210});

    DrawRectangle(0, 118, 320, 8,  Color{196, 122, 59, 255});
    DrawRectangle(0, 126, 320, 54, Color{72, 34, 24, 255});

    DrawRectangle(204, 102, 18, 18, Color{232, 227, 217, 255});
    DrawRectangle(206, 104, 14, 4,  Color{42, 23, 21, 255});
    DrawRectangle(222, 106, 4,  10, Color{232, 227, 217, 255});

    for (const auto& sp : m_steam) {
        float alpha = (1.0f - sp.age / sp.maxAge) * 190.0f;
        Color sc = Color{232, 227, 217, static_cast<unsigned char>(engine::math::Clamp(alpha, 0.0f, 255.0f))};
        DrawRectangle(static_cast<int>(sp.x), static_cast<int>(sp.y), 3, 3, sc);
    }

    DrawRectangle(248, 96, 46, 24, Color{42, 23, 21, 255});
    DrawRectangleLines(248, 96, 46, 24, Color{196, 122, 59, 255});
    for (int b = 0; b < 6; ++b) {
        int bh = 3 + (static_cast<int>(std::abs(std::sin(t * 8.0f + b * 1.3f)) * 12.0f));
        Color bc = (bh > 10) ? Color{214, 69, 69, 255} : Color{244, 184, 96, 255};
        DrawRectangle(254 + b * 6, 114 - bh, 4, bh, bc);
    }

    int swingX = static_cast<int>(std::round(196.0f + std::sin(t * 2.2f) * 34.0f));
    int swingY = static_cast<int>(std::round(48.0f  + std::abs(std::cos(t * 2.2f)) * 6.0f));
    DrawLine(196, 0, swingX, swingY, Color{42, 23, 21, 255});
    DrawTriangle(Vector2{static_cast<float>(swingX - 10), static_cast<float>(swingY)},
                 Vector2{static_cast<float>(swingX + 10), static_cast<float>(swingY)},
                 Vector2{static_cast<float>(swingX), static_cast<float>(swingY - 8)},
                 Color{196, 122, 59, 255});
    DrawCircle(swingX, swingY + 2, 4.0f, Color{255, 230, 165, 255});
}

void MainMenuScene::RenderUI(engine::core::Engine& eng) const {
    Font font = eng.Assets().GetMainFont();

    DrawRectangleGradientH(0, 0, 620, 720, Color{10, 13, 26, 245}, Color{10, 13, 26, 0});

    DrawTextEx(font, "КАФЕ «ТЁПЛЫЙ УГОЛ» — 1998", Vector2{58, 62}, 22.0f, 1.0f, Color{196, 122, 59, 255});
    DrawTextEx(font, "LAST ORDER", Vector2{60, 96}, 54.0f, 2.0f, Color{42, 23, 21, 255});
    DrawTextEx(font, "LAST ORDER", Vector2{58, 92}, 54.0f, 2.0f, Color{244, 184, 96, 255});
    DrawTextEx(font, "ПОСЛЕДНИЙ ЗАКАЗ", Vector2{58, 150}, 24.0f, 2.0f, Color{214, 69, 69, 255});

    DrawRectangle(58, 184, 460, 3, Color{168, 36, 36, 255});

    if (!m_inPlaySubMenu) {
        const char* mainButtons[4] = {
            "Играть",
            "Свободный режим",
            "Настройки",
            "Выход из игры"
        };

        for (int i = 0; i < 4; ++i) {
            bool sel = (m_selectedBtn == i);
            float bx = 58.0f;
            float by = 240.0f + i * 68.0f;
            float bw = 460.0f;
            float bh = 54.0f;

            Color bgCol = sel ? Color{107, 51, 34, 245} : Color{42, 23, 21, 215};
            Color borderCol = sel ? Color{244, 184, 96, 255} : Color{196, 122, 59, 160};

            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh), bgCol);
            DrawRectangleLinesEx(Rectangle{bx, by, bw, bh}, sel ? 2.5f : 1.0f, borderCol);

            if (sel) {
                DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 8, static_cast<int>(bh), Color{244, 184, 96, 255});
            }

            DrawTextEx(font, mainButtons[i], Vector2{bx + 24.0f, by + 15.0f}, 23.0f, 1.0f,
                       sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 230});
        }
    } else {
        const char* subButtons[7] = {
            "С обучением (Ночь 1)",
            "Без обучения (Ночь 1)",
            "Ночь 2: Дождь и Двойники",
            "Ночь 3: Голоса в трубах",
            "Ночь 4: Полночный звонок",
            "Ночь 5: Последний заказ (Финал)",
            "Назад"
        };

        for (int i = 0; i < 7; ++i) {
            bool sel = (m_selectedBtn == i);
            float bx = 58.0f;
            float by = 210.0f + i * 56.0f;
            float bw = 460.0f;
            float bh = 46.0f;

            Color bgCol = sel ? Color{107, 51, 34, 245} : Color{42, 23, 21, 215};
            Color borderCol = sel ? Color{244, 184, 96, 255} : Color{196, 122, 59, 160};

            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh), bgCol);
            DrawRectangleLinesEx(Rectangle{bx, by, bw, bh}, sel ? 2.0f : 1.0f, borderCol);

            if (sel) {
                DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 8, static_cast<int>(bh), Color{244, 184, 96, 255});
            }

            DrawTextEx(font, subButtons[i], Vector2{bx + 20.0f, by + 12.0f}, 20.0f, 1.0f,
                       sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 225});
        }
    }

    DrawRectangle(0, 686, 1280, 34, Color{10, 13, 26, 238});
    DrawTextEx(font, "[W/S, ENTER / ЛКМ] Выбор | [TAB] Сменить сцену | [F11] Полный экран",
               Vector2{24, 694}, 17.5f, 1.0f, Color{188, 178, 162, 230});
}

}
