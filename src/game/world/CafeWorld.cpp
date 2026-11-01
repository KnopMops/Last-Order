#include <nlohmann/json.hpp>
#include <fstream>
#include "game/world/CafeWorld.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace game::world {

engine::math::Vec2 CafeWorld::ComputeCameraOffset(engine::math::Vec2 ) const {
    return engine::math::Vec2{0.0f, 0.0f};
}

void CafeWorld::BuildCollidersAndTriggers(engine::core::Engine& eng, int nightNumber) {
    auto& phys = eng.Physics();
    phys.Clear();

    std::string noteDlg =
        (nightNumber == 1) ? "note_mikhail_intro" :
        (nightNumber == 2) ? "n2_intro_diary" :
        (nightNumber == 3) ? "n3_intro_diary" :
        (nightNumber == 4) ? "n4_intro_diary" :
                             "n5_intro_diary";

    std::string mirrorDlg = (nightNumber == 1) ? "mirror_night1" : ((nightNumber == 2) ? "n2_mirror_inspect" : "n3_mirror_inspect");
    std::string windowDlg = (nightNumber >= 3) ? "n3_window_hands_inspect" : "backdoor_window_inspect";
    std::string basementDlg = (nightNumber < 3) ? "basement_locked" : ((nightNumber == 3) ? "n3_basement_inspect" : ((nightNumber == 4) ? "n4_basement_rift" : "basement_locked"));
    std::string fridgeDlg = (nightNumber < 3) ? "fridge_locked" : ((nightNumber == 3) ? "n3_fridge_inspect" : "fridge_locked");
    std::string breakerDlg = (nightNumber == 2) ? "n2_fusebox_use" : "breaker_normal";
    std::string phoneDlg = (nightNumber == 4) ? "n4_phone_ring_talk" : "phone_silent";
    std::string jukeDlg = "jukebox_use";
    std::string table6Dlg = (nightNumber == 1) ? "table6_closed" : "";
    std::string radioDlg = eng.State().HasFlag("heard_radio_0214") ? "radio_tuned" :
                           (eng.State().HasFlag("radio_0214_ringing") ? "radio_0214" : "radio_turned_off");

    auto resolveDlg = [&](const std::string& tid, const std::string& def) -> std::string {
        if (tid == "trig_note") return noteDlg;
        if (tid == "trig_window") return windowDlg;
        if (tid == "trig_basement") return basementDlg;
        if (tid == "trig_fridge") return fridgeDlg;
        if (tid == "trig_breaker") return breakerDlg;
        if (tid == "trig_phone") return phoneDlg;
        if (tid == "trig_jukebox") return jukeDlg;
        if (tid == "trig_table6") return table6Dlg;
        if (tid == "trig_radio") return radioDlg;
        return def;
    };

    std::string mapPath = eng.Assets().GetBaseDir() + "/data/maps/chapter1_cafe.json";
    bool loadedJson = false;

    try {
        std::ifstream file(mapPath);
        if (file.is_open()) {
            nlohmann::json root;
            file >> root;

            if (root.contains("colliders") && root["colliders"].is_array()) {
                for (const auto& c : root["colliders"]) {
                    std::string cid = c.value("id", "");
                    float x = c.value("x", 0.0f);
                    float y = c.value("y", 0.0f);
                    float w = c.value("w", 16.0f);
                    float h = c.value("h", 16.0f);
                    phys.AddStaticCollider(cid, {x, y, w, h});
                }
            }

            if (root.contains("triggers") && root["triggers"].is_array()) {
                for (const auto& t : root["triggers"]) {
                    std::string tid = t.value("id", "");
                    std::string name = t.value("name", "");
                    std::string dlg = t.value("dialogue", "");
                    float x = t.value("x", 0.0f);
                    float y = t.value("y", 0.0f);
                    float w = t.value("w", 16.0f);
                    float h = t.value("h", 16.0f);
                    bool en = t.value("enabled", true);

                    if (tid == "trig_table6") {
                        en = (nightNumber == 1);
                    }
                    std::string finalDlg = resolveDlg(tid, dlg);
                    phys.AddTrigger({tid, name, finalDlg, {x, y, w, h}, en});
                }
            }
            loadedJson = true;
        }
    } catch (...) {
        loadedJson = false;
    }

    if (!loadedJson) {
        phys.AddStaticCollider("wall_top",       {0.0f,   0.0f,   320.0f, 22.0f});
        phys.AddStaticCollider("wall_bottom",    {0.0f,   172.0f, 320.0f, 8.0f});
        phys.AddStaticCollider("wall_left",      {0.0f,   0.0f,   10.0f,  180.0f});
        phys.AddStaticCollider("wall_right",     {310.0f, 0.0f,   10.0f,  180.0f});

        phys.AddStaticCollider("bar_counter",    {88.0f,  44.0f,  104.0f, 10.0f});
        phys.AddStaticCollider("prop_jukebox",   {52.0f,  22.0f,  16.0f,  18.0f});

        phys.AddStaticCollider("kitchen_oven",   {236.0f, 22.0f,  30.0f,  16.0f});
        phys.AddStaticCollider("kitchen_fridge", {274.0f, 22.0f,  28.0f,  18.0f});
        phys.AddStaticCollider("corr_generator", {12.0f,  64.0f,  18.0f,  18.0f});

        phys.AddStaticCollider("booth_1",        {68.0f,  84.0f,  20.0f,  12.0f});
        phys.AddStaticCollider("booth_2",        {68.0f,  126.0f, 20.0f,  12.0f});
        phys.AddStaticCollider("booth_3",        {148.0f, 84.0f,  20.0f,  12.0f});
        phys.AddStaticCollider("booth_4",        {148.0f, 126.0f, 20.0f,  12.0f});
        phys.AddStaticCollider("booth_5",        {228.0f, 84.0f,  20.0f,  12.0f});
        phys.AddStaticCollider("booth_table6",   {228.0f, 126.0f, 20.0f,  12.0f});

        phys.AddTrigger({"trig_note",     "Касса",               noteDlg,               {84.0f,  24.0f,  32.0f, 46.0f}, true});
        phys.AddTrigger({"trig_visor",    "Комплекс «ОКО-86»",   "visor_inspect",       {118.0f, 24.0f,  24.0f, 46.0f}, true});
        phys.AddTrigger({"trig_coffee",   "Кофемашина",          "coffee_machine_use",  {142.0f, 24.0f,  28.0f, 46.0f}, true});
        phys.AddTrigger({"trig_radio",    "Радиоприёмник",       radioDlg,              {168.0f, 24.0f,  28.0f, 46.0f}, true});

        phys.AddTrigger({"trig_jukebox",  "Музыкальный автомат", jukeDlg,               {48.0f,  18.0f,  24.0f, 28.0f}, true});
        phys.AddTrigger({"trig_rules",    "Устав смены",         "wall_rules_night1",   {106.0f, 20.0f,  34.0f, 50.0f}, true});
        phys.AddTrigger({"trig_phone",    "Телефон на стене",    phoneDlg,              {140.0f, 10.0f,  22.0f, 20.0f}, true});
        phys.AddTrigger({"trig_cat",      "Кот Уголёк",          "cat_ugolyok_talk",    {196.0f, 34.0f,  24.0f, 24.0f}, true});

        phys.AddTrigger({"trig_oven",     "Печь и Хозблок",      "pie_oven_use",        {232.0f, 18.0f,  36.0f, 26.0f}, true});
        phys.AddTrigger({"trig_fridge",   "Морозильная камера",  fridgeDlg,             {270.0f, 18.0f,  36.0f, 28.0f}, true});

        phys.AddTrigger({"trig_breaker",  "Электрощиток",        breakerDlg,            {10.0f,  58.0f,  28.0f, 28.0f}, true});
        phys.AddTrigger({"trig_basement", "Люк в подвал",        basementDlg,           {10.0f,  132.0f, 28.0f, 28.0f}, true});

        phys.AddTrigger({"trig_backdoor", "Уличный контейнер",   "backdoor_window_inspect", {114.0f, 138.0f, 40.0f, 34.0f}, true});
        phys.AddTrigger({"trig_window",   "Окно на шоссе (Стекло)", windowDlg,         {156.0f, 138.0f, 60.0f, 34.0f}, true});

        phys.AddTrigger({"trig_booth1",   "Стол №1",             "",                    {56.0f,  70.0f,  42.0f, 36.0f}, false});
        phys.AddTrigger({"trig_booth2",   "Стол №2",             "",                    {56.0f,  112.0f, 42.0f, 36.0f}, false});
        phys.AddTrigger({"trig_booth3",   "Стол №3",             "",                    {136.0f, 70.0f,  42.0f, 36.0f}, false});
        phys.AddTrigger({"trig_booth4",   "Стол №4",             "",                    {136.0f, 112.0f, 42.0f, 36.0f}, false});
        phys.AddTrigger({"trig_booth5",   "Стол №5",             "",                    {216.0f, 70.0f,  42.0f, 36.0f}, false});
        phys.AddTrigger({"trig_table6",   "Стол №6",             table6Dlg,             {216.0f, 112.0f, 42.0f, 36.0f}, (nightNumber == 1)});
    }
}

void CafeWorld::BuildLighting(engine::core::Engine& eng, int nightNumber) {
    auto& light = eng.Lighting();
    light.ClearLights();
    Color amb = (nightNumber == 1) ? Color{135, 112, 98, 255}
              : (nightNumber == 2) ? Color{110, 94, 90, 255}
                                   : Color{84, 72, 84, 255};
    light.SetAmbientColor(amb);

    std::string mapPath = eng.Assets().GetBaseDir() + "/data/maps/chapter1_cafe.json";
    bool loadedLights = false;
    try {
        std::ifstream file(mapPath);
        if (file.is_open()) {
            nlohmann::json root;
            file >> root;
            if (root.contains("lights") && root["lights"].is_array()) {
                for (const auto& l : root["lights"]) {
                    std::string lid = l.value("id", "lamp");
                    float lx = l.value("x", 160.0f);
                    float ly = l.value("y", 90.0f);
                    float lr = l.value("radius", 80.0f);
                    unsigned char cr = l.value("r", 244);
                    unsigned char cg = l.value("g", 184);
                    unsigned char cb = l.value("b", 96);
                    float lint = l.value("intensity", 1.0f);
                    float lspd = l.value("speed", 2.0f);
                    float lamt = l.value("amount", 0.03f);
                    light.AddPointLight({lid, {lx, ly}, lr, Color{cr, cg, cb, 255}, lint, lspd, lamt, true});
                }
                loadedLights = true;
            }
        }
    } catch (...) {
        loadedLights = false;
    }

    if (!loadedLights) {
        light.AddPointLight({"lamp_counter",  {140.0f, 44.0f},  95.0f, Color{244, 184, 96, 255}, 1.0f,  2.5f, 0.03f, true});
        light.AddPointLight({"lamp_hall_L",   {78.0f,  105.0f}, 85.0f, Color{244, 184, 96, 255}, 0.90f, 1.8f, 0.02f, true});
        light.AddPointLight({"lamp_hall_C",   {158.0f, 105.0f}, 85.0f, Color{244, 184, 96, 255}, 0.88f, 2.1f, 0.03f, true});
        light.AddPointLight({"lamp_hall_R",   {238.0f, 105.0f}, 80.0f, Color{244, 184, 96, 255}, 0.82f, 3.2f, 0.04f, true});
        light.AddPointLight({"lamp_jukebox",  {60.0f,  30.0f},  50.0f, Color{214, 69,  69, 255}, 0.70f, 6.0f, 0.08f, true});
        light.AddPointLight({"lamp_kitchen",  {268.0f, 36.0f},  75.0f, Color{216, 230, 242, 255},0.85f, 4.0f, 0.05f, true});
        light.AddPointLight({"lamp_corridor", {24.0f,  90.0f},  65.0f, Color{143, 169, 196, 255},0.75f, 5.0f, 0.08f, true});
    }

    auto& fl = light.GetFlashlight();
    fl.fovDeg = 65.0f;
    fl.range = 95.0f;
    fl.color = Color{216, 230, 242, 255};
    fl.intensity = 1.2f;
    fl.enabled = true;
}

void CafeWorld::Render(engine::core::Engine& eng,
                       int nightNumber,
                       const engine::physics::KinematicBody& playerBody,
                       int playerFacing,
                       bool isMoving,
                       float walkAnimTimer,
                       bool lightsFlickering,
                       bool blackoutMode,
                       bool showDebugPhysics,
                       const std::optional<engine::physics::TriggerVolume>& nearestTrigger,
                       const std::vector<GuestNpc>& guests,
                       const std::vector<VisorAnomaly3D>& anomalies) const {
    auto& assets = eng.Assets();
    Texture2D tsCafe    = assets.GetTexture("sprites/tilesets/ts_cafe_interior.png");
    Texture2D tsKitchen = assets.GetTexture("sprites/tilesets/ts_kitchen_storage.png");
    Texture2D tsBase    = assets.GetTexture("sprites/tilesets/ts_basement.png");

    eng.Lighting().SetCameraOffset({0.0f, 0.0f});

    for (int ty = 0; ty < 12; ++ty) {
        for (int tx = 0; tx < 20; ++tx) {
            float wx = static_cast<float>(tx * 16);
            float wy = static_cast<float>(ty * 16);
            if (tx < 2) {
                Rectangle src = {static_cast<float>((tx % 4) * 16), static_cast<float>((ty % 4) * 16), 16.0f, 16.0f};
                DrawTextureRec(tsBase, src, Vector2{wx, wy}, WHITE);
            } else if (tx >= 15 && ty < 4) {
                Rectangle src = {static_cast<float>((tx % 4) * 16), static_cast<float>((ty % 4) * 16), 16.0f, 16.0f};
                DrawTextureRec(tsKitchen, src, Vector2{wx, wy}, WHITE);
            } else {
                int rowSrc = (ty < 2) ? 8 : ((ty < 9) ? 0 : 4);
                Rectangle src = {static_cast<float>((tx % 8) * 16), static_cast<float>(rowSrc * 16), 16.0f, 16.0f};
                DrawTextureRec(tsCafe, src, Vector2{wx, wy}, WHITE);
            }
        }
    }

    DrawRectangle(0,   0,   320, 22,  Color{42, 23, 21, 255});
    DrawRectangle(0,   172, 320, 8,   Color{24, 16, 20, 255});
    DrawRectangle(0,   0,   10,  180, Color{42, 23, 21, 255});
    DrawRectangle(310, 0,   10,  180, Color{42, 23, 21, 255});

    Texture2D texGen    = assets.GetTexture("sprites/props/prop_generator.png");
    Texture2D texJuke   = assets.GetTexture("sprites/props/prop_jukebox.png");
    Texture2D texCoffee = assets.GetTexture("sprites/props/prop_coffee_machine.png");
    Texture2D texBooth  = assets.GetTexture("sprites/props/prop_table_booth.png");
    Texture2D texTable6 = assets.GetTexture("sprites/props/prop_table_06.png");
    Texture2D texCat    = assets.GetTexture("sprites/characters/spr_cat_ugolyok.png");

    float tNow = static_cast<float>(GetTime());

    DrawTexture(texGen,    12, 62, WHITE);
    DrawRectangle(14, 136, 20, 20, Color{32, 20, 18, 255});
    DrawRectangleLines(14, 136, 20, 20, Color{196, 122, 59, 255});

    DrawTexture(texJuke, 52, 18, WHITE);

    if (eng.State().HasFlag("jukebox_playing")) {

        float jPulse = 0.5f + 0.5f * std::sin(tNow * 6.0f);

        Color jukeNeon = Color{static_cast<unsigned char>(180 + 75 * jPulse), 40, static_cast<unsigned char>(220 - 60 * jPulse), 230};

        DrawRectangleLinesEx(Rectangle{50, 16, 20, 22}, 1.5f, jukeNeon);

    }
    DrawRectangle(110, 8, 14, 12, Color{232, 227, 217, 255});
    DrawRectangleLines(110, 8, 14, 12, Color{107, 51, 34, 255});

    DrawRectangle(144, 8, 12, 12, Color{180, 42, 42, 255});
    DrawRectangleLines(144, 8, 12, 12, Color{244, 184, 96, 255});

    DrawRectangle(88, 44, 104, 10, Color{107, 51, 34, 255});
    DrawRectangle(88, 42, 104, 3,  Color{196, 122, 59, 255});
    DrawRectangle(92, 40, 14,  10, Color{216, 194, 162, 255});

    DrawRectangle(118, 41, 12, 8, Color{40, 180, 130, 255});
    DrawRectangleLines(118, 41, 12, 8, Color{244, 184, 96, 255});

    DrawTexture(texCoffee, 142, 34, WHITE);
    DrawRectangle(170, 41, 16, 9, Color{52, 34, 28, 255});
    DrawRectangleLines(170, 41, 16, 9, Color{214, 69, 69, 255});

    int catFrame = (lightsFlickering || blackoutMode || nightNumber >= 3) ? 1 : 0;
    DrawTextureRec(texCat, Rectangle{static_cast<float>(catFrame * 16), 0.0f, 16.0f, 16.0f}, Vector2{200.0f, 40.0f}, WHITE);

    DrawRectangle(236, 20, 30, 16, Color{80, 86, 98, 255});
    DrawRectangleLines(236, 20, 30, 16, Color{196, 122, 59, 255});
    DrawRectangle(274, 18, 28, 18, Color{143, 169, 196, 255});
    DrawRectangleLines(274, 18, 28, 18, Color{42, 23, 21, 255});

    DrawRectangle(122, 162, 28, 10, Color{46, 68, 52, 255});
    DrawRectangleLines(122, 162, 28, 10, Color{143, 169, 196, 255});
    DrawRectangle(156, 162, 60, 10, Color{24, 30, 42, 255});
    DrawRectangleLines(156, 162, 60, 10, Color{107, 51, 34, 255});
    DrawRectangle(158, 164, 56, 6, Color{75, 135, 175, 195});
    float rainAnim = std::fmod(tNow * 16.0f, 56.0f);
    for (int rw = 0; rw < 4; rw++) {
        int rx = 159 + static_cast<int>(std::fmod(rainAnim + rw * 14.0f, 54.0f));
        DrawLine(rx, 164, rx - 2, 169, Color{195, 225, 255, 210});
    }
    DrawLine(160, 169, 212, 164, Color{255, 255, 255, 110});

    DrawTexture(texBooth,  64,  80,  WHITE);
    DrawTexture(texBooth,  64,  122, WHITE);
    DrawTexture(texBooth,  144, 80,  WHITE);
    DrawTexture(texBooth,  144, 122, WHITE);
    DrawTexture(texBooth,  224, 80,  WHITE);
    DrawTexture(texTable6, 224, 122, WHITE);

        for (const auto& a : anomalies) {
        if (!a.active) continue;
        int ax = static_cast<int>(a.pos.x);
        int ay = static_cast<int>(a.pos.y);
        int jx = (std::rand() % 5) - 2;
        int jy = (std::rand() % 5) - 2;

        float pulse = 0.6f + 0.4f * std::sin(tNow * 9.0f + static_cast<float>(a.id));
        DrawEllipse(ax, ay + 6, 11.0f * pulse, 5.0f * pulse, Color{8, 2, 10, 210});
        DrawEllipseLines(ax, ay + 6, 12.0f * pulse, 5.5f * pulse, Color{195, 18, 28, 175});

        if (a.type == 0) {
            DrawCircle(ax + jx, ay - 2 + jy, 6.0f * pulse, Color{140, 10, 20, 190});
            DrawCircle(ax, ay - 2, 3.0f, Color{255, 45, 55, 230});
        } else {
            DrawEllipse(ax + jx, ay - 2 + jy, 6.0f, 10.0f, Color{6, 4, 10, 230});
            DrawCircle(ax + jx, ay - 11 + jy, 4.5f, Color{10, 6, 14, 240});
            DrawPixel(ax + jx - 2, ay - 12 + jy, Color{255, 25, 25, 255});
            DrawPixel(ax + jx + 2, ay - 12 + jy, Color{255, 25, 25, 255});
        }

        for (int g = 0; g < 3; ++g) {
            int gx = ax - 9 + (std::rand() % 14);
            int gy = ay - 14 + (std::rand() % 20);
            int gw = 4 + (std::rand() % 8);
            Color gc = (g % 2 == 0) ? Color{215, 20, 30, 180} : Color{12, 4, 16, 210};
            DrawRectangle(gx, gy, gw, 1, gc);
        }
    }

    for (const auto& g : guests) {
        if (!g.spawned) continue;
        Texture2D gTex = assets.GetTexture(g.spritePath);
        bool isUnmasked = eng.State().HasFlag("n2_mirror_checked") || eng.State().HasFlag("mimic_unmasked_" + g.id);
        float shadowOffsetX = (g.anomalous && isUnmasked) ? -5.0f : 3.0f;
        DrawEllipse(static_cast<int>(g.pos.x + 16.0f + shadowOffsetX),
                    static_cast<int>(g.pos.y + 28.0f), 9.0f, 4.0f,
                    (g.anomalous && isUnmasked) ? Color{85, 8, 14, 195} : Color{10, 13, 26, 165});

        if (g.anomalous && isUnmasked && (std::rand() % 6 == 0)) {
            DrawRectangle(static_cast<int>(g.pos.x + 6), static_cast<int>(g.pos.y + (std::rand() % 24)), 20, 1, Color{215, 25, 35, 190});
        }

        Rectangle src = {static_cast<float>((g.spriteFrame % 6) * 32), 0.0f, 32.0f, 32.0f};
        if (src.x + 32.0f > static_cast<float>(gTex.width)) src.x = 0.0f;
        DrawTextureRec(gTex, src, Vector2{std::round(g.pos.x), std::round(g.pos.y)}, WHITE);
    }

    Texture2D texLera = assets.GetTexture("sprites/characters/spr_lera.png");
    int animCol = isMoving ? (static_cast<int>(walkAnimTimer) % 4) : 0;
    int animRow = std::clamp(playerFacing, 0, 3);
    DrawEllipse(static_cast<int>(playerBody.Center().x), static_cast<int>(playerBody.Center().y + 4.0f),
                7.0f, 3.5f, Color{10, 13, 26, 150});
    DrawTextureRec(texLera,
                   Rectangle{static_cast<float>(animCol * 32), static_cast<float>(animRow * 32), 32.0f, 32.0f},
                   Vector2{std::round(playerBody.position.x), std::round(playerBody.position.y)},
                   WHITE);

    if (nearestTrigger.has_value()) {
        const auto& tr = nearestTrigger.value();
        DrawRectangleLinesEx(Rectangle{tr.bounds.x, tr.bounds.y, tr.bounds.w, tr.bounds.h},
                             1.0f, Color{244, 184, 96, 190});
    }

    if (showDebugPhysics) {
        for (const auto& c : eng.Physics().GetColliders()) {
            DrawRectangleLinesEx(Rectangle{c.bounds.x, c.bounds.y, c.bounds.w, c.bounds.h},
                                 1.0f, Color{0, 255, 90, 220});
        }
        for (const auto& t : eng.Physics().GetTriggers()) {
            Color tc = t.enabled ? Color{0, 200, 255, 220} : Color{140, 140, 140, 140};
            DrawRectangleLinesEx(Rectangle{t.bounds.x, t.bounds.y, t.bounds.w, t.bounds.h},
                                 1.0f, tc);
        }
        auto pb = playerBody.WorldBounds();
        DrawRectangleLinesEx(Rectangle{pb.x, pb.y, pb.w, pb.h}, 1.0f, Color{255, 60, 60, 255});
    }
}

void CafeWorld::Render3DVisor(engine::core::Engine& eng,
                              int ,
                              engine::math::Vec2 playerCenter,
                              float visorYawRad,
                              float visorPitchRad,
                              const std::vector<GuestNpc>& guests,
                              const std::vector<VisorAnomaly3D>& anomalies,
                              float walkBobTimer,
                              float gunRecoilTimer) const {
    constexpr float S = 0.05f;
    float bobY = std::sin(walkBobTimer * 2.2f) * 0.045f;
    Vector3 camPos = {playerCenter.x * S, 1.15f + bobY, playerCenter.y * S};
    Vector3 forward = {
        std::cos(visorYawRad) * std::cos(visorPitchRad),
        std::sin(visorPitchRad),
        std::sin(visorYawRad) * std::cos(visorPitchRad)
    };

    Camera3D cam{};
    cam.position = camPos;
    cam.target = {camPos.x + forward.x, camPos.y + forward.y, camPos.z + forward.z};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 65.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    ClearBackground(Color{10, 12, 22, 255});
    float tNow = static_cast<float>(GetTime());
    BeginMode3D(cam);

    DrawPlane(Vector3{8.0f, 0.0f, 4.5f}, Vector2{16.0f, 9.0f}, Color{42, 24, 22, 255});
    for (int gx = 0; gx <= 16; gx += 1) {
        DrawLine3D(Vector3{static_cast<float>(gx), 0.01f, 0.0f},
                   Vector3{static_cast<float>(gx), 0.01f, 9.0f}, Color{68, 38, 34, 180});
    }
    for (int gz = 0; gz <= 9; gz += 1) {
        DrawLine3D(Vector3{0.0f, 0.01f, static_cast<float>(gz)},
                   Vector3{16.0f, 0.01f, static_cast<float>(gz)}, Color{68, 38, 34, 180});
    }

    auto drawBlock = [&](float x, float y, float w, float h, float height3D, Color c, Color wire) {
        Vector3 pos = {(x + w * 0.5f) * S, height3D * 0.5f, (y + h * 0.5f) * S};
        Vector3 sz  = {w * S, height3D, h * S};
        DrawCubeV(pos, sz, c);
        DrawCubeWiresV(pos, sz, wire);
    };

    drawBlock(0,   0,   320, 22,  2.6f, Color{48, 26, 24, 255}, Color{90, 220, 180, 90});
    drawBlock(0,   172, 320, 8,   2.6f, Color{38, 22, 24, 255}, Color{90, 220, 180, 90});
    drawBlock(0,   0,   10,  180, 2.6f, Color{48, 26, 24, 255}, Color{90, 220, 180, 90});
    drawBlock(310, 0,   10,  180, 2.6f, Color{48, 26, 24, 255}, Color{90, 220, 180, 90});

    drawBlock(82,  42,  112, 12,  1.05f, Color{125, 62, 38, 255}, Color{244, 184, 96, 200});
    drawBlock(52,  22,  16,  18,  1.70f, Color{168, 42, 68, 255}, Color{255, 120, 160, 255});

    const float bx[3] = {68.0f, 148.0f, 228.0f};
    for (int i = 0; i < 3; ++i) {
        drawBlock(bx[i], 84.0f,  20.0f, 12.0f, 0.85f, Color{110, 56, 36, 255}, Color{196, 122, 59, 200});
        Color row2Col = (i == 2) ? Color{88, 20, 28, 255} : Color{110, 56, 36, 255};
        Color row2Wire = (i == 2) ? Color{255, 50, 50, 255} : Color{196, 122, 59, 200};
        drawBlock(bx[i], 126.0f, 20.0f, 12.0f, 0.85f, row2Col, row2Wire);
    }

    drawBlock(236, 22,  30,  16,  1.2f, Color{90, 100, 115, 255}, Color{143, 169, 196, 200});
    drawBlock(274, 22,  28,  18,  2.1f, Color{120, 150, 180, 255}, Color{180, 220, 255, 220});
    drawBlock(12,  64,  18,  18,  1.5f, Color{70, 95, 75, 255},   Color{120, 240, 150, 220});

        for (const auto& g : guests) {
        if (!g.spawned) continue;
        Texture2D gTex = eng.Assets().GetTexture(g.spritePath);
        Rectangle src = {0.0f, 0.0f, 32.0f, 32.0f};
        Vector3 gPos = {(g.pos.x + 16.0f) * S, 1.05f, (g.pos.y + 16.0f) * S};
        bool isUnmasked = eng.State().HasFlag("n2_mirror_checked") || eng.State().HasFlag("mimic_unmasked_" + g.id);
        bool showAnomaly = (g.anomalous && isUnmasked);
        Color tint = showAnomaly ? Color{255, 65, 65, 255} : Color{190, 255, 225, 255};
        DrawBillboardRec(cam, gTex, src, gPos, Vector2{1.5f, 1.5f}, tint);
        if (showAnomaly) {
            float pulse = 1.35f + 0.25f * std::sin(tNow * 8.0f);
            DrawCubeWiresV(gPos, Vector3{1.1f * pulse, 1.9f, 1.1f * pulse}, Color{255, 35, 35, 255});
        }
    }

    Texture2D reaperTex = eng.Assets().GetTexture("sprites/screamers/sc_night9_shadow_reaper.png");
    Texture2D skullTex  = eng.Assets().GetTexture("sprites/screamers/sc_night14_skull_glitch.png");

    for (const auto& a : anomalies) {
        if (!a.active) continue;
        float hover = std::sin(tNow * 4.5f + static_cast<float>(a.id)) * 0.18f;
        Vector3 aPos = {a.pos.x * S, a.heightY + hover, a.pos.y * S};
        Color baseCol = (a.hitFlash > 0.0f) ? WHITE : Color{235, 30, 50, 235};
        Color wireCol = (a.hitFlash > 0.0f) ? YELLOW : Color{90, 255, 210, 255};

        if (a.type == 0) {
            float s = 0.65f + 0.15f * std::sin(tNow * 7.0f);
            DrawSphere(aPos, s * 0.45f, baseCol);
            DrawCubeWiresV(aPos, Vector3{s, s * 1.4f, s}, wireCol);
            DrawCubeWiresV(aPos, Vector3{s * 1.3f, s * 0.7f, s * 1.3f}, Color{255, 60, 80, 255});
        } else if (a.type == 1) {
            Rectangle src = {0.0f, 0.0f, static_cast<float>(reaperTex.width), static_cast<float>(reaperTex.height)};
            DrawBillboardRec(cam, reaperTex, src, aPos, Vector2{1.55f, 1.75f}, (a.hitFlash > 0.0f) ? YELLOW : Color{255, 90, 90, 255});
            DrawCubeWiresV(aPos, Vector3{1.1f, 1.8f, 1.1f}, wireCol);
        } else {
            Rectangle src = {0.0f, 0.0f, static_cast<float>(skullTex.width), static_cast<float>(skullTex.height)};
            DrawBillboardRec(cam, skullTex, src, aPos, Vector2{1.25f, 1.25f}, (a.hitFlash > 0.0f) ? YELLOW : Color{120, 255, 220, 255});
            DrawSphereWires(aPos, 0.65f, 6, 6, Color{255, 50, 70, 240});
        }
    }

    Vector3 right = {
        -std::sin(visorYawRad),
        0.0f,
        std::cos(visorYawRad)
    };
    float recoilBack = gunRecoilTimer * 0.35f;
    float bobGun = std::cos(walkBobTimer * 2.2f) * 0.025f;
    Vector3 gunPos = {
        camPos.x + forward.x * (0.46f - recoilBack) + right.x * 0.20f,
        camPos.y - 0.20f + bobGun + forward.y * (0.46f - recoilBack),
        camPos.z + forward.z * (0.46f - recoilBack) + right.z * 0.20f
    };
    Vector3 barrelBack = {
        gunPos.x - forward.x * 0.12f,
        gunPos.y - forward.y * 0.12f,
        gunPos.z - forward.z * 0.12f
    };
    Vector3 barrelMid = {
        gunPos.x + forward.x * 0.08f,
        gunPos.y + forward.y * 0.08f,
        gunPos.z + forward.z * 0.08f
    };
    Vector3 barrelMuzzle = {
        gunPos.x + forward.x * 0.28f - right.x * 0.03f,
        gunPos.y + forward.y * 0.28f,
        gunPos.z + forward.z * 0.28f - right.z * 0.03f
    };
    Vector3 gripBottom = {
        barrelBack.x - forward.x * 0.03f,
        barrelBack.y - 0.12f,
        barrelBack.z - forward.z * 0.03f
    };

    DrawCylinderEx(barrelBack, gripBottom, 0.038f, 0.030f, 6, Color{58, 34, 28, 255});
    DrawCylinderEx(barrelBack, barrelMid, 0.062f, 0.052f, 8, Color{38, 48, 58, 255});
    DrawCylinderWiresEx(barrelBack, barrelMid, 0.065f, 0.055f, 8, Color{90, 245, 200, 255});
    DrawCylinderEx(barrelMid, barrelMuzzle, 0.040f, 0.028f, 8, Color{24, 34, 44, 255});
    DrawCylinderWiresEx(barrelMid, barrelMuzzle, 0.043f, 0.031f, 8, Color{110, 255, 220, 255});
    DrawSphere(barrelMuzzle, 0.026f, Color{90, 255, 215, 255});

    if (gunRecoilTimer > 0.06f) {
        Vector3 beamEnd = {
            camPos.x + forward.x * 16.0f,
            camPos.y + forward.y * 16.0f,
            camPos.z + forward.z * 16.0f
        };
        DrawCylinderEx(barrelMuzzle, beamEnd, 0.018f, 0.008f, 6, Color{110, 255, 225, 255});
        DrawSphere(barrelMuzzle, 0.075f, Color{255, 230, 120, 255});
    }

    EndMode3D();
}

}
