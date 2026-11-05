#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>

struct lua_State;

namespace knopmops::runtime {

class PlaySession;

// Аргумент хука/события для Lua.
struct LuaArg {
    enum class Type { Nil, Number, String, Bool, Object } type = Type::Nil;
    double number = 0.0;
    std::string str;
    bool boolean = false;
    uint32_t objId = 0;

    LuaArg() = default;
    LuaArg(double v) : type(Type::Number), number(v) {}
    LuaArg(int v) : type(Type::Number), number(v) {}
    LuaArg(float v) : type(Type::Number), number(v) {}
    LuaArg(const std::string& v) : type(Type::String), str(v) {}
    LuaArg(const char* v) : type(Type::String), str(v) {}
    LuaArg(bool v) : type(Type::Bool), boolean(v) {}
    static LuaArg Object(uint32_t id) { LuaArg a; a.type = Type::Object; a.objId = id; return a; }
};

struct ExportVar {
    std::string name;
    std::string defaultValue;
    std::string type; // number | string | bool
};

// Lua 5.4: скрипты объектов (scripts/*.lua) + глобальный scripts/main.lua.
class ScriptHost {
public:
    explicit ScriptHost(PlaySession* session);
    ~ScriptHost();

    bool LoadMain();                    // scripts/main.lua (если есть); живёт всю главу
    void BindAllObjects();              // скрипты всех объектов текущей сцены + on_start
    void BindObject(uint32_t objId);    // для объектов, созданных во время игры
    void UnbindObject(uint32_t objId);
    void UnloadLevelScripts();          // перед сменой уровня

    void Update(float dt);              // on_update, таймеры, корутины

    // Вызывает хук объекта: on_interact, on_trigger_enter ...
    bool CallHook(uint32_t objId, const char* hook, const std::vector<LuaArg>& args = {});
    // Глобальное событие, подписанное через game.on(event, fn).
    void Emit(const char* event, const std::vector<LuaArg>& args = {});

    static bool CheckSyntax(const std::string& path, std::string& error);
    static bool CheckSyntaxText(const std::string& text, const std::string& chunkName, std::string& error);
    static std::vector<ExportVar> ReadExports(const std::string& path);

private:
    struct Instance {
        int envRef = -1;
        int selfRef = -1;
        std::string file;
        bool faulted = false;
    };
    struct Timer {
        int id = 0;
        int fnRef = -1;
        double wake = 0.0;
        double interval = 0.0; // 0 = один раз
        uint32_t owner = 0;
    };
    struct Routine {
        int threadRef = -1;
        lua_State* co = nullptr;
        uint32_t owner = 0;
        double wake = 0.0;
    };
    struct Handler { int fnRef; uint32_t owner; };

    friend struct LuaApi;

    bool CreateInstance(uint32_t key, const std::string& file, const std::map<std::string, std::string>* params, uint32_t objId);
    bool LoadChunkInto(const std::string& file, int envRef);
    bool PCall(int nargs, int nres, const std::string& what);
    void PushObject(uint32_t id, lua_State* st = nullptr);
    void PushArg(const LuaArg& a);
    void RegisterApi();
    void ReportError(const std::string& what, const std::string& msg);
    void StepRoutine(size_t index);

    PlaySession* S;
    lua_State* L = nullptr;
    std::unordered_map<uint32_t, Instance> m_instances; // ключ: id объекта; 0 = main.lua
    std::vector<Timer> m_timers;
    std::vector<Routine> m_routines;
    std::unordered_map<std::string, std::vector<Handler>> m_handlers;
    int m_nextTimerId = 1;
    bool m_inUpdate = false;
    uint32_t m_owner = 0; // ключ скрипта, который сейчас исполняется (0 = main.lua)
};

}
