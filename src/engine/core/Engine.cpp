#include "engine/core/Engine.h"
#include "engine/core/PathUtils.h"
#include "raylib.h"
#include <algorithm>

namespace engine::core {

bool Engine::Init(int windowW, int windowH, const char* title, const std::string& assetDir) {

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(windowW, windowH, title);
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL);
    InitAudioDevice();

    m_assets.Init(assetDir);
    m_settings.Init(GetUserSettingsPath());
    m_audio.Init(&m_assets);
    m_renderer.Init(windowW, windowH, assetDir + "/shaders/crt.fs");
    m_lighting.Init(graphics::Renderer::NATIVE_W, graphics::Renderer::NATIVE_H);
    m_time.Reset(22, 0, 1.5f);
    m_state.Reset();
    return true;
}

void Engine::Run(IGame& game, bool singleFrameSmokeTest) {
    m_dialogue.Init(&m_state, [&](const EngineAction& act) {
        if (act.type == "play_sfx") {
            m_audio.PlaySfx(act.target, 0.75f);
        }
        game.OnAction(*this, act);
    });

    game.OnInit(*this);

    constexpr float FIXED_DT = 1.0f / 60.0f;
    float accumulator = 0.0f;

    while (!WindowShouldClose() && !m_quitRequested) {
        float dt = std::min(GetFrameTime(), 0.05f);
        accumulator += dt;

        if (IsKeyPressed(KEY_F1)) {
            m_settings.Get().crtEnabled = !m_settings.Get().crtEnabled;
            m_settings.Save();
        }
        if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) {
            m_settings.ToggleFullscreen();
        }

        if (m_dialogue.IsActive()) {
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
                m_dialogue.OnSelectDelta(-1);
                m_audio.PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.5f);
            }
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
                m_dialogue.OnSelectDelta(1);
                m_audio.PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.5f);
            }
            if (IsKeyPressed(KEY_ONE))   m_dialogue.OnSelectIndex(0);
            if (IsKeyPressed(KEY_TWO))   m_dialogue.OnSelectIndex(1);
            if (IsKeyPressed(KEY_THREE)) m_dialogue.OnSelectIndex(2);

            if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                m_dialogue.OnAdvanceOrConfirm();
                m_audio.PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.45f);
            }
        }

        while (accumulator >= FIXED_DT) {
            if (!m_renderer.IsScreamerActive()) {
                game.OnFixedUpdate(*this, FIXED_DT);
            }
            accumulator -= FIXED_DT;
        }

        m_dialogue.Update(dt);
        game.OnFrameUpdate(*this, dt);
        m_renderer.Update(dt, m_time.GetTotalRealTime(), m_settings.Get());
        m_audio.Update(dt, m_settings.Get());

        m_renderer.BeginWorldPass();
        game.OnRenderWorld(*this);
        m_renderer.EndWorldPass(m_lighting, m_time.GetTotalRealTime());

        m_renderer.BeginUIPass();
        game.OnRenderUI(*this);
        m_dialogueUI.Render(m_dialogue, m_assets.GetMainFont(),
                            m_renderer.GetUIWidth(), m_renderer.GetUIHeight());
        m_renderer.EndUIPassAndPresent();

        if (singleFrameSmokeTest) break;
    }

    m_settings.Save();
    game.OnShutdown(*this);
}

void Engine::Shutdown() {
    m_lighting.Shutdown();
    m_renderer.Shutdown();
    m_audio.Shutdown();
    m_assets.Shutdown();
    CloseAudioDevice();
    CloseWindow();
}

}
