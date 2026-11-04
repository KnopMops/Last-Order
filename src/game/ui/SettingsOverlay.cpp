#include "game/ui/SettingsOverlay.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game::ui {

void SettingsOverlay::Update(engine::core::Engine& eng) {
    if (!m_open) return;

    if (IsKeyPressed(KEY_ESCAPE)) {
        eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
        Close(eng);
        return;
    }

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
        m_selectedRow = (m_selectedRow - 1 + TOTAL_ROWS) % TOTAL_ROWS;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.45f);
    }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
        m_selectedRow = (m_selectedRow + 1) % TOTAL_ROWS;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.45f);
    }

    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
        AdjustRow(eng, m_selectedRow, -1, false);
    }
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
        AdjustRow(eng, m_selectedRow, +1, false);
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) {
        AdjustRow(eng, m_selectedRow, +1, true);
    }

    engine::math::Vec2 mpos = eng.Renderer().GetVirtualMousePos();
    bool leftClick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool leftDown  = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    float rx = 340.0f;
    for (int r = 0; r < 6; ++r) {
        float ry = 155.0f + r * 54.0f;
        engine::math::Rect rowRect = {rx, ry, 600.0f, 46.0f};
        if (rowRect.Contains(mpos)) {
            m_selectedRow = r;
            if ((r == 1 || r == 2) && leftDown) {
                float sliderX = rx + 320.0f;
                float sliderW = 200.0f;
                float norm = engine::math::Clamp((mpos.x - sliderX) / sliderW, 0.0f, 1.0f);
                SetRowNormalizedValue(eng, r, norm);
            } else if (leftClick && r != 1 && r != 2) {
                AdjustRow(eng, r, +1, true);
            }
        }
    }

    engine::math::Rect btnClose = {340.0f, 492.0f, 600.0f, 44.0f};
    if (btnClose.Contains(mpos)) {
        m_selectedRow = 6;
        if (leftClick) {
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
            Close(eng);
        }
    }

    engine::math::Rect btnMenu = {340.0f, 544.0f, 600.0f, 44.0f};
    if (btnMenu.Contains(mpos)) {
        m_selectedRow = 7;
        if (leftClick) {
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
            m_wantsQuitToMenu = true;
            Close(eng);
        }
    }
}

void SettingsOverlay::SetRowNormalizedValue(engine::core::Engine& eng, int row, float norm01) {
    auto& s = eng.Settings().Get();
    if (row == 1) s.masterVolume = norm01;
    if (row == 2) s.stingerVolume = norm01;
}

void SettingsOverlay::AdjustRow(engine::core::Engine& eng, int row, int dir, bool isClickToggle) {
    auto& s = eng.Settings().Get();
    const float step = 0.05f * static_cast<float>(dir);

    switch (row) {
        case 0:
            eng.Settings().ToggleFullscreen();
            break;
        case 1:
            if (!isClickToggle) s.masterVolume = engine::math::Clamp(s.masterVolume + step, 0.0f, 1.0f);
            break;
        case 2:
            if (!isClickToggle) s.stingerVolume = engine::math::Clamp(s.stingerVolume + step, 0.0f, 1.0f);
            break;
        case 3:
            s.crtEnabled = !s.crtEnabled;
            break;
        case 4:
            s.screenShakeEnabled = !s.screenShakeEnabled;
            break;
        case 5:
            s.safeScreamerMode = !s.safeScreamerMode;
            break;
        case 6:
            Close(eng);
            return;
        case 7:
            m_wantsQuitToMenu = true;
            Close(eng);
            return;
        default:
            break;
    }
    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.5f);
    eng.Settings().Save();
}

void SettingsOverlay::Render(engine::core::Engine& eng) const {
    if (!m_open) return;

    Font font = eng.Assets().GetMainFont();
    const auto& s = eng.Settings().Get();

    DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 232});
    DrawRectangle(300, 80, 680, 540, Color{42, 23, 21, 250});
    DrawRectangleLinesEx(Rectangle{300, 80, 680, 540}, 3.0f, Color{196, 122, 59, 255});

    engine::graphics::TextLayout::DrawCentered(font, "НАСТРОЙКИ", 640.0f, 102.0f, 28.0f, Color{244, 184, 96, 255});

    float rx = 340.0f;

    auto drawToggleRow = [&](int rowIdx, float y, const char* label, const char* valText, bool activeAccent) {
        bool sel = (m_selectedRow == rowIdx);
        if (sel) {
            DrawRectangle(static_cast<int>(rx), static_cast<int>(y), 600, 46, Color{107, 51, 34, 195});
            DrawRectangleLinesEx(Rectangle{rx, y, 600, 46}, 1.5f, Color{244, 184, 96, 255});
        }
        DrawTextEx(font, label, Vector2{rx + 14, y + 13}, 20.0f, 1.0f, sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 255});

        DrawRectangle(static_cast<int>(rx + 320), static_cast<int>(y + 7), 264, 32, Color{10, 13, 26, 235});
        DrawRectangleLinesEx(Rectangle{rx + 320, y + 7, 264, 32}, 1.0f, activeAccent ? Color{244, 184, 96, 255} : Color{143, 169, 196, 180});
        DrawTextEx(font, valText, Vector2{rx + 334, y + 13}, 19.0f, 1.0f, activeAccent ? Color{244, 184, 96, 255} : Color{188, 178, 162, 255});
    };

    auto drawSliderRow = [&](int rowIdx, float y, const char* label, float norm01, int pctDisplay) {
        bool sel = (m_selectedRow == rowIdx);
        if (sel) {
            DrawRectangle(static_cast<int>(rx), static_cast<int>(y), 600, 46, Color{107, 51, 34, 195});
            DrawRectangleLinesEx(Rectangle{rx, y, 600, 46}, 1.5f, Color{244, 184, 96, 255});
        }
        DrawTextEx(font, label, Vector2{rx + 14, y + 13}, 20.0f, 1.0f, sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 255});

        float sx = rx + 320.0f;
        float sw = 200.0f;
        DrawRectangle(static_cast<int>(sx), static_cast<int>(y + 13), static_cast<int>(sw), 20, Color{10, 13, 26, 255});
        DrawRectangle(static_cast<int>(sx), static_cast<int>(y + 13), static_cast<int>(sw * engine::math::Clamp(norm01, 0.0f, 1.0f)), 20, Color{196, 122, 59, 255});
        DrawRectangleLinesEx(Rectangle{sx, y + 13, sw, 20}, 1.0f, Color{244, 184, 96, 255});

        char pctBuf[32];
        std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", pctDisplay);
        DrawTextEx(font, pctBuf, Vector2{sx + sw + 12.0f, y + 13}, 19.0f, 1.0f, Color{244, 184, 96, 255});
    };

    drawToggleRow(0, 155 + 0 * 54, "Полный экран [F11]", s.fullscreen ? "< ВКЛЮЧЕН >" : "< ОКОННЫЙ >", s.fullscreen);
    drawSliderRow(1, 155 + 1 * 54, "Общая громкость", s.masterVolume, static_cast<int>(std::round(s.masterVolume * 100.0f)));
    drawSliderRow(2, 155 + 2 * 54, "Громкость скримеров", s.stingerVolume, static_cast<int>(std::round(s.stingerVolume * 100.0f)));
    drawToggleRow(3, 155 + 3 * 54, "Эффект ЭЛТ (CRT)", s.crtEnabled ? "< ВКЛЮЧЕН >" : "< ВЫКЛЮЧЕН >", s.crtEnabled);
    drawToggleRow(4, 155 + 4 * 54, "Тряска камеры", s.screenShakeEnabled ? "< ВКЛЮЧЕНА >" : "< ВЫКЛЮЧЕНА >", s.screenShakeEnabled);
    drawToggleRow(5, 155 + 5 * 54, "Мягкие скримеры", s.safeScreamerMode ? "< ВКЛ >" : "< ВЫКЛ (ХОРРОР) >", s.safeScreamerMode);

    bool selClose = (m_selectedRow == 6);
    DrawRectangle(static_cast<int>(rx), 492, 600, 44, selClose ? Color{150, 82, 46, 255} : Color{107, 51, 34, 240});
    DrawRectangleLinesEx(Rectangle{rx, 492, 600, 44}, 2.0f, Color{244, 184, 96, 255});
    engine::graphics::TextLayout::DrawCentered(font, "Сохранить и продолжить (ESC)", 640.0f, 504.0f, 20.0f, Color{244, 184, 96, 255});

    bool selMenu = (m_selectedRow == 7);
    DrawRectangle(static_cast<int>(rx), 544, 600, 44, selMenu ? Color{120, 36, 36, 255} : Color{60, 20, 20, 220});
    DrawRectangleLinesEx(Rectangle{rx, 544, 600, 44}, 1.5f, selMenu ? Color{244, 120, 96, 255} : Color{180, 70, 70, 180});
    engine::graphics::TextLayout::DrawCentered(font, "Выйти в главное меню", 640.0f, 556.0f, 20.0f, selMenu ? Color{255, 230, 220, 255} : Color{210, 180, 180, 220});
}

}
