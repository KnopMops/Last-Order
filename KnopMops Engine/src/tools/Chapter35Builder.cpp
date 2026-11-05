// Генератор проекта «Глава 3.5: Разъезд 86» (запуск: KnopMops Engine.exe --build-chapter35).
// Геометрия и объекты строятся кодом; логика — Lua-скрипты, записываемые рядом.
#include "tools/Chapter35Builder.h"
#include "map/BrushBuilder.h"
#include "map/MapSerializer.h"
#include "runtime/ChapterProject.h"
#include "scene/Scene.h"
#include <cmath>
#include <filesystem>
#include <fstream>

namespace knopmops::tools {

namespace fs = std::filesystem;
using scene::GameObject;
using scene::Scene;

namespace {

constexpr float kT = 0.25f;
constexpr float kH = 3.0f;

void Box(map::MapDocument& d, float x0, float y0, float z0, float x1, float y1, float z1, const char* tex, const std::string& name) {
    map::MapBrush b = map::BrushBuilder::CreateBox({x0, y0, z0}, {x1, y1, z1}, tex);
    b.name = name;
    d.AddBrush(b);
}

// Стена вдоль Z (x..x+kT), проём [g0,g1] по Z.
void WallZ(map::MapDocument& d, float x, float z0, float z1, float g0, float g1, const char* tex, const std::string& n, float h = kH) {
    if (g0 >= g1) { Box(d, x, 0, z0, x + kT, h, z1, tex, n); return; }
    if (g0 > z0) Box(d, x, 0, z0, x + kT, h, g0, tex, n + "_a");
    if (g1 < z1) Box(d, x, 0, g1, x + kT, h, z1, tex, n + "_b");
    Box(d, x, 2.2f, g0, x + kT, h, g1, tex, n + "_lintel");
}
void WallX(map::MapDocument& d, float z, float x0, float x1, float g0, float g1, const char* tex, const std::string& n, float h = kH) {
    if (g0 >= g1) { Box(d, x0, 0, z, x1, h, z + kT, tex, n); return; }
    if (g0 > x0) Box(d, x0, 0, z, g0, h, z + kT, tex, n + "_a");
    if (g1 < x1) Box(d, g1, 0, z, x1, h, z + kT, tex, n + "_b");
    Box(d, g0, 2.2f, z, g1, h, z + kT, tex, n + "_lintel");
}

// Комната с внутренней областью [x0,x1]x[z0,z1]; опционально проёмы в стенах.
struct Gap { float a = 0, b = 0; };
void Room(map::MapDocument& d, float x0, float z0, float x1, float z1, const char* wall, const char* floor, const char* ceil,
          Gap west, Gap east, Gap south, Gap north, const std::string& n, bool roof = true, float floorTop = 0.03f) {
    // верх пола выше земли (и разный у соседних комнат) — иначе компланарные грани дают мерцание (z-fighting)
    Box(d, x0 - kT, -0.2f, z0 - kT, x1 + kT, floorTop, z1 + kT, floor, n + "_floor");
    if (roof) Box(d, x0 - kT, kH, z0 - kT, x1 + kT, kH + 0.2f, z1 + kT, ceil, n + "_roof");
    WallZ(d, x0 - kT, z0 - kT, z1 + kT, west.a, west.b, wall, n + "_W");
    WallZ(d, x1, z0 - kT, z1 + kT, east.a, east.b, wall, n + "_E");
    WallX(d, z0 - kT, x0, x1, south.a, south.b, wall, n + "_S");
    WallX(d, z1, x0, x1, north.a, north.b, wall, n + "_N");
}

GameObject* Obj(Scene& s, const std::string& name, Vector3 pos, Vector3 scale = {1, 1, 1}, const char* tag = "") {
    auto* o = s.CreateGameObject(name);
    o->transform.position = pos;
    o->transform.scale = scale;
    o->tag = tag;
    o->meshRenderer.meshName = "None";
    return o;
}

GameObject* Prop(Scene& s, const std::string& name, Vector3 pos, Vector3 size, const char* tex, bool collide = true, float yaw = 0.0f) {
    auto* o = Obj(s, name, pos, size);
    o->meshRenderer.meshName = "Cube";
    o->meshRenderer.textureName = tex;
    o->transform.rotation.y = yaw;
    if (collide) { o->collider.enabled = true; o->collider.size = size; }
    return o;
}

GameObject* Lamp(Scene& s, const std::string& name, Vector3 pos, float radius, float inten, bool flick, const char* tag = "") {
    auto* l = Prop(s, name, pos, {0.3f, 0.12f, 0.3f}, "", false);
    l->tag = tag;
    l->meshRenderer.textureName.clear();
    l->meshRenderer.tint = Color{255, 240, 200, 255};
    l->meshRenderer.emissive = true;
    l->light.enabled = true;
    l->light.radius = radius;
    l->light.intensity = inten;
    l->light.flicker = flick;
    return l;
}

GameObject* Slot(Scene& s, const std::string& name, Vector3 pos, Vector3 size, const char* tex, const std::string& item, const std::string& display) {
    auto* c = Prop(s, name, pos, size, tex);
    c->searchSlot.enabled = true;
    c->searchSlot.slotId = name;
    c->searchSlot.displayName = display;
    c->searchSlot.containedItem = item;
    return c;
}

GameObject* Script(GameObject* o, const char* file, const char* prompt, bool interactable = true) {
    o->script.enabled = true;
    o->script.scriptFile = file;
    o->script.interactable = interactable;
    o->script.interactPrompt = prompt;
    return o;
}

GameObject* Player(Scene& s, Vector3 pos, float yaw) {
    auto* p = Obj(s, "Лера", pos);
    p->tag = "player";
    p->transform.rotation.y = yaw;
    p->player.enabled = true;
    p->player.walkSpeed = 2.3f;    // хромота после дома смотрителя
    p->player.sprintSpeed = 3.4f;
    p->camera.enabled = true;
    p->camera.isMainPlayerCamera = true;
    return p;
}

GameObject* Monster(Scene& s, const std::string& name, Vector3 pos, float yaw, const char* tag) {
    auto* m = Obj(s, name, pos, {1, 1, 1}, tag);
    m->transform.rotation.y = yaw;
    m->aiSpawn.enabled = true;
    return m;
}

GameObject* Trigger(Scene& s, const std::string& name, Vector3 pos, Vector3 size, const char* text) {
    auto* t = Obj(s, name, pos);
    t->collider.enabled = true;
    t->collider.isTrigger = true;
    t->collider.size = size;
    t->script.enabled = true;
    t->script.scriptFile = "hint_trigger.lua";
    t->script.parameters["text"] = text;
    return t;
}

void Write(const std::string& path, const std::string& text) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

// ------------------------------------------------------------------------------------------
void BuildRazyezd(Scene& sc, map::MapDocument& d) {
    sc.SetName("razyezd");
    sc.Env().ambient = 0.13f;
    sc.Env().fogEnd = 34.0f;
    sc.Env().background = Color{14, 17, 24, 255};

    // земля, ручей, берега
    Box(d, -40, -0.2f, -30, 40, 0.0f, -16, "motel_grass", "Берег_юг");
    Box(d, -40, -0.8f, -16, 40, -0.6f, -12, "motel_mirror_glass", "Ручей");
    Box(d, -40, -0.8f, -16, 40, 0.0f, -15.9f, "motel_gravel", "Ручей_стенка_юг");
    Box(d, -40, -0.8f, -12.1f, 40, 0.0f, -12, "motel_gravel", "Ручей_стенка_север");
    Box(d, -40, -0.2f, -12, 40, 0.0f, 32, "motel_grass", "Берег_север");
    // периметр
    Box(d, -41, 0, -31, 41, 5, -30, "motel_fence", "Забор_юг");
    Box(d, -41, 0, 32, 41, 5, 33, "motel_fence", "Забор_север");
    Box(d, -41, 0, -31, -40, 5, 33, "motel_fence", "Забор_запад");
    Box(d, 40, 0, -31, 41, 5, 33, "motel_fence", "Забор_восток");
    // платформа и рельсы
    Box(d, -8, 0, 10, 30, 0.05f, 14, "motel_asphalt", "Платформа");
    Box(d, -12, 0, 6.0f, 36, 0.1f, 6.25f, "motel_metal", "Рельс_1");
    Box(d, -12, 0, 7.45f, 36, 0.1f, 7.7f, "motel_metal", "Рельс_2");
    Box(d, -12, 0, 5.5f, 36, 0.03f, 8.2f, "motel_gravel", "Балласт");
    // будка стрелочника и депо
    Room(d, -1, 19, 5, 25, "motel_wall_wood", "motel_carpet", "motel_ceiling", {}, {}, {1.3f, 2.7f}, {}, "Будка");
    Room(d, 16, 16, 32, 27, "motel_brick", "motel_concrete", "motel_roof", {20.5f, 22.0f}, {}, {}, {}, "Депо");

    // --- игрок и точки появления (зависят от концовки Главы 3) ---
    Player(sc, {-30, 0, -8}, 70);
    struct Sp { const char* n; Vector3 p; float yaw; };
    for (Sp sp : {Sp{"Spawn_front_door", {-30, 0, -8}, 70}, Sp{"Spawn_car", {-26, 0, -6}, 80}, Sp{"Spawn_sewer", {-33, 0, -10}, 60},
                  Sp{"Spawn_radio", {-29, 0, -7}, 75}, Sp{"Spawn_police", {-23, 0, -4}, 85}}) {
        auto* n = Obj(sc, sp.n, sp.p);
        n->tag = "spawn";
        n->transform.rotation.y = sp.yaw;
    }
    Prop(sc, "Копейка", {-27.5f, 0.7f, -5.5f}, {1.7f, 1.4f, 4.2f}, "motel_car", true, 20);

    // сущность на южном берегу — стоит и ждёт
    Script(Monster(sc, "Эхо", {-26, 0, -24}, 0, "echo"), "echo.lua", "", false);

    // деревья по периметру
    for (int i = 0; i < 26; ++i) {
        float t = i / 26.0f;
        float x = -38 + 76 * t, z = (i % 2 ? 29.5f : 25.5f);
        auto* tr = Obj(sc, "Сосна_" + std::to_string(i), {x, 3.5f, z}, {0.7f, 7.0f, 0.7f}, "tree");
        tr->meshRenderer.meshName = "Cylinder";
        tr->meshRenderer.tint = Color{70, 52, 40, 255};
        tr->collider.enabled = true; tr->collider.shape = 1; tr->collider.radius = 0.35f;
        auto* tr2 = Obj(sc, "Сосна_ю_" + std::to_string(i), {x + 0.8f, 3.5f, -27.0f + (i % 3)}, {0.7f, 7.0f, 0.7f}, "tree");
        tr2->meshRenderer.meshName = "Cylinder";
        tr2->meshRenderer.tint = Color{70, 52, 40, 255};
    }

    // свет платформы
    int k = 0;
    for (float x : {-6.0f, 2.0f, 10.0f, 18.0f, 26.0f}) {
        ++k;
        Prop(sc, "Столб_" + std::to_string(k), {x, 1.65f, 11}, {0.16f, 3.2f, 0.16f}, "motel_metal");
        Prop(sc, "Кронштейн_" + std::to_string(k), {x, 3.22f, 11.15f}, {0.14f, 0.06f, 0.6f}, "motel_metal", false);
        Lamp(sc, "Фонарь_платформы_" + std::to_string(k), {x, 3.12f, 11.4f}, 10.0f, 0.75f, (k == 2 || k == 4));
    }
    Lamp(sc, "Лампа_будки", {2, 2.94f, 22}, 6.0f, 0.8f, true);
    Lamp(sc, "Лампа_депо_1", {22, 2.94f, 21.5f}, 9.0f, 0.55f, true);
    Lamp(sc, "Лампа_депо_2", {29, 2.94f, 21.5f}, 8.0f, 0.5f, false);
    Lamp(sc, "Лунный_свет", {-25, 9.0f, -2}, 30.0f, 0.35f, false)->meshRenderer.meshName = "None"; // невидимый источник

    // будка: стол, магнитофон, кассета №1
    Prop(sc, "Стол_стрелочника", {2, 0.4f, 23.6f}, {1.6f, 0.8f, 0.8f}, "motel_wall_wood");
    Script(Prop(sc, "Магнитофон", {2, 0.9f, 23.6f}, {0.5f, 0.2f, 0.3f}, "motel_radio", false), "magnetofon.lua", "Включить магнитофон");
    Slot(sc, "Ящик_будки", {-0.3f, 0.35f, 20.0f}, {0.8f, 0.7f, 0.8f}, "motel_cardboard", "item_tape_1", "Ящик с кассетами");

    // платформа: шкафчик с ключом стрелки, скамейка с кассетой №2
    Slot(sc, "Шкафчик_платформы", {-6, 1.0f, 12.6f}, {0.8f, 2.0f, 0.5f}, "motel_metal", "item_switch_key", "Железный шкафчик");
    Slot(sc, "Скамейка", {9, 0.3f, 12.8f}, {1.6f, 0.5f, 0.5f}, "motel_wall_wood", "item_tape_2", "Под скамейкой");

    // депо: предохранитель у входа, кассета №3 в дальнем углу, «стоячие»
    Slot(sc, "Ящик_депо", {17.2f, 0.35f, 25.5f}, {0.8f, 0.7f, 0.8f}, "motel_cardboard", "item_fuse", "Ящик у входа");
    Slot(sc, "Верстак_депо", {30.8f, 0.45f, 26.0f}, {1.2f, 0.9f, 0.7f}, "motel_wall_wood", "item_tape_3", "Верстак");
    int m = 0;
    for (Vector3 p : {Vector3{21, 0, 18}, Vector3{23.5f, 0, 24.5f}, Vector3{26, 0, 18.5f}, Vector3{27.5f, 0, 24}, Vector3{29.5f, 0, 20}}) {
        Script(Monster(sc, "Стоячий_" + std::to_string(++m), p, 270, "mimic"), "mimic.lua", "", false);
    }
    Trigger(sc, "Подсказка_депо", {16.5f, 1.0f, 21.2f}, {1.5f, 2.0f, 2.5f}, "Они стоят ровными рядами. Пока на них смотришь — они не двигаются.");
    Trigger(sc, "Подсказка_рельсы", {-14, 1.0f, 2}, {4, 2, 6}, "Рельсы идут на восток. Там, в тумане, горят фонари платформы.");

    // дрезина и пульт
    Prop(sc, "Дрезина", {33, 0.3f, 6.85f}, {2.6f, 0.5f, 1.5f}, "motel_metal", false);
    Script(Prop(sc, "Пульт_дрезины", {33, 0.9f, 8.8f}, {0.6f, 0.9f, 0.4f}, "motel_fuse_box"), "dresina.lua", "Запустить дрезину");
}

void BuildTunnel(Scene& sc, map::MapDocument& d) {
    sc.SetName("tunnel");
    sc.Env().ambient = 0.06f;
    sc.Env().fogEnd = 22.0f;
    sc.Env().background = Color{4, 4, 6, 255};

    Room(d, 0, -1.75f, 70, 1.75f, "motel_concrete", "motel_gravel", "motel_concrete", {}, {-1.75f, 1.75f}, {}, {}, "Туннель");
    Box(d, 0, 0, -0.65f, 70, 0.1f, -0.4f, "motel_metal", "Рельс_1");
    Box(d, 0, 0, 0.4f, 70, 0.1f, 0.65f, "motel_metal", "Рельс_2");
    Room(d, 70, -6, 84, 6, "motel_concrete", "motel_tile", "motel_ceiling", {-1.75f, 1.75f}, {-1.5f, 1.5f}, {}, {}, "Шлюз", true, 0.045f);
    Room(d, 84.25f, -2, 94, 2, "motel_concrete", "motel_tile", "motel_ceiling", {-1.5f, 1.5f}, {}, {}, {}, "Коридор", true, 0.06f);

    Player(sc, {2, 0.05f, 0}, 90);
    auto* dres = Prop(sc, "Дрезина", {2, 0.2f, 0}, {3.0f, 0.4f, 1.8f}, "motel_metal", false);
    dres->tag = "dynamic";
    dres->props["hd_slide"] = "0,72";
    auto* lever = Prop(sc, "Рычаг_дрезины", {3.2f, 0.85f, 0}, {0.14f, 0.9f, 0.14f}, "motel_metal", false);
    lever->tag = "dynamic";
    sc.SetParent(lever->id, dres->id);
    auto* lantern = Lamp(sc, "Фонарь_дрезины", {3.2f, 1.4f, 0}, 7.0f, 0.65f, false);
    lantern->tag = "dynamic";
    lantern->meshRenderer.tint = Color{255, 210, 120, 255};
    sc.SetParent(lantern->id, dres->id);
    Script(Obj(sc, "Маршрут", {0, 0, 0}), "ride.lua", "", false);

    // тень позади
    Script(Monster(sc, "Эхо_позади", {-9, 0, 0}, 90, "echo"), "echo.lua", "", false);

    for (int i = 0; i < 8; ++i) Lamp(sc, "Лампа_туннеля_" + std::to_string(i), {8.0f + i * 8.0f, 2.94f, 0}, 7.0f, 0.8f, i % 3 == 1, "tunnel_lamp");
    Lamp(sc, "Лампа_шлюза", {77, 2.94f, 0}, 12.0f, 0.9f, false);
    auto* gl = Lamp(sc, "Лампа_коридора", {90, 2.94f, 0}, 8.0f, 1.0f, false);
    gl->light.color = Color{160, 255, 210, 255};

    // табличка и ворота
    Prop(sc, "Табличка_Заслон", {77, 1.8f, -5.85f}, {2.4f, 0.9f, 0.05f}, "motel_sign", false);
    auto* gate = Prop(sc, "Ворота", {84.125f, 1.1f, 0}, {3.0f, 2.2f, 0.12f}, "motel_metal", true, 90);
    gate->door.enabled = true;
    gate->door.isLocked = true;
    gate->door.requiredKey = "";
    gate->door.openSound = "audio/sfx/sfx_door_smash.wav";

    // выбор у ворот
    auto* table = Script(Prop(sc, "Стол_у_ворот", {77, 0.4f, 3.2f}, {1.8f, 0.8f, 0.9f}, "motel_wall_wood"), "gate_choice.lua", "Оставить кассеты на столе");
    table->script.parameters["take"] = "false";
    auto* bag = Script(Prop(sc, "Рюкзак_Леры", {77, 0.25f, -3.2f}, {0.6f, 0.5f, 0.4f}, "motel_fabric"), "gate_choice.lua", "Забрать кассеты с собой");
    bag->script.parameters["take"] = "true";
}

}

// ------------------------------------------------------------------------------------------
bool BuildChapter35(const std::string& projectsDir, std::string& error) {
    std::string root = projectsDir + "/chapter_3_5";
    try {
        for (const char* sub : {"levels", "scripts", "prefabs", "textures", "audio"}) fs::create_directories(root + "/" + sub);
        std::ofstream(root + "/chapter.json") << "{}";
        runtime::ChapterProject p;
        if (!p.Load(root)) { error = "chapter.json"; return false; }
        auto& m = p.Manifest();
        m.id = "chapter_3_5";
        m.title = "Глава 3.5: Разъезд 86";
        m.description = "Переходная глава между домом смотрителя и «Заслоном».";
        m.startLevel = "razyezd";
        m.winTitle = "ГЛАВА 3.5 ЗАВЕРШЕНА";
        m.loseText = "Ты оглянулась слишком поздно...";
        m.flashlightDrain = 0.0035f;
        m.items = {
            {"item_fuse", "Предохранитель", "Керамический предохранитель 25А. Подходит к пульту дрезины."},
            {"item_switch_key", "Ключ стрелки", "Тяжёлый ключ с биркой «Стрелка №4»."},
            {"item_tape_1", "Кассета №1", "«Тамара, ночная смена». Дата стёрта."},
            {"item_tape_2", "Кассета №2", "«Повар». Лента в пятнах."},
            {"item_tape_3", "Кассета №3", "Без подписи. На корпусе цифры: 11 и 86."},
        };
        p.Save();

        {
            Scene sc; map::MapDocument d;
            BuildRazyezd(sc, d);
            sc.SaveToFile(p.LevelScenePath("razyezd"));
            map::MapSerializer::SaveToFile(p.LevelMapPath("razyezd"), d);
        }
        {
            Scene sc; map::MapDocument d;
            BuildTunnel(sc, d);
            sc.SaveToFile(p.LevelScenePath("tunnel"));
            map::MapSerializer::SaveToFile(p.LevelMapPath("tunnel"), d);
        }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

}
