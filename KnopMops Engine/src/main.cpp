#include "raylib.h"
#include "rlImGui.h"
#include "map/MapData.h"
#include "map/MapParser.h"
#include "scene/Scene.h"
#include "render/TextureManager.h"
#include "render/ViewportRenderer.h"
#include "ui/EditorUI.h"
#include "core/Log.h"

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1600, 900, "KnopMops Engine - Unity-Style 3D Survival Horror Engine (v1.1.0)");
    SetWindowMinSize(1024, 600);
    SetTargetFPS(144);

    rlImGuiSetup(true);

    knopmops::render::TextureManager::Instance().Init("assets/textures");

    knopmops::map::MapDocument doc;
    if (!knopmops::map::MapParser::LoadFromFile("assets/maps/template_motel.map", doc)) {
        doc.CreateDefaultScene();
    }

    knopmops::scene::Scene scene("Chapter4_Bunker_Escape");
    scene.SetupDefaultChapter4Scene();

    knopmops::render::ViewportRenderer renderer;
    renderer.Init(1280, 720);

    knopmops::ui::EditorUI editorUI;
    editorUI.Init(&doc, &scene, &renderer);

    while (!WindowShouldClose() && !editorUI.ShouldExit()) {
        BeginDrawing();
        ClearBackground(Color{25, 25, 25, 255});

        rlImGuiBegin();

        editorUI.Render();

        rlImGuiEnd();

        EndDrawing();
    }

    renderer.Shutdown();
    knopmops::render::TextureManager::Instance().Shutdown();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
