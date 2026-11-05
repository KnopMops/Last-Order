cmake_minimum_required(VERSION 3.25)
project(last_order VERSION 2.2.0 LANGUAGES C CXX)
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
set(USE_AVX2 OFF CACHE BOOL "" FORCE)
set(USE_AVX512 OFF CACHE BOOL "" FORCE)
set(USE_LZCNT OFF CACHE BOOL "" FORCE)
set(USE_TZCNT OFF CACHE BOOL "" FORCE)
set(USE_F16C OFF CACHE BOOL "" FORCE)
set(USE_FMADD OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    JoltPhysics
    URL https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.2.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(JoltPhysics)
target_compile_options(Jolt PRIVATE -O2)
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()
execute_process(
    COMMAND git rev-list --count HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_BUILD_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_BUILD_COUNT)
    set(GIT_BUILD_COUNT "0")
endif()
# Рантайм KnopMops Engine: сцена, Lua-скрипты, PlaySession (Глава 3.5 и последующие, собранные в редакторе)
include("${CMAKE_CURRENT_SOURCE_DIR}/KnopMops Engine/cmake/kmruntime.cmake")

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
    src/engine/3d/QuakeMap.cpp
    src/engine/3d/Physics3D.cpp
    src/engine/3d/Lighting3D.cpp
    src/engine/3d/LightBaker.cpp
    src/engine/3d/CameraFPS.cpp
    src/engine/hd/HdRendererD3D11.cpp
    src/engine/hd/HdRendererD3D12.cpp
)
target_include_directories(lo_engine PUBLIC src)
target_link_libraries(lo_engine PUBLIC raylib nlohmann_json::nlohmann_json Jolt kmruntime)
if(WIN32)
    target_link_libraries(lo_engine PUBLIC d3d11 d3d12 dxgi d3dcompiler dxguid)
endif()
set(APP_ICON_OBJ "${CMAKE_CURRENT_BINARY_DIR}/app_icon.obj")
add_custom_command(
    OUTPUT "${APP_ICON_OBJ}"
    COMMAND ${CMAKE_RC_COMPILER} -i "${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc" -o "${APP_ICON_OBJ}" -O coff
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc" "${CMAKE_CURRENT_SOURCE_DIR}/app.ico"
    COMMENT "Compiling application icon resource into COFF object..."
)
set(GAME_SOURCES
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
    src/game/systems/SaveSystem.cpp
    src/game/chapter2/MotelLevel.cpp
    src/game/chapter2/Chapter2Script.cpp
    src/game/chapter2/ItemVisuals.cpp
    src/game/chapter2/MotelWorld3D.cpp
    src/game/chapter2/MotelWorld3DAction.cpp
    src/game/chapter2/Vehicle3D.cpp
    src/game/chapter3/Cutscene3D.cpp
    src/game/chapter3/ItemRandomizer.cpp
    src/game/chapter3/WeaponAssembly.cpp
    src/game/chapter3/ProwlerAI.cpp
    src/game/chapter3/EscapeManager.cpp
    src/game/chapter3/HouseLevel.cpp
    src/game/chapter3/HouseHd.cpp
    src/game/chapter3/Ch3Graphics.cpp
    src/game/chapter3/HouseWorld3D.cpp
    src/game/custom/CustomChapter.cpp
    src/game/custom/HdPlayView.cpp
)
add_executable(last_order ${GAME_SOURCES})
set_target_properties(last_order PROPERTIES OUTPUT_NAME "Last Order")
target_link_libraries(last_order PRIVATE lo_engine)
target_link_options(last_order PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
target_compile_definitions(last_order PRIVATE LO_ENABLE_DEBUG=0 LO_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets" LO_GAME_VERSION="2.2.0" LO_COMMIT_HASH="${GIT_COMMIT_HASH}" LO_BUILD_ID="${GIT_BUILD_COUNT}")
add_executable(last_order_debug ${GAME_SOURCES})
set_target_properties(last_order_debug PROPERTIES OUTPUT_NAME "Last Order_debug")
target_link_libraries(last_order_debug PRIVATE lo_engine)
target_link_options(last_order_debug PRIVATE -static -static-libgcc -static-libstdc++)
target_compile_definitions(last_order_debug PRIVATE LO_ENABLE_DEBUG=1 LO_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets" LO_GAME_VERSION="2.2.0" LO_COMMIT_HASH="${GIT_COMMIT_HASH}" LO_BUILD_ID="${GIT_BUILD_COUNT}")
add_custom_command(TARGET last_order POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/release"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:last_order>" "${CMAKE_SOURCE_DIR}/release/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_SOURCE_DIR}/release/assets"
    COMMENT "Deploying last_order executable and assets to release/ directory..."
)
add_custom_command(TARGET last_order_debug POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/release"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:last_order_debug>" "${CMAKE_SOURCE_DIR}/release/"
    COMMENT "Deploying last_order_debug executable to release/ directory..."
)
