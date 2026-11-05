#include "runtime/ProjectTemplate.h"
#include "map/BrushBuilder.h"
#include "map/MapSerializer.h"
#include "runtime/ChapterProject.h"
#include <filesystem>
#include <fstream>

namespace knopmops::runtime {

namespace fs = std::filesystem;

void ProjectTemplate::BuildEmptyLevel(scene::Scene& sc, map::MapDocument& doc, const std::string& name) {
    sc.Clear();
    sc.SetName(name);
    doc.Clear();

    map::MapBrush floor = map::BrushBuilder::CreateBox({-8.0f, -0.2f, -8.0f}, {8.0f, 0.0f, 8.0f}, "motel_concrete");
    floor.name = "Пол";
    doc.AddBrush(floor);

    auto* p = sc.CreateGameObject("Игрок");
    p->tag = "player";
    p->transform.position = {0.0f, 0.0f, -3.0f};
    p->player.enabled = true;
    p->camera.enabled = true;
    p->camera.isMainPlayerCamera = true;
    p->meshRenderer.meshName = "None";

    auto* l = sc.CreateGameObject("Лампа");
    l->transform.position = {0.0f, 3.0f, 0.0f};
    l->transform.scale = {0.3f, 0.12f, 0.3f};
    l->light.enabled = true;
    l->light.radius = 12.0f;
    l->meshRenderer.meshName = "Cube";
    l->meshRenderer.tint = Color{255, 245, 210, 255};
    l->meshRenderer.emissive = true;

    sc.Env().ambient = 0.12f;
    sc.SetModified(false);
}

bool ProjectTemplate::Create(const std::string& parentDir, const std::string& id, const std::string& title, std::string& error) {
    std::string root = parentDir + "/" + id;
    if (fs::exists(root)) { error = "Папка уже существует: " + root; return false; }
    try {
        for (const char* sub : {"levels", "scripts", "prefabs", "textures", "audio"}) fs::create_directories(root + "/" + sub);
        std::ofstream(root + "/chapter.json") << "{}";

        ChapterProject p;
        if (!p.Load(root)) { error = "Не удалось создать chapter.json"; return false; }
        p.Manifest().id = id;
        p.Manifest().title = title;
        p.Manifest().startLevel = "level1";
        p.Save();

        scene::Scene sc;
        map::MapDocument doc;
        BuildEmptyLevel(sc, doc, "level1");
        sc.SaveToFile(p.LevelScenePath("level1"));
        map::MapSerializer::SaveToFile(p.LevelMapPath("level1"), doc);
        std::ofstream(root + "/scripts/main.lua") << "-- main.lua: глобальный скрипт главы (работает всю игру)\n";
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

}
