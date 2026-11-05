#include "game/chapter3/Cutscene3D.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace game::chapter3 {

using nlohmann::json;

bool CutsceneEngine::Load(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) return false;

    json root;
    try {
        file >> root;
    } catch (...) {
        return false;
    }

    m_keyframes.clear();
    m_events.clear();
    m_currentTime = 0.0f;
    m_playing = false;
    m_activeSubtitle.clear();
    m_subtitleRemaining = 0.0f;
    m_fadeAlpha = 0.0f;

    m_name = root.value("name", "cutscene");
    m_duration = root.value("duration", 5.0f);
    m_relative = root.value("relative", false);

    if (root.contains("camera_tracks") && root["camera_tracks"].is_array()) {
        for (const auto& item : root["camera_tracks"]) {
            CameraKeyframe kf;
            kf.time = item.value("time", 0.0f);
            if (item.contains("pos") && item["pos"].is_array() && item["pos"].size() >= 3) {
                kf.position = {item["pos"][0].get<float>(), item["pos"][1].get<float>(), item["pos"][2].get<float>()};
            }
            if (item.contains("target") && item["target"].is_array() && item["target"].size() >= 3) {
                kf.target = {item["target"][0].get<float>(), item["target"][1].get<float>(), item["target"][2].get<float>()};
            }
            kf.fov = item.value("fov", 65.0f);
            kf.roll = item.value("roll", 0.0f);
            m_keyframes.push_back(kf);
        }
    }

    std::sort(m_keyframes.begin(), m_keyframes.end(), [](const CameraKeyframe& a, const CameraKeyframe& b) {
        return a.time < b.time;
    });

    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& item : root["events"]) {
            CutsceneEvent ev;
            ev.time = item.value("time", 0.0f);
            ev.type = item.value("type", "");
            ev.param = item.value("param", "");
            ev.text = item.value("text", "");
            ev.duration = item.value("duration", 2.0f);
            ev.volume = item.value("volume", 1.0f);
            if (item.contains("coords") && item["coords"].is_array() && item["coords"].size() >= 3) {
                ev.coords = {item["coords"][0].get<float>(), item["coords"][1].get<float>(), item["coords"][2].get<float>()};
            }
            ev.executed = false;
            m_events.push_back(ev);
        }
    }

    std::sort(m_events.begin(), m_events.end(), [](const CutsceneEvent& a, const CutsceneEvent& b) {
        return a.time < b.time;
    });

    return true;
}

void CutsceneEngine::Play() {
    m_currentTime = 0.0f;
    m_playing = true;
    m_activeSubtitle.clear();
    m_subtitleRemaining = 0.0f;
    for (auto& ev : m_events) {
        ev.executed = false;
    }
}

void CutsceneEngine::Stop() {
    m_playing = false;
    m_activeSubtitle.clear();
    m_subtitleRemaining = 0.0f;
}

CameraKeyframe CutsceneEngine::InterpolateCamera(float t) const {
    if (m_keyframes.empty()) {
        CameraKeyframe def;
        def.position = {0.0f, 1.7f, 0.0f};
        def.target = {0.0f, 1.7f, 1.0f};
        def.fov = 65.0f;
        return def;
    }
    if (t <= m_keyframes.front().time || m_keyframes.size() == 1) {
        return m_keyframes.front();
    }
    if (t >= m_keyframes.back().time) {
        return m_keyframes.back();
    }

    for (size_t i = 0; i + 1 < m_keyframes.size(); ++i) {
        const auto& k0 = m_keyframes[i];
        const auto& k1 = m_keyframes[i + 1];
        if (t >= k0.time && t <= k1.time) {
            float dt = k1.time - k0.time;
            float alpha = (dt > 0.0001f) ? (t - k0.time) / dt : 0.0f;
            float s = alpha * alpha * (3.0f - 2.0f * alpha);

            CameraKeyframe out;
            out.time = t;
            out.position = Vector3Lerp(k0.position, k1.position, s);
            out.target = Vector3Lerp(k0.target, k1.target, s);
            out.fov = k0.fov + (k1.fov - k0.fov) * s;
            out.roll = k0.roll + (k1.roll - k0.roll) * s;
            return out;
        }
    }

    return m_keyframes.back();
}

void CutsceneEngine::Update(engine::core::Engine& eng, float dt, Camera3D& outCam) {
    if (!m_playing) return;

    m_currentTime += dt;

    if (std::abs(m_fadeAlpha - m_fadeTarget) > 0.001f) {
        float dir = (m_fadeTarget > m_fadeAlpha) ? 1.0f : -1.0f;
        m_fadeAlpha += dir * m_fadeSpeed * dt;
        if ((dir > 0.0f && m_fadeAlpha > m_fadeTarget) || (dir < 0.0f && m_fadeAlpha < m_fadeTarget)) {
            m_fadeAlpha = m_fadeTarget;
        }
    }

    if (m_subtitleRemaining > 0.0f) {
        m_subtitleRemaining -= dt;
        if (m_subtitleRemaining <= 0.0f) {
            m_activeSubtitle.clear();
        }
    }

    for (auto& ev : m_events) {
        if (!ev.executed && m_currentTime >= ev.time) {
            ev.executed = true;
            if (ev.type == "sfx") {
                eng.Audio().PlaySfx(ev.param, ev.volume);
            } else if (ev.type == "subtitle") {
                m_activeSubtitle = ev.text;
                m_subtitleRemaining = ev.duration;
            } else if (ev.type == "fade_in") {
                m_fadeAlpha = 1.0f;
                m_fadeTarget = 0.0f;
                m_fadeSpeed = (ev.duration > 0.01f ? 1.0f / ev.duration : 10.0f);
            } else if (ev.type == "fade_out") {
                m_fadeAlpha = 0.0f;
                m_fadeTarget = 1.0f;
                m_fadeSpeed = (ev.duration > 0.01f ? 1.0f / ev.duration : 10.0f);
            } else if (ev.type == "shake") {
                eng.Renderer().TriggerShake(ev.volume, ev.duration);
            }

            if (m_customCallback) {
                m_customCallback(ev.type, ev.param, ev.coords);
            }
        }
    }

    CameraKeyframe kf = InterpolateCamera(m_currentTime);
    Vector3 offset = m_relative ? m_origin : Vector3{0.0f, 0.0f, 0.0f};
    outCam.position = Vector3Add(kf.position, offset);
    outCam.target = Vector3Add(kf.target, offset);
    outCam.fovy = kf.fov;
    outCam.up = {0.0f, 1.0f, 0.0f};

    if (m_currentTime >= m_duration) {
        m_playing = false;
        m_activeSubtitle.clear();
    }
}

void CutsceneEngine::RenderUI(engine::core::Engine& eng) {
    if (!m_playing && m_fadeAlpha <= 0.001f) return;

    int screenW = eng.Renderer().GetUIWidth();
    int screenH = eng.Renderer().GetUIHeight();

    if (m_playing) {
        int barH = screenH / 10;
        DrawRectangle(0, 0, screenW, barH, BLACK);
        DrawRectangle(0, screenH - barH, screenW, barH, BLACK);

        if (!m_activeSubtitle.empty()) {
            Font font = eng.Assets().GetMainFont();
            float fontSize = 24.0f;
            Vector2 textSize = MeasureTextEx(font, m_activeSubtitle.c_str(), fontSize, 1.0f);
            Vector2 pos = {(screenW - textSize.x) * 0.5f, static_cast<float>(screenH - barH - 45)};

            DrawRectangleRounded({pos.x - 16.0f, pos.y - 6.0f, textSize.x + 32.0f, textSize.y + 12.0f}, 0.3f, 4, ColorAlpha(BLACK, 0.75f));
            DrawTextEx(font, m_activeSubtitle.c_str(), {pos.x + 1.0f, pos.y + 1.0f}, fontSize, 1.0f, ColorAlpha(BLACK, 0.9f));
            DrawTextEx(font, m_activeSubtitle.c_str(), pos, fontSize, 1.0f, RAYWHITE);
        }

        Font fontSmall = eng.Assets().GetMainFont();
        DrawTextEx(fontSmall, "[SPACE / ESC] Пропустить", {20.0f, static_cast<float>(screenH - 30)}, 16.0f, 1.0f, ColorAlpha(LIGHTGRAY, 0.4f));
    }

    if (m_fadeAlpha > 0.001f) {
        DrawRectangle(0, 0, screenW, screenH, ColorAlpha(m_fadeColor, m_fadeAlpha));
    }
}

}
