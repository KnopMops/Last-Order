#pragma once
#include <cstdio>
#include <string>

namespace engine::core {

class TimeSystem {
public:
    void Reset(int startHour = 22, int startMinute = 0, float minutesPerRealSec = 1.5f) {
        m_inGameMinutes = static_cast<float>(startHour * 60 + startMinute);
        m_minutesPerSecond = minutesPerRealSec;
        m_paused = false;
        m_pauseTimer = 0.0f;
    }

    void Tick(float dt) {
        m_totalRealTime += dt;
        if (m_pauseTimer > 0.0f) {
            m_pauseTimer -= dt;
            return;
        }
        if (m_paused) return;

        m_inGameMinutes += dt * m_minutesPerSecond;
        if (m_inGameMinutes >= 24.0f * 60.0f) {
            m_inGameMinutes -= 24.0f * 60.0f;
        }
    }

    void PauseFor(float seconds) { m_pauseTimer = seconds; }
    void SetPaused(bool paused) { m_paused = paused; }
    bool IsPaused() const { return m_paused || m_pauseTimer > 0.0f; }

    void SetTimeHHMM(int hour, int minute) {
        m_inGameMinutes = static_cast<float>(hour * 60 + minute);
    }

    int GetHours24() const {
        int totalMins = static_cast<int>(m_inGameMinutes) % 1440;
        return totalMins / 60;
    }

    int GetMinutes() const {
        int totalMins = static_cast<int>(m_inGameMinutes) % 1440;
        return totalMins % 60;
    }

    float GetShiftElapsedMinutes() const {
        int mins = static_cast<int>(m_inGameMinutes);
        if (mins >= 22 * 60) return static_cast<float>(mins - 22 * 60);
        return static_cast<float>((24 * 60 - 22 * 60) + mins);
    }

    std::string FormatHHMM() const {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", GetHours24(), GetMinutes());
        return std::string(buf);
    }

    float GetTotalRealTime() const { return m_totalRealTime; }

private:
    float m_inGameMinutes = 22.0f * 60.0f;
    float m_minutesPerSecond = 1.5f;
    float m_totalRealTime = 0.0f;
    float m_pauseTimer = 0.0f;
    bool m_paused = false;
};

}
