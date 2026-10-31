#include "engine/core/Engine.h"
#include "game/LastOrderGame.h"
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

    engine::core::Engine engine;
    if (!engine.Init(1280, 720, "Последний заказ — Кафе «Тёплый угол»", assetDir)) {
        return 1;
    }

    game::LastOrderGame game;
    engine.Run(game, smokeTest);
    engine.Shutdown();
    return 0;
}
