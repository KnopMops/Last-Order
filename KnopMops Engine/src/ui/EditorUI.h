#pragma once

#include "map/MapData.h"
#include "render/ViewportRenderer.h"
#include "runtime/ChapterProject.h"
#include "runtime/PlaySession.h"
#include "runtime/ScriptHost.h"
#include "scene/Scene.h"
#include "scripting/CodeEditor.h"
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace knopmops::ui {

class EditorUI {
public:
    EditorUI();
    ~EditorUI();

    void Init(map::MapDocument* doc, scene::Scene* scene, render::ViewportRenderer* renderer);
    void Shutdown();
    void Render();

    bool ShouldExit() const { return m_shouldExit; }

    // Автотест: открывает проект, проходит главу через PlaySession без ввода. 0 = успех.
    int RunSelfTest(const std::string& projectName);

    // Консольный экспорт: KnopMops Engine.exe --export <проект>. 0 = успех.
    int RunExport(const std::string& projectName);
    // Генерация проекта «Глава 3.5» и её автопрохождение.
    int RunBuildChapter35();
    int RunSelfTest35();

    // Для скриншот-тестов (--shots): доступ к текущей Play-сессии и запуск Play.
    runtime::PlaySession* DebugPlay() { return m_play.get(); }
    void DebugStartPlay() { StartPlay(); }

private:
    map::MapDocument* m_doc = nullptr;
    scene::Scene* m_scene = nullptr;
    render::ViewportRenderer* m_renderer = nullptr;
    scripting::CodeEditor m_codeEditor;

    // --- проект ---
    runtime::ChapterProject m_project;
    std::string m_levelName;
    std::string m_projectsDir;
    std::string m_gameRoot;
    std::vector<runtime::ValidationIssue> m_issues;
    bool m_ignoreErrorsOnExport = false;

    // --- состояние окна ---
    bool m_shouldExit = false;
    bool m_resetLayout = false;
    bool m_layoutBuilt = false;
    Font m_hudFont{};
    bool m_hasHudFont = false;

    // --- Play-In-Editor ---
    std::unique_ptr<runtime::PlaySession> m_play;
    RenderTexture2D m_gameRT{};
    int m_gameW = 0, m_gameH = 0;
    bool m_captured = false;
    bool m_focusGameTab = false;
    bool m_focusSceneTab = false;
    float m_playTime = 0.0f;

    // --- буферы UI ---
    char m_hierarchyFilter[128]{};
    char m_textureFilter[128]{};
    char m_newProjectId[64] = "my_chapter";
    char m_newProjectTitle[128] = "Моя глава";
    char m_newLevelName[64] = "level3";
    char m_newPropKey[64]{};
    char m_newPropVal[128]{};
    char m_itemFilter[64]{};
    float m_roomWidth = 8.0f;
    float m_roomLength = 8.0f;
    float m_roomHeight = 3.0f;
    float m_roomWallThick = 0.25f;
    char m_roomWallTex[64] = "motel_wallpaper";
    char m_roomFloorTex[64] = "motel_tile";
    char m_roomCeilingTex[64] = "motel_ceiling";
    bool m_dirtyTitle = false;
    std::string m_externalMap;        // открытая для просмотра внешняя карта (не из проекта)
    std::string m_explorerDir;        // текущая папка в проводнике
    int m_explorerRoot = 0;
    std::string m_pendingDelete;      // файл/папка к удалению (ждёт подтверждения)
    std::string m_pendingDeleteLevel;
    char m_newFileName[96] = "novyi_fail.lua";
    char m_newFolderName[96] = "novaya_papka";
    char m_renameBuf[64]{};
    std::string m_renameLevel;

    struct ExportCache {
        std::filesystem::file_time_type mtime{};
        std::vector<runtime::ExportVar> vars;
    };
    std::map<std::string, ExportCache> m_exports;

    // --- проект и уровни ---
    bool OpenProject(const std::string& name);
    bool CreateProject(const std::string& id, const std::string& title);
    bool OpenLevel(const std::string& level);
    bool CreateLevel(const std::string& level);
    bool SaveAll();
    bool ExportToGame();
    void RunInGame();
    void FindPaths();
    void SaveLastProject();

    // --- Play ---
    void StartPlay();
    void StopPlay();

    // --- интерфейс ---
    void BuildDefaultLayout(unsigned int dockspaceId, float w, float h);
    void RenderMainMenuBar();
    void RenderToolbar();
    void RenderHierarchy();
    void RenderHierarchyNode(scene::GameObject* obj, const std::string& filter);
    void RenderSceneViewport();
    void RenderGameViewport();
    void RenderInspector();
    void RenderObjectInspector(scene::GameObject& obj);
    void RenderBrushInspector(int index);
    void RenderSceneSettings();
    void RenderProjectPanel();
    void RenderTexturesTab();
    void RenderMapTools();
    void RenderChapterPanel();
    void RenderConsole();
    void RenderFileExplorer();
    void RenderSceneManager();
    bool OpenExternalMap(const std::string& path);
    bool ImportMapAsLevel(const std::string& path);
    void RenameLevel(const std::string& from, const std::string& to);
    void DeleteLevel(const std::string& level);
    void DuplicateLevel(const std::string& level);

    scene::GameObject* CreateObjectAtFocus(const std::string& name, float yOffset = 0.0f);
    void CreateObjectMenu();
    void ActionDuplicateSelected();
    void ActionDeleteSelected();
    Vector3 FocusPoint() const;
    const std::vector<runtime::ExportVar>& ExportsFor(const std::string& scriptFile);
};

}
