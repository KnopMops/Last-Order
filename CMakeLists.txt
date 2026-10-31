cmake_minimum_required(VERSION 3.25)
project(last_order LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raylib)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)
add_library(lo_engine STATIC
    src/engine/physics/PhysicsWorld.cpp
    src/engine/graphics/LightingSystem.cpp
    src/engine/graphics/Renderer.cpp
    src/engine/graphics/TextLayout.cpp
    src/engine/assets/AssetManager.cpp
    src/engine/audio/AudioBus.cpp
    src/engine/core/SettingsSystem.cpp
    src/engine/dialogue/DialogueSystem.cpp
    src/engine/dialogue/DialogueUI.cpp
    src/engine/core/Engine.cpp
)
target_include_directories(lo_engine PUBLIC src)
target_link_libraries(lo_engine PUBLIC raylib nlohmann_json::nlohmann_json)
set(APP_ICON_OBJ "${CMAKE_CURRENT_BINARY_DIR}/app_icon.obj")
add_custom_command(
    OUTPUT "${APP_ICON_OBJ}"
    COMMAND ${CMAKE_RC_COMPILER} -i "${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc" -o "${APP_ICON_OBJ}" -O coff
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc" "${CMAKE_CURRENT_SOURCE_DIR}/app.ico"
    COMMENT "Compiling application icon resource into COFF object..."
)
add_executable(last_order
    src/main.cpp
    "${APP_ICON_OBJ}"
    src/game/LastOrderGame.cpp
    src/game/world/CafeWorld.cpp
    src/game/ui/MainMenuScene.cpp
    src/game/ui/SettingsOverlay.cpp
    src/game/ui/InspectionOverlay.cpp
    src/game/systems/TutorialSystem.cpp
    src/game/systems/CookingSystem.cpp
    src/game/systems/SanitySystem.cpp
    src/game/systems/ChaseMinigame.cpp
    src/game/systems/EndlessDirector.cpp
)
set_target_properties(last_order PROPERTIES OUTPUT_NAME "Last Order")
target_link_libraries(last_order PRIVATE lo_engine)
target_link_options(last_order PRIVATE -static -static-libgcc -static-libstdc++)
target_compile_definitions(last_order PRIVATE LO_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
add_custom_command(TARGET last_order POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/release"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:last_order>" "${CMAKE_SOURCE_DIR}/release/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_SOURCE_DIR}/release/assets"
    COMMENT "Deploying last_order executable and assets to release/ directory..."
)
