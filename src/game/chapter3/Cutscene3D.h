#pragma once
#include "engine/core/Engine.h"
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

namespace game::chapter3 {

struct CameraKeyframe {
    float time = 0.0f;
    Vector3 position{};
    Vector3 target{};
    float fov = 65.0f;
    float roll = 0.0f;
};

struct CutsceneEvent {
    float time = 0.0f;
    std::string type;
    std::string param;
    std::string text;
    float duration = 0.0f;
    float volume = 1.0f;
    Vector3 coords{};
    bool executed = false;
};

class CutsceneEngine {
public:
    using EventCallback = std::function<void(const std::string& type, const std::string& param, Vector3 coords)>;

    bool Load(const std::string& jsonPath);
    void Play();
    void Stop();
    void Update(engine::core::Engine& eng, float dt, Camera3D& outCam);
    void RenderUI(engine::core::Engine& eng);
    bool IsPlaying() const { return m_playing; }
    float GetCurrentTime() const { return m_currentTime; }
    float GetDuration() const { return m_duration; }
    const std::string& GetName() const { return m_name; }
    void SetCustomCallback(EventCallback cb) { m_customCallback = std::move(cb); }

private:
    std::string m_name;
    float m_duration = 0.0f;
    float m_currentTime = 0.0f;
    bool m_playing = false;
    std::vector<CameraKeyframe> m_keyframes;
    std::vector<CutsceneEvent> m_events;
    std::string m_activeSubtitle;
    float m_subtitleRemaining = 0.0f;
    float m_fadeAlpha = 0.0f;
    float m_fadeTarget = 0.0f;
    float m_fadeSpeed = 2.0f;
    Color m_fadeColor{0, 0, 0, 255};
    EventCallback m_customCallback;

    CameraKeyframe InterpolateCamera(float t) const;
};

}

