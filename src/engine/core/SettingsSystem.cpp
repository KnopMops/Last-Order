#include "engine/core/SettingsSystem.h"
#include "raylib.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace engine::core {

void SettingsSystem::Init(const std::string& configFilePath) {
    m_configPath = configFilePath;
    if (!Load()) {
        ResetToDefaults();
        Save();
    }
    ApplyWindowAndFps(true);
}

void SettingsSystem::ResetToDefaults() {
    m_settings = GameSettings{};

    m_settings.resolutionIndex = 0;
    m_settings.fullscreen = false;
}

bool SettingsSystem::Load() {
    std::ifstream in(m_configPath);
    if (!in.is_open()) return false;

    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        return false;
    }

    m_settings.resolutionIndex    = std::clamp(j.value("resolutionIndex", 0), 0, NUM_RESOLUTIONS - 1);
    m_settings.fullscreen         = j.value("fullscreen", false);
    m_settings.stretchToUltrawide = j.value("stretchToUltrawide", true);
    m_settings.vsync              = j.value("vsync", true);
    m_settings.targetFpsIndex     = std::clamp(j.value("targetFpsIndex", 0), 0, 3);
    m_settings.crtEnabled         = j.value("crtEnabled", true);
    m_settings.barrelDistortion   = j.value("barrelDistortion", 0.45f);
    m_settings.scanlineIntensity  = j.value("scanlineIntensity", 0.60f);
    m_settings.chromaAberration   = j.value("chromaAberration", 0.50f);
    m_settings.brightness         = j.value("brightness", 1.08f);
    m_settings.screenShakeEnabled = j.value("screenShakeEnabled", true);
    m_settings.masterVolume       = j.value("masterVolume", 0.85f);
    m_settings.ambientVolume      = j.value("ambientVolume", 0.70f);
    m_settings.sfxVolume          = j.value("sfxVolume", 0.80f);
    m_settings.stingerVolume      = j.value("stingerVolume", 0.85f);
    m_settings.safeScreamerMode   = j.value("safeScreamerMode", false);
    m_settings.showWorldLabels    = j.value("showWorldLabels", true);
    m_settings.showObjectiveLine  = j.value("showObjectiveLine", true);
    return true;
}

bool SettingsSystem::Save() const {
    if (m_configPath.empty()) return false;
    nlohmann::json j = {
        {"resolutionIndex",    m_settings.resolutionIndex},
        {"fullscreen",         m_settings.fullscreen},
        {"stretchToUltrawide", m_settings.stretchToUltrawide},
        {"vsync",              m_settings.vsync},
        {"targetFpsIndex",     m_settings.targetFpsIndex},
        {"crtEnabled",         m_settings.crtEnabled},
        {"barrelDistortion",   m_settings.barrelDistortion},
        {"scanlineIntensity",  m_settings.scanlineIntensity},
        {"chromaAberration",   m_settings.chromaAberration},
        {"brightness",         m_settings.brightness},
        {"screenShakeEnabled", m_settings.screenShakeEnabled},
        {"masterVolume",       m_settings.masterVolume},
        {"ambientVolume",      m_settings.ambientVolume},
        {"sfxVolume",          m_settings.sfxVolume},
        {"stingerVolume",      m_settings.stingerVolume},
        {"safeScreamerMode",   m_settings.safeScreamerMode},
        {"showWorldLabels",    m_settings.showWorldLabels},
        {"showObjectiveLine",  m_settings.showObjectiveLine}
    };

    std::ofstream out(m_configPath);
    if (!out.is_open()) return false;
    out << j.dump(2);
    return true;
}

void SettingsSystem::GetResolutionDims(int index, int& outW, int& outH) {
    int mon = GetCurrentMonitor();
    int mw = std::max(1280, GetMonitorWidth(mon));
    int mh = std::max(720,  GetMonitorHeight(mon));

    switch (index) {
        case 0: outW = 1280; outH = 720;  break;
        case 1: outW = 1600; outH = 900;  break;
        case 2: outW = 1920; outH = 1080; break;
        case 3: outW = 2560; outH = 1440; break;
        case 4: outW = 3440; outH = 1440; break;
        case 5: outW = mw;   outH = mh;   break;
        default: outW = 1280; outH = 720; break;
    }
}

const char* SettingsSystem::GetResolutionLabel(int index) {
    switch (index) {
        case 0: return "1280x720 (16:9 HD)";
        case 1: return "1600x900 (16:9 HD+)";
        case 2: return "1920x1080 (Full HD)";
        case 3: return "2560x1440 (2K QHD)";
        case 4: return "3440x1440 (21:9 Ultrawide)";
        case 5: return "Родное монитора (Авто)";
        default: return "1280x720 (16:9 HD)";
    }
}

int SettingsSystem::GetTargetFpsValue(int index) {
    switch (index) {
        case 0: return 60;
        case 1: return 120;
        case 2: return 144;
        case 3: return 0;
        default: return 60;
    }
}

const char* SettingsSystem::GetTargetFpsLabel(int index) {
    switch (index) {
        case 0: return "60 FPS";
        case 1: return "120 FPS";
        case 2: return "144 FPS";
        case 3: return "Без лимита";
        default: return "60 FPS";
    }
}

void SettingsSystem::ToggleFullscreen() {
    m_settings.fullscreen = !m_settings.fullscreen;
    ApplyWindowAndFps(true);
    Save();
}

void SettingsSystem::ApplyWindowAndFps(bool forceWindowUpdate) {
    if (!IsWindowReady()) return;

    if (m_settings.vsync) {
        SetWindowState(FLAG_VSYNC_HINT);
    } else {
        ClearWindowState(FLAG_VSYNC_HINT);
    }
    SetTargetFPS(GetTargetFpsValue(m_settings.targetFpsIndex));

    if (!forceWindowUpdate &&
        m_appliedFullscreen == m_settings.fullscreen &&
        m_appliedResIndex == m_settings.resolutionIndex) {
        return;
    }

    int mon = GetCurrentMonitor();
    Vector2 monPos = GetMonitorPosition(mon);
    int mw = std::max(1280, GetMonitorWidth(mon));
    int mh = std::max(720,  GetMonitorHeight(mon));

    if (m_settings.fullscreen) {

        if (IsWindowMaximized()) RestoreWindow();
        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowPosition(static_cast<int>(monPos.x), static_cast<int>(monPos.y));
        SetWindowSize(mw, mh);
    } else {
        ClearWindowState(FLAG_WINDOW_UNDECORATED);
        int w = 1280, h = 720;
        GetResolutionDims(m_settings.resolutionIndex, w, h);

        if (w >= mw && h >= mh) {
            SetWindowPosition(static_cast<int>(monPos.x), static_cast<int>(monPos.y));
            SetWindowSize(mw, mh);
        } else {
            w = std::min(w, mw);
            h = std::min(h, mh - 48);
            SetWindowSize(w, h);
            int cx = static_cast<int>(monPos.x) + std::max(0, (mw - w) / 2);
            int cy = static_cast<int>(monPos.y) + std::max(24, (mh - h) / 2);
            SetWindowPosition(cx, cy);
        }
    }

    m_appliedFullscreen = m_settings.fullscreen;
    m_appliedResIndex = m_settings.resolutionIndex;
}

}
