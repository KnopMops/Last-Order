#include "runtime/ScriptHost.h"
#include "core/Log.h"
#include "runtime/PlaySession.h"
#include "scene/Scene.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace knopmops::runtime {

namespace {

std::string ReadFileText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF) s.erase(0, 3);
    return s;
}

int MsgHandler(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    if (!msg) {
        if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING) return 1;
        msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1);
    return 1;
}

int KeyFromName(const char* name) {
    std::string n = name;
    for (auto& c : n) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (n.size() == 1 && n[0] >= 'A' && n[0] <= 'Z') return KEY_A + (n[0] - 'A');
    if (n.size() == 1 && n[0] >= '0' && n[0] <= '9') return KEY_ZERO + (n[0] - '0');
    if (n == "SPACE") return KEY_SPACE;
    if (n == "ENTER") return KEY_ENTER;
    if (n == "TAB") return KEY_TAB;
    if (n == "SHIFT") return KEY_LEFT_SHIFT;
    if (n == "CTRL") return KEY_LEFT_CONTROL;
    if (n == "ESC") return KEY_ESCAPE;
    if (n == "UP") return KEY_UP;
    if (n == "DOWN") return KEY_DOWN;
    if (n == "LEFT") return KEY_LEFT;
    if (n == "RIGHT") return KEY_RIGHT;
    return 0;
}

const char* ModeName(MonsterMode m) {
    switch (m) {
    case MonsterMode::Idle: return "idle";
    case MonsterMode::Patrol: return "patrol";
    case MonsterMode::Investigate: return "investigate";
    case MonsterMode::Chase: return "chase";
    case MonsterMode::Search: return "search";
    }
    return "idle";
}

}

// ---------------------------------------------------------------------------------------------
// Lua API
// ---------------------------------------------------------------------------------------------
struct LuaApi {
    static ScriptHost* H(lua_State* L) { return *static_cast<ScriptHost**>(lua_getextraspace(L)); }
    static PlaySession* S(lua_State* L) { return H(L)->S; }

    static uint32_t SelfId(lua_State* L, int idx) {
        if (!lua_istable(L, idx)) return 0;
        lua_getfield(L, idx, "id");
        uint32_t id = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        return id;
    }
    static scene::GameObject* Obj(lua_State* L, int idx) { return S(L)->GetScene().FindGameObject(SelfId(L, idx)); }

    static Vector3 WorldPos(lua_State* L, const scene::GameObject& o) {
        PlaySession* s = S(L);
        if (o.id == s->m_playerObj) return s->m_player.pos;
        if (MonsterState* m = s->FindMonster(o.id)) return m->pos;
        return s->GetScene().WorldPosition(o);
    }
    static void SetPos(lua_State* L, scene::GameObject& o, Vector3 p) {
        PlaySession* s = S(L);
        if (o.id == s->m_playerObj) { s->TeleportPlayer(p, s->m_player.yaw); return; }
        if (MonsterState* m = s->FindMonster(o.id)) { m->pos = p; m->mode = m->mode == MonsterMode::Chase ? MonsterMode::Search : m->mode; }
        s->GetScene().SetWorldPosition(o, p);
    }

    // ---- object methods ----
    static int O_get_position(lua_State* L) {
        auto* o = Obj(L, 1);
        if (!o) return 0;
        Vector3 p = WorldPos(L, *o);
        lua_pushnumber(L, p.x); lua_pushnumber(L, p.y); lua_pushnumber(L, p.z);
        return 3;
    }
    static int O_set_position(lua_State* L) {
        if (auto* o = Obj(L, 1)) SetPos(L, *o, Vector3{(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)});
        return 0;
    }
    static int O_get_rotation(lua_State* L) {
        auto* o = Obj(L, 1);
        lua_pushnumber(L, o ? o->transform.rotation.y : 0.0);
        return 1;
    }
    static int O_set_rotation(lua_State* L) {
        if (auto* o = Obj(L, 1)) {
            o->transform.rotation.y = (float)luaL_checknumber(L, 2);
            if (MonsterState* m = S(L)->FindMonster(o->id)) m->yaw = o->transform.rotation.y;
        }
        return 0;
    }
    static int O_set_active(lua_State* L) {
        S(L)->SetObjectActive(SelfId(L, 1), lua_toboolean(L, 2));
        return 0;
    }
    static int O_is_active(lua_State* L) {
        auto* o = Obj(L, 1);
        lua_pushboolean(L, o && o->active);
        return 1;
    }
    static int O_destroy(lua_State* L) {
        S(L)->DestroyObject(SelfId(L, 1));
        return 0;
    }
    static int O_exists(lua_State* L) {
        lua_pushboolean(L, Obj(L, 1) != nullptr);
        return 1;
    }
    static int O_distance_to_player(lua_State* L) {
        auto* o = Obj(L, 1);
        if (!o) { lua_pushnumber(L, 1e9); return 1; }
        Vector3 p = WorldPos(L, *o), pl = S(L)->m_player.pos;
        lua_pushnumber(L, std::sqrt((p.x - pl.x) * (p.x - pl.x) + (p.y - pl.y) * (p.y - pl.y) + (p.z - pl.z) * (p.z - pl.z)));
        return 1;
    }
    static int O_distance_to(lua_State* L) {
        auto* a = Obj(L, 1);
        auto* b = Obj(L, 2);
        if (!a || !b) { lua_pushnumber(L, 1e9); return 1; }
        Vector3 p = WorldPos(L, *a), q = WorldPos(L, *b);
        lua_pushnumber(L, std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z)));
        return 1;
    }
    static int O_get_prop(lua_State* L) {
        auto* o = Obj(L, 1);
        const char* k = luaL_checkstring(L, 2);
        if (o) {
            auto it = o->props.find(k);
            if (it != o->props.end()) {
                // числа и булевы возвращаем типизированно
                char* end = nullptr;
                double d = std::strtod(it->second.c_str(), &end);
                if (!it->second.empty() && end && *end == '\0') { lua_pushnumber(L, d); return 1; }
                if (it->second == "true" || it->second == "false") { lua_pushboolean(L, it->second == "true"); return 1; }
                lua_pushstring(L, it->second.c_str());
                return 1;
            }
        }
        if (lua_gettop(L) >= 3) { lua_pushvalue(L, 3); return 1; }
        lua_pushnil(L);
        return 1;
    }
    static int O_set_prop(lua_State* L) {
        auto* o = Obj(L, 1);
        const char* k = luaL_checkstring(L, 2);
        if (!o) return 0;
        if (lua_isboolean(L, 3)) o->props[k] = lua_toboolean(L, 3) ? "true" : "false";
        else if (lua_isnumber(L, 3)) { std::ostringstream ss; ss << lua_tonumber(L, 3); o->props[k] = ss.str(); }
        else if (lua_isnil(L, 3)) o->props.erase(k);
        else o->props[k] = luaL_tolstring(L, 3, nullptr);
        return 0;
    }
    static int O_lock(lua_State* L) {
        if (auto* o = Obj(L, 1)) { if (o->door.enabled) o->door.isLocked = true; if (o->searchSlot.enabled) o->searchSlot.isLocked = true; }
        return 0;
    }
    static int O_unlock(lua_State* L) {
        if (auto* o = Obj(L, 1)) { if (o->door.enabled) o->door.isLocked = false; if (o->searchSlot.enabled) o->searchSlot.isLocked = false; }
        return 0;
    }
    static int O_is_locked(lua_State* L) {
        auto* o = Obj(L, 1);
        lua_pushboolean(L, o && ((o->door.enabled && o->door.isLocked) || (o->searchSlot.enabled && o->searchSlot.isLocked)));
        return 1;
    }
    static int O_open(lua_State* L) { S(L)->OpenDoor(SelfId(L, 1), true); return 0; }
    static int O_close(lua_State* L) { S(L)->OpenDoor(SelfId(L, 1), false); return 0; }
    static int O_toggle(lua_State* L) {
        uint32_t id = SelfId(L, 1);
        S(L)->OpenDoor(id, !S(L)->Rt(id).doorOpen);
        return 0;
    }
    static int O_is_open(lua_State* L) { lua_pushboolean(L, S(L)->Rt(SelfId(L, 1)).doorOpen); return 1; }
    static int O_set_item(lua_State* L) {
        if (auto* o = Obj(L, 1)) { o->searchSlot.containedItem = luaL_optstring(L, 2, ""); S(L)->Rt(o->id).searched = false; }
        return 0;
    }
    static int O_get_item(lua_State* L) {
        auto* o = Obj(L, 1);
        lua_pushstring(L, o ? o->searchSlot.containedItem.c_str() : "");
        return 1;
    }
    static int O_set_light(lua_State* L) { S(L)->Rt(SelfId(L, 1)).lightOn = lua_toboolean(L, 2); return 0; }
    static int O_set_color(lua_State* L) {
        if (auto* o = Obj(L, 1)) {
            o->meshRenderer.tint = Color{(unsigned char)luaL_checkinteger(L, 2), (unsigned char)luaL_checkinteger(L, 3), (unsigned char)luaL_checkinteger(L, 4), 255};
            if (o->light.enabled) o->light.color = o->meshRenderer.tint;
        }
        return 0;
    }
    static int O_set_visible(lua_State* L) { S(L)->Rt(SelfId(L, 1)).visible = lua_toboolean(L, 2); return 0; }
    static int O_set_texture(lua_State* L) {
        if (auto* o = Obj(L, 1)) o->meshRenderer.textureName = luaL_checkstring(L, 2);
        return 0;
    }
    static int O_set_emissive(lua_State* L) {
        if (auto* o = Obj(L, 1)) o->meshRenderer.emissive = lua_toboolean(L, 2);
        return 0;
    }
    static int O_set_interactable(lua_State* L) {
        if (auto* o = Obj(L, 1)) { o->script.interactable = lua_toboolean(L, 2); if (lua_isstring(L, 3)) o->script.interactPrompt = lua_tostring(L, 3); }
        return 0;
    }
    static int O_set_mode(lua_State* L) {
        if (MonsterState* m = S(L)->FindMonster(SelfId(L, 1))) {
            std::string n = luaL_checkstring(L, 2);
            MonsterMode old = m->mode;
            if (n == "idle") m->mode = MonsterMode::Idle;
            else if (n == "patrol") m->mode = MonsterMode::Patrol;
            else if (n == "investigate") m->mode = MonsterMode::Investigate;
            else if (n == "chase") m->mode = MonsterMode::Chase;
            else if (n == "search") m->mode = MonsterMode::Search;
            if (old != m->mode) m->lastSeen = S(L)->m_player.pos;
        }
        return 0;
    }
    static int O_get_mode(lua_State* L) {
        MonsterState* m = S(L)->FindMonster(SelfId(L, 1));
        if (!m) return 0;
        lua_pushstring(L, ModeName(m->mode));
        return 1;
    }
    static int O_freeze(lua_State* L) {
        if (MonsterState* m = S(L)->FindMonster(SelfId(L, 1))) m->frozen = lua_toboolean(L, 2);
        return 0;
    }
    static int O_set_speed(lua_State* L) {
        if (auto* o = Obj(L, 1)) {
            if (o->aiSpawn.enabled) {
                o->aiSpawn.walkSpeed = (float)luaL_checknumber(L, 2);
                o->aiSpawn.chaseSpeed = (float)luaL_optnumber(L, 3, o->aiSpawn.chaseSpeed);
            }
        }
        return 0;
    }
    static int O_add_force(lua_State* L) {
        if (auto* o = Obj(L, 1)) {
            float m = std::max(0.05f, o->rigidbody.mass);
            ObjRuntime& rt = S(L)->Rt(o->id);
            rt.vel.x += (float)luaL_checknumber(L, 2) / m;
            rt.vel.y += (float)luaL_checknumber(L, 3) / m;
            rt.vel.z += (float)luaL_checknumber(L, 4) / m;
        }
        return 0;
    }
    static int O_set_velocity(lua_State* L) {
        if (auto* o = Obj(L, 1)) S(L)->Rt(o->id).vel = Vector3{(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)};
        return 0;
    }
    static int O_get_velocity(lua_State* L) {
        Vector3 v = S(L)->Rt(SelfId(L, 1)).vel;
        lua_pushnumber(L, v.x); lua_pushnumber(L, v.y); lua_pushnumber(L, v.z);
        return 3;
    }
    static int O_is_grounded(lua_State* L) { lua_pushboolean(L, S(L)->Rt(SelfId(L, 1)).grounded); return 1; }
    // obj:walk(dx, dz) — сместить объект с учётом стен и коллайдеров (радиус 0.35). Возвращает true при столкновении.
    static int O_walk(lua_State* L) {
        bool hit = S(L)->MoveObjectCollide(SelfId(L, 1), Vector3{(float)luaL_checknumber(L, 2), 0.0f, (float)luaL_checknumber(L, 3)}, (float)luaL_optnumber(L, 4, 0.35));
        lua_pushboolean(L, hit);
        return 1;
    }
    // obj:walk_to(x, z, скорость, dt) — идти к точке по кратчайшему пути с обходом стен. Возвращает оставшееся расстояние.
    static int O_walk_to(lua_State* L) {
        float rem = S(L)->NavMove(SelfId(L, 1), Vector3{(float)luaL_checknumber(L, 2), 0.0f, (float)luaL_checknumber(L, 3)}, (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5));
        lua_pushnumber(L, rem);
        return 1;
    }
    static int O_visible_to_player(lua_State* L) { lua_pushboolean(L, S(L)->VisibleToPlayer(SelfId(L, 1))); return 1; }
    static int O_investigate(lua_State* L) {
        if (MonsterState* m = S(L)->FindMonster(SelfId(L, 1))) {
            if (m->mode != MonsterMode::Chase) {
                m->mode = MonsterMode::Investigate;
                m->target = Vector3{(float)luaL_checknumber(L, 2), m->pos.y, (float)luaL_checknumber(L, 4)};
            }
        }
        return 0;
    }

    // ---- object metatable ----
    static int Obj_index(lua_State* L) {
        lua_pushvalue(L, 2);
        lua_gettable(L, lua_upvalueindex(1));
        if (!lua_isnil(L, -1)) return 1;
        lua_pop(L, 1);
        if (lua_type(L, 2) != LUA_TSTRING) { lua_pushnil(L); return 1; }
        std::string k = lua_tostring(L, 2);
        auto* o = Obj(L, 1);
        if (!o) { lua_pushnil(L); return 1; }
        if (k == "name") lua_pushstring(L, o->name.c_str());
        else if (k == "tag") lua_pushstring(L, o->tag.c_str());
        else if (k == "active") lua_pushboolean(L, o->active);
        else if (k == "x") lua_pushnumber(L, WorldPos(L, *o).x);
        else if (k == "y") lua_pushnumber(L, WorldPos(L, *o).y);
        else if (k == "z") lua_pushnumber(L, WorldPos(L, *o).z);
        else if (k == "yaw") lua_pushnumber(L, o->transform.rotation.y);
        else lua_pushnil(L);
        return 1;
    }
    static int Obj_newindex(lua_State* L) {
        if (lua_type(L, 2) == LUA_TSTRING) {
            std::string k = lua_tostring(L, 2);
            auto* o = Obj(L, 1);
            if (o) {
                if (k == "name") { o->name = luaL_checkstring(L, 3); return 0; }
                if (k == "tag") { o->tag = luaL_checkstring(L, 3); return 0; }
                if (k == "active") { S(L)->SetObjectActive(o->id, lua_toboolean(L, 3)); return 0; }
                if (k == "yaw") { o->transform.rotation.y = (float)luaL_checknumber(L, 3); return 0; }
                if (k == "x" || k == "y" || k == "z") {
                    Vector3 p = WorldPos(L, *o);
                    float v = (float)luaL_checknumber(L, 3);
                    if (k == "x") p.x = v; else if (k == "y") p.y = v; else p.z = v;
                    SetPos(L, *o, p);
                    return 0;
                }
            }
        }
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_rawset(L, 1);
        return 0;
    }

    // ---- game.* ----
    static int G_notify(lua_State* L) { S(L)->Notify(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 3.0)); return 0; }
    static int G_subtitle(lua_State* L) { S(L)->Subtitle(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 4.0)); return 0; }
    static int G_log(lua_State* L) {
        int n = lua_gettop(L);
        std::string out;
        for (int i = 1; i <= n; ++i) {
            size_t len = 0;
            const char* s = luaL_tolstring(L, i, &len);
            if (i > 1) out += "\t";
            out.append(s, len);
            lua_pop(L, 1);
        }
        core::Log::Info("[lua] " + out);
        return 0;
    }
    static int G_has_item(lua_State* L) { lua_pushboolean(L, S(L)->HasItem(luaL_checkstring(L, 1))); return 1; }
    static int G_give_item(lua_State* L) { lua_pushboolean(L, S(L)->GiveItem(luaL_checkstring(L, 1), lua_toboolean(L, 2))); return 1; }
    static int G_take_item(lua_State* L) { lua_pushboolean(L, S(L)->TakeItem(luaL_checkstring(L, 1))); return 1; }
    static int G_items(lua_State* L) {
        const auto& inv = S(L)->Inventory();
        lua_createtable(L, (int)inv.size(), 0);
        for (size_t i = 0; i < inv.size(); ++i) { lua_pushstring(L, inv[i].c_str()); lua_rawseti(L, -2, (lua_Integer)i + 1); }
        return 1;
    }
    static int G_set(lua_State* L) {
        Var v;
        if (lua_isboolean(L, 2)) { v.type = Var::Type::Bool; v.boolean = lua_toboolean(L, 2); }
        else if (lua_type(L, 2) == LUA_TNUMBER) { v.type = Var::Type::Number; v.number = lua_tonumber(L, 2); }
        else if (lua_type(L, 2) == LUA_TSTRING) { v.type = Var::Type::String; v.str = lua_tostring(L, 2); }
        S(L)->SetVar(luaL_checkstring(L, 1), v);
        return 0;
    }
    static int G_get(lua_State* L) {
        Var v = S(L)->GetVar(luaL_checkstring(L, 1));
        switch (v.type) {
        case Var::Type::Bool: lua_pushboolean(L, v.boolean); break;
        case Var::Type::Number: lua_pushnumber(L, v.number); break;
        case Var::Type::String: lua_pushstring(L, v.str.c_str()); break;
        default:
            if (lua_gettop(L) >= 2) lua_pushvalue(L, 2); else lua_pushnil(L);
        }
        return 1;
    }
    static int G_get_flag(lua_State* L) {
        Var v = S(L)->GetVar(luaL_checkstring(L, 1));
        lua_pushboolean(L, v.type == Var::Type::Bool ? v.boolean : (v.type == Var::Type::Number ? v.number != 0.0 : false));
        return 1;
    }
    static int G_play_sound(lua_State* L) { S(L)->PlaySfx(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 1.0)); return 0; }
    static int G_play_ambient(lua_State* L) { S(L)->PlayAmbient(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 0.5)); return 0; }
    static int G_stop_ambient(lua_State* L) { S(L)->StopAmbient(); return 0; }
    static int G_find(lua_State* L) {
        if (auto* o = S(L)->GetScene().FindByName(luaL_checkstring(L, 1))) H(L)->PushObject(o->id, L);
        else lua_pushnil(L);
        return 1;
    }
    static int G_find_all(lua_State* L) {
        std::string tag = luaL_checkstring(L, 1);
        lua_newtable(L);
        int n = 0;
        for (const auto& o : S(L)->GetScene().GetGameObjects()) {
            if (o->tag == tag) { H(L)->PushObject(o->id, L); lua_rawseti(L, -2, ++n); }
        }
        return 1;
    }
    static int G_player(lua_State* L) { H(L)->PushObject(S(L)->m_playerObj, L); return 1; }
    static int G_player_pos(lua_State* L) {
        Vector3 p = S(L)->m_player.pos;
        lua_pushnumber(L, p.x); lua_pushnumber(L, p.y); lua_pushnumber(L, p.z);
        return 3;
    }
    static int G_teleport_player(lua_State* L) {
        S(L)->TeleportPlayer(Vector3{(float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)}, (float)luaL_optnumber(L, 4, S(L)->m_player.yaw));
        return 0;
    }
    static int G_lock_player(lua_State* L) { S(L)->m_player.locked = lua_toboolean(L, 1); return 0; }
    static int G_freeze_movement(lua_State* L) { S(L)->m_player.moveLocked = lua_toboolean(L, 1); return 0; }
    static int G_player_yaw(lua_State* L) { lua_pushnumber(L, S(L)->m_player.yaw); return 1; }
    static int G_player_pitch(lua_State* L) { lua_pushnumber(L, S(L)->m_player.pitch); return 1; }
    static int G_set_battery(lua_State* L) { S(L)->m_player.battery = (float)luaL_checknumber(L, 1); if (S(L)->m_player.battery <= 0) S(L)->m_player.flashlight = false; return 0; }
    static int G_player_hidden(lua_State* L) { lua_pushboolean(L, S(L)->m_player.hiddenIn != 0); return 1; }
    static int G_enable_flashlight(lua_State* L) { S(L)->m_player.hasFlashlight = lua_toboolean(L, 1); if (!S(L)->m_player.hasFlashlight) S(L)->m_player.flashlight = false; return 0; }
    static int G_spawn(lua_State* L) {
        auto* o = S(L)->Spawn(luaL_checkstring(L, 1), Vector3{(float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0), (float)luaL_optnumber(L, 4, 0)});
        if (o) H(L)->PushObject(o->id, L); else lua_pushnil(L);
        return 1;
    }
    static int G_raycast(lua_State* L) {
        float d = 0; std::string name;
        Vector3 o{(float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)};
        Vector3 dir{(float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6)};
        float maxD = (float)luaL_optnumber(L, 7, 100.0);
        if (!S(L)->Raycast(o, dir, maxD, &d, &name)) { lua_pushboolean(L, 0); return 1; }
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        lua_pushboolean(L, 1);
        lua_pushnumber(L, d);
        lua_pushnumber(L, o.x + dir.x / len * d); lua_pushnumber(L, o.y + dir.y / len * d); lua_pushnumber(L, o.z + dir.z / len * d);
        lua_pushstring(L, name.c_str());
        return 6;
    }
    static int G_make_noise(lua_State* L) {
        S(L)->MakeNoise(Vector3{(float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)}, (float)luaL_optnumber(L, 4, 8.0));
        return 0;
    }
    static int G_alert_monsters(lua_State* L) {
        S(L)->AlertMonsters(Vector3{(float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)});
        return 0;
    }
    static int G_load_level(lua_State* L) { S(L)->LoadLevel(luaL_checkstring(L, 1), luaL_optstring(L, 2, "")); return 0; }
    static int G_level(lua_State* L) { lua_pushstring(L, S(L)->m_level.c_str()); return 1; }
    static int G_win(lua_State* L) { S(L)->Win(luaL_optstring(L, 1, ""), luaL_optstring(L, 2, "")); return 0; }
    static int G_lose(lua_State* L) { S(L)->Lose(luaL_optstring(L, 1, "")); return 0; }
    static int G_fade(lua_State* L) { S(L)->Fade((float)luaL_checknumber(L, 1), (float)luaL_optnumber(L, 2, 1.0)); return 0; }
    static int G_shake(lua_State* L) { S(L)->Shake((float)luaL_optnumber(L, 1, 0.3), (float)luaL_optnumber(L, 2, 0.5)); return 0; }
    static int G_cutscene_camera(lua_State* L) {
        S(L)->SetCutsceneCamera(true, Vector3{(float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)},
                                Vector3{(float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6)});
        return 0;
    }
    static int G_cutscene_camera_end(lua_State* L) { S(L)->SetCutsceneCamera(false); return 0; }
    static int G_play_tv(lua_State* L) {
        int tape = (int)luaL_checkinteger(L, 1);
        float dur = (float)luaL_optnumber(L, 2, 12.0);
        S(L)->PlayTvVideo(tape, dur);
        return 0;
    }
    static int G_stop_tv(lua_State* L) {
        S(L)->StopTvVideo();
        return 0;
    }
    static int G_is_tv_playing(lua_State* L) {
        lua_pushboolean(L, S(L)->IsTvVideoPlaying());
        return 1;
    }
    static int G_time(lua_State* L) { lua_pushnumber(L, S(L)->Time()); return 1; }
    static int G_distance(lua_State* L) {
        double dx = luaL_checknumber(L, 1) - luaL_checknumber(L, 4), dy = luaL_checknumber(L, 2) - luaL_checknumber(L, 5), dz = luaL_checknumber(L, 3) - luaL_checknumber(L, 6);
        lua_pushnumber(L, std::sqrt(dx * dx + dy * dy + dz * dz));
        return 1;
    }
    static int G_key_pressed(lua_State* L) {
        int k = KeyFromName(luaL_checkstring(L, 1));
        lua_pushboolean(L, S(L)->m_inputActive && k != 0 && IsKeyPressed(k));
        return 1;
    }
    static int G_key_down(lua_State* L) {
        int k = KeyFromName(luaL_checkstring(L, 1));
        lua_pushboolean(L, S(L)->m_inputActive && k != 0 && IsKeyDown(k));
        return 1;
    }
    static int G_ask_code(lua_State* L) {
        std::string prompt = luaL_checkstring(L, 1);
        std::string expected = luaL_checkstring(L, 2);
        luaL_checktype(L, 3, LUA_TFUNCTION);
        lua_pushvalue(L, 3);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        ScriptHost* h = H(L);
        S(L)->AskCode(prompt, expected, [h, ref](bool ok, const std::string& entered) {
            lua_State* L2 = h->L;
            lua_rawgeti(L2, LUA_REGISTRYINDEX, ref);
            lua_pushboolean(L2, ok);
            lua_pushstring(L2, entered.c_str());
            h->PCall(2, 0, "ask_code");
            luaL_unref(L2, LUA_REGISTRYINDEX, ref);
        });
        return 0;
    }

    static int G_after(lua_State* L) {
        double secs = luaL_checknumber(L, 1);
        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        ScriptHost::Timer t;
        t.fnRef = luaL_ref(L, LUA_REGISTRYINDEX);
        t.id = H(L)->m_nextTimerId++;
        t.wake = S(L)->Time() + secs;
        t.owner = H(L)->m_owner;
        H(L)->m_timers.push_back(t);
        lua_pushinteger(L, t.id);
        return 1;
    }
    static int G_every(lua_State* L) {
        double secs = luaL_checknumber(L, 1);
        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        ScriptHost::Timer t;
        t.fnRef = luaL_ref(L, LUA_REGISTRYINDEX);
        t.id = H(L)->m_nextTimerId++;
        t.interval = std::max(0.01, secs);
        t.wake = S(L)->Time() + t.interval;
        t.owner = H(L)->m_owner;
        H(L)->m_timers.push_back(t);
        lua_pushinteger(L, t.id);
        return 1;
    }
    static int G_cancel(lua_State* L) {
        int id = (int)luaL_checkinteger(L, 1);
        for (auto& t : H(L)->m_timers) {
            if (t.id == id) { t.interval = 0.0; t.wake = -1.0; luaL_unref(L, LUA_REGISTRYINDEX, t.fnRef); t.fnRef = -1; }
        }
        return 0;
    }
    static int G_start(lua_State* L) {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        lua_State* co = lua_newthread(L);
        ScriptHost::Routine r;
        r.threadRef = luaL_ref(L, LUA_REGISTRYINDEX);
        r.co = co;
        r.owner = H(L)->m_owner;
        r.wake = S(L)->Time();
        lua_pushvalue(L, 1);
        lua_xmove(L, co, 1);
        H(L)->m_routines.push_back(r);
        H(L)->StepRoutine(H(L)->m_routines.size() - 1);
        return 0;
    }
    static int G_wait(lua_State* L) {
        double s = luaL_optnumber(L, 1, 0.0);
        if (!lua_isyieldable(L)) return luaL_error(L, "game.wait можно вызывать только внутри game.start(function() ... end)");
        lua_pushnumber(L, s);
        return lua_yield(L, 1);
    }
    static int G_on(lua_State* L) {
        std::string ev = luaL_checkstring(L, 1);
        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        H(L)->m_handlers[ev].push_back({ref, H(L)->m_owner});
        return 0;
    }
    static int G_print(lua_State* L) { return G_log(L); }
    static int NoopFn(lua_State*) { return 0; }
};

// ---------------------------------------------------------------------------------------------

ScriptHost::ScriptHost(PlaySession* session) : S(session) {
    L = luaL_newstate();
    *static_cast<ScriptHost**>(lua_getextraspace(L)) = this;

    luaL_requiref(L, "_G", luaopen_base, 1); lua_pop(L, 1);
    luaL_requiref(L, "coroutine", luaopen_coroutine, 1); lua_pop(L, 1);
    luaL_requiref(L, "table", luaopen_table, 1); lua_pop(L, 1);
    luaL_requiref(L, "string", luaopen_string, 1); lua_pop(L, 1);
    luaL_requiref(L, "math", luaopen_math, 1); lua_pop(L, 1);
    luaL_requiref(L, "utf8", luaopen_utf8, 1); lua_pop(L, 1);
    for (const char* name : {"dofile", "loadfile", "require", "collectgarbage"}) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
    RegisterApi();
}

ScriptHost::~ScriptHost() {
    if (L) lua_close(L);
}

void ScriptHost::RegisterApi() {
    // методы объекта
    static const luaL_Reg methods[] = {
        {"get_position", LuaApi::O_get_position}, {"set_position", LuaApi::O_set_position},
        {"get_rotation", LuaApi::O_get_rotation}, {"set_rotation", LuaApi::O_set_rotation},
        {"set_active", LuaApi::O_set_active}, {"is_active", LuaApi::O_is_active},
        {"destroy", LuaApi::O_destroy}, {"exists", LuaApi::O_exists},
        {"distance_to_player", LuaApi::O_distance_to_player}, {"distance_to", LuaApi::O_distance_to},
        {"get_prop", LuaApi::O_get_prop}, {"set_prop", LuaApi::O_set_prop},
        {"lock", LuaApi::O_lock}, {"unlock", LuaApi::O_unlock}, {"is_locked", LuaApi::O_is_locked},
        {"open", LuaApi::O_open}, {"close", LuaApi::O_close}, {"toggle", LuaApi::O_toggle}, {"is_open", LuaApi::O_is_open},
        {"set_item", LuaApi::O_set_item}, {"get_item", LuaApi::O_get_item},
        {"set_light", LuaApi::O_set_light}, {"set_color", LuaApi::O_set_color}, {"set_visible", LuaApi::O_set_visible},
        {"set_texture", LuaApi::O_set_texture}, {"set_emissive", LuaApi::O_set_emissive},
        {"set_interactable", LuaApi::O_set_interactable},
        {"set_mode", LuaApi::O_set_mode}, {"get_mode", LuaApi::O_get_mode}, {"freeze", LuaApi::O_freeze},
        {"walk", LuaApi::O_walk}, {"walk_to", LuaApi::O_walk_to}, {"visible_to_player", LuaApi::O_visible_to_player}, {"add_force", LuaApi::O_add_force}, {"set_velocity", LuaApi::O_set_velocity}, {"get_velocity", LuaApi::O_get_velocity}, {"is_grounded", LuaApi::O_is_grounded},
        {"set_speed", LuaApi::O_set_speed}, {"investigate", LuaApi::O_investigate},
        {nullptr, nullptr}};
    lua_newtable(L);
    luaL_setfuncs(L, methods, 0);
    int methodsIdx = lua_gettop(L);

    luaL_newmetatable(L, "km.obj");
    lua_pushvalue(L, methodsIdx);
    lua_pushcclosure(L, LuaApi::Obj_index, 1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, LuaApi::Obj_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, [](lua_State* L2) -> int {
        lua_getfield(L2, 1, "name");
        lua_pushfstring(L2, "Object(%s)", lua_tostring(L2, -1));
        return 1;
    });
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, [](lua_State* L2) -> int {
        lua_pushboolean(L2, LuaApi::SelfId(L2, 1) == LuaApi::SelfId(L2, 2));
        return 1;
    });
    lua_setfield(L, -2, "__eq");
    lua_pop(L, 2);

    static const luaL_Reg api[] = {
        {"notify", LuaApi::G_notify}, {"subtitle", LuaApi::G_subtitle}, {"log", LuaApi::G_log},
        {"has_item", LuaApi::G_has_item}, {"give_item", LuaApi::G_give_item}, {"take_item", LuaApi::G_take_item}, {"items", LuaApi::G_items},
        {"set", LuaApi::G_set}, {"get", LuaApi::G_get}, {"set_flag", LuaApi::G_set}, {"get_flag", LuaApi::G_get_flag},
        {"play_sound", LuaApi::G_play_sound}, {"play_ambient", LuaApi::G_play_ambient}, {"stop_ambient", LuaApi::G_stop_ambient},
        {"find", LuaApi::G_find}, {"find_all", LuaApi::G_find_all}, {"spawn", LuaApi::G_spawn},
        {"player", LuaApi::G_player}, {"player_pos", LuaApi::G_player_pos}, {"teleport_player", LuaApi::G_teleport_player},
        {"lock_player", LuaApi::G_lock_player}, {"freeze_movement", LuaApi::G_freeze_movement}, {"player_yaw", LuaApi::G_player_yaw}, {"player_pitch", LuaApi::G_player_pitch}, {"set_battery", LuaApi::G_set_battery}, {"player_hidden", LuaApi::G_player_hidden}, {"enable_flashlight", LuaApi::G_enable_flashlight},
        {"raycast", LuaApi::G_raycast}, {"make_noise", LuaApi::G_make_noise}, {"alert_monsters", LuaApi::G_alert_monsters},
        {"load_level", LuaApi::G_load_level}, {"level", LuaApi::G_level}, {"win", LuaApi::G_win}, {"lose", LuaApi::G_lose},
        {"fade", LuaApi::G_fade}, {"shake", LuaApi::G_shake},
        {"cutscene_camera", LuaApi::G_cutscene_camera}, {"cutscene_camera_end", LuaApi::G_cutscene_camera_end},
        {"play_tv", LuaApi::G_play_tv}, {"stop_tv", LuaApi::G_stop_tv}, {"is_tv_playing", LuaApi::G_is_tv_playing},
        {"time", LuaApi::G_time}, {"distance", LuaApi::G_distance},
        {"key_pressed", LuaApi::G_key_pressed}, {"key_down", LuaApi::G_key_down}, {"ask_code", LuaApi::G_ask_code},
        {"after", LuaApi::G_after}, {"every", LuaApi::G_every}, {"cancel", LuaApi::G_cancel},
        {"start", LuaApi::G_start}, {"wait", LuaApi::G_wait}, {"on", LuaApi::G_on},
        {nullptr, nullptr}};
    lua_newtable(L);
    luaL_setfuncs(L, api, 0);
    lua_setglobal(L, "game");

    lua_pushcfunction(L, LuaApi::G_print);
    lua_setglobal(L, "print");
}

void ScriptHost::PushObject(uint32_t id, lua_State* st) {
    lua_State* T = st ? st : L; // вызов из корутины должен класть объект на стек корутины
    lua_createtable(T, 0, 2);
    lua_pushinteger(T, id);
    lua_setfield(T, -2, "id");
    luaL_getmetatable(T, "km.obj");
    lua_setmetatable(T, -2);
}

void ScriptHost::PushArg(const LuaArg& a) {
    switch (a.type) {
    case LuaArg::Type::Number: lua_pushnumber(L, a.number); break;
    case LuaArg::Type::String: lua_pushstring(L, a.str.c_str()); break;
    case LuaArg::Type::Bool: lua_pushboolean(L, a.boolean); break;
    case LuaArg::Type::Object: PushObject(a.objId); break;
    default: lua_pushnil(L); break;
    }
}

void ScriptHost::ReportError(const std::string& what, const std::string& msg) {
    core::Log::Error("[Lua] " + what + ": " + msg);
}

bool ScriptHost::PCall(int nargs, int nres, const std::string& what) {
    int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, MsgHandler);
    lua_insert(L, base);
    int rc = lua_pcall(L, nargs, nres, base);
    lua_remove(L, base);
    if (rc != LUA_OK) {
        const char* m = lua_tostring(L, -1);
        ReportError(what, m ? m : "?");
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool ScriptHost::LoadChunkInto(const std::string& file, int envRef) {
    std::string path = file == "main.lua" ? S->Project().ScriptPath("main.lua") : S->Project().ScriptPath(file);
    std::string text = ReadFileText(path);
    if (text.empty()) {
        ReportError(file, "файл не найден или пуст: " + path);
        return false;
    }
    std::string chunkName = "@scripts/" + file;
    if (luaL_loadbufferx(L, text.data(), text.size(), chunkName.c_str(), "t") != LUA_OK) {
        ReportError(file, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, envRef);
    lua_setupvalue(L, -2, 1);
    return PCall(0, 0, file);
}

bool ScriptHost::CreateInstance(uint32_t key, const std::string& file, const std::map<std::string, std::string>* params, uint32_t objId) {
    Instance inst;
    inst.file = file;

    lua_newtable(L);
    lua_newtable(L);
    lua_pushglobaltable(L);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    inst.envRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);

    if (objId != 0) {
        PushObject(objId);
        lua_pushvalue(L, -1);
        inst.selfRef = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_pop(L, 1);
    }

    uint32_t prevOwner = m_owner;
    m_owner = key;
    bool ok = LoadChunkInto(file, inst.envRef);
    m_owner = prevOwner;
    if (!ok) inst.faulted = true;

    // параметры из exports + значения, заданные в инспекторе
    if (ok && objId != 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, inst.selfRef);
        lua_newtable(L); // params
        lua_rawgeti(L, LUA_REGISTRYINDEX, inst.envRef);
        lua_getfield(L, -1, "exports");
        std::vector<std::pair<std::string, int>> typed; // имя -> lua type
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                if (lua_type(L, -2) == LUA_TSTRING) {
                    std::string name = lua_tostring(L, -2);
                    int t = lua_type(L, -1);
                    lua_pushvalue(L, -1);
                    lua_setfield(L, -6, name.c_str()); // params[name] = default
                    typed.emplace_back(name, t);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 2); // exports, env
        if (params) {
            for (const auto& kv : *params) {
                int t = LUA_TSTRING;
                for (auto& tp : typed) if (tp.first == kv.first) t = tp.second;
                if (t == LUA_TNUMBER) lua_pushnumber(L, std::atof(kv.second.c_str()));
                else if (t == LUA_TBOOLEAN) lua_pushboolean(L, kv.second == "true" || kv.second == "1");
                else lua_pushstring(L, kv.second.c_str());
                lua_setfield(L, -2, kv.first.c_str());
            }
        }
        lua_setfield(L, -2, "params");
        lua_pop(L, 1); // self
    }

    m_instances[key] = inst;
    return ok;
}

bool ScriptHost::LoadMain() {
    std::string path = S->Project().ScriptPath("main.lua");
    std::ifstream f(path);
    if (!f.good()) return true;
    return CreateInstance(0, "main.lua", nullptr, 0);
}

void ScriptHost::BindObject(uint32_t objId) {
    auto* o = S->GetScene().FindGameObject(objId);
    if (!o || !o->script.enabled || o->script.scriptFile.empty()) return;
    CreateInstance(objId, o->script.scriptFile, &o->script.parameters, objId);
}

void ScriptHost::BindAllObjects() {
    std::vector<uint32_t> ids;
    for (const auto& o : S->GetScene().GetGameObjects()) {
        if (o->script.enabled && !o->script.scriptFile.empty()) ids.push_back(o->id);
    }
    for (uint32_t id : ids) BindObject(id);
    for (uint32_t id : ids) CallHook(id, "on_start");
}

void ScriptHost::UnbindObject(uint32_t objId) {
    auto it = m_instances.find(objId);
    if (it == m_instances.end()) return;
    luaL_unref(L, LUA_REGISTRYINDEX, it->second.envRef);
    if (it->second.selfRef >= 0) luaL_unref(L, LUA_REGISTRYINDEX, it->second.selfRef);
    m_instances.erase(it);
}

void ScriptHost::UnloadLevelScripts() {
    std::vector<uint32_t> keys;
    for (auto& kv : m_instances) if (kv.first != 0) keys.push_back(kv.first);
    for (uint32_t k : keys) UnbindObject(k);

    for (auto& kv : m_handlers) {
        auto& v = kv.second;
        for (auto& h : v) if (h.owner != 0) luaL_unref(L, LUA_REGISTRYINDEX, h.fnRef);
        v.erase(std::remove_if(v.begin(), v.end(), [](const Handler& h) { return h.owner != 0; }), v.end());
    }
    for (auto& t : m_timers) {
        if (t.owner != 0 && t.fnRef >= 0) { luaL_unref(L, LUA_REGISTRYINDEX, t.fnRef); t.fnRef = -1; t.wake = -1; t.interval = 0; }
    }
    for (auto& r : m_routines) {
        if (r.owner != 0 && r.threadRef >= 0) { luaL_unref(L, LUA_REGISTRYINDEX, r.threadRef); r.threadRef = -2; }
    }
    m_routines.erase(std::remove_if(m_routines.begin(), m_routines.end(), [](const Routine& r) { return r.threadRef == -2; }), m_routines.end());
}

bool ScriptHost::CallHook(uint32_t objId, const char* hook, const std::vector<LuaArg>& args) {
    auto it = m_instances.find(objId);
    if (it == m_instances.end() || it->second.faulted) return false;
    Instance& inst = it->second;

    lua_rawgeti(L, LUA_REGISTRYINDEX, inst.envRef);
    lua_getfield(L, -1, hook);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    lua_remove(L, -2);

    int nargs = 0;
    if (inst.selfRef >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, inst.selfRef);
        ++nargs;
    }
    for (const auto& a : args) { PushArg(a); ++nargs; }

    uint32_t prevOwner = m_owner;
    m_owner = objId;
    bool ok = PCall(nargs, 0, inst.file + ":" + hook);
    m_owner = prevOwner;
    if (!ok) {
        auto it2 = m_instances.find(objId);
        if (it2 != m_instances.end()) it2->second.faulted = true;
        core::Log::Warn("Скрипт " + inst.file + " отключён после ошибки (исправьте и перезапустите Play).");
    }
    return true;
}

void ScriptHost::Emit(const char* event, const std::vector<LuaArg>& args) {
    auto it = m_handlers.find(event);
    if (it == m_handlers.end()) return;
    std::vector<Handler> copy = it->second;
    for (const auto& h : copy) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, h.fnRef);
        for (const auto& a : args) PushArg(a);
        uint32_t prevOwner = m_owner;
        m_owner = h.owner;
        PCall((int)args.size(), 0, std::string("event ") + event);
        m_owner = prevOwner;
    }
}

void ScriptHost::StepRoutine(size_t index) {
    if (index >= m_routines.size()) return;
    Routine& r = m_routines[index];
    lua_State* co = r.co;
    int nres = 0;
    uint32_t prevOwner = m_owner;
    m_owner = r.owner;
    int rc = lua_resume(co, L, 0, &nres);
    m_owner = prevOwner;
    // r может быть инвалидирован, если корутина запустила другие корутины
    Routine& rr = m_routines[index];
    if (rc == LUA_YIELD) {
        double secs = nres > 0 && lua_isnumber(co, -1) ? lua_tonumber(co, -1) : 0.0;
        lua_pop(co, nres);
        rr.wake = S->Time() + std::max(0.0, secs);
    } else {
        if (rc != LUA_OK) {
            const char* m = lua_tostring(co, -1);
            luaL_traceback(L, co, m ? m : "?", 0);
            ReportError("coroutine", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        luaL_unref(L, LUA_REGISTRYINDEX, rr.threadRef);
        rr.threadRef = -2;
    }
}

void ScriptHost::Update(float dt) {
    // on_update
    std::vector<uint32_t> keys;
    for (auto& kv : m_instances) keys.push_back(kv.first);
    for (uint32_t k : keys) {
        if (k != 0) {
            auto* o = S->GetScene().FindGameObject(k);
            if (!o || !o->active) continue;
        }
        CallHook(k, "on_update", {LuaArg(dt)});
    }

    // таймеры
    double now = S->Time();
    for (size_t i = 0; i < m_timers.size(); ++i) {
        if (m_timers[i].fnRef < 0 || m_timers[i].wake > now) continue;
        int ref = m_timers[i].fnRef;
        bool repeat = m_timers[i].interval > 0.0;
        uint32_t owner = m_timers[i].owner;
        if (repeat) m_timers[i].wake = now + m_timers[i].interval;
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        uint32_t prev = m_owner;
        m_owner = owner;
        PCall(0, 0, "timer");
        m_owner = prev;
        if (!repeat && i < m_timers.size() && m_timers[i].fnRef == ref) {
            luaL_unref(L, LUA_REGISTRYINDEX, ref);
            m_timers[i].fnRef = -1;
        }
    }
    m_timers.erase(std::remove_if(m_timers.begin(), m_timers.end(), [](const Timer& t) { return t.fnRef < 0; }), m_timers.end());

    // корутины
    for (size_t i = 0; i < m_routines.size(); ++i) {
        if (m_routines[i].threadRef < 0 || m_routines[i].wake > now) continue;
        StepRoutine(i);
    }
    m_routines.erase(std::remove_if(m_routines.begin(), m_routines.end(), [](const Routine& r) { return r.threadRef < 0; }), m_routines.end());
}

// ---------------------------------------------------------------------------------------------
// статические утилиты для редактора
// ---------------------------------------------------------------------------------------------
bool ScriptHost::CheckSyntaxText(const std::string& text, const std::string& chunkName, std::string& error) {
    lua_State* L = luaL_newstate();
    std::string name = "@" + chunkName;
    int rc = luaL_loadbufferx(L, text.data(), text.size(), name.c_str(), "t");
    if (rc != LUA_OK) error = lua_tostring(L, -1);
    lua_close(L);
    return rc == LUA_OK;
}

bool ScriptHost::CheckSyntax(const std::string& path, std::string& error) {
    std::string text = ReadFileText(path);
    lua_State* L = luaL_newstate();
    std::string name = "@" + path.substr(path.find_last_of("/\\") == std::string::npos ? 0 : path.find_last_of("/\\") + 1);
    int rc = luaL_loadbufferx(L, text.data(), text.size(), name.c_str(), "t");
    if (rc != LUA_OK) error = lua_tostring(L, -1);
    lua_close(L);
    return rc == LUA_OK;
}

std::vector<ExportVar> ScriptHost::ReadExports(const std::string& path) {
    std::vector<ExportVar> out;
    std::string text = ReadFileText(path);
    if (text.empty()) return out;

    lua_State* L = luaL_newstate();
    luaL_requiref(L, "_G", luaopen_base, 1); lua_pop(L, 1);
    luaL_requiref(L, "table", luaopen_table, 1); lua_pop(L, 1);
    luaL_requiref(L, "string", luaopen_string, 1); lua_pop(L, 1);
    luaL_requiref(L, "math", luaopen_math, 1); lua_pop(L, 1);

    // заглушка game: любой вызов — no-op
    lua_newtable(L);
    lua_newtable(L);
    lua_pushcfunction(L, [](lua_State* L2) -> int {
        lua_pushcfunction(L2, LuaApi::NoopFn);
        return 1;
    });
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    lua_setglobal(L, "game");

    // env
    lua_newtable(L);
    lua_newtable(L);
    lua_pushglobaltable(L);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    int envIdx = lua_gettop(L);

    if (luaL_loadbufferx(L, text.data(), text.size(), "@exports", "t") == LUA_OK) {
        lua_pushvalue(L, envIdx);
        lua_setupvalue(L, -2, 1);
        lua_pcall(L, 0, 0, 0); // ошибки игнорируем: берём то, что успело определиться
    }
    lua_pop(L, lua_gettop(L) - envIdx);

    lua_getfield(L, envIdx, "exports");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            if (lua_type(L, -2) == LUA_TSTRING) {
                ExportVar e;
                e.name = lua_tostring(L, -2);
                int t = lua_type(L, -1);
                if (t == LUA_TNUMBER) { e.type = "number"; e.defaultValue = luaL_tolstring(L, -1, nullptr); lua_pop(L, 1); }
                else if (t == LUA_TBOOLEAN) { e.type = "bool"; e.defaultValue = lua_toboolean(L, -1) ? "true" : "false"; }
                else if (t == LUA_TSTRING) { e.type = "string"; e.defaultValue = lua_tostring(L, -1); }
                else { lua_pop(L, 1); continue; }
                out.push_back(e);
            }
            lua_pop(L, 1);
        }
    }
    lua_close(L);
    std::sort(out.begin(), out.end(), [](const ExportVar& a, const ExportVar& b) { return a.name < b.name; });
    return out;
}

}
