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
        float ry = 175.0f + r * 58.0f;
        engine::math::Rect rowRect = {rx, ry, 600.0f, 48.0f};
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

    engine::math::Rect btnClose = {460.0f, 545.0f, 360.0f, 50.0f};
    if (btnClose.Contains(mpos)) {
        m_selectedRow = 6;
        if (leftClick) {
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
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
    DrawRectangle(300, 100, 680, 520, Color{42, 23, 21, 250});
    DrawRectangleLinesEx(Rectangle{300, 100, 680, 520}, 3.0f, Color{196, 122, 59, 255});

    engine::graphics::TextLayout::DrawCentered(font, "НАСТРОЙКИ", 640.0f, 124.0f, 28.0f, Color{244, 184, 96, 255});

    float rx = 340.0f;

    auto drawToggleRow = [&](int rowIdx, float y, const char* label, const char* valText, bool activeAccent) {
        bool sel = (m_selectedRow == rowIdx);
        if (sel) {
            DrawRectangle(static_cast<int>(rx), static_cast<int>(y), 600, 48, Color{107, 51, 34, 195});
            DrawRectangleLinesEx(Rectangle{rx, y, 600, 48}, 1.5f, Color{244, 184, 96, 255});
        }
        DrawTextEx(font, label, Vector2{rx + 14, y + 14}, 20.0f, 1.0f, sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 255});

        DrawRectangle(static_cast<int>(rx + 320), static_cast<int>(y + 8), 264, 32, Color{10, 13, 26, 235});
        DrawRectangleLinesEx(Rectangle{rx + 320, y + 8, 264, 32}, 1.0f, activeAccent ? Color{244, 184, 96, 255} : Color{143, 169, 196, 180});
        DrawTextEx(font, valText, Vector2{rx + 334, y + 14}, 19.0f, 1.0f, activeAccent ? Color{244, 184, 96, 255} : Color{188, 178, 162, 255});
    };

    auto drawSliderRow = [&](int rowIdx, float y, const char* label, float norm01, int pctDisplay) {
        bool sel = (m_selectedRow == rowIdx);
        if (sel) {
            DrawRectangle(static_cast<int>(rx), static_cast<int>(y), 600, 48, Color{107, 51, 34, 195});
            DrawRectangleLinesEx(Rectangle{rx, y, 600, 48}, 1.5f, Color{244, 184, 96, 255});
        }
        DrawTextEx(font, label, Vector2{rx + 14, y + 14}, 20.0f, 1.0f, sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 255});

        float sx = rx + 320.0f;
        float sw = 200.0f;
        DrawRectangle(static_cast<int>(sx), static_cast<int>(y + 14), static_cast<int>(sw), 20, Color{10, 13, 26, 255});
        DrawRectangle(static_cast<int>(sx), static_cast<int>(y + 14), static_cast<int>(sw * engine::math::Clamp(norm01, 0.0f, 1.0f)), 20, Color{196, 122, 59, 255});
        DrawRectangleLinesEx(Rectangle{sx, y + 14, sw, 20}, 1.0f, Color{244, 184, 96, 255});

        char pctBuf[32];
        std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", pctDisplay);
        DrawTextEx(font, pctBuf, Vector2{sx + sw + 12.0f, y + 14}, 19.0f, 1.0f, Color{244, 184, 96, 255});
    };

    drawToggleRow(0, 175 + 0 * 58, "Полный экран [F11]", s.fullscreen ? "< ВКЛЮЧЕН >" : "< ОКОННЫЙ >", s.fullscreen);
    drawSliderRow(1, 175 + 1 * 58, "Общая громкость", s.masterVolume, static_cast<int>(std::round(s.masterVolume * 100.0f)));
    drawSliderRow(2, 175 + 2 * 58, "Громкость скримеров", s.stingerVolume, static_cast<int>(std::round(s.stingerVolume * 100.0f)));
    drawToggleRow(3, 175 + 3 * 58, "Эффект ЭЛТ (CRT)", s.crtEnabled ? "< ВКЛЮЧЕН >" : "< ВЫКЛЮЧЕН >", s.crtEnabled);
    drawToggleRow(4, 175 + 4 * 58, "Тряска камеры", s.screenShakeEnabled ? "< ВКЛЮЧЕНА >" : "< ВЫКЛЮЧЕНА >", s.screenShakeEnabled);
    drawToggleRow(5, 175 + 5 * 58, "Мягкие скримеры", s.safeScreamerMode ? "< ВКЛ >" : "< ВЫКЛ (ХОРРОР) >", s.safeScreamerMode);

    bool selClose = (m_selectedRow == 6);
    DrawRectangle(460, 545, 360, 50, selClose ? Color{150, 82, 46, 255} : Color{107, 51, 34, 240});
    DrawRectangleLinesEx(Rectangle{460, 545, 360, 50}, 2.0f, Color{244, 184, 96, 255});
    engine::graphics::TextLayout::DrawCentered(font, "Сохранить и закрыть (ESC)", 640.0f, 560.0f, 20.0f, Color{244, 184, 96, 255});
}

}
