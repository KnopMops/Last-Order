# KnopMops runtime: общая библиотека (сцена, карта, Lua-скрипты, PlaySession).
# Подключается и редактором (KnopMops Engine/CMakeLists.txt), и игрой (корневой CMakeLists.txt).
# Требует: цели raylib и nlohmann_json::nlohmann_json уже существуют.
include_guard(GLOBAL)
include(FetchContent)

if(NOT TARGET lua54)
    FetchContent_Declare(
        lua
        URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(lua)
    file(GLOB KM_LUA_SOURCES "${lua_SOURCE_DIR}/src/*.c")
    list(FILTER KM_LUA_SOURCES EXCLUDE REGEX ".*/(lua|luac)\.c$")
    add_library(lua54 STATIC ${KM_LUA_SOURCES})
    target_include_directories(lua54 PUBLIC "${lua_SOURCE_DIR}/src")
    set_target_properties(lua54 PROPERTIES LINKER_LANGUAGE C C_STANDARD 99)
endif()

get_filename_component(KM_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/../src" ABSOLUTE)

add_library(kmruntime STATIC
    "${KM_SRC_DIR}/map/BrushBuilder.cpp"
    "${KM_SRC_DIR}/map/MapData.cpp"
    "${KM_SRC_DIR}/map/MapParser.cpp"
    "${KM_SRC_DIR}/map/MapSerializer.cpp"
    "${KM_SRC_DIR}/scene/Scene.cpp"
    "${KM_SRC_DIR}/render/TextureManager.cpp"
    "${KM_SRC_DIR}/runtime/ChapterProject.cpp"
    "${KM_SRC_DIR}/runtime/ProjectTemplate.cpp"
    "${KM_SRC_DIR}/runtime/ScriptHost.cpp"
    "${KM_SRC_DIR}/runtime/PlaySession.cpp"
)
target_include_directories(kmruntime PUBLIC "${KM_SRC_DIR}")
target_link_libraries(kmruntime PUBLIC raylib nlohmann_json::nlohmann_json lua54)
