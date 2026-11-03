#include "game/chapter2/MotelWorld3D.h"
#include "engine/graphics/TextLayout.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace game::chapter2 {

void MotelWorld3D::Init(engine::core::Engine& eng, int nightNumber) {
    m_nightNumber = nightNumber;
    m_globalTime = 0.0f;
    m_returnToMenu = false;
    m_stepSoundIndex = 0;

    m_camera.Init(Vector3{0.0f, 0.0f, 15.0f}, 3.14159f, 0.0f);
    m_camera.ResetMouseLook();

    auto& assets = eng.Assets();
    m_texWall     = assets.GetTexture("sprites/3d/motel_wall_wood.png");
    m_texCarpet   = assets.GetTexture("sprites/3d/motel_carpet.png");
    m_texTile     = assets.GetTexture("sprites/3d/motel_tile.png");
    m_texConcrete = assets.GetTexture("sprites/3d/motel_concrete.png");
    m_texDoor     = assets.GetTexture("sprites/3d/motel_door.png");
    m_texCeiling  = assets.GetTexture("sprites/3d/motel_ceiling.png");
    m_texNeon     = assets.GetTexture("sprites/3d/motel_neon.png");
    m_texKeyTag   = assets.GetTexture("sprites/3d/motel_key_tag.png");
    m_texMirror   = assets.GetTexture("sprites/3d/motel_mirror.png");
    m_texRadio    = assets.GetTexture("sprites/3d/motel_radio.png");

    SetTextureWrap(m_texWall, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(m_texCarpet, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(m_texTile, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(m_texConcrete, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(m_texCeiling, TEXTURE_WRAP_REPEAT);

    SetTextureFilter(m_texWall, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_texCarpet, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_texTile, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_texConcrete, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_texDoor, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_texCeiling, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_texNeon, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_texKeyTag, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_texMirror, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_texRadio, TEXTURE_FILTER_POINT);

    if (FileExists("assets/shaders/lit3d.vs") && FileExists("assets/shaders/lit3d.fs")) {
        m_litShader = LoadShader("assets/shaders/lit3d.vs", "assets/shaders/lit3d.fs");
        if (m_litShader.id > 0) {
            m_uViewPosLoc = GetShaderLocation(m_litShader, "uViewPos");
            m_uFlashlightPosLoc = GetShaderLocation(m_litShader, "uFlashlightPos");
            m_uFlashlightDirLoc = GetShaderLocation(m_litShader, "uFlashlightDir");
            m_uFlashlightColorLoc = GetShaderLocation(m_litShader, "uFlashlightColor");
            m_uFlashlightCutoffLoc = GetShaderLocation(m_litShader, "uFlashlightCutoff");
            m_uFlashlightOuterCutoffLoc = GetShaderLocation(m_litShader, "uFlashlightOuterCutoff");
            m_uFlashlightIntensityLoc = GetShaderLocation(m_litShader, "uFlashlightIntensity");
            m_uAmbientColorLoc = GetShaderLocation(m_litShader, "uAmbientColor");
            m_uFogColorLoc = GetShaderLocation(m_litShader, "uFogColor");
            m_uFogStartLoc = GetShaderLocation(m_litShader, "uFogStart");
            m_uFogEndLoc = GetShaderLocation(m_litShader, "uFogEnd");
            m_shaderLoaded = true;
        }
    }

    m_flashlightOn = true;
    m_flashlightBattery = 100.0f;
    m_flashlightFocus = false;
    m_flashlightFlickerTimer = 0.0f;

    m_hasKey101 = false;
    m_door101Open = false;
    m_inWardrobe = false;
    m_bellRings = 0;
    m_radioPlaying = false;
    m_radioPlaybackTimer = 0.0f;
    m_mirrorTriggered = false;
    m_mirrorAnomalyAlpha = 0.0f;

    m_inspectMode = false;
    m_inspectedItem = InteractiveType::None;
    m_inspectRotX = 0.0f;
    m_inspectRotY = 0.0f;

    m_bannerText = "ГЛАВА 2: МОТЕЛЬ «СЕВЕРНОЕ СИЯНИЕ». НАЙДИТЕ УКРЫТИЕ ОТ ДОЖДЯ В РЕГИСТРАТУРЕ.";
    m_bannerTimer = 6.0f;

    BuildCollidersAndInteractives();

    m_rain.clear();
    for (int i = 0; i < 90; ++i) {
        RainDrop3D drop;
        drop.pos = Vector3{
            static_cast<float>((std::rand() % 320) - 160) * 0.1f,
            static_cast<float>(std::rand() % 90) * 0.1f + 1.0f,
            static_cast<float>((std::rand() % 220) - 20) * 0.1f + 8.0f
        };
        drop.speed = 14.0f + static_cast<float>(std::rand() % 6);
        drop.len = 0.35f + static_cast<float>(std::rand() % 3) * 0.1f;
        m_rain.push_back(drop);
    }

    eng.Audio().PlayAmbient("audio/bgm/amb_cafe_tension_lp.ogg", 0.65f);
    DisableCursor();
}

void MotelWorld3D::Shutdown(engine::core::Engine& ) {
    if (m_shaderLoaded) {
        UnloadShader(m_litShader);
        m_shaderLoaded = false;
    }
    EnableCursor();
}

void MotelWorld3D::BuildCollidersAndInteractives() {
    m_colliders.clear();
    m_interactives.clear();

    m_colliders.push_back({Vector3{-18.0f, 0.0f, 26.0f}, Vector3{18.0f, 6.0f, 27.0f}});
    m_colliders.push_back({Vector3{-18.0f, 0.0f, 7.5f},  Vector3{-2.5f, 5.0f, 8.5f}});
    m_colliders.push_back({Vector3{2.5f,   0.0f, 7.5f},  Vector3{18.0f, 5.0f, 8.5f}});

    m_colliders.push_back({Vector3{-10.5f, 0.0f, -12.0f}, Vector3{-9.5f, 4.0f, 8.0f}});
    m_colliders.push_back({Vector3{9.5f,   0.0f, -12.0f}, Vector3{10.5f, 4.0f, 8.0f}});
    m_colliders.push_back({Vector3{-10.0f, 0.0f, -12.5f}, Vector3{10.0f, 4.0f, -11.5f}});

    m_colliders.push_back({Vector3{1.5f, 0.0f, 0.2f}, Vector3{6.8f, 1.2f, 2.5f}});

    m_colliders.push_back({Vector3{-5.5f, 0.0f, 8.0f}, Vector3{5.5f, 3.5f, 8.5f}});

    m_colliders.push_back({Vector3{-7.8f, 0.0f, -11.0f}, Vector3{-7.2f, 3.5f, -2.5f}});
    m_colliders.push_back({Vector3{-18.5f, 0.0f, -12.0f}, Vector3{-17.5f, 3.5f, -2.0f}});
    m_colliders.push_back({Vector3{-18.0f, 0.0f, -12.5f}, Vector3{-7.5f, 3.5f, -11.5f}});
    m_colliders.push_back({Vector3{-18.0f, 0.0f, -2.5f}, Vector3{-7.5f, 3.5f, -1.8f}});

    m_colliders.push_back({Vector3{-15.0f, 0.0f, -9.5f}, Vector3{-12.0f, 0.9f, -6.0f}});
    m_colliders.push_back({Vector3{-16.2f, 0.0f, -5.7f}, Vector3{-13.6f, 2.5f, -3.0f}});

    m_interactives.push_back({
        InteractiveType::EntranceDoor,
        {Vector3{-1.8f, 0.0f, 7.2f}, Vector3{1.8f, 2.8f, 9.2f}},
        "Дверь в регистратуру мотеля",
        true
    });

    m_interactives.push_back({
        InteractiveType::DeskBell,
        {Vector3{2.0f, 1.0f, 0.8f}, Vector3{2.8f, 1.4f, 1.6f}},
        "Колокольчик на стойке вызова [Дзинь]",
        true
    });

    m_interactives.push_back({
        InteractiveType::Key101Tag,
        {Vector3{6.2f, 1.4f, 1.0f}, Vector3{7.2f, 2.2f, 1.8f}},
        "Ключ от номера 101 с изумрудным брелоком",
        true
    });

    m_interactives.push_back({
        InteractiveType::RadioTransistor,
        {Vector3{4.0f, 1.0f, 1.0f}, Vector3{4.8f, 1.5f, 1.8f}},
        "Радиоприёмник «Океан-214» [Частота 94.5 FM]",
        true
    });

    m_interactives.push_back({
        InteractiveType::DoorRoom101,
        {Vector3{-8.5f, 0.0f, -5.5f}, Vector3{-6.5f, 2.8f, -2.5f}},
        "Дверь номера 101",
        true
    });

    m_interactives.push_back({
        InteractiveType::WardrobeCloset,
        {Vector3{-16.4f, 0.0f, -5.8f}, Vector3{-13.4f, 2.6f, -3.0f}},
        "Платяной шкаф (Укрытие)",
        true
    });

    m_interactives.push_back({
        InteractiveType::BathroomMirror,
        {Vector3{-16.0f, 1.1f, -12.4f}, Vector3{-13.2f, 2.7f, -11.3f}},
        "Настенное зеркало в ванной",
        true
    });

    m_interactives.push_back({
        InteractiveType::HighwayCar,
        {Vector3{-7.5f, 0.0f, 14.5f}, Vector3{-3.5f, 2.0f, 19.5f}},
        "Ваша заглохшая машина (Аварийка мигает)",
        true
    });
}

void MotelWorld3D::UpdateRain(float dt) {
    for (auto& drop : m_rain) {
        drop.pos.y -= drop.speed * dt;
        if (drop.pos.y < 0.05f) {
            drop.pos.y = 8.5f + static_cast<float>(std::rand() % 20) * 0.1f;
            drop.pos.x = static_cast<float>((std::rand() % 320) - 160) * 0.1f;
            drop.pos.z = static_cast<float>((std::rand() % 220) - 20) * 0.1f + 8.0f;
        }
    }
}

void MotelWorld3D::Update(engine::core::Engine& eng, float dt) {
    m_globalTime += dt;

    if (m_bannerTimer > 0.0f) {
        m_bannerTimer -= dt;
    }

    if (m_inspectMode) {
        Vector2 md = GetMouseDelta();
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            m_inspectRotY += md.x * 0.02f;
            m_inspectRotX += md.y * 0.02f;
        }
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) {
            m_inspectMode = false;
            m_camera.ResetMouseLook();
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.6f);
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        m_returnToMenu = true;
        EnableCursor();
        return;
    }

    if (IsKeyPressed(KEY_F)) {
        m_flashlightOn = !m_flashlightOn;
        eng.Audio().PlaySfx("audio/sfx/sfx_cassette_click.wav", 0.7f);
    }

    m_flashlightFocus = (m_flashlightOn && IsMouseButtonDown(MOUSE_BUTTON_RIGHT));

    if (m_flashlightOn) {
        float drainRate = m_flashlightFocus ? 0.8f : 0.25f;
        m_flashlightBattery = std::max(0.0f, m_flashlightBattery - dt * drainRate);
    }

    if (m_flashlightFlickerTimer > 0.0f) {
        m_flashlightFlickerTimer -= dt;
    }

    if (m_radioPlaying) {
        m_radioPlaybackTimer += dt;
        if (m_radioPlaybackTimer > 16.0f) {
            m_radioPlaying = false;
        }
    }

    UpdateRain(dt);

    m_camera.Update(dt, m_colliders);

    if (m_camera.PopFootstepTriggered()) {
        bool outdoor = (m_camera.GetPosition().z > 8.0f);
        int stepIdx = (m_stepSoundIndex % 4) + 1;
        m_stepSoundIndex++;
        char sfxPath[64];
        if (outdoor) {
            std::snprintf(sfxPath, sizeof(sfxPath), "audio/sfx/sfx_footstep_concrete_%02d.wav", stepIdx);
        } else {
            std::snprintf(sfxPath, sizeof(sfxPath), "audio/sfx/sfx_footstep_wood_%02d.wav", stepIdx);
        }
        float vol = m_camera.IsSprinting() ? 0.75f : (m_camera.IsCrouching() ? 0.25f : 0.48f);
        eng.Audio().PlaySfx(sfxPath, vol);
    }

    Ray ray = m_camera.GetViewRay();
    float bestDist = 2.85f;
    m_focusedType = InteractiveType::None;
    m_focusPrompt.clear();

    for (const auto& obj : m_interactives) {
        if (!obj.active) continue;
        RayCollision col = GetRayCollisionBox(ray, obj.bounds);
        if (col.hit && col.distance < bestDist) {
            bestDist = col.distance;
            m_focusedType = obj.type;
            m_focusPrompt = obj.label;
        }
    }

    if (IsKeyPressed(KEY_E) && m_focusedType != InteractiveType::None) {
        HandleInteraction(eng);
    }
}

void MotelWorld3D::HandleInteraction(engine::core::Engine& eng) {
    switch (m_focusedType) {
        case InteractiveType::EntranceDoor:
            eng.Audio().PlaySfx("audio/sfx/sfx_door_bell.wav", 0.8f);
            if (m_camera.GetPosition().z > 8.0f) {
                m_camera.SetPosition(Vector3{0.0f, 0.0f, 5.5f});
                m_camera.SetLookAngle(3.14159f, 0.0f);
                m_camera.ResetMouseLook();
                m_bannerText = "ВЫ ВОШЛИ В ХОЛЛ МОТЕЛЯ. ЗДЕСЬ СУХО, НО ВОЗДУХ ЛЕДЯНОЙ.";
            } else {
                m_camera.SetPosition(Vector3{0.0f, 0.0f, 10.5f});
                m_camera.SetLookAngle(0.0f, 0.0f);
                m_camera.ResetMouseLook();
                m_bannerText = "ВЫ ВЫШЛИ НА ДОЖДЛИВЫЙ ДВОР МОТЕЛЯ.";
            }
            m_bannerTimer = 4.0f;
            break;

        case InteractiveType::DeskBell:
            m_bellRings++;
            eng.Audio().PlaySfx("audio/sfx/sfx_cash_register.wav", 0.9f);
            m_bannerText = (m_bellRings == 1) ? "*ДЗИНЬ!* Звон колокольчика гулко разнёсся по пустому коридору." :
                           (m_bellRings == 3) ? "Где-то в глубине коридора послышался глухой шаг..." :
                                                "*ДЗИНЬ!* На стойке никто не появляется.";
            m_bannerTimer = 3.5f;
            if (m_bellRings == 3) {
                m_flashlightFlickerTimer = 2.5f;
                eng.Audio().PlaySfx("audio/sfx/sfx_heartbeat_dread.wav", 0.9f);
            }
            break;

        case InteractiveType::Key101Tag:
            m_hasKey101 = true;
            for (auto& obj : m_interactives) {
                if (obj.type == InteractiveType::Key101Tag) obj.active = false;
            }
            eng.Audio().PlaySfx("audio/sfx/sfx_key_pickup.wav", 0.9f);
            m_inspectMode = true;
            m_inspectedItem = InteractiveType::Key101Tag;
            m_inspectTitle = "КЛЮЧ ОТ НОМЕРА 101";
            m_inspectDesc = "Тяжёлый латунный ключ с изумрудным пластиковым брелоком.\nНа брелоке выбито: «МОТЕЛЬ 86 — КОМНАТА 101».";
            m_bannerText = "КЛЮЧ ОТ НОМЕРА 101 ДОБАВЛЕН В ИНВЕНТАРЬ!";
            m_bannerTimer = 4.0f;
            break;

        case InteractiveType::RadioTransistor:
            m_radioPlaying = !m_radioPlaying;
            if (m_radioPlaying) {
                m_radioPlaybackTimer = 0.0f;
                eng.Audio().PlaySfx("audio/sfx/sfx_radio_static.wav", 0.9f);
                m_bannerText = "РАДИО: «...Лера? Если ты добралась до 86-го... не верь зеркалам... они внутри...»";
            } else {
                m_bannerText = "РАДИО ВЫКЛЮЧЕНО.";
            }
            m_bannerTimer = 5.0f;
            break;

        case InteractiveType::DoorRoom101:
            if (!m_hasKey101 && !m_door101Open) {
                eng.Audio().PlaySfx("audio/sfx/sfx_door_creak_spooky.wav", 0.5f);
                m_bannerText = "ДВЕРЬ ЗАПЕРТА. НУЖЕН КЛЮЧ ОТ НОМЕРА 101 С РЕСЕПШЕНА.";
                m_bannerTimer = 3.5f;
            } else if (!m_door101Open) {
                m_door101Open = true;
                eng.Audio().PlaySfx("audio/sfx/sfx_door_bell.wav", 0.7f);
                m_bannerText = "ЗАМОК ЩЁЛКНУЛ. ДВЕРЬ В НОМЕР 101 ОТКРЫТА.";
                m_bannerTimer = 3.5f;
            } else {
                m_camera.SetPosition(Vector3{-10.5f, 0.0f, -5.0f});
                m_camera.SetLookAngle(3.14159f * 0.5f, 0.0f);
                m_camera.ResetMouseLook();
            }
            break;

        case InteractiveType::WardrobeCloset:
            m_inWardrobe = !m_inWardrobe;
            eng.Audio().PlaySfx("audio/sfx/sfx_door_creak_spooky.wav", 0.8f);
            if (m_inWardrobe) {
                m_camera.SetPosition(Vector3{-14.6f, 0.0f, -4.4f});
                m_camera.SetLookAngle(0.0f, 0.0f);
                m_camera.ResetMouseLook();
                m_bannerText = "ВЫ СПРЯТАЛИСЬ В ШКАФУ. ЗАДЕРЖИТЕ ДЫХАНИЕ [ПРОБЕЛ].";
            } else {
                m_camera.SetPosition(Vector3{-13.5f, 0.0f, -4.4f});
                m_camera.ResetMouseLook();
                m_bannerText = "ВЫ ВЫШЛИ ИЗ ШКАФА.";
            }
            m_bannerTimer = 3.5f;
            break;

        case InteractiveType::BathroomMirror:
            m_mirrorTriggered = true;
            m_flashlightFlickerTimer = 3.0f;
            eng.Audio().PlaySfx("audio/sfx/sfx_sting_dread.wav", 0.95f);
            eng.Audio().PlaySfx("audio/sfx/sfx_whisper_vera.wav", 0.9f);
            m_bannerText = "В ОТРАЖЕНИИ ЗЕРКАЛА КРОВЬЮ ПРОСТУПИЛА НАДПИСЬ: «НЕ ВЕРЬ ИМ»!";
            m_bannerTimer = 5.0f;
            break;

        case InteractiveType::HighwayCar:
            eng.Audio().PlaySfx("audio/sfx/sfx_cassette_click.wav", 0.8f);
            m_bannerText = "ДВИГАТЕЛЬ ЗАЛИТ ВОДОЙ. СВЕЧИ ЗАЖИГАНИЯ МЁРТВЫ. ВЫ НЕ УЕДЕТЕ.";
            m_bannerTimer = 4.0f;
            break;

        default:
            break;
    }
}

void MotelWorld3D::DrawFloorQuad(Texture2D tex, float x0, float z0, float x1, float z1, float y, float uTile, float vTile, Color tint) const {
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x0, y, z0);
    rlTexCoord2f(uTile, 0.0f);
    rlVertex3f(x1, y, z0);
    rlTexCoord2f(uTile, vTile);
    rlVertex3f(x1, y, z1);
    rlTexCoord2f(0.0f, vTile);
    rlVertex3f(x0, y, z1);
    rlEnd();
    rlSetTexture(0);
}

void MotelWorld3D::DrawCeilingQuad(Texture2D tex, float x0, float z0, float x1, float z1, float y, float uTile, float vTile, Color tint) const {
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(0.0f, -1.0f, 0.0f);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x0, y, y < 0.0f ? z0 : z0);
    rlTexCoord2f(0.0f, vTile);
    rlVertex3f(x0, y, z1);
    rlTexCoord2f(uTile, vTile);
    rlVertex3f(x1, y, z1);
    rlTexCoord2f(uTile, 0.0f);
    rlVertex3f(x1, y, z0);
    rlEnd();
    rlSetTexture(0);
}

void MotelWorld3D::DrawWallQuadX(Texture2D tex, float x, float y0, float z0, float y1, float z1, float uTile, float vTile, Color tint) const {
    float nx = (z1 > z0) ? 1.0f : -1.0f;
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(nx, 0.0f, 0.0f);
    rlTexCoord2f(0.0f, vTile);
    rlVertex3f(x, y0, z0);
    rlTexCoord2f(uTile, vTile);
    rlVertex3f(x, y0, z1);
    rlTexCoord2f(uTile, 0.0f);
    rlVertex3f(x, y1, z1);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x, y1, z0);
    rlEnd();
    rlSetTexture(0);
}

void MotelWorld3D::DrawWallQuadZ(Texture2D tex, float z, float x0, float y0, float x1, float y1, float uTile, float vTile, Color tint) const {
    float nz = (x1 > x0) ? -1.0f : 1.0f;
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(0.0f, 0.0f, nz);
    rlTexCoord2f(0.0f, vTile);
    rlVertex3f(x0, y0, z);
    rlTexCoord2f(uTile, vTile);
    rlVertex3f(x1, y0, z);
    rlTexCoord2f(uTile, 0.0f);
    rlVertex3f(x1, y1, z);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x0, y1, z);
    rlEnd();
    rlSetTexture(0);
}

void MotelWorld3D::DrawBoxTextured(Texture2D sideTex, Texture2D topTex, Vector3 min, Vector3 max, Color tint) const {
    DrawFloorQuad(topTex, min.x, min.z, max.x, max.z, max.y, 1.0f, 1.0f, tint);
    DrawCeilingQuad(topTex, min.x, min.z, max.x, max.z, min.y, 1.0f, 1.0f, tint);
    DrawWallQuadZ(sideTex, min.z, min.x, min.y, max.x, max.y, 1.0f, 1.0f, tint);
    DrawWallQuadZ(sideTex, max.z, max.x, min.y, min.x, max.y, 1.0f, 1.0f, tint);
    DrawWallQuadX(sideTex, min.x, min.y, max.z, max.y, min.z, 1.0f, 1.0f, tint);
    DrawWallQuadX(sideTex, max.x, min.y, min.z, max.y, max.z, 1.0f, 1.0f, tint);
}

void MotelWorld3D::Render3D(engine::core::Engine& ) {
    BeginMode3D(m_camera.GetCamera());

    ClearBackground(Color{10, 12, 18, 255});

    Vector3 camPos = m_camera.GetPosition();
    Vector3 fwd = m_camera.GetForward();
    float flashIntensity = 0.0f;
    if (m_flashlightOn && m_flashlightBattery > 0.0f) {
        if (m_flashlightFlickerTimer <= 0.0f || std::fmod(m_flashlightFlickerTimer, 0.22f) < 0.11f) {
            flashIntensity = m_flashlightFocus ? 1.6f : 1.15f;
        }
    }
    Vector3 flashPos = {camPos.x, camPos.y - 0.12f, camPos.z};
    Vector3 flashColor = m_flashlightFocus ? Vector3{1.0f, 0.96f, 0.85f} : Vector3{0.95f, 0.90f, 0.78f};
    float cutoff = m_flashlightFocus ? std::cos(14.0f * DEG2RAD) : std::cos(26.0f * DEG2RAD);
    float outerCutoff = m_flashlightFocus ? std::cos(22.0f * DEG2RAD) : std::cos(38.0f * DEG2RAD);

    Vector3 ambientCol = {0.14f, 0.16f, 0.22f};
    Vector3 fogCol = {0.05f, 0.06f, 0.09f};
    float fogStart = 8.0f;
    float fogEnd = 28.0f;

    if (m_shaderLoaded) {
        SetShaderValue(m_litShader, m_uViewPosLoc, &camPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(m_litShader, m_uFlashlightPosLoc, &flashPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(m_litShader, m_uFlashlightDirLoc, &fwd, SHADER_UNIFORM_VEC3);
        SetShaderValue(m_litShader, m_uFlashlightColorLoc, &flashColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(m_litShader, m_uFlashlightCutoffLoc, &cutoff, SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_litShader, m_uFlashlightOuterCutoffLoc, &outerCutoff, SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_litShader, m_uFlashlightIntensityLoc, &flashIntensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_litShader, m_uAmbientColorLoc, &ambientCol, SHADER_UNIFORM_VEC3);
        SetShaderValue(m_litShader, m_uFogColorLoc, &fogCol, SHADER_UNIFORM_VEC3);
        SetShaderValue(m_litShader, m_uFogStartLoc, &fogStart, SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_litShader, m_uFogEndLoc, &fogEnd, SHADER_UNIFORM_FLOAT);

        BeginShaderMode(m_litShader);
    }

    DrawFloorQuad(m_texConcrete, -25.0f, 8.0f, 25.0f, 32.0f, 0.0f, 10.0f, 6.0f, Color{210, 215, 230, 255});

    DrawWallQuadZ(m_texWall, 8.0f, -22.0f, 0.0f, -2.5f, 4.5f, 8.0f, 2.0f, WHITE);
    DrawWallQuadZ(m_texWall, 8.0f, 2.5f, 0.0f, 22.0f, 4.5f, 8.0f, 2.0f, WHITE);
    DrawWallQuadZ(m_texWall, 8.0f, -2.5f, 2.8f, 2.5f, 4.5f, 2.0f, 1.0f, WHITE);

    DrawWallQuadZ(m_texDoor, 8.0f, -2.5f, 0.0f, 2.5f, 2.8f, 1.0f, 1.0f, WHITE);

    DrawFloorQuad(m_texCarpet, -10.0f, -12.0f, 10.0f, 8.0f, 0.0f, 6.0f, 6.0f, WHITE);
    DrawCeilingQuad(m_texCeiling, -10.0f, -12.0f, 10.0f, 8.0f, 3.4f, 6.0f, 6.0f, WHITE);
    DrawWallQuadZ(m_texWall, -12.0f, 10.0f, 0.0f, -10.0f, 3.4f, 6.0f, 1.5f, WHITE);
    DrawWallQuadX(m_texWall, 10.0f, 0.0f, -12.0f, 3.4f, 8.0f, 6.0f, 1.5f, WHITE);
    DrawWallQuadX(m_texWall, -10.0f, 0.0f, 8.0f, 3.4f, 1.0f, 2.5f, 1.5f, WHITE);
    DrawWallQuadX(m_texWall, -10.0f, 0.0f, -2.5f, 3.4f, -12.0f, 3.0f, 1.5f, WHITE);

    DrawFloorQuad(m_texTile, 1.5f, 0.2f, 6.8f, 2.5f, 0.01f, 3.0f, 2.0f, WHITE);
    DrawBoxTextured(m_texWall, m_texWall, Vector3{1.5f, 0.0f, 0.2f}, Vector3{6.8f, 1.15f, 2.5f}, WHITE);

    DrawFloorQuad(m_texCarpet, -18.0f, -12.0f, -10.0f, 1.0f, 0.0f, 4.0f, 4.0f, WHITE);
    DrawCeilingQuad(m_texCeiling, -18.0f, -12.0f, -10.0f, 1.0f, 3.4f, 4.0f, 4.0f, WHITE);
    DrawWallQuadX(m_texWall, -18.0f, 0.0f, -12.0f, 3.4f, 1.0f, 4.0f, 1.5f, WHITE);
    DrawWallQuadZ(m_texWall, 1.0f, -18.0f, 0.0f, -10.0f, 3.4f, 3.0f, 1.5f, WHITE);

    DrawWallQuadZ(m_texWall, -2.5f, -10.0f, 2.6f, -7.5f, 3.4f, 1.0f, 0.5f, WHITE);
    float doorZ = m_door101Open ? -3.2f : -2.5f;
    DrawWallQuadZ(m_texDoor, doorZ, -10.0f, 0.0f, -7.5f, 2.6f, 1.0f, 1.0f, WHITE);

    DrawBoxTextured(m_texWall, m_texWall, Vector3{-16.2f, 0.0f, -5.7f}, Vector3{-13.6f, 2.5f, -3.0f}, WHITE);

    DrawFloorQuad(m_texTile, -18.0f, -12.0f, -13.0f, -8.5f, 0.01f, 2.5f, 2.0f, WHITE);
    DrawWallQuadZ(m_texMirror, -11.95f, -15.8f, 1.2f, -13.4f, 2.6f, 1.0f, 1.0f, WHITE);

    DrawBoxTextured(m_texConcrete, m_texConcrete, Vector3{-7.2f, 0.0f, 14.8f}, Vector3{-3.8f, 1.35f, 19.2f}, Color{35, 55, 85, 255});

    if (m_shaderLoaded) {
        EndShaderMode();
    }

    bool hazardBlink = (std::fmod(m_globalTime, 1.0f) < 0.5f);
    Color hazardCol = hazardBlink ? Color{255, 170, 30, 255} : Color{50, 30, 10, 255};
    DrawSphere(Vector3{-6.8f, 0.7f, 19.3f}, 0.12f, hazardCol);
    DrawSphere(Vector3{-4.2f, 0.7f, 19.3f}, 0.12f, hazardCol);

    for (const auto& drop : m_rain) {
        DrawLine3D(drop.pos, Vector3{drop.pos.x, drop.pos.y - drop.len, drop.pos.z}, Color{175, 205, 240, 180});
    }

    float neonFlicker = 0.85f + 0.15f * std::sin(m_globalTime * 18.0f) * std::cos(m_globalTime * 7.0f);
    Color neonTint = Color{
        static_cast<unsigned char>(180 * neonFlicker),
        static_cast<unsigned char>(240 * neonFlicker),
        static_cast<unsigned char>(255 * neonFlicker),
        255
    };
    DrawBillboard(m_camera.GetCamera(), m_texNeon, Vector3{0.0f, 4.3f, 8.12f}, 6.2f, neonTint);

    DrawSphere(Vector3{2.4f, 1.22f, 1.2f}, 0.11f, Color{225, 195, 75, 255});

    DrawBillboard(m_camera.GetCamera(), m_texRadio, Vector3{4.4f, 1.35f, 1.4f}, 0.65f, WHITE);

    if (!m_hasKey101) {
        DrawBillboard(m_camera.GetCamera(), m_texKeyTag, Vector3{6.6f, 1.75f, 1.4f}, 0.40f, WHITE);
    }

    if (m_mirrorTriggered) {
        float pulse = 0.5f + 0.5f * std::sin(m_globalTime * 12.0f);
        DrawCube(Vector3{-14.6f, 1.9f, -11.90f}, 1.8f, 1.2f, 0.02f, Color{195, 20, 25, static_cast<unsigned char>(150 * pulse)});
        DrawSphere(Vector3{-14.6f, 1.9f, -11.2f}, 0.28f, Color{15, 8, 20, 210});
    }

    if (flashIntensity > 0.0f) {
        float flashRange = m_flashlightFocus ? 11.0f : 7.5f;
        Ray flashRay = m_camera.GetViewRay();
        float hitDist = flashRange;
        for (const auto& c : m_colliders) {
            RayCollision col = GetRayCollisionBox(flashRay, c);
            if (col.hit && col.distance < hitDist) {
                hitDist = col.distance;
            }
        }
        Vector3 hitPoint = Vector3{
            camPos.x + fwd.x * (hitDist - 0.04f),
            camPos.y + fwd.y * (hitDist - 0.04f),
            camPos.z + fwd.z * (hitDist - 0.04f)
        };
        Color spotCol = m_flashlightFocus ? Color{255, 250, 220, 110} : Color{255, 235, 190, 60};
        DrawSphere(hitPoint, m_flashlightFocus ? 0.35f : 0.65f, spotCol);
    }

    EndMode3D();
}

void MotelWorld3D::RenderUI(engine::core::Engine& eng) const {
    Font font = eng.Assets().GetMainFont();

    int cx = 1280 / 2;
    int cy = 720 / 2;

    if (!m_inspectMode && !m_inWardrobe) {
        DrawRectangle(cx - 5, cy - 1, 10, 2, Color{244, 184, 96, 210});
        DrawRectangle(cx - 1, cy - 5, 2, 10, Color{244, 184, 96, 210});
        DrawCircle(cx, cy, 1.5f, Color{255, 240, 210, 240});
    }

    if (!m_focusPrompt.empty() && !m_inspectMode && !m_inWardrobe) {
        std::string prompt = "[E] " + m_focusPrompt;
        Vector2 size = MeasureTextEx(font, prompt.c_str(), 20.0f, 1.0f);
        float px = cx - size.x * 0.5f;
        float py = cy + 34.0f;
        DrawRectangle(static_cast<int>(px - 14.0f), static_cast<int>(py - 6.0f), static_cast<int>(size.x + 28.0f), static_cast<int>(size.y + 12.0f), Color{14, 10, 18, 225});
        DrawRectangleLines(static_cast<int>(px - 14.0f), static_cast<int>(py - 6.0f), static_cast<int>(size.x + 28.0f), static_cast<int>(size.y + 12.0f), Color{244, 184, 96, 240});
        DrawTextEx(font, prompt.c_str(), Vector2{px, py}, 20.0f, 1.0f, Color{244, 184, 96, 255});
    }

    if (!m_bannerText.empty() && m_bannerTimer > 0.0f) {
        Vector2 bsize = MeasureTextEx(font, m_bannerText.c_str(), 19.0f, 1.0f);
        float bx = cx - bsize.x * 0.5f;
        float by = 36.0f;
        DrawRectangle(static_cast<int>(bx - 16.0f), static_cast<int>(by - 8.0f), static_cast<int>(bsize.x + 32.0f), static_cast<int>(bsize.y + 16.0f), Color{18, 12, 16, 235});
        DrawRectangleLines(static_cast<int>(bx - 16.0f), static_cast<int>(by - 8.0f), static_cast<int>(bsize.x + 32.0f), static_cast<int>(bsize.y + 16.0f), Color{196, 122, 59, 230});
        DrawTextEx(font, m_bannerText.c_str(), Vector2{bx, by}, 19.0f, 1.0f, Color{232, 227, 217, 255});
    }

    if (m_inWardrobe) {
        DrawRectangle(0, 0, 1280, 720, Color{8, 6, 10, 245});
        DrawRectangle(cx - 70, 60, 140, 600, Color{0, 0, 0, 0});
        DrawRectangleLines(cx - 70, 60, 140, 600, Color{90, 50, 30, 255});
        DrawTextEx(font, "ВЫ ВНУТРИ ШКАФА. ВЗГЛЯД СКВОЗЬ ЩЕЛЬ В ДВЕРЦЕ. [E] ВЫЙТИ", Vector2{cx - 240.0f, 680.0f}, 17.5f, 1.0f, Color{196, 178, 156, 220});
    }

    DrawRectangle(24, 638, 220, 44, Color{14, 10, 18, 210});
    DrawRectangleLines(24, 638, 220, 44, Color{107, 51, 34, 200});

    std::string flashStatus = m_flashlightOn ? "Фонарик: ВКЛ [F]" : "Фонарик: ВЫКЛ [F]";
    DrawTextEx(font, flashStatus.c_str(), Vector2{36, 644}, 16.0f, 1.0f, m_flashlightOn ? Color{244, 220, 140, 240} : Color{140, 130, 120, 180});

    float batteryW = (m_flashlightBattery / 100.0f) * 80.0f;
    Color batCol = (m_flashlightBattery > 40.0f) ? Color{60, 210, 120, 230} : ((m_flashlightBattery > 15.0f) ? Color{230, 180, 50, 230} : Color{230, 50, 50, 230});
    DrawRectangle(146, 648, static_cast<int>(batteryW), 10, batCol);
    DrawRectangleLines(146, 648, 80, 10, Color{180, 170, 160, 200});

    float stamW = (m_camera.GetStamina() / 100.0f) * 190.0f;
    DrawRectangle(36, 666, static_cast<int>(stamW), 6, Color{80, 160, 220, 220});

    if (m_hasKey101) {
        DrawRectangle(256, 638, 54, 44, Color{14, 10, 18, 210});
        DrawRectangleLines(256, 638, 54, 44, Color{244, 184, 96, 220});
        DrawTexturePro(m_texKeyTag, Rectangle{0, 0, 64, 64}, Rectangle{263, 642, 36, 36}, Vector2{0, 0}, 0.0f, WHITE);
    }

    DrawRectangle(0, 688, 1280, 32, Color{10, 12, 18, 240});
    DrawTextEx(font, "[W/A/S/D] Шаг | [SHIFT] Бег | [CTRL] Присед | [F] Фонарик | [ПКМ] Фокус луча | [E] Действие | [ESC] Меню",
               Vector2{24, 694}, 16.5f, 1.0f, Color{188, 178, 162, 230});

    if (m_inspectMode) {
        RenderInspectOverlay(font);
    }
}

void MotelWorld3D::RenderInspectOverlay(Font font) const {
    DrawRectangle(0, 0, 1280, 720, Color{8, 6, 12, 230});

    int cx = 1280 / 2;
    int cy = 720 / 2;

    float radX = m_inspectRotX;
    float radY = m_inspectRotY;

    Vector2 tagCenter = {static_cast<float>(cx), static_cast<float>(cy - 30)};
    float cosY = std::cos(radY);
    float sinY = std::sin(radY);
    float cosX = std::cos(radX);

    float tagW = 120.0f * std::abs(cosY);
    float tagH = 150.0f * std::abs(cosX);

    DrawRectangle(static_cast<int>(tagCenter.x - tagW * 0.5f), static_cast<int>(tagCenter.y - tagH * 0.5f),
                  static_cast<int>(std::max(12.0f, tagW)), static_cast<int>(std::max(12.0f, tagH)), Color{25, 125, 85, 255});
    DrawRectangleLines(static_cast<int>(tagCenter.x - tagW * 0.5f), static_cast<int>(tagCenter.y - tagH * 0.5f),
                       static_cast<int>(std::max(12.0f, tagW)), static_cast<int>(std::max(12.0f, tagH)), Color{244, 220, 140, 255});

    if (std::abs(cosY) > 0.35f && std::abs(cosX) > 0.35f) {
        DrawTextEx(font, "101", Vector2{tagCenter.x - 22.0f, tagCenter.y - 14.0f}, 28.0f, 1.0f, Color{245, 240, 210, 255});
        DrawTextEx(font, "МОТЕЛЬ 86", Vector2{tagCenter.x - 38.0f, tagCenter.y + 22.0f}, 16.0f, 1.0f, Color{210, 195, 160, 220});
    }

    DrawRectangle(static_cast<int>(tagCenter.x - 12.0f), static_cast<int>(tagCenter.y + tagH * 0.5f), 24, 80, Color{215, 185, 75, 255});
    DrawRectangle(static_cast<int>(tagCenter.x + 8.0f), static_cast<int>(tagCenter.y + tagH * 0.5f + 35.0f), 14, 8, Color{215, 185, 75, 255});
    DrawRectangle(static_cast<int>(tagCenter.x + 8.0f), static_cast<int>(tagCenter.y + tagH * 0.5f + 55.0f), 14, 10, Color{215, 185, 75, 255});

    DrawRectangle(cx - 320, 520, 640, 125, Color{18, 14, 22, 235});
    DrawRectangleLines(cx - 320, 520, 640, 125, Color{244, 184, 96, 230});

    DrawTextEx(font, m_inspectTitle.c_str(), Vector2{static_cast<float>(cx - 300), 532.0f}, 22.0f, 1.0f, Color{244, 184, 96, 255});
    DrawTextEx(font, m_inspectDesc.c_str(), Vector2{static_cast<float>(cx - 300), 564.0f}, 17.0f, 1.0f, Color{225, 220, 210, 240});
    DrawTextEx(font, "[Зажмите ЛКМ и тяните] Вращать предмет | [E / ESC] Убрать в карман",
               Vector2{static_cast<float>(cx - 300), 616.0f}, 15.0f, 1.0f, Color{180, 160, 140, 200});
}

}
