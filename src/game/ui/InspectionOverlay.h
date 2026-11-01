#pragma once
#include "engine/core/Engine.h"
#include <string>

namespace game::ui {

struct MirrorWorldState {
    bool guestAtTable2 = false;
    bool table2HasReflection = true;
    bool guestAtTable1 = false;
    bool table1HasReflection = true;
    bool guestAtTable6 = false;
    bool eyelessLera = false;
    bool showVeraSilhouette = false;
};

class InspectionOverlay {
public:
    void Reset();

    void ToggleNotebook(engine::core::Engine& eng);
    void ToggleRulesSheet(engine::core::Engine& eng);
    void ToggleNotesGuide(engine::core::Engine& eng);
    void CloseAll();

    bool IsNotebookOpen() const { return m_notebookOpen; }
    bool IsRulesSheetOpen() const { return m_rulesSheetOpen; }
    bool IsNotesGuideOpen() const { return m_notesGuideOpen; }
    bool IsAnyModalOpen() const { return m_notebookOpen || m_rulesSheetOpen || m_notesGuideOpen; }

    void Render(engine::core::Engine& eng, int currentNight, const MirrorWorldState& mirrorState) const;

private:
    void RenderNotebook(engine::core::Engine& eng, int currentNight) const;
    void RenderRulesSheet(engine::core::Engine& eng, int currentNight) const;
    void RenderLiveMirrorInspector(engine::core::Engine& eng, const MirrorWorldState& ms) const;
    void RenderNotesGuide(engine::core::Engine& eng, int currentNight) const;

    bool m_notebookOpen = false;
    bool m_rulesSheetOpen = false;
    bool m_notesGuideOpen = false;
};

}
