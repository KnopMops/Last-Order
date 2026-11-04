#pragma once
#include "engine/core/Engine.h"
#include <string>

namespace game::chapter3 {

enum class Chapter3Ending {
    None,
    FrontDoor,
    GarageCar,
    SewerTunnel,
    RadioMayak,
    GameOverDeath
};

struct FrontDoorLocks {
    int planksRemoved = 0;
    int totalPlanks = 3;
    bool padlockOpen = false;
    bool alarmCut = false;
    bool keypadUnlocked = false;
    bool masterKeyUsed = false;

    bool IsFullyUnlocked() const {
        return planksRemoved >= totalPlanks && padlockOpen && alarmCut && keypadUnlocked && masterKeyUsed;
    }
};

struct CarEscapeState {
    bool batteryInstalled = false;
    bool sparkplugInstalled = false;
    bool gasFilled = false;
    bool keyInserted = false;
    bool garageDoorsOpen = false;

    bool IsReady() const {
        return batteryInstalled && sparkplugInstalled && gasFilled && keyInserted && garageDoorsOpen;
    }
};

struct SewerEscapeState {
    bool steamValveClosed = false;
    bool sewerGrateUnlocked = false;

    bool IsReady() const {
        return steamValveClosed && sewerGrateUnlocked;
    }
};

struct RadioEscapeState {
    bool fuseInstalled = false;
    bool tubeInstalled = false;
    bool frequencyTuned = false;

    bool IsReady() const {
        return fuseInstalled && tubeInstalled && frequencyTuned;
    }
};

class EscapeManager {
public:
    void Reset();
    void AdvanceDay();
    int GetCurrentDay() const { return m_currentDay; }
    bool IsFinalDay() const { return m_currentDay >= 5; }
    float GetPlayerSpeedMultiplier() const;

    FrontDoorLocks& GetDoorLocks() { return m_door; }
    const FrontDoorLocks& GetDoorLocks() const { return m_door; }

    CarEscapeState& GetCarState() { return m_car; }
    const CarEscapeState& GetCarState() const { return m_car; }

    SewerEscapeState& GetSewerState() { return m_sewer; }
    const SewerEscapeState& GetSewerState() const { return m_sewer; }

    RadioEscapeState& GetRadioState() { return m_radio; }
    const RadioEscapeState& GetRadioState() const { return m_radio; }

    bool TryInteractFrontDoor(engine::core::Engine& eng, const std::string& activeItem, const std::string& enteredCode, const std::string& actualCode, std::string& outMsg);
    bool TryInteractCar(engine::core::Engine& eng, const std::string& activeItem, std::string& outMsg);
    bool TryInteractSewer(engine::core::Engine& eng, const std::string& activeItem, std::string& outMsg);
    bool TryInteractRadio(engine::core::Engine& eng, const std::string& activeItem, float dialFrequency, std::string& outMsg);

    Chapter3Ending CheckTriggeredEnding() const;

private:
    int m_currentDay = 1;
    FrontDoorLocks m_door;
    CarEscapeState m_car;
    SewerEscapeState m_sewer;
    RadioEscapeState m_radio;
};

}

