#include "game/chapter2/MotelWorld3D.h"
#include "engine/graphics/TextLayout.h"
#include "raymath.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace game::chapter2 {

void MotelWorld3D::Init(engine::core::Engine& eng, int nightNumber) {
    m_nightNumber = nightNumber;
    m_globalTime = 0.0f;
    m_returnToMenu = false;

    m_camera.Init(Vector3{0.0f, 0.0f, 15.0f}, 3.14159f, 0.0f);

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

    DisableCursor();
}

void MotelWorld3D::Shutdown(engine::core::Engine& ) {
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

    m_colliders.push_back({Vector3{1.5f, 0.0f, 0.2f}, Vector3{5.8f, 1.2f, 3.2f}});

    m_colliders.push_back({Vector3{-5.5f, 0.0f, 8.0f}, Vector3{5.5f, 3.5f, 8.5f}});

    m_colliders.push_back({Vector3{-7.8f, 0.0f, -11.0f}, Vector3{-7.2f, 3.5f, -2.5f}});
    m_colliders.push_back({Vector3{-16.5f, 0.0f, -12.0f}, Vector3{-15.5f, 3.5f, -2.0f}});
    m_colliders.push_back({Vector3{-16.0f, 0.0f, -12.5f}, Vector3{-7.5f, 3.5f, -11.5f}});
    m_colliders.push_back({Vector3{-16.0f, 0.0f, -2.5f}, Vector3{-7.5f, 3.5f, -1.8f}});

    m_colliders.push_back({Vector3{-14.2f, 0.0f, -9.5f}, Vector3{-11.5f, 0.9f, -6.0f}});
    m_colliders.push_back({Vector3{-15.4f, 0.0f, -5.5f}, Vector3{-13.8f, 2.4f, -3.2f}});

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
        {Vector3{-8.2f, 0.0f, -5.2f}, Vector3{-6.8f, 2.6f, -3.2f}},
        "Дверь номера 101",
        true
    });

    m_interactives.push_back({
        InteractiveType::WardrobeCloset,
        {Vector3{-15.6f, 0.0f, -5.7f}, Vector3{-13.6f, 2.5f, -3.0f}},
        "Платяной шкаф (Укрытие)",
        true
    });

    m_interactives.push_back({
        InteractiveType::BathroomMirror,
        {Vector3{-15.8f, 1.2f, -12.4f}, Vector3{-14.0f, 2.6f, -11.4f}},
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
                m_bannerText = "ВЫ ВОШЛИ В ХОЛЛ МОТЕЛЯ. ЗДЕСЬ СУХО, НО ВОЗДУХ ЛЕДЯНОЙ.";
            } else {
                m_camera.SetPosition(Vector3{0.0f, 0.0f, 10.5f});
                m_camera.SetLookAngle(0.0f, 0.0f);
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
            }
            break;

        case InteractiveType::WardrobeCloset:
            m_inWardrobe = !m_inWardrobe;
            eng.Audio().PlaySfx("audio/sfx/sfx_door_creak_spooky.wav", 0.8f);
            if (m_inWardrobe) {
                m_camera.SetPosition(Vector3{-14.6f, 0.0f, -4.4f});
                m_camera.SetLookAngle(0.0f, 0.0f);
                m_bannerText = "ВЫ СПРЯТАЛИСЬ В ШКАФУ. ЗАДЕРЖИТЕ ДЫХАНИЕ [ПРОБЕЛ].";
            } else {
                m_camera.SetPosition(Vector3{-13.5f, 0.0f, -4.4f});
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

void MotelWorld3D::Render3D(engine::core::Engine& ) {
    BeginMode3D(m_camera.GetCamera());

    ClearBackground(Color{14, 16, 22, 255});

    DrawPlane(Vector3{0.0f, 0.0f, 18.0f}, Vector2{36.0f, 22.0f}, Color{32, 35, 42, 255});

    DrawCube(Vector3{-5.5f, 0.6f, 17.0f}, 2.2f, 1.2f, 4.4f, Color{28, 45, 75, 255});
    DrawCubeWires(Vector3{-5.5f, 0.6f, 17.0f}, 2.22f, 1.22f, 4.42f, Color{15, 25, 45, 255});
    bool hazardBlink = (std::fmod(m_globalTime, 1.0f) < 0.5f);
    Color hazardCol = hazardBlink ? Color{255, 170, 30, 255} : Color{50, 30, 10, 255};
    DrawSphere(Vector3{-6.4f, 0.7f, 19.2f}, 0.12f, hazardCol);
    DrawSphere(Vector3{-4.6f, 0.7f, 19.2f}, 0.12f, hazardCol);

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
    DrawCube(Vector3{0.0f, 4.2f, 8.1f}, 6.2f, 1.4f, 0.2f, Color{20, 25, 35, 255});
    DrawBillboard(m_camera.GetCamera(), m_texNeon, Vector3{0.0f, 4.2f, 8.22f}, 5.8f, neonTint);

    DrawCube(Vector3{-9.0f, 2.0f, 8.0f}, 10.0f, 4.0f, 0.6f, Color{55, 38, 28, 255});
    DrawCube(Vector3{9.0f,  2.0f, 8.0f}, 10.0f, 4.0f, 0.6f, Color{55, 38, 28, 255});
    DrawCube(Vector3{0.0f,  3.6f, 8.0f}, 8.0f,  0.8f, 0.6f, Color{55, 38, 28, 255});

    DrawCube(Vector3{0.0f, -0.05f, 0.0f}, 20.0f, 0.1f, 16.0f, Color{75, 24, 30, 255});

    DrawCube(Vector3{5.0f, -0.04f, 2.0f}, 6.0f, 0.1f, 6.0f, Color{45, 85, 80, 255});

    DrawCube(Vector3{0.0f, 3.2f, 0.0f}, 20.0f, 0.1f, 16.0f, Color{130, 125, 115, 255});

    DrawCube(Vector3{0.0f, 1.6f, -8.0f}, 20.0f, 3.2f, 0.4f, Color{65, 42, 28, 255});
    DrawCube(Vector3{10.0f, 1.6f, 0.0f}, 0.4f, 3.2f, 16.0f, Color{65, 42, 28, 255});
    DrawCube(Vector3{-10.0f, 1.6f, 4.0f}, 0.4f, 3.2f, 8.0f,  Color{65, 42, 28, 255});

    DrawCube(Vector3{3.6f, 0.55f, 1.6f}, 3.8f, 1.1f, 1.2f, Color{85, 50, 32, 255});
    DrawCubeWires(Vector3{3.6f, 0.55f, 1.6f}, 3.82f, 1.12f, 1.22f, Color{45, 25, 15, 255});

    DrawSphere(Vector3{2.4f, 1.16f, 1.2f}, 0.11f, Color{225, 195, 75, 255});

    DrawCube(Vector3{4.4f, 1.22f, 1.4f}, 0.65f, 0.35f, 0.32f, Color{65, 45, 30, 255});
    DrawBillboard(m_camera.GetCamera(), m_texRadio, Vector3{4.4f, 1.24f, 1.4f}, 0.55f, WHITE);

    DrawCube(Vector3{6.8f, 1.8f, 1.4f}, 0.1f, 0.8f, 1.2f, Color{45, 28, 18, 255});
    if (!m_hasKey101) {
        DrawBillboard(m_camera.GetCamera(), m_texKeyTag, Vector3{6.72f, 1.8f, 1.4f}, 0.35f, WHITE);
    }

    DrawCube(Vector3{-7.5f, 1.6f, -6.8f}, 0.3f, 3.2f, 7.6f, Color{65, 42, 28, 255});
    DrawCube(Vector3{-7.5f, 1.6f, -0.6f}, 0.3f, 3.2f, 4.8f, Color{65, 42, 28, 255});
    DrawCube(Vector3{-7.5f, 2.8f, -4.0f}, 0.3f, 0.8f, 2.0f, Color{65, 42, 28, 255});

    Vector3 doorPos = m_door101Open ? Vector3{-8.3f, 1.2f, -4.8f} : Vector3{-7.5f, 1.2f, -4.0f};
    DrawCube(doorPos, 0.15f, 2.4f, 1.8f, Color{95, 55, 35, 255});
    DrawBillboard(m_camera.GetCamera(), m_texDoor, doorPos, 2.3f, WHITE);

    DrawCube(Vector3{-12.0f, -0.05f, -6.5f}, 8.0f, 0.1f, 10.0f, Color{60, 22, 28, 255});
    DrawCube(Vector3{-12.0f, 3.2f, -6.5f}, 8.0f, 0.1f, 10.0f, Color{130, 125, 115, 255});
    DrawCube(Vector3{-16.0f, 1.6f, -6.5f}, 0.3f, 3.2f, 10.0f, Color{65, 42, 28, 255});
    DrawCube(Vector3{-12.0f, 1.6f, -11.5f}, 8.0f, 3.2f, 0.3f, Color{65, 42, 28, 255});
    DrawCube(Vector3{-12.0f, 1.6f, -1.8f},  8.0f, 3.2f, 0.3f, Color{65, 42, 28, 255});

    DrawCube(Vector3{-13.0f, 0.45f, -8.0f}, 2.4f, 0.8f, 3.2f, Color{115, 35, 45, 255});
    DrawCube(Vector3{-13.0f, 0.88f, -9.2f}, 1.8f, 0.2f, 0.6f, Color{225, 220, 210, 255});

    DrawCube(Vector3{-14.6f, 1.25f, -4.4f}, 1.1f, 2.5f, 1.8f, Color{55, 32, 20, 255});
    DrawCubeWires(Vector3{-14.6f, 1.25f, -4.4f}, 1.12f, 2.52f, 1.82f, Color{30, 18, 10, 255});

    Vector3 mirrorPos = Vector3{-15.0f, 1.8f, -11.35f};
    DrawCube(mirrorPos, 1.6f, 1.2f, 0.08f, Color{120, 130, 145, 255});
    DrawBillboard(m_camera.GetCamera(), m_texMirror, mirrorPos, 1.4f, WHITE);

    if (m_mirrorTriggered) {
        float pulse = 0.5f + 0.5f * std::sin(m_globalTime * 12.0f);
        DrawCube(Vector3{-15.0f, 1.8f, -11.32f}, 1.4f, 1.0f, 0.02f, Color{180, 20, 25, static_cast<unsigned char>(140 * pulse)});
        DrawSphere(Vector3{-15.0f, 1.8f, -10.5f}, 0.28f, Color{15, 8, 20, 210});
    }

    if (m_flashlightOn && (m_flashlightFlickerTimer <= 0.0f || std::fmod(m_flashlightFlickerTimer, 0.25f) < 0.12f)) {
        Vector3 camPos = m_camera.GetPosition();
        Vector3 fwd = m_camera.GetForward();
        float flashRange = m_flashlightFocus ? 9.5f : 6.5f;
        Vector3 lightSpot = Vector3{camPos.x + fwd.x * flashRange, camPos.y + fwd.y * flashRange, camPos.z + fwd.z * flashRange};
        Color beamCol = m_flashlightFocus ? Color{255, 250, 220, 75} : Color{255, 235, 190, 45};
        DrawLine3D(camPos, lightSpot, beamCol);
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
