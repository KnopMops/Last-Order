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

    phys.AddStaticCollider("wall_top",         {0.0f,   0.0f,   640.0f, 36.0f});
    phys.AddStaticCollider("wall_bottom",      {0.0f,   344.0f, 640.0f, 16.0f});
    phys.AddStaticCollider("wall_left",        {0.0f,   0.0f,   20.0f,  360.0f});
    phys.AddStaticCollider("wall_right",       {620.0f, 0.0f,   20.0f,  360.0f});

    phys.AddStaticCollider("wall_corr_top",    {124.0f, 36.0f,  12.0f,  90.0f});
    phys.AddStaticCollider("wall_corr_bot",    {124.0f, 184.0f, 12.0f,  96.0f});

    phys.AddStaticCollider("wall_kitch_top",   {444.0f, 36.0f,  12.0f,  64.0f});
    phys.AddStaticCollider("wall_kitch_bot",   {444.0f, 164.0f, 12.0f,  116.0f});

    phys.AddStaticCollider("wall_porch_left",  {20.0f,  280.0f, 230.0f, 12.0f});
    phys.AddStaticCollider("wall_porch_right", {320.0f, 280.0f, 300.0f, 12.0f});

    phys.AddStaticCollider("bar_counter",      {220.0f, 74.0f,  176.0f, 20.0f});
    phys.AddStaticCollider("prop_jukebox",     {146.0f, 38.0f,  18.0f,  28.0f});

    phys.AddStaticCollider("booth_1",          {156.0f, 134.0f, 32.0f,  24.0f});
    phys.AddStaticCollider("booth_2",          {156.0f, 214.0f, 32.0f,  24.0f});
    phys.AddStaticCollider("booth_3",          {268.0f, 134.0f, 32.0f,  24.0f});
    phys.AddStaticCollider("booth_4",          {268.0f, 214.0f, 32.0f,  24.0f});
    phys.AddStaticCollider("booth_5",          {376.0f, 134.0f, 32.0f,  24.0f});
    phys.AddStaticCollider("booth_table6",     {376.0f, 214.0f, 32.0f,  24.0f});

    phys.AddStaticCollider("kitchen_oven",     {476.0f, 40.0f,  56.0f,  24.0f});
    phys.AddStaticCollider("kitchen_fridge",   {560.0f, 38.0f,  44.0f,  28.0f});
    phys.AddStaticCollider("kitchen_prep",     {510.0f, 150.0f, 64.0f,  24.0f});
    phys.AddStaticCollider("corr_generator",   {28.0f,  92.0f,  28.0f,  28.0f});
    phys.AddStaticCollider("porch_dumpster",   {150.0f, 306.0f, 36.0f,  24.0f});

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

    DrawRectangle(0,   0,   640, 36,  Color{42, 23, 21, 255});
    DrawRectangle(0,   32,  640, 4,   Color{196, 122, 59, 255});
    DrawRectangle(0,   0,   20,  360, Color{42, 23, 21, 255});
    DrawRectangle(620, 0,   20,  360, Color{42, 23, 21, 255});
    DrawRectangle(0,   344, 640, 16,  Color{18, 22, 34, 255});

    DrawRectangle(124, 36,  12,  90,  Color{68, 34, 26, 255});
    DrawRectangle(124, 184, 12,  96,  Color{68, 34, 26, 255});
    DrawRectangleLines(124, 36, 12, 90, Color{196, 122, 59, 255});
    DrawRectangleLines(124, 184, 12, 96, Color{196, 122, 59, 255});

    DrawRectangle(444, 36,  12,  64,  Color{68, 34, 26, 255});
    DrawRectangle(444, 164, 12,  116, Color{68, 34, 26, 255});
    DrawRectangleLines(444, 36, 12, 64, Color{196, 122, 59, 255});
    DrawRectangleLines(444, 164, 12, 116, Color{196, 122, 59, 255});

    DrawRectangle(20,  280, 230, 12,  Color{42, 23, 21, 255});
    DrawRectangle(320, 280, 300, 12,  Color{42, 23, 21, 255});
    DrawRectangle(170, 282, 68,  8,   Color{143, 169, 196, 190});
    DrawRectangle(345, 282, 68,  8,   Color{143, 169, 196, 190});
    DrawRectangle(250, 282, 70,  6,   Color{196, 122, 59, 180});

    DrawRectangle(220, 74, 176, 20, Color{107, 51, 34, 255});
    DrawRectangle(220, 74, 176, 5,  Color{196, 122, 59, 255});
    DrawRectangleLines(220, 74, 176, 20, Color{42, 23, 21, 255});

    Texture2D booth  = assets.GetTexture("sprites/props/prop_table_booth.png");
    Texture2D booth6 = assets.GetTexture("sprites/props/prop_table_06.png");
    Texture2D jb     = assets.GetTexture("sprites/props/prop_jukebox.png");
    Texture2D cm     = assets.GetTexture("sprites/props/prop_coffee_machine.png");
    Texture2D mir    = assets.GetTexture("sprites/props/prop_mirror_wall.png");
    Texture2D gen    = assets.GetTexture("sprites/props/prop_generator.png");

    DrawTexture(jb,  146, 38, WHITE);
    DrawTexture(mir, 64,  4,  WHITE);
    DrawTexture(gen, 28,  92, WHITE);

    DrawRectangle(230, 72, 16, 12, Color{216, 194, 162, 255});
    DrawRectangle(270, 73, 14, 10, Color{59,  88,  130, 255});
    DrawCircle(277, 78, 3.0f, Color{214, 69, 69, 255});
    DrawTexture(cm,  312, 70, WHITE);
    DrawRectangle(362, 74, 16, 10, Color{42,  23,  21,  255});

    DrawRectangle(180, 16, 14, 16, Color{232, 227, 217, 255});
    DrawRectangle(206, 16, 12, 16, Color{168, 36,  36,  255});
    DrawRectangle(209, 19, 6,  10, Color{25,  25,  30,  255});

    DrawTexture(booth,  156, 130, WHITE);
    DrawTexture(booth,  156, 210, WHITE);
    DrawTexture(booth,  268, 130, WHITE);
    DrawTexture(booth,  268, 210, WHITE);
    DrawTexture(booth,  376, 130, WHITE);
    DrawTexture(booth6, 376, 210, WHITE);

    DrawRectangle(476, 40, 56, 24, Color{175, 188, 200, 255});
    DrawRectangleLines(476, 40, 56, 24, Color{59, 88, 130, 255});
    DrawRectangle(484, 46, 18, 12, Color{244, 184, 96, 220});

    DrawRectangle(560, 38, 44, 28, Color{143, 169, 196, 255});
    DrawRectangleLines(560, 38, 44, 28, Color{27, 40, 69, 255});
    DrawRectangle(566, 48, 5,  8,  Color{42, 23, 21, 255});

    DrawRectangle(510, 150, 64, 24, Color{130, 140, 150, 255});
    DrawRectangleLines(510, 150, 64, 24, Color{42, 23, 21, 255});

    DrawRectangle(54, 218, 26, 24, Color{72, 36, 26, 255});
    DrawRectangleLines(54, 218, 26, 24, Color{244, 184, 96, 200});
    DrawLine(54, 230, 80, 230, Color{42, 23, 21, 255});

    DrawRectangle(150, 306, 36, 24, Color{45, 72, 54, 255});
    DrawRectangleLines(150, 306, 36, 24, Color{20, 35, 25, 255});

    float rt = static_cast<float>(GetTime());
    if (nightNumber >= 2) {
        for (int i = 0; i < 36; ++i) {
            int rx = (i * 37 + static_cast<int>(rt * 65.0f)) % 620;
            int ry = 294 + ((i * 19 + static_cast<int>(rt * 180.0f)) % 46);
            DrawLine(rx + 2, ry, rx, ry + 6, Color{143, 169, 196, 145});
        }
    }

    if (!eng.Dialogue().IsActive() && nearestTrigger.has_value()) {
        DrawRectangleLinesEx(nearestTrigger->bounds, 1.0f, Color{244, 184, 96, 175});
    }

    Texture2D leraTex = assets.GetTexture("sprites/characters/spr_lera.png");
    if (playerBody.position.x > 36.0f && playerBody.position.x < 104.0f && playerBody.position.y < 78.0f) {
        bool eyelessAnomaly = eng.State().HasFlag("event_0300_triggered") || blackoutMode || eng.State().GetSanity() < 45;
        Rectangle reflSrc = {eyelessAnomaly ? 64.0f : 0.0f, eyelessAnomaly ? 96.0f : 0.0f, 32.0f, 32.0f};
        DrawTextureRec(leraTex, reflSrc, Vector2{56.0f, 4.0f}, Color{143, 169, 196, 185});
    }

    Texture2D catTex = assets.GetTexture("sprites/characters/spr_cat_ugolyok.png");
    int catRow = (lightsFlickering || eng.State().HasFlag("radio_0214_ringing") || blackoutMode) ? 2 : 0;
    Rectangle catSrc = {0.0f, static_cast<float>(catRow * 16), 16.0f, 16.0f};
    DrawTextureRec(catTex, catSrc, Vector2{406.0f, 76.0f}, WHITE);

    for (const auto& g : guests) {
        if (!g.spawned) continue;
        Texture2D gTex = assets.GetTexture(g.spritePath);
        float srcX = static_cast<float>((g.spriteFrame % 6) * 32);
        if (srcX + 32.0f > static_cast<float>(gTex.width)) srcX = 0.0f;
        Rectangle gSrc = {srcX, 0.0f, 32.0f, 32.0f};
        DrawTextureRec(gTex, gSrc, g.pos.SnapToPixel(), WHITE);
    }

    bool carrying = !eng.State().GetHeldItem().empty();
    int animStep = isMoving ? (static_cast<int>(walkAnimTimer) % 4) : 0;
    int leraRow = carrying ? 2 : (isMoving ? 1 : 0);
    int leraCol = isMoving ? animStep : playerFacing;
    Rectangle leraSrc = {static_cast<float>(leraCol * 32), static_cast<float>(leraRow * 32), 32.0f, 32.0f};
    DrawTextureRec(leraTex, leraSrc, playerBody.position.SnapToPixel(), WHITE);

    if (showDebugPhysics) {
        for (const auto& c : eng.Physics().GetColliders()) {
            DrawRectangleLinesEx(c.bounds, 1.0f, Color{60, 230, 90, 230});
        }
        for (const auto& t : eng.Physics().GetTriggers()) {
            Color tc = t.enabled ? Color{244, 184, 96, 230} : Color{140, 140, 140, 120};
            DrawRectangleLinesEx(t.bounds, 1.0f, tc);
        }
        DrawRectangleLinesEx(playerBody.WorldBounds(), 1.0f, SKYBLUE);
    }

    EndMode2D();
}

void CafeWorld::Render3DVisor(engine::core::Engine& eng,
                              int nightNumber,
                              engine::math::Vec2 playerCenter,
                              float visorYawRad,
                              float visorPitchRad,
                              const std::vector<GuestNpc>& guests) const {
    constexpr float S = 0.05f;
    Vector3 camPos = {playerCenter.x * S, 1.15f, playerCenter.y * S};
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

    ClearBackground(Color{8, 10, 18, 255});
    BeginMode3D(cam);

    DrawPlane(Vector3{16.0f, 0.0f, 9.0f}, Vector2{32.0f, 18.0f}, Color{36, 20, 18, 255});
    DrawCube(Vector3{16.0f, 2.5f, 9.0f}, 32.0f, 0.1f, 18.0f, Color{18, 14, 22, 255});

    for (const auto& c : eng.Physics().GetColliders()) {
        float cx = (c.bounds.x + c.bounds.w * 0.5f) * S;
        float cz = (c.bounds.y + c.bounds.h * 0.5f) * S;
        float cw = c.bounds.w * S;
        float cd = c.bounds.h * S;

        bool isWall = (c.id.rfind("wall_", 0) == 0);
        float ch = isWall ? 2.4f : 0.95f;
        Color col = isWall ? Color{78, 38, 28, 255} : Color{140, 78, 42, 255};
        if (c.id == "booth_table6") col = Color{130, 22, 28, 255};
        if (c.id == "kitchen_fridge") col = Color{110, 145, 175, 255};

        DrawCube(Vector3{cx, ch * 0.5f, cz}, cw, ch, cd, col);
        DrawCubeWires(Vector3{cx, ch * 0.5f, cz}, cw, ch, cd, Color{24, 12, 14, 255});
    }

    float t = static_cast<float>(GetTime());
    for (const auto& g : guests) {
        if (!g.spawned) continue;
        Vector3 gPos = {(g.pos.x + 16.0f) * S, 0.95f, (g.pos.y + 16.0f) * S};
        Texture2D gTex = eng.Assets().GetTexture(g.spritePath);
        Rectangle src = {static_cast<float>((g.spriteFrame % 6) * 32), 0.0f, 32.0f, 32.0f};
        if (src.x + 32.0f > static_cast<float>(gTex.width)) src.x = 0.0f;

        Color tint = g.anomalous ? Color{255, 65, 65, 255} : Color{210, 240, 220, 255};
        DrawBillboardRec(cam, gTex, src, gPos, Vector2{1.5f, 1.8f}, tint);

        if (g.anomalous) {
            float pulse = 0.35f + 0.15f * std::sin(t * 10.0f);
            DrawSphereWires(Vector3{gPos.x, gPos.y + 0.6f, gPos.z}, pulse, 6, 6, Color{214, 40, 40, 230});
            DrawCube(Vector3{gPos.x, 1.95f, gPos.z}, 0.25f, 0.25f, 0.25f, Color{255, 30, 30, 255});
        }
    }

    DrawSphereWires(Vector3{18.8f, 1.1f, 10.8f}, 0.45f + 0.1f * std::sin(t * 6.0f), 8, 8, Color{214, 45, 45, 200});
    if (nightNumber >= 2) {
        DrawCube(Vector3{3.6f, 1.3f, 1.9f}, 0.8f, 0.8f, 0.08f, Color{180, 25, 30, 220});
    }

    EndMode3D();

    DrawRectangleLinesEx(Rectangle{8, 8, 304, 164}, 2.0f, Color{90, 220, 140, 180});
    DrawLine(152, 90, 168, 90, Color{90, 220, 140, 200});
    DrawLine(160, 82, 160, 98, Color{90, 220, 140, 200});
}

}
