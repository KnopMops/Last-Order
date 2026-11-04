#include "game/chapter3/EscapeManager.h"
#include <cmath>

namespace game::chapter3 {

void EscapeManager::Reset() {
    m_currentDay = 1;
    m_door = FrontDoorLocks{};
    m_car = CarEscapeState{};
    m_sewer = SewerEscapeState{};
    m_radio = RadioEscapeState{};
}

void EscapeManager::AdvanceDay() {
    m_currentDay++;
}

float EscapeManager::GetPlayerSpeedMultiplier() const {
    if (m_currentDay <= 2) return 1.0f;
    if (m_currentDay <= 4) return 0.85f;
    return 0.75f;
}

bool EscapeManager::TryInteractFrontDoor(engine::core::Engine& eng, const std::string& activeItem, const std::string& enteredCode, const std::string& actualCode, std::string& outMsg) {
    if (m_door.planksRemoved < m_door.totalPlanks) {
        if (activeItem == "item_crowbar") {
            m_door.planksRemoved++;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_crowbar_pry.wav", 1.0f);
            outMsg = "Доска оторвана! Осталось: " + std::to_string(m_door.totalPlanks - m_door.planksRemoved);
            return true;
        }
        outMsg = "Дверь заколочена крепкими досками. Нужна монтировка.";
        return false;
    }

    if (!m_door.padlockOpen) {
        if (activeItem == "item_padlock_key") {
            m_door.padlockOpen = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_padlock_open.wav", 1.0f);
            outMsg = "Навесной замок открыт!";
            return true;
        }
        outMsg = "Массивный навесной замок. Нужен ключ от замка.";
        return false;
    }

    if (!m_door.alarmCut) {
        if (activeItem == "item_wire_cutters") {
            m_door.alarmCut = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_wire_cut.wav", 1.0f);
            outMsg = "Провода охранной сигнализации перекушены!";
            return true;
        }
        outMsg = "К замку подведен провод сигнализации. Нужны кусачки!";
        return false;
    }

    if (!m_door.keypadUnlocked) {
        if (!enteredCode.empty() && enteredCode == actualCode) {
            m_door.keypadUnlocked = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_keypad_beep.wav", 1.0f);
            outMsg = "Код верный! Электронный замок разблокирован.";
            return true;
        }
        outMsg = "Электронный кодовый замок. Требуется 4-значный код.";
        return false;
    }

    if (!m_door.masterKeyUsed) {
        if (activeItem == "item_master_key") {
            m_door.masterKeyUsed = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_padlock_open.wav", 1.0f);
            outMsg = "Главный ригель открыт! Парадная дверь не заперта!";
            return true;
        }
        outMsg = "Тяжелый ригельный замок. Нужен Главный ключ.";
        return false;
    }

    outMsg = "Дверь открыта!";
    return true;
}

bool EscapeManager::TryInteractCar(engine::core::Engine& eng, const std::string& activeItem, std::string& outMsg) {
    if (!m_car.batteryInstalled) {
        if (activeItem == "item_car_battery") {
            m_car.batteryInstalled = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_item_drop.wav", 0.9f);
            outMsg = "Аккумулятор установлен в моторный отсек!";
            return true;
        }
        outMsg = "Под капотом пусто. Нужен автомобильный аккумулятор.";
        return false;
    }

    if (!m_car.sparkplugInstalled) {
        if (activeItem == "item_car_sparkplug") {
            m_car.sparkplugInstalled = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_item_drop.wav", 0.7f);
            outMsg = "Свеча зажигания вкручена!";
            return true;
        }
        outMsg = "В двигателе нет искры. Нужна свеча зажигания.";
        return false;
    }

    if (!m_car.gasFilled) {
        if (activeItem == "item_gas_canister") {
            m_car.gasFilled = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_pipe_valve.wav", 0.8f);
            outMsg = "Бензобак заправлен топливом!";
            return true;
        }
        outMsg = "Бак абсолютно сух. Нужна канистра с бензином.";
        return false;
    }

    if (!m_car.garageDoorsOpen) {
        if (activeItem == "item_garage_key" || activeItem == "item_crowbar") {
            m_car.garageDoorsOpen = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_crowbar_pry.wav", 1.0f);
            outMsg = "Запорный брус гаражных ворот сбит!";
            return true;
        }
        outMsg = "Ворота гаража заперты на внешний засов. Нужен ключ или монтировка.";
        return false;
    }

    if (!m_car.keyInserted) {
        if (activeItem == "item_car_key") {
            m_car.keyInserted = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_keypad_beep.wav", 0.9f);
            outMsg = "Ключ в замке зажигания! Машина готова к пуску!";
            return true;
        }
        outMsg = "В замке зажигания нет ключа.";
        return false;
    }

    outMsg = "Машина готова!";
    return true;
}

bool EscapeManager::TryInteractSewer(engine::core::Engine& eng, const std::string& activeItem, std::string& outMsg) {
    if (!m_sewer.steamValveClosed) {
        if (activeItem == "item_wrench") {
            m_sewer.steamValveClosed = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_pipe_valve.wav", 1.0f);
            outMsg = "Вентиль перекрыт! Обжигающий пар рассеялся.";
            return true;
        }
        outMsg = "Из трубы бьет раскаленный пар! Нужен гаечный ключ 24.";
        return false;
    }

    if (!m_sewer.sewerGrateUnlocked) {
        if (activeItem == "item_sewer_key") {
            m_sewer.sewerGrateUnlocked = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_padlock_open.wav", 1.0f);
            outMsg = "Решетка коллектора открыта! Путь свободен!";
            return true;
        }
        outMsg = "Решетка заперта на массивный замок. Нужен ключ от коллектора.";
        return false;
    }

    outMsg = "Коллектор открыт!";
    return true;
}

bool EscapeManager::TryInteractRadio(engine::core::Engine& eng, const std::string& activeItem, float dialFrequency, std::string& outMsg) {
    if (!m_radio.fuseInstalled) {
        if (activeItem == "item_radio_fuse") {
            m_radio.fuseInstalled = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_keypad_beep.wav", 0.8f);
            outMsg = "Предохранитель установлен в блок питания радиостанции!";
            return true;
        }
        outMsg = "Блок передатчика обесточен. Перегорел предохранитель 25A.";
        return false;
    }

    if (!m_radio.tubeInstalled) {
        if (activeItem == "item_vacuum_tube") {
            m_radio.tubeInstalled = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_item_drop.wav", 0.7f);
            outMsg = "Генераторная лампа ГУ-50 установлена в гнездо усилителя!";
            return true;
        }
        outMsg = "В каскаде усилителя отсутствует мощная радиолампа.";
        return false;
    }

    if (!m_radio.frequencyTuned) {
        if (std::abs(dialFrequency - 94.5f) < 0.25f) {
            m_radio.frequencyTuned = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_radio_tune.wav", 1.0f);
            outMsg = "Частота 94.5 FM настроена! Контур вошел в резонанс!";
            return true;
        }
        outMsg = "Сигнал сбивается. Настройте шкалу передатчика на частоту Маяка (94.5 FM).";
        return false;
    }

    outMsg = "Передатчик готов к излучению!";
    return true;
}

Chapter3Ending EscapeManager::CheckTriggeredEnding() const {
    if (m_currentDay > 5) return Chapter3Ending::GameOverDeath;
    if (m_door.IsFullyUnlocked()) return Chapter3Ending::FrontDoor;
    if (m_car.IsReady()) return Chapter3Ending::GarageCar;
    if (m_sewer.IsReady()) return Chapter3Ending::SewerTunnel;
    if (m_radio.IsReady()) return Chapter3Ending::RadioMayak;
    return Chapter3Ending::None;
}

}

