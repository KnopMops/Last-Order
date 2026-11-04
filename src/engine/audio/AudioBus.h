#pragma once
#include "engine/assets/AssetManager.h"
#include "engine/core/SettingsSystem.h"
#include "raylib.h"
#include <string>

namespace engine::audio {

class AudioBus {
public:
    void Init(assets::AssetManager* assets);
    void Shutdown();
    void Update(float dt, const core::GameSettings& settings);

    void PlayAmbient(const std::string& relativeOggPath, float baseVolume = 0.7f);
    void StopAmbient();
    void SetAmbientBaseVolume(float baseVolume);
    float GetAmbientBaseVolume() const;

    void PlaySfx(const std::string& relativeWavPath, float baseVolume = 0.8f);
    void PlaySfxSpatial(const std::string& relativeWavPath, float baseVolume, float pan);
    void PlayScreamerStinger(const std::string& stingerFilename, float silenceAfterSec = 4.0f);

private:
    assets::AssetManager* m_assets = nullptr;
    Music m_ambientStream{};
    std::string m_currentAmbientPath;
    float m_ambientBaseVolume = 0.7f;
    float m_postScareSilenceTimer = 0.0f;

    float m_masterVol = 0.95f;
    float m_ambientVol = 0.75f;
    float m_sfxVol = 0.90f;
    float m_stingerVol = 1.0f;
    bool m_safeMode = false;
};

}
