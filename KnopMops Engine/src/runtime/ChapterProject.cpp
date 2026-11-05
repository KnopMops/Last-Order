#include "runtime/ChapterProject.h"
#include "runtime/ScriptHost.h"
#include "core/Log.h"
#include "map/MapData.h"
#include "map/MapParser.h"
#include "render/TextureManager.h"
#include "scene/Scene.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <regex>
#include <set>
#include <sstream>

namespace knopmops::runtime {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

std::string ReadAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<std::string> ListByExt(const std::string& dir, const std::string& ext, bool withExt) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file() || e.path().extension() != ext) continue;
        out.push_back(withExt ? e.path().filename().string() : e.path().stem().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

}

std::string ChapterProject::ManifestPath() const { return m_root + "/chapter.json"; }
std::string ChapterProject::LevelScenePath(const std::string& level) const { return m_root + "/levels/" + level + ".kmscene"; }
std::string ChapterProject::LevelMapPath(const std::string& level) const { return m_root + "/levels/" + level + ".map"; }
std::string ChapterProject::ScriptsDir() const { return m_root + "/scripts"; }
std::string ChapterProject::ScriptPath(const std::string& file) const { return m_root + "/scripts/" + file; }
std::string ChapterProject::PrefabsDir() const { return m_root + "/prefabs"; }
std::string ChapterProject::TexturesDir() const { return m_root + "/textures"; }
std::string ChapterProject::AudioDir() const { return m_root + "/audio"; }

std::vector<std::string> ChapterProject::ListLevels() const { return ListByExt(m_root + "/levels", ".kmscene", false); }
std::vector<std::string> ChapterProject::ListScripts() const { return ListByExt(ScriptsDir(), ".lua", true); }
std::vector<std::string> ChapterProject::ListPrefabs() const { return ListByExt(PrefabsDir(), ".kmprefab", true); }

bool ChapterProject::Load(const std::string& dir) {
    try {
        std::string path = dir + "/chapter.json";
        std::ifstream f(path);
        if (!f.is_open()) return false;
        json j;
        f >> j;

        ChapterManifest m;
        m.id = j.value("id", fs::path(dir).filename().string());
        m.title = j.value("title", std::string("Глава"));
        m.description = j.value("description", std::string());
        m.intro = j.value("intro", std::string());
        m.startLevel = j.value("startLevel", std::string("level1"));
        m.winTitle = j.value("winTitle", std::string("ГЛАВА ПРОЙДЕНА"));
        m.loseText = j.value("loseText", std::string("Тебя поймали..."));
        m.flashlightDrain = j.value("flashlightDrain", 0.004f);
        if (j.contains("startItems")) {
            for (const auto& s : j["startItems"]) m.startItems.push_back(s.get<std::string>());
        }
        if (j.contains("items")) {
            for (const auto& it : j["items"]) {
                m.items.push_back(ItemDef{it.value("id", std::string()), it.value("name", std::string()), it.value("desc", std::string())});
            }
        }
        m_manifest = m;
        m_root = dir;
        while (m_root.size() > 1 && (m_root.back() == '/' || m_root.back() == '\\')) m_root.pop_back();
        return true;
    } catch (const std::exception& e) {
        core::Log::Error(std::string("chapter.json: ") + e.what());
        return false;
    }
}

bool ChapterProject::Save() const {
    if (m_root.empty()) return false;
    try {
        json j;
        j["format"] = 1;
        j["id"] = m_manifest.id;
        j["title"] = m_manifest.title;
        j["description"] = m_manifest.description;
        j["intro"] = m_manifest.intro;
        j["startLevel"] = m_manifest.startLevel;
        j["winTitle"] = m_manifest.winTitle;
        j["loseText"] = m_manifest.loseText;
        j["flashlightDrain"] = m_manifest.flashlightDrain;
        j["startItems"] = m_manifest.startItems;
        j["items"] = json::array();
        for (const auto& it : m_manifest.items) j["items"].push_back({{"id", it.id}, {"name", it.name}, {"desc", it.description}});
        fs::create_directories(m_root);
        std::ofstream f(ManifestPath());
        if (!f.is_open()) return false;
        f << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

const ItemDef* ChapterProject::FindItem(const std::string& id) const {
    for (const auto& it : m_manifest.items) {
        if (it.id == id) return &it;
    }
    return nullptr;
}

std::string ChapterProject::ItemName(const std::string& id) const {
    const ItemDef* d = FindItem(id);
    return (d && !d->name.empty()) ? d->name : id;
}

std::vector<ValidationIssue> ChapterProject::Validate() const {
    using Sev = ValidationIssue::Severity;
    std::vector<ValidationIssue> out;
    auto add = [&](Sev s, const std::string& t) { out.push_back({s, t}); };

    auto levels = ListLevels();
    if (levels.empty()) add(Sev::Error, "В главе нет ни одного уровня (levels/*.kmscene).");
    if (std::find(levels.begin(), levels.end(), m_manifest.startLevel) == levels.end()) {
        add(Sev::Error, "Стартовый уровень '" + m_manifest.startLevel + "' не найден.");
    }

    struct Source { std::string item; std::string needs; std::string where; };
    std::vector<Source> sources;
    std::set<std::string> usedItems;
    std::set<std::string> doorKeys;
    std::set<std::string> scriptsChecked;

    for (const auto& lvl : levels) {
        scene::Scene sc;
        if (!sc.LoadFromFile(LevelScenePath(lvl))) {
            add(Sev::Error, "Уровень " + lvl + ": не удалось прочитать .kmscene");
            continue;
        }
        if (!fs::exists(LevelMapPath(lvl))) add(Sev::Warning, "Уровень " + lvl + ": нет файла .map (геометрии).");

        int players = 0;
        for (const auto& up : sc.GetGameObjects()) {
            const auto& o = *up;
            std::string where = lvl + "/" + o.name;
            if (o.player.enabled) ++players;
            if (o.searchSlot.enabled) {
                if (!o.searchSlot.containedItem.empty()) {
                    sources.push_back({o.searchSlot.containedItem, o.searchSlot.isLocked ? o.searchSlot.requiredTool : "", where});
                    usedItems.insert(o.searchSlot.containedItem);
                }
                if (o.searchSlot.isLocked) {
                    if (o.searchSlot.requiredTool.empty()) add(Sev::Warning, where + ": заперт, но не указан Required Tool.");
                    else usedItems.insert(o.searchSlot.requiredTool);
                }
            }
            if (o.itemPickup.enabled) {
                sources.push_back({o.itemPickup.itemId, "", where});
                usedItems.insert(o.itemPickup.itemId);
            }
            if (o.door.enabled && o.door.isLocked) {
                if (o.door.requiredKey.empty()) add(Sev::Info, where + ": дверь заперта без ключа — откроется только скриптом (obj:unlock()).");
                else { doorKeys.insert(o.door.requiredKey); usedItems.insert(o.door.requiredKey); }
            }
            if (o.script.enabled) {
                if (o.script.scriptFile.empty()) {
                    add(Sev::Warning, where + ": компонент Script без файла.");
                } else if (scriptsChecked.insert(o.script.scriptFile).second) {
                    std::string err;
                    if (!fs::exists(ScriptPath(o.script.scriptFile))) add(Sev::Error, where + ": скрипт '" + o.script.scriptFile + "' не найден.");
                    else if (!ScriptHost::CheckSyntax(ScriptPath(o.script.scriptFile), err)) add(Sev::Error, "Lua: " + err);
                }
            }
        }
        if (players == 0) add(Sev::Error, "Уровень " + lvl + ": нет объекта с компонентом Player.");
        if (players > 1) add(Sev::Warning, "Уровень " + lvl + ": несколько Player — будет использован первый.");
    }

    {
        std::string err;
        std::string mainPath = ScriptPath("main.lua");
        if (fs::exists(mainPath) && !ScriptHost::CheckSyntax(mainPath, err)) add(Sev::Error, "Lua: " + err);
    }

    // Предметы, которые выдаются скриптами (эвристика: give_item("id")).
    std::set<std::string> scriptGiven;
    std::regex giveRe(R"(give_item\s*\(\s*["']([^"']+)["'])");
    for (const auto& s : ListScripts()) {
        std::string text = ReadAll(ScriptPath(s));
        for (std::sregex_iterator it(text.begin(), text.end(), giveRe), end; it != end; ++it) scriptGiven.insert((*it)[1].str());
    }

    for (const auto& id : usedItems) {
        if (!FindItem(id) && !scriptGiven.count(id)) add(Sev::Warning, "Предмет '" + id + "' используется, но не описан в chapter.json (Items).");
    }

    // Софтлок: фикспоинт доступности предметов.
    std::set<std::string> have(m_manifest.startItems.begin(), m_manifest.startItems.end());
    have.insert(scriptGiven.begin(), scriptGiven.end());
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& s : sources) {
            if (have.count(s.item)) continue;
            if (s.needs.empty() || have.count(s.needs)) {
                have.insert(s.item);
                changed = true;
            }
        }
    }
    for (const auto& s : sources) {
        if (!have.count(s.item)) add(Sev::Error, "СОФТЛОК: предмет '" + s.item + "' (" + s.where + ") недостижим — заперт инструментом '" + s.needs + "', который сам недоступен.");
    }
    for (const auto& k : doorKeys) {
        if (!have.count(k)) add(Sev::Error, "СОФТЛОК: ключ '" + k + "' от двери нигде не лежит и не выдаётся скриптом.");
    }

    if (out.empty()) add(Sev::Info, "Проблем не найдено.");
    return out;
}

bool ChapterProject::ExportToGame(const std::string& gameRoot, std::string* log) const {
    auto say = [&](const std::string& s) { if (log) *log += s + "\n"; };
    if (m_root.empty()) return false;
    if (!Save()) { say("Не удалось сохранить chapter.json"); return false; }

    std::vector<std::string> targets;
    targets.push_back(gameRoot + "/assets/data/chapters/" + m_manifest.id);
    if (fs::exists(gameRoot + "/release/assets")) targets.push_back(gameRoot + "/release/assets/data/chapters/" + m_manifest.id);

    // Текстуры, реально используемые главой.
    std::set<std::string> textures;
    for (const auto& lvl : ListLevels()) {
        scene::Scene sc;
        if (sc.LoadFromFile(LevelScenePath(lvl))) {
            for (const auto& o : sc.GetGameObjects()) {
                if (!o->meshRenderer.textureName.empty()) textures.insert(o->meshRenderer.textureName);
            }
        }
        map::MapDocument doc;
        if (fs::exists(LevelMapPath(lvl)) && map::MapParser::LoadFromFile(LevelMapPath(lvl), doc)) {
            for (const auto& b : doc.GetWorldBrushes()) for (const auto& f : b.faces) textures.insert(f.texture);
            for (const auto& e : doc.GetEntities()) for (const auto& b : e.brushes) for (const auto& f : b.faces) textures.insert(f.texture);
        }
    }
    for (const char* t : {"motel_door", "motel_wall_wood", "motel_cardboard", "motel_metal", "motel_concrete"}) textures.insert(t);

    try {
        for (const auto& dst : targets) {
            std::error_code ec;
            fs::remove_all(dst, ec);
            fs::create_directories(dst);
            for (const char* sub : {"levels", "scripts", "prefabs", "audio", "textures"}) {
                std::string src = m_root + "/" + sub;
                if (fs::exists(src)) fs::copy(src, dst + "/" + sub, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            }
            fs::create_directories(dst + "/textures");
            fs::copy_file(ManifestPath(), dst + "/chapter.json", fs::copy_options::overwrite_existing);
            int copied = 0;
            for (const auto& name : textures) {
                std::string src = render::TextureManager::Instance().FindFile(name);
                if (src.empty()) continue;
                std::string dstFile = dst + "/textures/" + name + ".png";
                if (!fs::exists(dstFile)) {
                    fs::copy_file(src, dstFile, fs::copy_options::overwrite_existing);
                    ++copied;
                }
            }
            say("Экспорт: " + dst + " (+" + std::to_string(copied) + " текстур)");
        }
        return true;
    } catch (const std::exception& e) {
        say(std::string("Ошибка экспорта: ") + e.what());
        return false;
    }
}

std::string ChapterProject::DetectGameRoot(const std::string& startDir) {
    std::error_code ec;
    fs::path p = fs::absolute(startDir, ec);
    for (int i = 0; i < 6 && !p.empty(); ++i) {
        if (fs::exists(p / "assets" / "data", ec) && fs::exists(p / "src" / "game", ec)) return p.string();
        if (!p.has_parent_path() || p.parent_path() == p) break;
        p = p.parent_path();
    }
    return "";
}

std::vector<std::string> ChapterProject::ListProjects(const std::string& projectsDir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::exists(projectsDir, ec)) return out;
    for (const auto& e : fs::directory_iterator(projectsDir, ec)) {
        if (e.is_directory() && fs::exists(e.path() / "chapter.json")) out.push_back(e.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> ChapterProject::ListInstalledChapters(const std::string& assetsDir) {
    return ListProjects(assetsDir + "/data/chapters");
}

}
