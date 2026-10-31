#pragma once
#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioBus.h"
#include "engine/core/SettingsSystem.h"
#include "engine/core/StateStore.h"
#include "engine/core/TimeSystem.h"
#include "engine/dialogue/DialogueSystem.h"
#include "engine/dialogue/DialogueUI.h"
#include "engine/graphics/LightingSystem.h"
#include "engine/graphics/Renderer.h"
#include "engine/physics/PhysicsWorld.h"
#include <string>

namespace engine::core {

class Engine;

class IGame {
public:
    virtual ~IGame() = default;
    virtual void OnInit(Engine& eng) = 0;
    virtual void OnFixedUpdate(Engine& eng, float fixedDt) = 0;
    virtual void OnFrameUpdate(Engine& eng, float dt) = 0;
    virtual void OnRenderWorld(Engine& eng) = 0;
    virtual void OnRenderUI(Engine& eng) = 0;
    virtual void OnAction(Engine& eng, const EngineAction& action) = 0;
    virtual void OnShutdown(Engine& eng) = 0;
};

class Engine {
public:
    bool Init(int windowW, int windowH, const char* title, const std::string& assetDir);
    void Run(IGame& game, bool singleFrameSmokeTest = false);
    void Shutdown();

    void RequestQuit() { m_quitRequested = true; }

    assets::AssetManager& Assets() { return m_assets; }
    const assets::AssetManager& Assets() const { return m_assets; }
    audio::AudioBus& Audio() { return m_audio; }
    const audio::AudioBus& Audio() const { return m_audio; }
    physics::PhysicsWorld& Physics() { return m_physics; }
    const physics::PhysicsWorld& Physics() const { return m_physics; }
    graphics::LightingSystem& Lighting() { return m_lighting; }
    const graphics::LightingSystem& Lighting() const { return m_lighting; }
    graphics::Renderer& Renderer() { return m_renderer; }
    const graphics::Renderer& Renderer() const { return m_renderer; }
    TimeSystem& Time() { return m_time; }
    const TimeSystem& Time() const { return m_time; }
    StateStore& State() { return m_state; }
    const StateStore& State() const { return m_state; }
    SettingsSystem& Settings() { return m_settings; }
    const SettingsSystem& Settings() const { return m_settings; }
    dialogue::DialogueSystem& Dialogue() { return m_dialogue; }
    const dialogue::DialogueSystem& Dialogue() const { return m_dialogue; }

private:
    assets::AssetManager m_assets;
    audio::AudioBus m_audio;
    physics::PhysicsWorld m_physics;
    graphics::LightingSystem m_lighting;
    graphics::Renderer m_renderer;
    TimeSystem m_time;
    StateStore m_state;
    SettingsSystem m_settings;
    dialogue::DialogueSystem m_dialogue;
    dialogue::DialogueUI m_dialogueUI;
    bool m_quitRequested = false;
};

}
