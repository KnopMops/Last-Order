#pragma once

#include "map/MapData.h"
#include "scene/Scene.h"
#include "render/ViewportRenderer.h"
#include "scripting/CodeEditor.h"
#include "chapter4/Chapter4Director.h"
#include <string>

namespace knopmops::ui {

class EditorUI {
public:
    EditorUI();
    ~EditorUI();

    void Init(map::MapDocument* doc, scene::Scene* scene, render::ViewportRenderer* renderer);
    void Render();

    bool ShouldExit() const { return m_shouldExit; }

private:
    map::MapDocument* m_doc = nullptr;
    scene::Scene* m_scene = nullptr;
    render::ViewportRenderer* m_renderer = nullptr;
    scripting::CodeEditor m_codeEditor;
    chapter4::Chapter4Director m_ch4Director;

    bool m_shouldExit = false;

    char m_hierarchyFilter[128]{};
    char m_textureFilter[128]{};
    char m_newPropKey[64]{};
    char m_newPropVal[128]{};

    float m_roomWidth = 8.0f;
    float m_roomLength = 8.0f;
    float m_roomHeight = 3.2f;
    float m_roomWallThick = 0.25f;
    char m_roomWallTex[64] = "motel_wallpaper";
    char m_roomFloorTex[64] = "motel_carpet";
    char m_roomCeilingTex[64] = "motel_ceiling";

    bool m_isPlaying = false;
    float m_playTime = 0.0f;
    int m_activeViewportTab = 0; // 0: Scene, 1: Game

    void RenderMainMenuBar();
    void RenderToolbar();
    void RenderHierarchy();
    void RenderSceneViewport();
    void RenderGameViewport();
    void RenderInspector();
    void RenderAssetBrowser();
    void RenderMapTools();
    void RenderConsole();

    void ActionNewMap();
    void ActionOpenMap(const std::string& path);
    void ActionSaveMap();
    void ActionSaveMapAs(const std::string& path);
    void ActionDuplicateSelected();
    void ActionDeleteSelected();
};

}
