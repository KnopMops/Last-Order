#include "game/ui/SettingsOverlay.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game::ui {

void SettingsOverlay::Update(engine::core::Engine& eng) {
    if (!m_open) return;
    if (m_ch3.IsOpen()) {
        m_ch3.Update(eng);
        return;
    }

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
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E)) {
        AdjustRow(eng, m_selectedRow, +1, false);
    }

    engine::math::Vec2 mpos = eng.Renderer().GetVirtualMousePos();
    bool leftClick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool leftDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    for (int r = 0; r < 6; ++r) {
        float y = 140.0f + r * 46.0f;
        engine::math::Rect rowRect = {340.0f, y, 600.0f, 42.0f};
        if (rowRect.Contains(mpos)) {
            m_selectedRow = r;
            if ((r == 1 || r == 2) && leftDown) {
                float sx = 340.0f + 320.0f;
                float sw = 200.0f;
                float norm = engine::math::Clamp((mpos.x - sx) / sw, 0.0f, 1.0f);
                auto& s = eng.Settings().Get();
                if (r == 1) s.masterVolume = norm;
                else s.stingerVolume = norm;
            } else if (leftClick && r != 1 && r != 2) {
                AdjustRow(eng, r, +1, true);
            }
        }
    }

    engine::math::Rect btnClose = {340.0f, 426.0f, 600.0f, 40.0f};
    if (btnClose.Contains(mpos)) {
        m_selectedRow = 6;
        if (leftClick) {
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
            Close(eng);
            return;
        }
    }

    engine::math::Rect btnGfx = {340.0f, 472.0f, 600.0f, 40.0f};
    if (btnGfx.Contains(mpos)) {
        m_selectedRow = 7;
        if (leftClick) {
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
            m_ch3.Open(eng);
            return;
        }
    }

    engine::math::Rect btnMenu = {340.0f, 518.0f, 600.0f, 40.0f};
    if (btnMenu.Contains(mpos)) {
        m_selectedRow = 8;
        if (leftClick) {
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
            m_wantsQuitToMenu = true;
            Close(eng);
            return;
        }
    }

    engine::math::Rect btnQuit = {340.0f, 564.0f, 600.0f, 40.0f};
    if (btnQuit.Contains(mpos)) {
        m_selectedRow = 9;
        if (leftClick) {
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
            eng.RequestQuit();
            return;
        }
    }
}

void SettingsOverlay::AdjustRow(engine::core::Engine& eng, int row, int dir, bool isClick) {
    auto& s = eng.Settings().Get();
    const float step = 0.05f * static_cast<float>(dir);

    switch (row) {
        case 0:
            eng.Settings().ToggleFullscreen();
            break;
        case 1:
            if (!isClick) s.masterVolume = engine::math::Clamp(s.masterVolume + step, 0.0f, 1.0f);
            break;
        case 2:
            if (!isClick) s.stingerVolume = engine::math::Clamp(s.stingerVolume + step, 0.0f, 1.0f);
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
            m_ch3.Open(eng);
            return;
        case 8:
            m_wantsQuitToMenu = true;
            Close(eng);
            return;
        case 9:
            eng.RequestQuit();
            return;
        default:
            break;
    }

    eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.5f);
    eng.Settings().Save();
}

void SettingsOverlay::Open(engine::core::Engine&) {
    m_open = true;
    m_wantsQuitToMenu = false;
    m_selectedRow = 0;
}

void SettingsOverlay::Close(engine::core::Engine& eng) {
    m_open = false;
    eng.Settings().Save();
}

void SettingsOverlay::Render(engine::core::Engine& eng) const {
    if (!m_open) return;
    if (m_ch3.IsOpen()) {
        m_ch3.Render(eng);
        return;
    }

    Font font = eng.Assets().GetMainFont();
    const auto& s = eng.Settings().Get();

    DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 232});
    DrawRectangle(300, 72, 680, 560, Color{42, 23, 21, 250});
    DrawRectangleLinesEx(Rectangle{300, 72, 680, 560}, 3.0f, Color{196, 122, 59, 255});

    engine::graphics::TextLayout::DrawCentered(font, "НАСТРОЙКИ", 640.0f, 96.0f, 28.0f, Color{244, 184, 96, 255});

    float rx = 340.0f;

    auto drawToggleRow = [&](int rowIdx, float y, const char* label, const char* valText, bool activeAccent) {
        bool sel = (m_selectedRow == rowIdx);
        if (sel) {
            DrawRectangle(static_cast<int>(rx), static_cast<int>(y), 600, 42, Color{107, 51, 34, 195});
            DrawRectangleLinesEx(Rectangle{rx, y, 600, 42}, 1.5f, Color{244, 184, 96, 255});
        }
        DrawTextEx(font, label, Vector2{rx + 14, y + 11}, 19.0f, 1.0f, sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 255});

        DrawRectangle(static_cast<int>(rx + 320), static_cast<int>(y + 6), 264, 30, Color{10, 13, 26, 235});
        DrawRectangleLinesEx(Rectangle{rx + 320, y + 6, 264, 30}, 1.0f, activeAccent ? Color{244, 184, 96, 255} : Color{143, 169, 196, 180});
        DrawTextEx(font, valText, Vector2{rx + 334, y + 11}, 18.0f, 1.0f, activeAccent ? Color{244, 184, 96, 255} : Color{188, 178, 162, 255});
    };

    auto drawSliderRow = [&](int rowIdx, float y, const char* label, float norm01, int pctDisplay) {
        bool sel = (m_selectedRow == rowIdx);
        if (sel) {
            DrawRectangle(static_cast<int>(rx), static_cast<int>(y), 600, 42, Color{107, 51, 34, 195});
            DrawRectangleLinesEx(Rectangle{rx, y, 600, 42}, 1.5f, Color{244, 184, 96, 255});
        }
        DrawTextEx(font, label, Vector2{rx + 14, y + 11}, 19.0f, 1.0f, sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 255});

        float sx = rx + 320.0f;
        float sw = 200.0f;
        DrawRectangle(static_cast<int>(sx), static_cast<int>(y + 11), static_cast<int>(sw), 20, Color{10, 13, 26, 255});
        DrawRectangle(static_cast<int>(sx), static_cast<int>(y + 11), static_cast<int>(sw * engine::math::Clamp(norm01, 0.0f, 1.0f)), 20, Color{196, 122, 59, 255});
        DrawRectangleLinesEx(Rectangle{sx, y + 11, sw, 20}, 1.0f, Color{244, 184, 96, 255});

        char pctBuf[32];
        std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", pctDisplay);
        DrawTextEx(font, pctBuf, Vector2{sx + sw + 12.0f, y + 11}, 18.0f, 1.0f, Color{244, 184, 96, 255});
    };

    drawToggleRow(0, 140 + 0 * 46, "Полный экран [F11]", s.fullscreen ? "< ВКЛЮЧЕН >" : "< ОКОННЫЙ >", s.fullscreen);
    drawSliderRow(1, 140 + 1 * 46, "Общая громкость", s.masterVolume, static_cast<int>(std::round(s.masterVolume * 100.0f)));
    drawSliderRow(2, 140 + 2 * 46, "Громкость скримеров", s.stingerVolume, static_cast<int>(std::round(s.stingerVolume * 100.0f)));
    drawToggleRow(3, 140 + 3 * 46, "Эффект ЭЛТ (CRT)", s.crtEnabled ? "< ВКЛЮЧЕН >" : "< ВЫКЛЮЧЕН >", s.crtEnabled);
    drawToggleRow(4, 140 + 4 * 46, "Тряска камеры", s.screenShakeEnabled ? "< ВКЛЮЧЕНА >" : "< ВЫКЛЮЧЕНА >", s.screenShakeEnabled);
    drawToggleRow(5, 140 + 5 * 46, "Мягкие скримеры", s.safeScreamerMode ? "< ВКЛ >" : "< ВЫКЛ (ХОРРОР) >", s.safeScreamerMode);

    bool selClose = (m_selectedRow == 6);
    DrawRectangle(static_cast<int>(rx), 426, 600, 40, selClose ? Color{150, 82, 46, 255} : Color{107, 51, 34, 240});
    DrawRectangleLinesEx(Rectangle{rx, 426, 600, 40}, 2.0f, Color{244, 184, 96, 255});
    engine::graphics::TextLayout::DrawCentered(font, "Сохранить и продолжить (ESC)", 640.0f, 436.0f, 19.0f, Color{244, 184, 96, 255});

    bool selGfx = (m_selectedRow == 7);
    DrawRectangle(static_cast<int>(rx), 472, 600, 40, selGfx ? Color{70, 60, 110, 255} : Color{40, 34, 66, 230});
    DrawRectangleLinesEx(Rectangle{rx, 472, 600, 40}, 1.5f, selGfx ? Color{160, 150, 240, 255} : Color{110, 100, 180, 200});
    engine::graphics::TextLayout::DrawCentered(font, "Графика: DirectX 11/12, трассировка, тени, FOV (Главы 3 и 3.5)  >", 640.0f, 482.0f, 19.0f, selGfx ? Color{230, 225, 255, 255} : Color{190, 185, 230, 230});

    bool selMenu = (m_selectedRow == 8);
    DrawRectangle(static_cast<int>(rx), 518, 600, 40, selMenu ? Color{120, 46, 46, 255} : Color{60, 24, 24, 220});
    DrawRectangleLinesEx(Rectangle{rx, 518, 600, 40}, 1.5f, selMenu ? Color{244, 140, 110, 255} : Color{180, 80, 80, 180});
    engine::graphics::TextLayout::DrawCentered(font, "Выйти в главное меню", 640.0f, 528.0f, 19.0f, selMenu ? Color{255, 230, 220, 255} : Color{210, 180, 180, 220});

    bool selQuit = (m_selectedRow == 9);
    DrawRectangle(static_cast<int>(rx), 564, 600, 40, selQuit ? Color{150, 30, 30, 255} : Color{80, 16, 16, 230});
    DrawRectangleLinesEx(Rectangle{rx, 564, 600, 40}, 1.5f, selQuit ? Color{255, 90, 90, 255} : Color{210, 50, 50, 190});
    engine::graphics::TextLayout::DrawCentered(font, "Выйти на рабочий стол (Выход из игры)", 640.0f, 574.0f, 19.0f, selQuit ? Color{255, 240, 240, 255} : Color{240, 190, 190, 220});
}

}
