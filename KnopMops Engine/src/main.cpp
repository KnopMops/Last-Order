#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "map/MapData.h"
#include "scene/Scene.h"
#include "render/TextureManager.h"
#include "render/ViewportRenderer.h"
#include "ui/EditorUI.h"
#include "core/Log.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void LoadEditorFonts() {
    ImGuiIO& io = ImGui::GetIO();
    std::string path = std::string(GetApplicationDirectory()) + "assets/fonts/font_main.ttf";
    if (!FileExists(path.c_str())) path = "assets/fonts/font_main.ttf";
    if (FileExists(path.c_str())) {
        io.Fonts->AddFontFromFileTTF(path.c_str(), 17.0f); // ImGui 1.92: глифы (в т.ч. кириллица) подгружаются динамически
    } else {
        io.Fonts->AddFontDefault();
    }
}

}

void InstallCrashHandler();

int main(int argc, char** argv) {
    InstallCrashHandler();
    bool selfTest = false;
    std::string shotsDir;
    std::string selfTestProject = "bunker_demo";
    bool doExport = false;
    bool build35 = false, test35 = false;
    std::string exportProject = "bunker_demo";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--shots") == 0 && i + 1 < argc) shotsDir = argv[++i];
        if (std::strcmp(argv[i], "--build-chapter35") == 0) build35 = true;
        if (std::strcmp(argv[i], "--selftest35") == 0) test35 = true;
        if (std::strcmp(argv[i], "--export") == 0) {
            doExport = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') exportProject = argv[i + 1];
        }
        if (std::strcmp(argv[i], "--selftest") == 0) {
            selfTest = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') selfTestProject = argv[i + 1];
        }
    }

    unsigned int flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
    if (selfTest || doExport || build35 || test35) flags |= FLAG_WINDOW_HIDDEN;
    SetConfigFlags(flags);
    SetTraceLogLevel(selfTest ? LOG_WARNING : LOG_INFO);
    InitWindow(1600, 900, "KnopMops Engine 2.0");
    SetWindowMinSize(1024, 600);
    SetTargetFPS(144);
    SetExitKey(KEY_NULL);
    InitAudioDevice();

    rlImGuiSetLoadFontsCallback(LoadEditorFonts);
    rlImGuiSetup(true);
    {
        // Докинг не включён по умолчанию — без этого окна нельзя стыковать как в Unity.
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.ConfigDockingWithShift = false;
    }

    std::string texDir = std::string(GetApplicationDirectory()) + "assets/textures";
    if (!DirectoryExists(texDir.c_str())) texDir = "assets/textures";
    knopmops::render::TextureManager::Instance().Init(texDir);

    knopmops::map::MapDocument doc;
    knopmops::scene::Scene scene("Level");
    knopmops::render::ViewportRenderer renderer;
    renderer.Init(1280, 720);

    knopmops::ui::EditorUI editorUI;
    editorUI.Init(&doc, &scene, &renderer);

    int exitCode = 0;
    if (build35) {
        exitCode = editorUI.RunBuildChapter35();
    } else if (test35) {
        exitCode = editorUI.RunSelfTest35();
    } else if (doExport) {
        exitCode = editorUI.RunExport(exportProject);
    } else if (selfTest) {
        exitCode = editorUI.RunSelfTest(selfTestProject);
    } else {
        int frame = 0;
        auto shot = [&](const char* name) {
            Image img = LoadImageFromScreen();
            ExportImage(img, (shotsDir + "/" + name).c_str());
            UnloadImage(img);
        };
        while (!WindowShouldClose() && !editorUI.ShouldExit()) {
            BeginDrawing();
            ClearBackground(Color{25, 25, 25, 255});
            rlImGuiBegin();
            editorUI.Render();
            rlImGuiEnd();
            ++frame;
            if (!shotsDir.empty()) {
                auto* ps = editorUI.DebugPlay();
                if (frame == 12) shot("editor.png");
                if (frame == 13) editorUI.DebugStartPlay();
                if (ps) {
                    if (frame == 220) shot("play_start.png");
                    if (frame == 221) { ps->TeleportPlayer({-3.0f, 0.0f, -1.5f}, 200.0f); ps->Player().pitch = -25.0f; }
                    if (frame == 260) shot("play_crate.png");
                    if (frame == 261) { ps->Player().hasFlashlight = true; ps->Player().flashlight = true; ps->TeleportPlayer({6.0f, 0.0f, 0.0f}, 90.0f); ps->Player().pitch = 0.0f; ps->GiveItem("item_crowbar", true); }
                    if (frame == 300) shot("play_corridor.png");
                    if (frame == 301) { ps->TeleportPlayer({15.0f, 0.0f, 0.0f}, 70.0f); }
                    if (frame == 340) { shot("play_roomB.png"); break; }
                }
            }
            EndDrawing();
        }
    }

    editorUI.Shutdown();
    renderer.Shutdown();
    knopmops::render::TextureManager::Instance().Shutdown();
    if (!selfTest && !doExport && !build35 && !test35) rlImGuiShutdown();
    CloseAudioDevice();
    CloseWindow();
    return exitCode;
}
