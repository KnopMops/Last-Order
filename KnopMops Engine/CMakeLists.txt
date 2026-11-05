cmake_minimum_required(VERSION 3.25)
project(knopmops_engine VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Raylib dependency
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/debug/_deps/raylib-src/CMakeLists.txt")
    set(raylib_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/debug/_deps/raylib-src")
    add_subdirectory("${raylib_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/raylib_build" EXCLUDE_FROM_ALL)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../Last Order/build/debug/_deps/raylib-src/CMakeLists.txt")
    set(raylib_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Last Order/build/debug/_deps/raylib-src")
    add_subdirectory("${raylib_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/raylib_build" EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

# nlohmann_json dependency
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/debug/_deps/json-src/CMakeLists.txt")
    set(json_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/debug/_deps/json-src")
    add_subdirectory("${json_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/json_build" EXCLUDE_FROM_ALL)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../Last Order/build/debug/_deps/json-src/CMakeLists.txt")
    set(json_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Last Order/build/debug/_deps/json-src")
    add_subdirectory("${json_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/json_build" EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

# Dear ImGui library (Docking branch)
add_library(imgui STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp"
)
target_include_directories(imgui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui"
)
target_compile_definitions(imgui PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
)

# rlImGui (Raylib Dear ImGui backend)
add_library(rlimgui STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rlImGui/rlImGui.cpp"
)
target_include_directories(rlimgui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rlImGui"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rlImGui/extras"
)
target_link_libraries(rlimgui PUBLIC
    imgui
    raylib
)

# Engine sources
file(GLOB_RECURSE ENGINE_SOURCES
    "src/*.cpp"
    "src/*.h"
)

add_executable(knopmops_engine ${ENGINE_SOURCES})
set_target_properties(knopmops_engine PROPERTIES OUTPUT_NAME "KnopMops Engine")

target_include_directories(knopmops_engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rlImGui"
)

target_link_libraries(knopmops_engine PRIVATE
    rlimgui
    imgui
    raylib
    nlohmann_json::nlohmann_json
    winmm
    gdi32
    opengl32
)

target_link_options(knopmops_engine PRIVATE -static -static-libgcc -static-libstdc++)

add_custom_command(TARGET knopmops_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:knopmops_engine>" "${CMAKE_CURRENT_SOURCE_DIR}/bin/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_CURRENT_SOURCE_DIR}/bin/assets"
    COMMENT "Deploying KnopMops Engine and assets to bin/..."
)

