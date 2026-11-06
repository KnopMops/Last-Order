#include "engine/core/Engine.h"
#include "game/LastOrderGame.h"
#include "game/GameVersion.h"
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    bool smokeTest = (argc > 1 && std::string(argv[1]) == "--smoke-test");

    std::string assetDir = LO_ASSET_DIR;

    if (argc > 0 && argv[0]) {
        std::error_code ec;
        std::filesystem::path exeDir = std::filesystem::absolute(argv[0], ec).parent_path();
        if (!ec && std::filesystem::exists(exeDir / "assets", ec)) {
            assetDir = (exeDir / "assets").string();
        }
    }

    if (!std::filesystem::exists(assetDir)) {
        if (std::filesystem::exists("assets")) {
            assetDir = std::filesystem::absolute("assets").string();
        } else if (std::filesystem::exists("release/assets")) {
            assetDir = std::filesystem::absolute("release/assets").string();
        }
    }

    std::string title = std::string("Последний заказ (Last Order) ") + game::GetVersionBuildString();
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    title += " [DEBUG BUILD]";
#endif

    engine::core::Engine engine;
    if (!engine.Init(1280, 720, title.c_str(), assetDir)) {
        return 1;
    }

    game::LastOrderGame game;
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--chapter2") game.RequestChapter2Tour("");
        if (arg == "--ch2-tour" && i + 1 < argc) game.RequestChapter2Tour(argv[i + 1]);
        if (arg == "--ch2-drive") game.RequestChapter2Tour("@drive");
        if (arg == "--chapter3") game.RequestChapter3(false, 0);
        if (arg == "--ch3-tour") game.RequestChapter3(true, 0);
        if (arg == "--ch3-hd") game.RequestChapter3(false, 1);
        if (arg == "--ch3-hd-tour") game.RequestChapter3(true, 1);
        if (arg == "--ch3-dx12-tour") game.RequestChapter3(true, 2);
        if (arg == "--ch3-dx12") game.RequestChapter3(false, 2);
        if (arg == "--ch3-holdout") game.RequestChapter3Holdout(2);
        if (arg == "--ch3-cutscene-tour") game.RequestChapter3CutsceneTour(2);
    }
#endif
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--chapter35") game.RequestChapter35(i + 1 < argc && argv[i + 1][0] != '-' ? argv[i + 1] : "");
        if (std::string(argv[i]) == "--chapter4") game.RequestChapter4();
    }
    engine.Run(game, smokeTest);
    engine.Shutdown();
    return 0;
}
