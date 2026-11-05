#pragma once

#include "map/MapData.h"
#include "scene/Scene.h"
#include <string>

namespace knopmops::runtime {

// Создание пустых проектов и уровней (без готового контента).
class ProjectTemplate {
public:
    // Создаёт папку проекта: chapter.json, пустой уровень level1, пустой scripts/main.lua.
    static bool Create(const std::string& parentDir, const std::string& id, const std::string& title, std::string& error);

    // Пустой уровень: пол, игрок и лампа — чтобы Play сразу запускался.
    static void BuildEmptyLevel(scene::Scene& sc, map::MapDocument& doc, const std::string& name);
};

}
