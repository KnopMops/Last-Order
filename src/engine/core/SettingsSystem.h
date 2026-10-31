#pragma once
#include <string>

namespace engine::core {

struct GameSettings {

    int resolutionIndex = 4;
    bool fullscreen = false;
    bool stretchToUltrawide = true;
    bool vsync = true;
    int targetFpsIndex = 0;

    bool crtEnabled = true;
    float barrelDistortion = 0.45f;
    float scanlineIntensity = 0.60f;
    float chromaAberration = 0.50f;
    float brightness = 1.08f;
    bool screenShakeEnabled = true;

    float masterVolume = 0.85f;
    float ambientVolume = 0.70f;
    float sfxVolume = 0.80f;
    float stingerVolume = 0.85f;
    bool safeScreamerMode = false;

    bool showWorldLabels = true;
    bool showObjectiveLine = true;
};

class SettingsSystem {
public:
    static constexpr int NUM_RESOLUTIONS = 6;

    void Init(const std::string& configFilePath);
    bool Load();
    bool Save() const;
    void ResetToDefaults();

    void ApplyWindowAndFps(bool forceWindowUpdate = false);
    void ToggleFullscreen();

    GameSettings& Get() { return m_settings; }
    const GameSettings& Get() const { return m_settings; }

    static void GetResolutionDims(int index, int& outW, int& outH);
    static const char* GetResolutionLabel(int index);
    static int GetTargetFpsValue(int index);
    static const char* GetTargetFpsLabel(int index);

private:
    std::string m_configPath;
    GameSettings m_settings{};
    int m_appliedResIndex = -1;
    bool m_appliedFullscreen = false;
};

}
