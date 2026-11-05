#pragma once

#include <string>
#include <vector>

namespace knopmops::runtime {

struct ItemDef {
    std::string id;
    std::string name;
    std::string description;
};

// chapter.json — паспорт главы.
struct ChapterManifest {
    std::string id = "my_chapter";
    std::string title = "Новая глава";
    std::string description;
    std::string intro;                  // текст на старте главы (субтитр)
    std::string startLevel = "level1";
    std::vector<std::string> startItems;
    std::vector<ItemDef> items;
    std::string winTitle = "ГЛАВА ПРОЙДЕНА";
    std::string loseText = "Тебя поймали...";
    float flashlightDrain = 0.004f;     // расход батареи фонаря в секунду (0 = бесконечно)
};

struct ValidationIssue {
    enum class Severity { Info, Warning, Error } severity = Severity::Info;
    std::string text;
};

// Папка проекта главы:
//   chapter.json | levels/<name>.kmscene + <name>.map | scripts/*.lua | prefabs/*.kmprefab | textures/ | audio/
class ChapterProject {
public:
    bool Load(const std::string& dir);
    bool Save() const;

    const std::string& Root() const { return m_root; }
    bool IsLoaded() const { return !m_root.empty(); }
    ChapterManifest& Manifest() { return m_manifest; }
    const ChapterManifest& Manifest() const { return m_manifest; }

    std::string ManifestPath() const;
    std::string LevelScenePath(const std::string& level) const;
    std::string LevelMapPath(const std::string& level) const;
    std::string ScriptsDir() const;
    std::string ScriptPath(const std::string& file) const;
    std::string PrefabsDir() const;
    std::string TexturesDir() const;
    std::string AudioDir() const;

    std::vector<std::string> ListLevels() const;
    std::vector<std::string> ListScripts() const;
    std::vector<std::string> ListPrefabs() const;

    const ItemDef* FindItem(const std::string& id) const;
    std::string ItemName(const std::string& id) const;

    // Проверка главы: недостающие скрипты, ошибки Lua, софтлоки, неизвестные предметы.
    std::vector<ValidationIssue> Validate() const;

    // Копирует главу (вместе с используемыми текстурами) в <gameRoot>/assets/data/chapters/<id>
    // (и в <gameRoot>/release/assets/..., если такая папка есть).
    bool ExportToGame(const std::string& gameRoot, std::string* log = nullptr) const;

    static std::string DetectGameRoot(const std::string& startDir);
    static std::vector<std::string> ListProjects(const std::string& projectsDir);
    static std::vector<std::string> ListInstalledChapters(const std::string& assetsDir);

private:
    std::string m_root;
    ChapterManifest m_manifest;
};

}
