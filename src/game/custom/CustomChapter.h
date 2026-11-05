#pragma once
#include "engine/core/Engine.h"
#include "game/custom/HdPlayView.h"
#include "runtime/ChapterProject.h"
#include "runtime/PlaySession.h"
#include <map>
#include <memory>
#include <string>

namespace game::custom {

class CustomChapter {
public:
    bool Init(engine::core::Engine& eng, const std::string& chapterId, const std::map<std::string, knopmops::runtime::Var>& initialVars);
    void Update(engine::core::Engine& eng, float dt);
    void RenderWorld(engine::core::Engine& eng);
    void RenderUI(engine::core::Engine& eng);
    void Shutdown(engine::core::Engine& eng);

    void ApplyGraphics(engine::core::Engine& eng);
    bool CanOpenSettings() const;
    bool WantsReturnToMenu() const { return m_returnToMenu; }
    bool Completed() const { return m_completed; }
    const std::string& ChapterId() const { return m_id; }
    const std::string& ChapterTitle() const { return m_title; }
    const std::map<std::string, knopmops::runtime::Var>& FinalVars() const { return m_finalVars; }

private:
    knopmops::runtime::ChapterProject m_project;
    std::unique_ptr<knopmops::runtime::PlaySession> m_session;
    std::map<std::string, knopmops::runtime::Var> m_finalVars;
    std::string m_id;
    std::string m_title;
    bool m_returnToMenu = false;
    bool m_completed = false;
    bool m_texturesReady = false;
    HdPlayView m_hd;
    int m_appliedRenderer = -1, m_appliedRt = -1, m_appliedShadow = -1;
    void InitRenderer(engine::core::Engine& eng);
    int m_savedVsync = -1;
    int m_savedFpsIndex = 0;
};

}
