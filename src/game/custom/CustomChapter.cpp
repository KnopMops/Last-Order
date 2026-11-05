#include "game/custom/CustomChapter.h"
#include "game/chapter3/Ch3Graphics.h"
#include "render/TextureManager.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace game::custom {

namespace km = knopmops;

bool CustomChapter::Init(engine::core::Engine& eng, const std::string& chapterId, const std::map<std::string, km::runtime::Var>& initialVars) {
    m_id = chapterId;
    m_returnToMenu = false;
    m_completed = false;
    m_finalVars.clear();

    std::string dir = eng.Assets().GetBaseDir() + "/data/chapters/" + chapterId;
    if (!m_project.Load(dir)) {
        TraceLog(LOG_ERROR, "CustomChapter: глава не найдена: %s", dir.c_str());
        m_returnToMenu = true;
        return false;
    }
    m_title = m_project.Manifest().title;

    km::render::TextureManager::Instance().Init(m_project.TexturesDir());
    m_texturesReady = true;

    m_session = std::make_unique<km::runtime::PlaySession>();
    km::runtime::PlaySession::Hooks hooks;
    hooks.gameAssetsDir = eng.Assets().GetBaseDir();
    hooks.initialVars = initialVars;
    engine::core::Engine* e = &eng;
    hooks.sfxVolume = [e]() { return e->Settings().Get().sfxVolume; };
    m_session->SetHooks(hooks);
    m_session->SetFont(eng.Assets().GetMainFont());
    m_session->SetPauseEnabled(false); // Esc открывает общее меню настроек игры
    m_savedVsync = -1;

    if (!m_session->Start(m_project, m_project.Manifest().startLevel)) {
        m_returnToMenu = true;
        return false;
    }
    if (const char* lv = std::getenv("LO_CH35_LEVEL")) m_session->LoadLevel(lv); // только для разработки
    if (const char* tp = std::getenv("LO_CH35_TP")) { // только для разработки: телепорт на старте
        float x = 0, y = 0, z = 0, yaw = 0;
        if (std::sscanf(tp, "%f,%f,%f,%f", &x, &y, &z, &yaw) == 4) m_session->TeleportPlayer({x, y, z}, yaw);
    }
    InitRenderer(eng);
    ApplyGraphics(eng);
    DisableCursor();
    return true;
}

void CustomChapter::InitRenderer(engine::core::Engine& eng) {
    const auto& g = game::chapter3::Ch3Graphics::Get();
    m_appliedRenderer = g.renderer;
    m_appliedRt = g.rayTracing;
    m_appliedShadow = g.shadowQuality;
    m_hd.Shutdown(eng);
    if (!m_session || g.renderer == 0) return; // 0 = классический растровый рендер
    engine::hd::HdApi api = g.renderer == 2 ? engine::hd::HdApi::D3D12 : engine::hd::HdApi::D3D11;
    bool ok = m_hd.Init(eng, *m_session, m_project, api);
    if (!ok && api == engine::hd::HdApi::D3D12) ok = m_hd.Init(eng, *m_session, m_project, engine::hd::HdApi::D3D11);
    if (!ok) TraceLog(LOG_ERROR, "CustomChapter: HD-рендер не запустился (%s), классический режим", m_hd.Info().c_str());
}

bool CustomChapter::CanOpenSettings() const {
    return m_session && m_session->State() == km::runtime::EndState::Playing && !m_session->HasOverlay();
}

void CustomChapter::ApplyGraphics(engine::core::Engine& eng) {
    if (!m_session) return;
    const auto& g = game::chapter3::Ch3Graphics::Get();
    if (g.renderer != m_appliedRenderer || g.rayTracing != m_appliedRt || g.shadowQuality != m_appliedShadow) InitRenderer(eng);
    m_session->SetFovOverride(static_cast<float>(g.fov));
    auto& s = eng.Settings().Get();
    if (m_savedVsync < 0) {
        m_savedVsync = s.vsync ? 1 : 0;
        m_savedFpsIndex = s.targetFpsIndex;
    }
    s.vsync = g.vsync;
    eng.Settings().ApplyWindowAndFps(false);
    SetTargetFPS(g.FpsLimitValue());
}

void CustomChapter::Update(engine::core::Engine& eng, float dt) {
    (void)eng;
    if (!m_session) return;
    m_session->Update(dt, true);
    if (m_session->WantsQuit()) {
        m_completed = m_session->State() == km::runtime::EndState::Won;
        m_finalVars = m_session->AllVars();
        m_returnToMenu = true;
    }
}

void CustomChapter::RenderWorld(engine::core::Engine& eng) {
    if (!m_session) return;
    float aspect = 1280.0f / 720.0f;
    if (eng.Settings().Get().stretchToUltrawide) {
        aspect = (float)std::max(1, GetScreenWidth()) / (float)std::max(1, GetScreenHeight());
    }
    if (m_hd.Ready()) {
        if (m_hd.NeedsRebuild(*m_session)) InitRenderer(eng); // загружен другой уровень
    }
    if (m_hd.Ready()) {
        ClearBackground(BLACK);
        m_hd.Render(eng, *m_session);
        return;
    }
    m_session->Render3D(aspect);
}

void CustomChapter::RenderUI(engine::core::Engine& eng) {
    if (!m_session) return;
    m_session->RenderHUD(eng.Renderer().GetUIWidth(), eng.Renderer().GetUIHeight());
}

void CustomChapter::Shutdown(engine::core::Engine& eng) {
    m_hd.Shutdown(eng);
    if (m_savedVsync >= 0) {
        auto& s = eng.Settings().Get();
        s.vsync = m_savedVsync != 0;
        s.targetFpsIndex = m_savedFpsIndex;
        eng.Settings().ApplyWindowAndFps(false);
        m_savedVsync = -1;
    }
    m_session.reset();
    if (m_texturesReady) {
        km::render::TextureManager::Instance().Shutdown();
        m_texturesReady = false;
    }
    EnableCursor();
}

}
