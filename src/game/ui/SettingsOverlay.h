#pragma once
#include "engine/core/Engine.h"
#include "game/chapter3/Ch3Graphics.h"

namespace game::ui {

class SettingsOverlay {
public:
    void Open(engine::core::Engine& eng);
    void Open() {
        m_open = true;
        m_wantsQuitToMenu = false;
        m_selectedRow = 0;
    }
    void Close(engine::core::Engine& eng);
    void Toggle(engine::core::Engine& eng) {
        if (m_open) Close(eng);
        else Open(eng);
    }
    void Save(engine::core::Engine& eng) {
        eng.Settings().Save();
    }
    bool IsOpen() const { return m_open; }
    bool IsGraphicsPageOpen() const { return m_ch3.IsOpen(); }
    bool WantsQuitToMenu() const { return m_wantsQuitToMenu; }
    void ClearQuitToMenu() { m_wantsQuitToMenu = false; }

    void Update(engine::core::Engine& eng);
    void Render(engine::core::Engine& eng) const;

private:
    void AdjustRow(engine::core::Engine& eng, int row, int dir, bool isClick);

    bool m_open = false;
    bool m_wantsQuitToMenu = false;
    int m_selectedRow = 0;
    static constexpr int TOTAL_ROWS = 10;
    chapter3::Ch3GraphicsPanel m_ch3;
};

}
