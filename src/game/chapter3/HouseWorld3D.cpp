#include "game/chapter3/HouseWorld3D.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <ctime>

namespace game::chapter3 {

void HouseWorld3D::Init(engine::core::Engine& eng) {
    DisableCursor();

    m_camera.position = m_playerPos;
    m_camera.target = Vector3Add(m_playerPos, {0.0f, 0.0f, -1.0f});
    m_camera.up = {0.0f, 1.0f, 0.0f};
    m_camera.fovy = 68.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;

    m_inventory.clear();
    m_activeSlot = 0;
    m_returnToMenu = false;
    m_chapterComplete = false;
    m_inCutscene = false;
    m_keypadModalOpen = false;
    m_noteModalOpen = false;
    m_enteredKeypadCode.clear();

    SetupGeometry(eng);
    SetupProps();

    m_randomizer.Init();
    m_randomizer.Generate(static_cast<unsigned int>(std::time(nullptr)));

    m_weapon.Reset();
    m_granny.Init();
    m_escape.Reset();

    StartNewDay(eng, 1);

    m_cutscene.Load("assets/data/chapter3/cutscenes/intro_waking_up.json");
    m_cutscene.Play();
    m_inCutscene = true;

    eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.65f);
}

void HouseWorld3D::SetupGeometry(engine::core::Engine& eng) {
    m_rooms.clear();
    m_wallQuads.clear();
    m_colliders.clear();

    m_rooms.push_back({"Bedroom2F", {{-4.0f, 3.0f, 0.5f}, {-0.5f, 5.5f, 5.0f}}, 2});
    m_rooms.push_back({"Hallway2F", {{-0.5f, 3.0f, 0.5f}, {1.0f, 5.5f, 5.0f}}, 2});
    m_rooms.push_back({"GuestRoom2F", {{1.0f, 3.0f, 2.5f}, {5.0f, 5.5f, 5.0f}}, 2});
    m_rooms.push_back({"Bathroom2F", {{1.0f, 3.0f, 0.5f}, {5.0f, 5.5f, 2.5f}}, 2});

    m_rooms.push_back({"LivingRoom1F", {{-4.0f, 0.0f, -6.0f}, {1.0f, 3.0f, -1.0f}}, 1});
    m_rooms.push_back({"Kitchen1F", {{1.0f, 0.0f, -6.0f}, {5.0f, 3.0f, -1.0f}}, 1});
    m_rooms.push_back({"FrontHall1F", {{-2.0f, 0.0f, -1.0f}, {2.0f, 3.0f, 2.5f}}, 1});
    m_rooms.push_back({"Garage1F", {{-10.0f, 0.0f, -9.0f}, {-4.0f, 3.0f, -1.0f}}, 1});

    m_rooms.push_back({"Attic", {{-4.0f, 5.5f, -2.0f}, {4.0f, 8.0f, 5.0f}}, 3});
    m_rooms.push_back({"Basement", {{-4.0f, -3.0f, -3.0f}, {4.0f, 0.0f, 4.0f}}, 0});

    for (const auto& r : m_rooms) {
        BoundingBox b = r.bounds;
        std::string wallTex = (r.floor == 0) ? "sprites/3d/ch3/house_basement_brick.png" :
                              ((r.floor == 3) ? "sprites/3d/ch3/house_attic_dust.png" :
                              ((r.name == "Garage1F") ? "sprites/3d/ch3/house_garage_wall.png" : "sprites/3d/ch3/house_wallpaper_peeled.png"));

        std::string floorTex = (r.floor == 0) ? "sprites/3d/ch3/house_basement_brick.png" :
                               ((r.floor == 3) ? "sprites/3d/ch3/house_attic_dust.png" :
                               ((r.name == "Garage1F") ? "sprites/3d/ch3/house_garage_wall.png" : "sprites/3d/ch3/house_floor_parquet.png"));

        m_wallQuads.push_back({{b.min.x, b.min.y, b.min.z}, {b.max.x, b.min.y, b.min.z},
                               {b.max.x, b.min.y, b.max.z}, {b.min.x, b.min.y, b.max.z},
                               {0.0f, 1.0f, 0.0f}, floorTex, 2.0f});

        m_wallQuads.push_back({{b.min.x, b.max.y, b.min.z}, {b.max.x, b.max.y, b.min.z},
                               {b.max.x, b.max.y, b.max.z}, {b.min.x, b.max.y, b.max.z},
                               {0.0f, -1.0f, 0.0f}, (r.floor == 3 ? "sprites/3d/ch3/house_attic_dust.png" : "sprites/3d/ch3/house_wood_planks.png"), 2.0f});

        m_wallQuads.push_back({{b.min.x, b.min.y, b.min.z}, {b.max.x, b.min.y, b.min.z},
                               {b.max.x, b.max.y, b.min.z}, {b.min.x, b.max.y, b.min.z},
                               {0.0f, 0.0f, 1.0f}, wallTex, 1.5f});

        m_wallQuads.push_back({{b.min.x, b.min.y, b.max.z}, {b.max.x, b.min.y, b.max.z},
                               {b.max.x, b.max.y, b.max.z}, {b.min.x, b.max.y, b.max.z},
                               {0.0f, 0.0f, -1.0f}, wallTex, 1.5f});

        m_wallQuads.push_back({{b.min.x, b.min.y, b.min.z}, {b.min.x, b.min.y, b.max.z},
                               {b.min.x, b.max.y, b.max.z}, {b.min.x, b.max.y, b.min.z},
                               {1.0f, 0.0f, 0.0f}, wallTex, 1.5f});

        m_wallQuads.push_back({{b.max.x, b.min.y, b.min.z}, {b.max.x, b.min.y, b.max.z},
                               {b.max.x, b.max.y, b.max.z}, {b.max.x, b.max.y, b.min.z},
                               {-1.0f, 0.0f, 0.0f}, wallTex, 1.5f});
    }

    m_colliders.push_back({{-10.5f, -3.5f, -9.5f}, {5.5f, -3.0f, 5.5f}});
    m_colliders.push_back({{-10.5f, 0.0f, -9.5f}, {-10.0f, 3.0f, -1.0f}});
    m_colliders.push_back({{5.0f, 0.0f, -6.0f}, {5.5f, 5.5f, 5.0f}});
    m_colliders.push_back({{-4.0f, 0.0f, -6.5f}, {5.0f, 3.0f, -6.0f}});
    m_colliders.push_back({{-10.0f, 0.0f, -9.5f}, {-4.0f, 3.0f, -9.0f}});
}

void HouseWorld3D::SetupProps() {
    m_props.clear();

    InteractiveProp bed;
    bed.id = "prop_bed";
    bed.name = "Кровать";
    bed.bounds = {{-2.8f, 3.0f, 2.0f}, {-1.2f, 3.8f, 4.0f}};
    bed.center = {-2.0f, 3.4f, 3.0f};
    bed.isHidingSpot = true;
    m_props.push_back(bed);

    InteractiveProp wardrobe;
    wardrobe.id = "prop_wardrobe_bed";
    wardrobe.name = "Платяной шкаф";
    wardrobe.bounds = {{-3.8f, 3.0f, 1.0f}, {-3.2f, 5.2f, 2.0f}};
    wardrobe.center = {-3.5f, 4.1f, 1.5f};
    wardrobe.isHidingSpot = true;
    wardrobe.slotId = "slot_bed_wardrobe";
    m_props.push_back(wardrobe);

    InteractiveProp dresser;
    dresser.id = "prop_dresser";
    dresser.name = "Комод спальни";
    dresser.bounds = {{-1.5f, 3.0f, 4.2f}, {-0.5f, 4.0f, 4.8f}};
    dresser.center = {-1.0f, 3.5f, 4.5f};
    dresser.slotId = "slot_bed_dresser";
    m_props.push_back(dresser);

    InteractiveProp nightstand;
    nightstand.id = "prop_nightstand";
    nightstand.name = "Тумбочка у кровати";
    nightstand.bounds = {{-3.2f, 3.0f, 3.6f}, {-2.8f, 3.8f, 4.4f}};
    nightstand.center = {-3.0f, 3.4f, 4.0f};
    nightstand.slotId = "slot_bed_drawer";
    m_props.push_back(nightstand);

    InteractiveProp frontDoor;
    frontDoor.id = "prop_front_door";
    frontDoor.name = "Парадная дверь";
    frontDoor.bounds = {{-0.8f, 0.0f, 2.0f}, {0.8f, 2.4f, 2.6f}};
    frontDoor.center = {0.0f, 1.2f, 2.3f};
    frontDoor.isFrontDoor = true;
    m_props.push_back(frontDoor);

    InteractiveProp car;
    car.id = "prop_garage_car";
    car.name = "Автомобиль ВАЗ-2101";
    car.bounds = {{-9.0f, 0.0f, -8.0f}, {-6.5f, 1.6f, -4.5f}};
    car.center = {-7.7f, 0.8f, -6.2f};
    car.isCar = true;
    m_props.push_back(car);

    InteractiveProp workbench;
    workbench.id = "prop_workbench";
    workbench.name = "Оружейный верстак";
    workbench.bounds = {{-3.2f, -3.0f, -1.8f}, {-1.8f, -1.8f, -0.6f}};
    workbench.center = {-2.5f, -2.4f, -1.2f};
    workbench.isWorkbench = true;
    m_props.push_back(workbench);

    InteractiveProp sewer;
    sewer.id = "prop_sewer_grate";
    sewer.name = "Дренажный коллектор";
    sewer.bounds = {{1.8f, -3.0f, 2.8f}, {3.2f, -1.5f, 3.8f}};
    sewer.center = {2.5f, -2.2f, 3.3f};
    sewer.isSewer = true;
    m_props.push_back(sewer);

    InteractiveProp radio;
    radio.id = "prop_attic_radio";
    radio.name = "Радиостанция Маяка";
    radio.bounds = {{-0.8f, 5.5f, -1.9f}, {0.8f, 6.8f, -1.1f}};
    radio.center = {0.0f, 6.1f, -1.5f};
    radio.isRadio = true;
    m_props.push_back(radio);
}

void HouseWorld3D::StartNewDay(engine::core::Engine& eng, int dayNumber) {
    m_playerPos = {-2.0f, 3.5f, 3.0f};
    m_playerVel = {0.0f, 0.0f, 0.0f};
    m_playerYaw = 3.14159f * 0.5f;
    m_playerPitch = -0.1f;
    m_isCrouching = false;
    m_isHiding = false;
    m_currentHidingSpot.clear();

    m_camera.position = m_playerPos;
    m_camera.target = Vector3Add(m_playerPos, {0.0f, 0.0f, 1.0f});

    m_granny.ResetToDay(dayNumber);

    m_dayTitleTimer = 4.5f;
    m_dayTitleAlpha = 1.0f;

    if (dayNumber >= 3) {
        ShowNotification("Вы хромаете из-за травмы ноги (скорость бега снижена).", 4.0f);
    }
}

void HouseWorld3D::TriggerNoise(engine::core::Engine& eng, Vector3 pos, float radius, float volume) {
    m_granny.NotifySound(pos, radius, volume);
}

void HouseWorld3D::ShowNotification(const std::string& text, float duration) {
    m_notificationText = text;
    m_notificationTimer = duration;
}

bool HouseWorld3D::CheckCollision(Vector3 targetPos, float radius) const {
    for (const auto& b : m_colliders) {
        if (targetPos.x + radius > b.min.x && targetPos.x - radius < b.max.x &&
            targetPos.y + radius > b.min.y && targetPos.y - radius < b.max.y &&
            targetPos.z + radius > b.min.z && targetPos.z - radius < b.max.z) {
            return true;
        }
    }
    return false;
}

void HouseWorld3D::ResolvePlayerPhysics(Vector3& pos, Vector3& vel, float dt) {
    float playerRadius = 0.35f;
    Vector3 nextPos = Vector3Add(pos, Vector3Scale(vel, dt));

    if (!CheckCollision({nextPos.x, pos.y, pos.z}, playerRadius)) {
        pos.x = nextPos.x;
    } else {
        vel.x = 0.0f;
    }

    if (!CheckCollision({pos.x, pos.y, nextPos.z}, playerRadius)) {
        pos.z = nextPos.z;
    } else {
        vel.z = 0.0f;
    }

    pos.y = std::clamp(pos.y, -2.5f, 6.5f);
}

void HouseWorld3D::HandlePlayerMovement(engine::core::Engine& eng, float dt) {
    Vector2 mouseDelta = GetMouseDelta();
    float sensitivity = 0.0025f;
    m_playerYaw -= mouseDelta.x * sensitivity;
    m_playerPitch -= mouseDelta.y * sensitivity;
    m_playerPitch = std::clamp(m_playerPitch, -1.45f, 1.45f);

    m_isCrouching = IsKeyDown(KEY_C) || IsKeyDown(KEY_LEFT_CONTROL);
    bool isSprinting = IsKeyDown(KEY_LEFT_SHIFT) && !m_isCrouching;

    float baseSpeed = m_isCrouching ? 1.5f : (isSprinting ? 4.5f : 2.8f);
    float speedMult = m_escape.GetPlayerSpeedMultiplier();
    float currentSpeed = baseSpeed * speedMult;

    Vector3 forward = {std::sin(m_playerYaw), 0.0f, std::cos(m_playerYaw)};
    Vector3 right = {std::cos(m_playerYaw), 0.0f, -std::sin(m_playerYaw)};

    Vector3 wishDir{0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_W)) wishDir = Vector3Add(wishDir, forward);
    if (IsKeyDown(KEY_S)) wishDir = Vector3Subtract(wishDir, forward);
    if (IsKeyDown(KEY_D)) wishDir = Vector3Add(wishDir, right);
    if (IsKeyDown(KEY_A)) wishDir = Vector3Subtract(wishDir, right);

    float len = Vector3Length(wishDir);
    bool moving = (len > 0.1f);
    if (moving) {
        wishDir = Vector3Scale(wishDir, 1.0f / len);
        m_playerVel = Vector3Scale(wishDir, currentSpeed);

        m_stepTimer += dt;
        float stepInterval = isSprinting ? 0.32f : (m_isCrouching ? 0.65f : 0.48f);
        if (m_stepTimer >= stepInterval) {
            m_stepTimer = 0.0f;
            if (isSprinting) {
                eng.Audio().PlaySfx("audio/sfx/sfx_ch3_creak_wood_02.wav", 0.7f);
                TriggerNoise(eng, m_playerPos, 8.5f, 0.8f);
            } else if (!m_isCrouching) {
                eng.Audio().PlaySfx("audio/sfx/sfx_ch3_creak_wood_01.wav", 0.4f);
                TriggerNoise(eng, m_playerPos, 4.0f, 0.4f);
            }
        }

        m_headBobTimer += dt * (isSprinting ? 12.0f : 8.0f);
        m_headBobOffset = std::sin(m_headBobTimer) * (isSprinting ? 0.06f : 0.035f);
    } else {
        m_playerVel = {0.0f, 0.0f, 0.0f};
        m_headBobOffset = 0.0f;
    }

    if (IsKeyDown(KEY_SPACE) && m_playerPos.y > 0.0f && m_playerPos.y < 3.2f && m_playerPos.z > 0.5f && m_playerPos.z < 2.5f) {
        m_playerPos.y += 2.5f * dt;
    }

    ResolvePlayerPhysics(m_playerPos, m_playerVel, dt);

    float eyeHeight = m_isCrouching ? 0.85f : 1.65f;
    m_camera.position = {m_playerPos.x, m_playerPos.y + eyeHeight + m_headBobOffset, m_playerPos.z};

    Vector3 lookDir = {
        std::sin(m_playerYaw) * std::cos(m_playerPitch),
        std::sin(m_playerPitch),
        std::cos(m_playerYaw) * std::cos(m_playerPitch)
    };
    m_camera.target = Vector3Add(m_camera.position, lookDir);
}

void HouseWorld3D::CheckInteractions(engine::core::Engine& eng) {
    m_focusPrompt.clear();

    Vector3 rayOrigin = m_camera.position;
    Vector3 rayDir = Vector3Normalize(Vector3Subtract(m_camera.target, m_camera.position));
    float interactRange = 2.4f;

    InteractiveProp* focusedProp = nullptr;
    float closestDist = interactRange;

    for (auto& prop : m_props) {
        float d = Vector3Distance(rayOrigin, prop.center);
        if (d < closestDist) {
            closestDist = d;
            focusedProp = &prop;
        }
    }

    if (!focusedProp) return;

    std::string activeItem;
    if (m_activeSlot >= 0 && m_activeSlot < static_cast<int>(m_inventory.size())) {
        activeItem = m_inventory[m_activeSlot];
    }

    if (focusedProp->isHidingSpot) {
        m_focusPrompt = m_isHiding ? "[E] Выйти из укрытия" : "[E] Спрятаться: " + focusedProp->name;
        if (IsKeyPressed(KEY_E)) {
            m_isHiding = !m_isHiding;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_creak_wood_01.wav", 0.6f);
            if (m_isHiding) {
                m_currentHidingSpot = focusedProp->id;
                ShowNotification("Вы спрятались. Не шумите!");
            } else {
                m_currentHidingSpot.clear();
            }
        }
        return;
    }

    if (focusedProp->isWorkbench) {
        int parts = m_weapon.GetPartsAttachedCount();
        if (m_weapon.IsAssembled()) {
            m_focusPrompt = "Верстак: Ружье ТОЗ-34 собрано";
        } else if (m_weapon.CanAssemble()) {
            m_focusPrompt = "[E] Завершить сборку ружья ТОЗ-34!";
            if (IsKeyPressed(KEY_E)) {
                m_weapon.CompleteAssembly(eng);
                ShowNotification("Ружье собрано! Нажмите [ЛКМ] для выстрела, [R] перезарядка.");
            }
        } else {
            m_focusPrompt = "[E] Верстак (деталей: " + std::to_string(parts) + "/3)";
            if (IsKeyPressed(KEY_E)) {
                if (!activeItem.empty() && m_weapon.TryAttachPart(activeItem)) {
                    m_inventory.erase(m_inventory.begin() + m_activeSlot);
                    if (m_activeSlot >= static_cast<int>(m_inventory.size()) && m_activeSlot > 0) m_activeSlot--;
                    eng.Audio().PlaySfx("audio/sfx/sfx_ch3_shotgun_assemble.wav", 0.8f);
                    ShowNotification("Деталь установлена на верстак!");
                } else {
                    ShowNotification("Возьмите деталь ружья в руки (ствол, ложе или спуск).");
                }
            }
        }
        return;
    }

    if (focusedProp->isFrontDoor) {
        m_focusPrompt = "[E] Осмотреть парадную дверь";
        if (IsKeyPressed(KEY_E)) {
            std::string msg;
            std::string code = m_randomizer.GetDoorKeypadCode();
            bool ok = m_escape.TryInteractFrontDoor(eng, activeItem, m_enteredKeypadCode, code, msg);
            ShowNotification(msg);
            if (ok && activeItem == "item_master_key") {
                m_inventory.erase(m_inventory.begin() + m_activeSlot);
                if (m_activeSlot >= static_cast<int>(m_inventory.size()) && m_activeSlot > 0) m_activeSlot--;
            }
            if (m_escape.GetDoorLocks().IsFullyUnlocked()) {
                m_cutscene.Load("assets/data/chapter3/cutscenes/front_door_escape.json");
                m_cutscene.Play();
                m_inCutscene = true;
            }
        }
        return;
    }

    if (focusedProp->isCar) {
        m_focusPrompt = "[E] Осмотреть автомобиль Копейка";
        if (IsKeyPressed(KEY_E)) {
            std::string msg;
            bool ok = m_escape.TryInteractCar(eng, activeItem, msg);
            ShowNotification(msg);
            if (ok && (activeItem == "item_car_battery" || activeItem == "item_car_sparkplug" || activeItem == "item_gas_canister" || activeItem == "item_car_key")) {
                m_inventory.erase(m_inventory.begin() + m_activeSlot);
                if (m_activeSlot >= static_cast<int>(m_inventory.size()) && m_activeSlot > 0) m_activeSlot--;
            }
            if (m_escape.GetCarState().IsReady()) {
                m_cutscene.Load("assets/data/chapter3/cutscenes/car_escape.json");
                m_cutscene.Play();
                m_inCutscene = true;
            }
        }
        return;
    }

    if (focusedProp->isSewer) {
        m_focusPrompt = "[E] Дренажный коллектор подвала";
        if (IsKeyPressed(KEY_E)) {
            std::string msg;
            bool ok = m_escape.TryInteractSewer(eng, activeItem, msg);
            ShowNotification(msg);
            if (ok && activeItem == "item_sewer_key") {
                m_inventory.erase(m_inventory.begin() + m_activeSlot);
                if (m_activeSlot >= static_cast<int>(m_inventory.size()) && m_activeSlot > 0) m_activeSlot--;
            }
            if (m_escape.GetSewerState().IsReady()) {
                m_cutscene.Load("assets/data/chapter3/cutscenes/sewer_escape.json");
                m_cutscene.Play();
                m_inCutscene = true;
            }
        }
        return;
    }

    if (focusedProp->isRadio) {
        m_focusPrompt = "[E] Радиостанция (94.5 FM)";
        if (IsKeyPressed(KEY_E)) {
            std::string msg;
            bool ok = m_escape.TryInteractRadio(eng, activeItem, 94.5f, msg);
            ShowNotification(msg);
            if (ok && (activeItem == "item_radio_fuse" || activeItem == "item_vacuum_tube")) {
                m_inventory.erase(m_inventory.begin() + m_activeSlot);
                if (m_activeSlot >= static_cast<int>(m_inventory.size()) && m_activeSlot > 0) m_activeSlot--;
            }
            if (m_escape.GetRadioState().IsReady()) {
                m_cutscene.Load("assets/data/chapter3/cutscenes/radio_ending.json");
                m_cutscene.Play();
                m_inCutscene = true;
            }
        }
        return;
    }

    if (!focusedProp->slotId.empty()) {
        auto* slot = m_randomizer.GetSlotMut(focusedProp->slotId);
        if (slot) {
            if (!slot->searched) {
                m_focusPrompt = "[E] Обыскать: " + slot->name;
                if (IsKeyPressed(KEY_E)) {
                    slot->searched = true;
                    eng.Audio().PlaySfx("audio/sfx/sfx_ch3_creak_wood_01.wav", 0.5f);
                    if (!slot->containedItem.empty()) {
                        const auto* def = m_randomizer.GetItemDef(slot->containedItem);
                        if (static_cast<int>(m_inventory.size()) < 4) {
                            m_inventory.push_back(slot->containedItem);
                            std::string iname = def ? def->name : slot->containedItem;
                            ShowNotification("Получено: " + iname);
                            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.6f);
                            slot->containedItem.clear();
                        } else {
                            ShowNotification("Инвентарь полон! Нажмите [G], чтобы выбросить предмет.");
                            slot->searched = false;
                        }
                    } else {
                        ShowNotification("Здесь ничего нет.");
                    }
                }
            } else {
                m_focusPrompt = slot->name + " (пусто)";
            }
        }
    }
}

void HouseWorld3D::Update(engine::core::Engine& eng, float dt) {
    if (m_settingsUI.IsOpen()) {
        m_settingsUI.Update(eng);
        if (m_settingsUI.WantsQuitToMenu()) {
            m_settingsUI.ClearQuitToMenu();
            m_returnToMenu = true;
        }
        if (!m_settingsUI.IsOpen()) {
            DisableCursor();
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) && !m_inCutscene) {
        m_settingsUI.Open();
        EnableCursor();
        return;
    }

    if (m_inCutscene) {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
            m_cutscene.Stop();
        }
        m_cutscene.Update(eng, dt, m_camera);
        if (!m_cutscene.IsPlaying()) {
            m_inCutscene = false;
            std::string cname = m_cutscene.GetName();
            if (cname == "entity_knockout") {
                if (m_escape.GetCurrentDay() >= 5) {
                    m_cutscene.Load("assets/data/chapter3/cutscenes/game_over_day5.json");
                    m_cutscene.Play();
                    m_inCutscene = true;
                } else {
                    m_escape.AdvanceDay();
                    StartNewDay(eng, m_escape.GetCurrentDay());
                }
            } else if (cname == "game_over_day5") {
                m_returnToMenu = true;
            } else if (cname == "car_escape" || cname == "front_door_escape" || cname == "sewer_escape" || cname == "radio_ending") {
                m_chapterComplete = true;
            }
        }
        return;
    }

    if (IsKeyPressed(KEY_F)) {
        m_flashlightOn = !m_flashlightOn;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.7f);
    }

    if (IsKeyPressed(KEY_ONE)) m_activeSlot = 0;
    if (IsKeyPressed(KEY_TWO)) m_activeSlot = 1;
    if (IsKeyPressed(KEY_THREE)) m_activeSlot = 2;
    if (IsKeyPressed(KEY_FOUR)) m_activeSlot = 3;

    float wheel = GetMouseWheelMove();
    if (wheel > 0.1f) {
        m_activeSlot = (m_activeSlot + 1) % 4;
    } else if (wheel < -0.1f) {
        m_activeSlot = (m_activeSlot + 3) % 4;
    }

    if (IsKeyPressed(KEY_G)) {
        if (m_activeSlot >= 0 && m_activeSlot < static_cast<int>(m_inventory.size())) {
            std::string dropped = m_inventory[m_activeSlot];
            m_inventory.erase(m_inventory.begin() + m_activeSlot);
            if (m_activeSlot >= static_cast<int>(m_inventory.size()) && m_activeSlot > 0) m_activeSlot--;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_item_drop.wav", 0.8f);
            TriggerNoise(eng, m_playerPos, 10.0f, 0.8f);
            ShowNotification("Предмет выброшен со звоном! Сущность могла услышать!");
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m_weapon.IsAssembled()) {
        Vector3 hitPos;
        bool hitEntity = false;
        Vector3 shootDir = Vector3Normalize(Vector3Subtract(m_camera.target, m_camera.position));
        if (m_weapon.Fire(eng, m_camera.position, shootDir, 25.0f, hitPos, hitEntity)) {
            TriggerNoise(eng, m_playerPos, 50.0f, 1.0f);
            float distToGranny = Vector3Distance(m_granny.GetPosition(), m_camera.position);
            Vector3 toGranny = Vector3Normalize(Vector3Subtract(m_granny.GetPosition(), m_camera.position));
            float dot = Vector3DotProduct(shootDir, toGranny);
            if (dot > 0.85f && distToGranny < 20.0f) {
                m_granny.ApplyShotgunStun(eng);
                ShowNotification("Прямое попадание! Сущность повержена на 30 секунд!");
            }
        }
    }

    if (IsKeyPressed(KEY_R) && m_weapon.IsAssembled()) {
        if (!m_weapon.Reload(eng)) {
            ShowNotification("Нет патронов для перезарядки!");
        }
    }

    HandlePlayerMovement(eng, dt);
    CheckInteractions(eng);

    m_granny.Update(eng, dt, m_playerPos, m_isHiding);
    if (m_granny.DidCatchPlayer()) {
        m_granny.ClearCaughtFlag();
        m_cutscene.Load("assets/data/chapter3/cutscenes/entity_knockout.json");
        m_cutscene.Play();
        m_inCutscene = true;
    }

    m_weapon.Update(dt);

    if (m_notificationTimer > 0.0f) {
        m_notificationTimer -= dt;
    }

    if (m_dayTitleTimer > 0.0f) {
        m_dayTitleTimer -= dt;
        if (m_dayTitleTimer < 1.0f) {
            m_dayTitleAlpha = m_dayTitleTimer;
        }
    }
}

void HouseWorld3D::FixedUpdate(engine::core::Engine& eng, float fixedDt) {
}

void HouseWorld3D::RenderOffscreen(engine::core::Engine& eng) {
}

void HouseWorld3D::RenderSceneMesh(engine::core::Engine& eng) {
    rlBegin(RL_QUADS);
    for (const auto& q : m_wallQuads) {
        rlNormal3f(q.normal.x, q.normal.y, q.normal.z);
        rlTexCoord2f(0.0f, 0.0f);
        rlVertex3f(q.p0.x, q.p0.y, q.p0.z);

        rlTexCoord2f(q.uvScale, 0.0f);
        rlVertex3f(q.p1.x, q.p1.y, q.p1.z);

        rlTexCoord2f(q.uvScale, q.uvScale);
        rlVertex3f(q.p2.x, q.p2.y, q.p2.z);

        rlTexCoord2f(0.0f, q.uvScale);
        rlVertex3f(q.p3.x, q.p3.y, q.p3.z);
    }
    rlEnd();

    for (const auto& prop : m_props) {
        Vector3 size = Vector3Subtract(prop.bounds.max, prop.bounds.min);
        Vector3 center = Vector3Scale(Vector3Add(prop.bounds.min, prop.bounds.max), 0.5f);
        Color c = prop.isFrontDoor ? DARKBROWN :
                 (prop.isWorkbench ? BROWN :
                 (prop.isCar ? DARKBLUE : (prop.isRadio ? DARKGRAY : GRAY)));
        DrawCube(center, size.x, size.y, size.z, c);
        DrawCubeWires(center, size.x, size.y, size.z, BLACK);
    }
}

void HouseWorld3D::RenderEntity(engine::core::Engine& eng) {
    Vector3 gpos = m_granny.GetPosition();
    bool stunned = (m_granny.GetState() == EntityState::Stunned);

    if (stunned) {
        DrawCube({gpos.x, gpos.y - 0.7f, gpos.z}, 1.2f, 0.4f, 0.8f, DARKGRAY);
        DrawCubeWires({gpos.x, gpos.y - 0.7f, gpos.z}, 1.2f, 0.4f, 0.8f, BLACK);
    } else {
        DrawCylinder(gpos, 0.35f, 0.35f, 1.8f, 12, Color{25, 25, 30, 255});
        Vector3 headPos = {gpos.x, gpos.y + 0.9f, gpos.z};
        DrawSphere(headPos, 0.24f, Color{210, 205, 195, 255});

        float yaw = m_granny.GetYaw();
        Vector3 eyeOffset = {std::cos(yaw) * 0.18f, 0.05f, std::sin(yaw) * 0.18f};
        DrawSphere(Vector3Add(headPos, eyeOffset), 0.04f, RED);
    }
}

void HouseWorld3D::Render3D(engine::core::Engine& eng) {
    BeginMode3D(m_camera);

    ClearBackground(Color{12, 12, 16, 255});
    RenderSceneMesh(eng);
    RenderEntity(eng);

    EndMode3D();
}

void HouseWorld3D::RenderHUD(engine::core::Engine& eng) {
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    DrawCircle(screenW / 2, screenH / 2, 2.5f, ColorAlpha(WHITE, 0.75f));

    if (!m_focusPrompt.empty()) {
        Font font = eng.Assets().GetMainFont();
        Vector2 size = MeasureTextEx(font, m_focusPrompt.c_str(), 20.0f, 1.0f);
        Vector2 pos = {(screenW - size.x) * 0.5f, screenH * 0.5f + 30.0f};
        DrawRectangleRounded({pos.x - 12.0f, pos.y - 4.0f, size.x + 24.0f, size.y + 8.0f}, 0.25f, 4, ColorAlpha(BLACK, 0.75f));
        DrawTextEx(font, m_focusPrompt.c_str(), pos, 20.0f, 1.0f, RAYWHITE);
    }

    if (m_notificationTimer > 0.0f && !m_notificationText.empty()) {
        Font font = eng.Assets().GetMainFont();
        Vector2 size = MeasureTextEx(font, m_notificationText.c_str(), 22.0f, 1.0f);
        Vector2 pos = {(screenW - size.x) * 0.5f, 50.0f};
        DrawRectangleRounded({pos.x - 16.0f, pos.y - 6.0f, size.x + 32.0f, size.y + 12.0f}, 0.3f, 4, ColorAlpha(BLACK, 0.85f));
        DrawTextEx(font, m_notificationText.c_str(), pos, 22.0f, 1.0f, GOLD);
    }

    if (m_dayTitleTimer > 0.0f && m_dayTitleAlpha > 0.01f) {
        Font font = eng.Assets().GetMainFont();
        std::string title = "ДЕНЬ " + std::to_string(m_escape.GetCurrentDay());
        Vector2 size = MeasureTextEx(font, title.c_str(), 48.0f, 2.0f);
        Vector2 pos = {(screenW - size.x) * 0.5f, screenH * 0.35f};
        DrawTextEx(font, title.c_str(), {pos.x + 2.0f, pos.y + 2.0f}, 48.0f, 2.0f, ColorAlpha(BLACK, m_dayTitleAlpha));
        DrawTextEx(font, title.c_str(), pos, 48.0f, 2.0f, ColorAlpha(RAYWHITE, m_dayTitleAlpha));
    }

    int invSlotSize = 64;
    int invGap = 12;
    int totalInvW = 4 * invSlotSize + 3 * invGap;
    int startX = (screenW - totalInvW) / 2;
    int invY = screenH - invSlotSize - 25;

    Font fontSmall = eng.Assets().GetMainFont();

    for (int i = 0; i < 4; ++i) {
        int x = startX + i * (invSlotSize + invGap);
        Rectangle slotRect{static_cast<float>(x), static_cast<float>(invY), static_cast<float>(invSlotSize), static_cast<float>(invSlotSize)};

        Color slotBg = (i == m_activeSlot) ? ColorAlpha(GOLD, 0.35f) : ColorAlpha(BLACK, 0.65f);
        DrawRectangleRounded(slotRect, 0.2f, 4, slotBg);
        DrawRectangleRoundedLinesEx(slotRect, 0.2f, 4, (i == m_activeSlot ? 2.5f : 1.0f), (i == m_activeSlot ? GOLD : DARKGRAY));

        std::string num = std::to_string(i + 1);
        DrawTextEx(fontSmall, num.c_str(), {static_cast<float>(x + 6), static_cast<float>(invY + 4)}, 14.0f, 1.0f, (i == m_activeSlot ? GOLD : LIGHTGRAY));

        if (i < static_cast<int>(m_inventory.size())) {
            const auto* def = m_randomizer.GetItemDef(m_inventory[i]);
            if (def) {
                Texture2D tex = eng.Assets().GetTexture(def->iconPath);
                if (tex.id != 0) {
                    DrawTexturePro(tex, {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)},
                                   {static_cast<float>(x + 8), static_cast<float>(invY + 12), static_cast<float>(invSlotSize - 16), static_cast<float>(invSlotSize - 20)},
                                   {0, 0}, 0.0f, WHITE);
                } else {
                    DrawTextEx(fontSmall, def->name.substr(0, 4).c_str(), {static_cast<float>(x + 10), static_cast<float>(invY + 24)}, 12.0f, 1.0f, WHITE);
                }
            }
        }
    }

    if (m_weapon.IsAssembled()) {
        std::string ammoStr = "ТОЗ-34: " + std::to_string(m_weapon.GetLoadedAmmo()) + " / " + std::to_string(m_weapon.GetReserveAmmo());
        Vector2 asize = MeasureTextEx(fontSmall, ammoStr.c_str(), 18.0f, 1.0f);
        DrawRectangleRounded({static_cast<float>(screenW - asize.x - 35), static_cast<float>(screenH - 55), asize.x + 20.0f, 32.0f}, 0.3f, 4, ColorAlpha(BLACK, 0.8f));
        DrawTextEx(fontSmall, ammoStr.c_str(), {static_cast<float>(screenW - asize.x - 25), static_cast<float>(screenH - 48)}, 18.0f, 1.0f, (m_weapon.GetLoadedAmmo() > 0 ? GREEN : RED));
    }
}

void HouseWorld3D::RenderUI(engine::core::Engine& eng) {
    if (m_inCutscene) {
        m_cutscene.RenderUI(eng);
        return;
    }

    RenderHUD(eng);

    if (m_settingsUI.IsOpen()) {
        m_settingsUI.Render(eng);
    }
}

void HouseWorld3D::Shutdown(engine::core::Engine& eng) {
    EnableCursor();
}

}
