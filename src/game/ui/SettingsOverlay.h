#pragma once
#include "engine/core/Engine.h"

namespace game::ui {

class SettingsOverlay {
public:
    void Open() { m_open = true; m_selectedRow = 0; }
    void Close(engine::core::Engine& eng) {
        m_open = false;
        eng.Settings().Save();
    }
    bool IsOpen() const { return m_open; }

    void Update(engine::core::Engine& eng);
    void Render(engine::core::Engine& eng) const;

private:
    void AdjustRow(engine::core::Engine& eng, int row, int dir, bool isClickToggle);
    void SetRowNormalizedValue(engine::core::Engine& eng, int row, float norm01);

    bool m_open = false;
    int m_selectedRow = 0;
    static constexpr int TOTAL_ROWS = 7;
};

}
