#include "chapter4/Chapter4Director.h"
#include "imgui.h"
#include "core/Log.h"
#include "map/MapSerializer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstring>

namespace knopmops::chapter4 {

using json = nlohmann::json;
namespace fs = std::filesystem;

Chapter4Director::Chapter4Director() {
    std::strncpy(m_titleBuf, m_config.title.c_str(), sizeof(m_titleBuf));
    std::strncpy(m_settingBuf, m_config.setting.c_str(), sizeof(m_settingBuf));
}

Chapter4Director::~Chapter4Director() = default;

void Chapter4Director::Init() {
    core::Log::Info("Chapter 4 Director initialized.");
}

void Chapter4Director::RenderUI(scene::Scene* scene, map::MapDocument* mapDoc) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "CHAPTER 4 DIRECTOR & STORY SUITE");
    ImGui::TextDisabled("Создание полноценной 4-й главы для «Последний заказ»");
    ImGui::Separator();

    if (ImGui::InputText("Название главы", m_titleBuf, sizeof(m_titleBuf))) {
        m_config.title = m_titleBuf;
    }

    if (ImGui::InputTextMultiline("Описание сеттинга", m_settingBuf, sizeof(m_settingBuf), ImVec2(-FLT_MIN, 50))) {
        m_config.setting = m_settingBuf;
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "Параметры выживания и ИИ:");
    ImGui::SliderInt("Количество дней", &m_config.survivalDays, 3, 7);
    ImGui::SliderFloat("Скорость маньяка (м/с)", &m_config.monsterSpeed, 1.8f, 4.5f, "%.1f м/с");
    ImGui::SliderFloat("Радиус слуха ИИ", &m_config.hearingSensitivity, 5.0f, 18.0f, "%.1f м");
    ImGui::Checkbox("Травма ноги на День 3 (хромота)", &m_config.enableLimpInjuryDay3);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Доступные концовки Главы 4:");
    ImGui::Checkbox("Концовка 1: Побег на машине (сбор мотора + ключи)", &m_config.enableCarEscape);
    ImGui::Checkbox("Концовка 2: Побег через дренажный тоннель / коллектор", &m_config.enableTunnelEscape);
    ImGui::Checkbox("Концовка 3: Вызов группы эвакуации по военной рации", &m_config.enableRadioEscape);

    ImGui::Separator();

    if (ImGui::Button("Экспортировать Главу 4 в игру Last Order", ImVec2(-FLT_MIN, 36))) {
        if (scene && mapDoc) {
            ExportChapter4(*scene, *mapDoc);
        }
    }
}

bool Chapter4Director::ExportChapter4(const scene::Scene& scene, const map::MapDocument& mapDoc) {
    try {
        std::string baseDir = "assets/data/chapter4";
        fs::create_directories(baseDir + "/cutscenes");
        fs::create_directories("assets/data/maps");

        GenerateChapterAssets(baseDir);

        std::string mapPath = "assets/data/maps/ch4_level.map";
        map::MapSerializer::SaveToFile(mapPath, mapDoc);

        std::string scenePath = baseDir + "/scene_layout.json";
        const_cast<scene::Scene&>(scene).SaveToFile(scenePath);

        core::Log::Success("=================================================");
        core::Log::Success("УСПЕХ! Глава 4 полностью скомпилирована в:");
        core::Log::Success("  - Карта: " + mapPath);
        core::Log::Success("  - Катсцены: " + baseDir + "/cutscenes/");
        core::Log::Success("  - Предметы: " + baseDir + "/items.json");
        core::Log::Success("  - Конфиг: " + baseDir + "/chapter_config.json");
        core::Log::Success("=================================================");
        return true;
    } catch (...) {
        core::Log::Error("Ошибка при экспорте Главы 4.");
        return false;
    }
}

void Chapter4Director::GenerateChapterAssets(const std::string& baseDir) {
    json cfg;
    cfg["title"] = m_config.title;
    cfg["setting"] = m_config.setting;
    cfg["survivalDays"] = m_config.survivalDays;
    cfg["monsterSpeed"] = m_config.monsterSpeed;
    cfg["hearingRadius"] = m_config.hearingSensitivity;
    cfg["limpDay3"] = m_config.enableLimpInjuryDay3;
    cfg["endings"] = {
        {"car", m_config.enableCarEscape},
        {"tunnel", m_config.enableTunnelEscape},
        {"radio", m_config.enableRadioEscape}
    };

    std::ofstream cfgFile(baseDir + "/chapter_config.json");
    if (cfgFile.is_open()) cfgFile << cfg.dump(2);

    json items = json::array({
        {{"id", "item_flashlight"}, {"name", "Фонарик"}, {"desc", "Карманный фонарь для освещения катакомб."}},
        {{"id", "item_crowbar"}, {"name", "Монтировка"}, {"desc", "Стальной гвоздодер для досок и запертых ящиков."}},
        {{"id", "item_wire_cutters"}, {"name", "Кусачки"}, {"desc", "Слесарные кусачки для проволоки на воротах."}},
        {{"id", "item_bunker_key"}, {"name", "Ключ от бункера"}, {"desc", "Латунный ключ с гравировкой Шлюз-1."}},
        {{"id", "item_radio_fuse"}, {"name", "Предохранитель"}, {"desc", "Керамический предохранитель 25A."}},
        {{"id", "item_car_battery"}, {"name", "Аккумулятор"}, {"desc", "Тяжелая батарея 12V для запуска мотора."}},
        {{"id", "item_gas_canister"}, {"name", "Канистра топлива"}, {"desc", "Бензин для аварийного генератора."}}
    });

    std::ofstream itemsFile(baseDir + "/items.json");
    if (itemsFile.is_open()) itemsFile << items.dump(2);

    auto writeCutscene = [](const std::string& path, const std::string& title, const std::string& sub, float dur) {
        json cs;
        cs["name"] = title;
        cs["duration"] = dur;
        cs["events"] = json::array({
            {{"time", 0.0}, {"type", "fade_in"}, {"duration", 1.5}},
            {{"time", 0.5}, {"type", "subtitle"}, {"text", sub}, {"duration", 4.0}},
            {{"time", dur - 1.0}, {"type", "fade_out"}, {"duration", 1.0}}
        });
        std::ofstream f(path);
        if (f.is_open()) f << cs.dump(2);
    };

    writeCutscene(baseDir + "/cutscenes/intro_waking_up.json", "Intro: Waking up in Bunker", "Голова раскалывается... Где я? За дверью кто-то ходит...", 5.5f);
    writeCutscene(baseDir + "/cutscenes/ending_car_escape.json", "Ending: Car Escape", "Мотор взревел! Ворота бункера пробиты! Свобода!", 6.0f);
    writeCutscene(baseDir + "/cutscenes/ending_tunnel_escape.json", "Ending: Tunnel Escape", "Канализационный люк вывел в ночной лес... Мы спаслись.", 6.0f);
}

}

