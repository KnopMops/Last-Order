#include "engine/audio/AudioBus.h"
#include <algorithm>

namespace engine::audio {

void AudioBus::Init(assets::AssetManager* assets) {
    m_assets = assets;
}

void AudioBus::Shutdown() {
    StopAmbient();
}

void AudioBus::Update(float dt, const core::GameSettings& settings) {
    m_masterVol  = settings.masterVolume;
    m_ambientVol = settings.ambientVolume;
    m_sfxVol     = settings.sfxVolume;
    m_stingerVol = settings.stingerVolume;
    m_safeMode   = settings.safeScreamerMode;

    SetMasterVolume(m_masterVol);

    if (m_postScareSilenceTimer > 0.0f) {
        m_postScareSilenceTimer = std::max(0.0f, m_postScareSilenceTimer - dt);
    }

    if (m_ambientStream.stream.buffer != nullptr) {
        m_ambientStream.looping = true;
        UpdateMusicStream(m_ambientStream);
        if (!IsMusicStreamPlaying(m_ambientStream)) {
            PlayMusicStream(m_ambientStream);
        }
        float targetVol = (m_postScareSilenceTimer > 0.0f) ? 0.0f : (m_ambientBaseVolume * m_ambientVol);
        SetMusicVolume(m_ambientStream, targetVol);
    }
}

void AudioBus::PlayAmbient(const std::string& relativeOggPath, float baseVolume) {
    if (!m_assets) return;
    m_ambientBaseVolume = baseVolume;
    if (m_currentAmbientPath == relativeOggPath && m_ambientStream.stream.buffer != nullptr) {
        return;
    }
    StopAmbient();
    std::string fullPath = m_assets->GetBaseDir() + "/" + relativeOggPath;
    m_ambientStream = LoadMusicStream(fullPath.c_str());
    if (m_ambientStream.stream.buffer != nullptr) {
        m_ambientStream.looping = true;
        m_currentAmbientPath = relativeOggPath;
        SetMusicVolume(m_ambientStream, m_ambientBaseVolume * m_ambientVol);
        PlayMusicStream(m_ambientStream);
    }
}

void AudioBus::StopAmbient() {
    if (m_ambientStream.stream.buffer != nullptr) {
        StopMusicStream(m_ambientStream);
        UnloadMusicStream(m_ambientStream);
        m_ambientStream = {};
        m_currentAmbientPath.clear();
    }
}

void AudioBus::PlaySfx(const std::string& relativeWavPath, float baseVolume) {
    if (!m_assets) return;
    Sound s = m_assets->GetSound(relativeWavPath);
    if (s.stream.buffer != nullptr) {
        SetSoundVolume(s, std::clamp(baseVolume * m_sfxVol, 0.0f, 1.0f));
        SetSoundPan(s, 0.5f);
        PlaySound(s);
    }
}

void AudioBus::PlaySfxSpatial(const std::string& relativeWavPath, float baseVolume, float pan) {
    if (!m_assets) return;
    Sound s = m_assets->GetSound(relativeWavPath);
    if (s.stream.buffer != nullptr) {
        SetSoundVolume(s, std::clamp(baseVolume * m_sfxVol, 0.0f, 1.0f));
        SetSoundPan(s, std::clamp(pan, 0.0f, 1.0f));
        PlaySound(s);
    }
}

void AudioBus::PlayScreamerStinger(const std::string& stingerFilename, float silenceAfterSec) {
    if (!m_assets) return;
    std::string folder = m_safeMode ? "audio/safe_mode/" : "audio/screamers/";
    Sound s = m_assets->GetSound(folder + stingerFilename);
    if (s.stream.buffer == nullptr) {
        s = m_assets->GetSound("audio/screamers/" + stingerFilename);
    }
    if (s.stream.buffer != nullptr) {
        float vol = m_safeMode ? 0.50f : 1.0f;
        SetSoundVolume(s, vol);
        PlaySound(s);
    }
    if (!m_safeMode) {
        Sound bang = m_assets->GetSound("audio/sfx/sfx_window_bang.wav");
        if (bang.stream.buffer != nullptr) {
            SetSoundVolume(bang, 1.0f);
            PlaySound(bang);
        }
        Sound dread = m_assets->GetSound("audio/sfx/sfx_sting_dread.wav");
        if (dread.stream.buffer != nullptr) {
            SetSoundVolume(dread, 1.0f);
            PlaySound(dread);
        }
    }
    m_postScareSilenceTimer = silenceAfterSec;
}

}
