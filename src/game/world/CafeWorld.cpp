#include "game/world/CafeWorld.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

namespace game::world {

engine::math::Vec2 CafeWorld::ComputeCameraOffset(engine::math::Vec2 playerCenter) const {
    float cx = std::clamp(playerCenter.x - 160.0f, 0.0f, WORLD_W - 320.0f);
    float cy = std::clamp(playerCenter.y - 90.0f,  0.0f, WORLD_H - 180.0f);
    return engine::math::Vec2{std::round(cx), std::round(cy)};
}

void CafeWorld::BuildCollidersAndTriggers(engine::core::Engine& eng, int nightNumber) {
    auto& phys = eng.Physics();
    phys.Clear();

    phys.AddStaticCollider("wall_top",         {0.0f,   0.0f,   640.0f, 34.0f});
    phys.AddStaticCollider("wall_bottom",      {0.0f,   344.0f, 640.0f, 16.0f});
    phys.AddStaticCollider("wall_left",        {0.0f,   0.0f,   20.0f,  360.0f});
    phys.AddStaticCollider("wall_right",       {620.0f, 0.0f,   20.0f,  360.0f});

    phys.AddStaticCollider("wall_corr_top",    {124.0f, 34.0f,  12.0f,  86.0f});
    phys.AddStaticCollider("wall_corr_bot",    {124.0f, 188.0f, 12.0f,  92.0f});

    phys.AddStaticCollider("wall_kitch_top",   {444.0f, 34.0f,  12.0f,  60.0f});
    phys.AddStaticCollider("wall_kitch_bot",   {444.0f, 168.0f, 12.0f,  112.0f});

    phys.AddStaticCollider("wall_porch_left",  {20.0f,  280.0f, 225.0f, 12.0f});
    phys.AddStaticCollider("wall_porch_right", {325.0f, 280.0f, 295.0f, 12.0f});

    phys.AddStaticCollider("bar_counter",      {220.0f, 74.0f,  176.0f, 18.0f});
    phys.AddStaticCollider("prop_jukebox",     {146.0f, 38.0f,  18.0f,  24.0f});

    phys.AddStaticCollider("booth_1",          {158.0f, 138.0f, 28.0f,  18.0f});
    phys.AddStaticCollider("booth_2",          {158.0f, 218.0f, 28.0f,  18.0f});
    phys.AddStaticCollider("booth_3",          {270.0f, 138.0f, 28.0f,  18.0f});
    phys.AddStaticCollider("booth_4",          {270.0f, 218.0f, 28.0f,  18.0f});
    phys.AddStaticCollider("booth_5",          {378.0f, 138.0f, 28.0f,  18.0f});
    phys.AddStaticCollider("booth_table6",     {378.0f, 218.0f, 28.0f,  18.0f});

    phys.AddStaticCollider("kitchen_oven",     {476.0f, 40.0f,  56.0f,  22.0f});
    phys.AddStaticCollider("kitchen_fridge",   {560.0f, 38.0f,  44.0f,  26.0f});
    phys.AddStaticCollider("kitchen_prep",     {510.0f, 150.0f, 64.0f,  20.0f});
    phys.AddStaticCollider("corr_generator",   {28.0f,  92.0f,  26.0f,  26.0f});
    phys.AddStaticCollider("porch_dumpster",   {150.0f, 306.0f, 34.0f,  22.0f});

    std::string noteDlg =
        (nightNumber == 1) ? "note_mikhail_intro" :
        (nightNumber == 2) ? "n2_intro_diary" :
        (nightNumber == 3) ? "n3_intro_diary" :
        (nightNumber == 4) ? "n4_intro_diary" :
                             "n5_intro_diary";

    std::string mirrorDlg = (nightNumber >= 3) ? "n3_mirror_inspect" : "n2_mirror_inspect";
    std::string windowDlg = (nightNumber >= 3) ? "n3_window_hands_inspect" : "backdoor_window_inspect";

    phys.AddTrigger({"trig_note",     "Касса",               noteDlg,               {222.0f, 66.0f,  32.0f, 36.0f}, true});
    phys.AddTrigger({"trig_visor",    "Визор «ОКО-86» [V]",  "visor_inspect",       {262.0f, 66.0f,  28.0f, 36.0f}, true});
    phys.AddTrigger({"trig_coffee",   "Кофемашина",          "coffee_machine_use",  {302.0f, 66.0f,  32.0f, 36.0f}, true});
    phys.AddTrigger({"trig_radio",    "Радиоприёмник",       "radio_0214",          {352.0f, 66.0f,  34.0f, 36.0f}, true});

    phys.AddTrigger({"trig_jukebox",  "Музыкальный автомат", "n4_jukebox_haunted",  {140.0f, 34.0f,  30.0f, 38.0f}, true});
    phys.AddTrigger({"trig_rules",    "Правила кафе",        "wall_rules_night1",   {172.0f, 24.0f,  26.0f, 26.0f}, true});
    phys.AddTrigger({"trig_phone",    "Телефон на стене",    "n4_phone_ring_talk",  {200.0f, 24.0f,  26.0f, 26.0f}, true});
    phys.AddTrigger({"trig_cat",      "Кот Уголёк",          "cat_ugolyok_talk",    {398.0f, 68.0f,  34.0f, 34.0f}, true});

    phys.AddTrigger({"trig_oven",     "Печь и Хозблок",      "pie_oven_use",        {470.0f, 36.0f,  68.0f, 36.0f}, true});
    phys.AddTrigger({"trig_fridge",   "Морозильная камера",  "n3_fridge_inspect",   {552.0f, 36.0f,  58.0f, 40.0f}, true});

    phys.AddTrigger({"trig_mirror",   "Настенное зеркало",   mirrorDlg,             {52.0f,  24.0f,  40.0f, 30.0f}, true});
    phys.AddTrigger({"trig_breaker",  "Электрощиток",        "n2_fusebox_use",      {22.0f,  84.0f,  42.0f, 44.0f}, true});
    phys.AddTrigger({"trig_basement", "Люк в подвал",        "n3_basement_inspect", {48.0f,  212.0f, 38.0f, 38.0f}, true});

    phys.AddTrigger({"trig_backdoor", "Уличный контейнер",   "backdoor_window_inspect", {142.0f, 298.0f, 52.0f, 40.0f}, true});
    phys.AddTrigger({"trig_window",   "Окно на шоссе",       windowDlg,             {200.0f, 260.0f, 44.0f, 26.0f}, true});

    phys.AddTrigger({"trig_booth1",   "Стол №1",             "",                    {148.0f, 122.0f, 48.0f, 46.0f}, false});
    phys.AddTrigger({"trig_booth2",   "Стол №2",             "",                    {148.0f, 202.0f, 48.0f, 46.0f}, false});
    phys.AddTrigger({"trig_booth3",   "Стол №3",             "",                    {260.0f, 122.0f, 48.0f, 46.0f}, false});
    phys.AddTrigger({"trig_booth4",   "Стол №4",             "",                    {260.0f, 202.0f, 48.0f, 46.0f}, false});
    phys.AddTrigger({"trig_booth5",   "Стол №5",             "",                    {368.0f, 122.0f, 48.0f, 46.0f}, false});
    phys.AddTrigger({"trig_table6",   "Стол №6",             "",                    {368.0f, 202.0f, 48.0f, 46.0f}, false});
}

void CafeWorld::BuildLighting(engine::core::Engine& eng, int nightNumber) {
    auto& light = eng.Lighting();
    light.ClearLights();
    Color amb = (nightNumber == 1) ? Color{135, 112, 98, 255}
              : (nightNumber == 2) ? Color{110, 94, 90, 255}
                                   : Color{84, 72, 84, 255};
    light.SetAmbientColor(amb);

    light.AddPointLight({"lamp_counter",  {300.0f, 84.0f},  115.0f, Color{244, 184, 96, 255}, 1.0f,  2.5f, 0.03f, true});
    light.AddPointLight({"lamp_hall_L",   {172.0f, 175.0f}, 95.0f,  Color{244, 184, 96, 255}, 0.90f, 1.8f, 0.02f, true});
    light.AddPointLight({"lamp_hall_C",   {284.0f, 175.0f}, 95.0f,  Color{244, 184, 96, 255}, 0.88f, 2.1f, 0.03f, true});
    light.AddPointLight({"lamp_hall_R",   {392.0f, 175.0f}, 90.0f,  Color{244, 184, 96, 255}, 0.82f, 3.2f, 0.04f, true});
    light.AddPointLight({"lamp_jukebox",  {155.0f, 52.0f},  60.0f,  Color{214, 69,  69, 255}, 0.70f, 6.0f, 0.08f, true});
    light.AddPointLight({"lamp_kitchen",  {535.0f, 120.0f}, 105.0f, Color{216, 230, 242, 255},0.85f, 4.0f, 0.05f, true});
    light.AddPointLight({"lamp_corridor", {72.0f,  135.0f}, 85.0f,  Color{143, 169, 196, 255},0.75f, 5.0f, 0.08f, true});
    light.AddPointLight({"lamp_porch",    {285.0f, 310.0f}, 80.0f,  Color{244, 184, 96, 255}, 0.70f, 3.5f, 0.06f, true});

    auto& fl = light.GetFlashlight();
    fl.fovDeg = 65.0f;
    fl.range = 105.0f;
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
                       const std::vector<GuestNpc>& guests) const {
    auto& assets = eng.Assets();
    Texture2D tsCafe    = assets.GetTexture("sprites/tilesets/ts_cafe_interior.png");
    Texture2D tsKitchen = assets.GetTexture("sprites/tilesets/ts_kitchen_storage.png");
    Texture2D tsBase    = assets.GetTexture("sprites/tilesets/ts_basement.png");
    Texture2D tsExt     = assets.GetTexture("sprites/tilesets/ts_exterior_highway.png");

    engine::math::Vec2 camOffset = ComputeCameraOffset(playerBody.Center());
    eng.Lighting().SetCameraOffset(camOffset);

    Camera2D cam{};
    cam.offset = {0.0f, 0.0f};
    cam.target = {camOffset.x, camOffset.y};
    cam.rotation = 0.0f;
    cam.zoom = 1.0f;

    BeginMode2D(cam);

    for (int ty = 0; ty < 23; ++ty) {
        for (int tx = 0; tx < 40; ++tx) {
            float wx = static_cast<float>(tx * 16);
            float wy = static_cast<float>(ty * 16);
            if (ty >= 18) {
                Rectangle src = {static_cast<float>((tx % 4) * 16), static_cast<float>((ty % 4) * 16), 16.0f, 16.0f};
                DrawTextureRec(tsExt, src, Vector2{wx, wy}, WHITE);
            } else if (tx < 8) {
                Rectangle src = {static_cast<float>((tx % 4) * 16), static_cast<float>((ty % 4) * 16), 16.0f, 16.0f};
                DrawTextureRec(tsBase, src, Vector2{wx, wy}, WHITE);
            } else if (tx >= 28) {
                Rectangle src = {static_cast<float>((tx % 4) * 16), static_cast<float>((ty % 4) * 16), 16.0f, 16.0f};
                DrawTextureRec(tsKitchen, src, Vector2{wx, wy}, WHITE);
            } else {
                int rowSrc = (ty < 3) ? 8 : ((ty < 14) ? 0 : 4);
                Rectangle src = {static_cast<float>((tx % 8) * 16), static_cast<float>(rowSrc * 16), 16.0f, 16.0f};
                DrawTextureRec(tsCafe, src, Vector2{wx, wy}, WHITE);
            }
        }
    }

    DrawRectangle(0,   0,   640, 34,  Color{42, 23, 21, 255});
    DrawRectangle(0,   344, 640, 16,  Color{24, 16, 20, 255});
    DrawRectangle(0,   0,   20,  360, Color{42, 23, 21, 255});
    DrawRectangle(620, 0,   20,  360, Color{42, 23, 21, 255});

    DrawRectangle(124, 34,  12,  86,  Color{72, 36, 26, 255});
    DrawRectangle(124, 188, 12,  92,  Color{72, 36, 26, 255});
    DrawRectangle(444, 34,  12,  60,  Color{72, 36, 26, 255});
    DrawRectangle(444, 168, 12,  112, Color{72, 36, 26, 255});
    DrawRectangle(20,  280, 225, 12,  Color{58, 30, 24, 255});
    DrawRectangle(325, 280, 295, 12,  Color{58, 30, 24, 255});

    Texture2D texMirror = assets.GetTexture("sprites/props/prop_mirror_wall.png");
    Texture2D texGen    = assets.GetTexture("sprites/props/prop_generator.png");
    Texture2D texJuke   = assets.GetTexture("sprites/props/prop_jukebox.png");
    Texture2D texCoffee = assets.GetTexture("sprites/props/prop_coffee_machine.png");
    Texture2D texBooth  = assets.GetTexture("sprites/props/prop_table_booth.png");
    Texture2D texTable6 = assets.GetTexture("sprites/props/prop_table_06.png");
    Texture2D texCat    = assets.GetTexture("sprites/characters/spr_cat_ugolyok.png");

    DrawTexture(texMirror, 56, 16, WHITE);
    DrawTexture(texGen,    28, 92, WHITE);
    DrawRectangle(52, 216, 30, 30, Color{32, 20, 18, 255});
    DrawRectangleLines(52, 216, 30, 30, Color{196, 122, 59, 255});

    DrawTexture(texJuke, 146, 36, WHITE);
    DrawRectangle(176, 18, 18, 16, Color{232, 227, 217, 255});
    DrawRectangleLines(176, 18, 18, 16, Color{107, 51, 34, 255});

    DrawRectangle(204, 18, 16, 16, Color{180, 42, 42, 255});
    DrawRectangleLines(204, 18, 16, 16, Color{244, 184, 96, 255});

    DrawRectangle(220, 74, 176, 18, Color{107, 51, 34, 255});
    DrawRectangle(220, 72, 176, 4,  Color{196, 122, 59, 255});
    DrawRectangle(228, 70, 18,  14, Color{216, 194, 162, 255});

    DrawRectangle(268, 72, 14,  10, Color{40, 180, 130, 255});
    DrawRectangleLines(268, 72, 14, 10, Color{244, 184, 96, 255});

    DrawTexture(texCoffee, 304, 64, WHITE);
    DrawRectangle(358, 72, 20, 12, Color{52, 34, 28, 255});
    DrawRectangleLines(358, 72, 20, 12, Color{214, 69, 69, 255});

    int catFrame = (lightsFlickering || blackoutMode || nightNumber >= 3) ? 1 : 0;
    DrawTextureRec(texCat, Rectangle{static_cast<float>(catFrame * 16), 0.0f, 16.0f, 16.0f}, Vector2{404.0f, 74.0f}, WHITE);

    DrawRectangle(476, 40, 56, 22, Color{80, 86, 98, 255});
    DrawRectangleLines(476, 40, 56, 22, Color{196, 122, 59, 255});
    DrawRectangle(560, 38, 44, 26, Color{143, 169, 196, 255});
    DrawRectangleLines(560, 38, 44, 26, Color{42, 23, 21, 255});
    DrawRectangle(510, 150, 64, 20, Color{120, 130, 142, 255});

    DrawRectangle(150, 306, 34, 22, Color{46, 68, 52, 255});
    DrawRectangleLines(150, 306, 34, 22, Color{143, 169, 196, 255});
    DrawRectangle(204, 276, 36, 6,  Color{143, 169, 196, 220});

    DrawTexture(texBooth,  156, 134, WHITE);
    DrawTexture(texBooth,  156, 214, WHITE);
    DrawTexture(texBooth,  268, 134, WHITE);
    DrawTexture(texBooth,  268, 214, WHITE);
    DrawTexture(texBooth,  376, 134, WHITE);
    DrawTexture(texTable6, 376, 214, WHITE);

    for (const auto& g : guests) {
        if (!g.spawned) continue;
        Texture2D gTex = assets.GetTexture(g.spritePath);
        float shadowOffsetX = g.anomalous ? -5.0f : 3.0f;
        DrawEllipse(static_cast<int>(g.pos.x + 16.0f + shadowOffsetX),
                    static_cast<int>(g.pos.y + 28.0f), 9.0f, 4.0f, Color{10, 13, 26, 165});

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

    EndMode2D();
}

void CafeWorld::Render3DVisor(engine::core::Engine& eng,
                              int nightNumber,
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
    BeginMode3D(cam);

    DrawPlane(Vector3{16.0f, 0.0f, 9.0f}, Vector2{32.0f, 18.0f}, Color{42, 24, 22, 255});
    for (int gx = 0; gx <= 32; gx += 2) {
        DrawLine3D(Vector3{static_cast<float>(gx), 0.01f, 0.0f},
                   Vector3{static_cast<float>(gx), 0.01f, 18.0f}, Color{68, 38, 34, 180});
    }
    for (int gz = 0; gz <= 18; gz += 2) {
        DrawLine3D(Vector3{0.0f, 0.01f, static_cast<float>(gz)},
                   Vector3{32.0f, 0.01f, static_cast<float>(gz)}, Color{68, 38, 34, 180});
    }
    DrawCube(Vector3{16.0f, 2.55f, 9.0f}, 32.0f, 0.1f, 18.0f, Color{18, 14, 24, 255});

    for (const auto& c : eng.Physics().GetColliders()) {
        float cx = (c.bounds.x + c.bounds.w * 0.5f) * S;
        float cz = (c.bounds.y + c.bounds.h * 0.5f) * S;
        float cw = c.bounds.w * S;
        float cd = c.bounds.h * S;

        bool isWall = (c.id.rfind("wall_", 0) == 0);
        float ch = isWall ? 2.5f : 0.95f;
        Color col = isWall ? Color{86, 42, 32, 255} : Color{148, 82, 46, 255};
        if (c.id == "booth_table6") col = Color{145, 24, 30, 255};
        if (c.id == "kitchen_fridge") col = Color{110, 145, 175, 255};

        DrawCube(Vector3{cx, ch * 0.5f, cz}, cw, ch, cd, col);
        DrawCubeWires(Vector3{cx, ch * 0.5f, cz}, cw, ch, cd, Color{28, 14, 16, 255});
    }

    float t = static_cast<float>(GetTime());
    for (const auto& g : guests) {
        if (!g.spawned) continue;
        Vector3 gPos = {(g.pos.x + 16.0f) * S, 0.95f, (g.pos.y + 16.0f) * S};
        Texture2D gTex = eng.Assets().GetTexture(g.spritePath);
        Rectangle src = {static_cast<float>((g.spriteFrame % 6) * 32), 0.0f, 32.0f, 32.0f};
        if (src.x + 32.0f > static_cast<float>(gTex.width)) src.x = 0.0f;

        Color tint = g.anomalous ? Color{255, 70, 70, 255} : Color{210, 245, 225, 255};
        DrawBillboardRec(cam, gTex, src, gPos, Vector2{1.5f, 1.8f}, tint);

        if (g.anomalous) {
            float pulse = 0.42f + 0.14f * std::sin(t * 10.0f);
            DrawSphereWires(Vector3{gPos.x, gPos.y + 0.45f, gPos.z}, pulse, 8, 8, Color{255, 45, 45, 245});
            DrawCubeWires(Vector3{gPos.x, 1.0f, gPos.z}, 1.2f, 2.0f, 1.2f, Color{255, 30, 30, 255});
        }
    }

    Texture2D reaperTex = eng.Assets().GetTexture("sprites/screamers/sc_night9_shadow_reaper.png");
    Texture2D skullTex  = eng.Assets().GetTexture("sprites/screamers/sc_night14_skull_glitch.png");

    for (const auto& a : anomalies) {
        if (!a.active) continue;
        Vector3 aPos = {a.pos.x * S, a.heightY + std::sin(t * 4.5f + static_cast<float>(a.id)) * 0.12f, a.pos.y * S};
        Color flashCol = (a.hitFlash > 0.0f) ? Color{255, 255, 255, 255} : Color{255, 45, 55, 255};

        if (a.type == 0) {
            float r = 0.42f + 0.08f * std::sin(t * 8.0f);
            DrawSphere(aPos, r * 0.55f, flashCol);
            DrawSphereWires(aPos, r, 8, 8, Color{255, 180, 90, 240});
            DrawCubeWires(aPos, r * 1.6f, r * 1.6f, r * 1.6f, flashCol);
        } else if (a.type == 1) {
            DrawBillboard(cam, reaperTex, Vector3{aPos.x, 1.05f, aPos.z}, 1.75f, flashCol);
            DrawCubeWires(Vector3{aPos.x, 1.05f, aPos.z}, 1.1f, 2.0f, 1.1f, Color{255, 40, 40, 220});
        } else {
            DrawBillboard(cam, skullTex, aPos, 1.35f, flashCol);
            DrawSphereWires(aPos, 0.55f, 6, 6, Color{244, 184, 96, 240});
        }
    }

    if (gunRecoilTimer > 0.08f) {
        Vector3 right = {-forward.z, 0.0f, forward.x};
        Vector3 muzzle = {
            camPos.x + forward.x * 0.6f + right.x * 0.22f,
            camPos.y - 0.18f + forward.y * 0.6f,
            camPos.z + forward.z * 0.6f + right.z * 0.22f
        };
        Vector3 beamEnd = {
            camPos.x + forward.x * 16.0f,
            camPos.y + forward.y * 16.0f,
            camPos.z + forward.z * 16.0f
        };
        DrawLine3D(muzzle, beamEnd, Color{110, 255, 235, 255});
        DrawSphere(beamEnd, 0.15f, Color{244, 184, 96, 255});
    }

    if (nightNumber >= 2) {
        DrawCube(Vector3{3.6f, 1.3f, 1.9f}, 0.8f, 0.8f, 0.08f, Color{180, 25, 30, 220});
    }

    EndMode3D();

    int bobX = static_cast<int>(std::cos(walkBobTimer * 1.1f) * 5.0f);
    int bobY2D = static_cast<int>(std::abs(std::sin(walkBobTimer * 2.2f)) * 6.0f);
    int recoilOff = static_cast<int>(gunRecoilTimer * 75.0f);

    int gx = 218 + bobX + recoilOff / 2;
    int gy = 128 + bobY2D + recoilOff;

    if (gunRecoilTimer > 0.08f) {
        DrawCircle(gx + 10, gy - 6, 16.0f, Color{120, 255, 240, 210});
        DrawCircle(gx + 10, gy - 6, 9.0f, Color{255, 245, 190, 255});
    }

    DrawRectangle(gx, gy, 32, 56, Color{38, 44, 56, 255});
    DrawRectangleLines(gx, gy, 32, 56, Color{110, 235, 215, 255});
    DrawRectangle(gx + 6, gy - 12, 16, 16, Color{65, 76, 96, 255});
    DrawRectangle(gx + 9, gy - 10, 10, 28, Color{90, 245, 215, 255});

    DrawRectangleLinesEx(Rectangle{6, 6, 308, 168}, 2.0f, Color{90, 220, 160, 180});
    DrawLine(152, 90, 168, 90, Color{90, 255, 180, 230});
    DrawLine(160, 82, 160, 98, Color{90, 255, 180, 230});
}

}
