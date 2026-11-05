#include "game/chapter3/HouseWorld3D.h"
#include "game/chapter3/Ch3Graphics.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <random>

namespace game::chapter3 {

using engine::physics3d::CharacterConfig;
using engine::physics3d::INVALID_BODY;
using engine::physics3d::RayHit;
using engine::render3d::DynamicLight;
using engine::render3d::SceneEnvironment;

namespace {

constexpr float UI_W = 1280.0f;
constexpr float UI_H = 720.0f;
constexpr float WALK_SPEED = 2.8f;
constexpr float RUN_SPEED = 4.5f;
constexpr float CROUCH_SPEED = 1.5f;
constexpr float MOUSE_SENS = 0.0022f;
constexpr float USE_RANGE = 2.6f;


}

void HouseWorld3D::Init(engine::core::Engine& eng, int rendererOverride) {
    Shutdown(eng);
    m_eng = &eng;
    m_returnToMenu = false;
    m_chapterComplete = false;
    m_inCutscene = false;
    m_inventory.clear();
    m_activeSlot = 0;
    m_enteredKeypadCode.clear();
    m_time = 0.0f;

    const std::string& assetDir = eng.Assets().GetBaseDir();
    if (!m_lighting.Init(assetDir, 1024)) {
        TraceLog(LOG_ERROR, "CH3: failed to init lighting shaders");
        m_returnToMenu = true;
        return;
    }
    m_physics.Init();
    m_level.Build(eng, m_physics, m_lighting.LitShader());
    BuildEntityMeshes();
    {
        std::mt19937 lampRng(static_cast<unsigned int>(std::time(nullptr)) ^ 0x51ED270Bu);
        std::uniform_real_distribution<float> uni(0.0f, 1.0f);
        const auto& lamps = m_level.Lamps();
        m_lampOn.assign(lamps.size(), 0);
        for (size_t i = 0; i < lamps.size(); ++i) {
            const Vector3& p = lamps[i].position;
            bool bedroom = p.x < -1.5f && p.y > 3.2f && p.y < 6.4f && p.z > 1.5f;
            m_lampOn[i] = (!bedroom && uni(lampRng) < 0.34f) ? 1 : 0;
        }
    }

    if (rendererOverride >= 0) Ch3Graphics::Get().renderer = rendererOverride;
    m_hasFlashlight = false;
    m_flashlightOn = false;
    m_battery = 1.0f;
    m_fear = 0.0f;
    m_heartTimer = 0.0f;
    m_heartPulse = 0.0f;
    m_searchProgress = 0.0f;
    m_searchProp = -1;
    m_droppedItems.clear();
    m_inspectMode = false;
    InitRenderer(eng);
    ApplyDisplay(eng);

    CharacterConfig cfg;
    m_physics.CreateCharacter(m_level.PlayerStart(), cfg);
    m_fps.Init(m_level.PlayerStartYaw(), -0.05f, 68.0f);
    m_fps.Update(0.0f, m_physics.GetCharacterPosition(), engine::camera3d::CameraMotion{});
    m_viewCam = m_fps.GetCamera();
    m_flashDir = m_fps.GetForward();

    SetupProps();

    m_randomizer.Init();
    m_randomizer.Generate(static_cast<unsigned int>(std::time(nullptr)));
    for (const auto& slot : m_randomizer.GetAllSlots()) {
        InteractiveProp p;
        p.id = slot.id;
        p.name = slot.name;
        p.kind = PropKind::Slot;
        p.center = slot.position;
        p.radius = 0.4f;
        p.slotId = slot.id;
        m_props.push_back(p);
    }

    m_weapon.Reset();
    m_granny.Init();
    m_granny.SetOcclusionQuery([this](Vector3 a, Vector3 b) { return m_physics.IsStaticOccluded(a, b); });
    m_escape.Reset();

    m_ready = true;
    StartNewDay(eng, 1);
    PlayCutscene("intro_waking_up");

    DisableCursor();
    eng.Audio().PlayAmbient("audio/bgm/amb_rain_roof_lp.ogg", 0.65f);
}

void HouseWorld3D::SetupProps() {
    m_props.clear();
    auto hide = [this](const char* id, const char* name, Vector3 center, Vector3 eye, float yaw) {
        InteractiveProp p;
        p.id = id;
        p.name = name;
        p.kind = PropKind::Hide;
        p.center = center;
        p.radius = 0.6f;
        p.hideEye = eye;
        p.hideYaw = yaw;
        m_props.push_back(p);
    };
    hide("hide_bed", "под кровать", {-3.4f, 3.55f, 4.4f}, {-4.2f, 3.42f, 4.3f}, PI * 0.5f);
    hide("hide_living_wardrobe", "в шкаф", {-2.0f, 1.0f, 5.2f}, {-2.0f, 1.55f, 5.22f}, 0.0f);
    hide("hide_guest_wardrobe", "в шкаф", {5.2f, 4.2f, 4.65f}, {5.22f, 4.8f, 4.65f}, -PI * 0.5f);
    hide("hide_basement_cupboard", "в кладовой шкаф", {-5.2f, -2.0f, 2.6f}, {-5.22f, -1.4f, 2.6f}, PI * 0.5f);
    hide("hide_bedroom_wardrobe", "в шкаф", {-5.2f, 4.2f, 2.35f}, {-5.22f, 4.8f, 2.35f}, PI * 0.5f);
    hide("hide_dining_table", "под стол", {3.8f, 0.45f, 3.6f}, {3.8f, 0.6f, 3.6f}, 0.0f);
    hide("hide_guest_bed", "под кровать", {2.15f, 3.4f, 3.1f}, {2.15f, 3.4f, 3.05f}, PI);
    hide("hide_garage_car", "под машину", {-9.5f, 0.3f, -2.6f}, {-9.5f, 0.3f, -2.6f}, 0.0f);

    auto special = [this](const char* id, const char* name, PropKind kind, Vector3 center, float radius) {
        InteractiveProp p;
        p.id = id;
        p.name = name;
        p.kind = kind;
        p.center = center;
        p.radius = radius;
        m_props.push_back(p);
    };
    special("vent_pantry", "Лаз", PropKind::Vent, {-5.15f, 0.6f, 1.5f}, 0.9f);
    special("vent_storage", "Лаз", PropKind::Vent, {-3.45f, 3.8f, 1.5f}, 0.9f);
    special("prop_workbench", "Оружейный верстак", PropKind::Workbench, {-1.5f, -2.05f, -5.2f}, 0.7f);
    special("prop_front_door", "Парадная дверь", PropKind::FrontDoor, {0.0f, 1.2f, 5.85f}, 0.7f);
    special("prop_garage_car", "Автомобиль ВАЗ-2101", PropKind::Car, {-9.5f, 0.95f, -4.65f}, 0.65f);
    special("prop_sewer_grate", "Дренажный коллектор", PropKind::Sewer, {5.7f, -2.4f, 3.7f}, 0.6f);
    special("prop_attic_radio", "Радиостанция Маяка", PropKind::Radio, {3.0f, 7.45f, -5.4f}, 0.6f);
    special("prop_nightstand_flashlight", "Фонарик", PropKind::FlashlightPickup, {-5.65f, 3.85f, 4.9f}, 0.95f);
}

void HouseWorld3D::BuildEntityMeshes() {
    m_phantomTex = m_eng->Assets().GetTexture("sprites/3d/motel_phantom.png");
}

void HouseWorld3D::UnloadEntityMeshes() {
    m_phantomTex = Texture2D{};
}

void HouseWorld3D::StartNewDay(engine::core::Engine& eng, int dayNumber) {
    m_isHiding = false;
    m_hideProp = -1;
    m_moveVelocity = {0.0f, 0.0f, 0.0f};
    m_physics.TeleportCharacter(m_level.PlayerStart());
    m_fps.SetLookAngles(m_level.PlayerStartYaw(), -0.32f);
    m_fps.Update(0.0f, m_physics.GetCharacterPosition(), engine::camera3d::CameraMotion{});
    m_viewCam = m_fps.GetCamera();

    m_granny.ResetToDay(dayNumber);

    m_dayTitleTimer = 4.5f;
    m_dayTitleAlpha = 1.0f;

    if (!m_hasFlashlight) ShowNotification("Темно... Нужен свет. Осмотрите тумбочку у кровати.", 7.0f);
    if (dayNumber >= 3) {
        ShowNotification("Вы хромаете из-за травмы ноги (скорость бега снижена).", 4.0f);
    }
}

void HouseWorld3D::PlayCutscene(const std::string& name) {
    m_cutscene.SetOrigin(m_physics.GetCharacterPosition());
    if (!m_cutscene.Load(m_eng->Assets().GetBaseDir() + "/data/chapter3/cutscenes/" + name + ".json")) {
        m_cutscene.Load("assets/data/chapter3/cutscenes/" + name + ".json");
    }
    m_cutscene.Play();
    m_inCutscene = true;
}

void HouseWorld3D::TriggerNoise(engine::core::Engine&, Vector3 pos, float radius, float volume) {
    m_granny.NotifySound(pos, radius, volume);
}

void HouseWorld3D::ShowNotification(const std::string& text, float duration) {
    m_notificationText = text;
    m_notificationTimer = duration;
}

void HouseWorld3D::HandlePlayerMovement(engine::core::Engine& eng, float dt) {
    if (m_invOpen) {
        m_physics.UpdateCharacter(dt, Vector3{0.0f, 0.0f, 0.0f}, false, INVALID_BODY);
        m_fps.Update(dt, m_physics.GetCharacterPosition(), engine::camera3d::CameraMotion{});
        return;
    }
    if (m_isHiding && m_hideProp >= 0) {
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_SPACE)) {
            m_isHiding = false;
            m_hideProp = -1;
            return;
        }
        const InteractiveProp& spot = m_props[m_hideProp];
        m_fps.ApplyMouseLook(GetMouseDelta(), MOUSE_SENS);
        m_fps.ClampYawAround(spot.hideYaw, 0.85f, -0.3f, 0.25f);
        m_physics.UpdateCharacter(dt, Vector3{0.0f, 0.0f, 0.0f}, false, INVALID_BODY);
        m_fps.Update(dt, Vector3Subtract(spot.hideEye, Vector3{0.0f, m_fps.GetEyeHeight(), 0.0f}), engine::camera3d::CameraMotion{});
        m_sprinting = false;
        return;
    }

    m_fps.ApplyMouseLook(GetMouseDelta(), MOUSE_SENS);

    Vector3 fwd = m_fps.GetFlatForward();
    Vector3 right = m_fps.GetRight();
    Vector3 input{0.0f, 0.0f, 0.0f};
    float strafe = 0.0f;
    if (IsKeyDown(KEY_W)) input = Vector3Add(input, fwd);
    if (IsKeyDown(KEY_S)) input = Vector3Subtract(input, fwd);
    if (IsKeyDown(KEY_D)) {
        input = Vector3Add(input, right);
        strafe += 1.0f;
    }
    if (IsKeyDown(KEY_A)) {
        input = Vector3Subtract(input, right);
        strafe -= 1.0f;
    }
    float len = Vector3Length(input);
    if (len > 0.001f) input = Vector3Scale(input, 1.0f / len);

    bool wantCrouch = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C);
    bool crouched = m_physics.IsCharacterCrouched();
    m_sprinting = IsKeyDown(KEY_LEFT_SHIFT) && len > 0.001f && !crouched && !wantCrouch;
    float speed = crouched ? CROUCH_SPEED : (m_sprinting ? RUN_SPEED : WALK_SPEED);
    speed *= m_escape.GetPlayerSpeedMultiplier();
    Vector3 target = Vector3Scale(input, speed);
    float accel = m_physics.IsCharacterOnGround() ? 12.0f : 2.0f;
    m_moveVelocity = Vector3Lerp(m_moveVelocity, target, std::min(1.0f, dt * accel));

    m_physics.UpdateCharacter(dt, m_moveVelocity, wantCrouch, INVALID_BODY);

    engine::camera3d::CameraMotion motion;
    Vector3 vel = m_physics.GetCharacterVelocity();
    motion.horizontalSpeed = std::sqrt(vel.x * vel.x + vel.z * vel.z);
    motion.grounded = m_physics.IsCharacterOnGround();
    motion.crouched = m_physics.IsCharacterCrouched();
    motion.sprinting = m_sprinting;
    motion.strafe = strafe;
    m_fps.Update(dt, m_physics.GetCharacterPosition(), motion);

    if (m_fps.PopFootstep()) {
        Vector3 feet = m_physics.GetCharacterPosition();
        bool concrete = m_level.IsConcreteFloor(feet);
        bool quiet = m_physics.IsCharacterCrouched();
        int idx = (m_stepIndex++ % 4) + 1;
        char path[80];
        std::snprintf(path, sizeof(path), "audio/sfx/sfx_footstep_%s_%02d.wav", concrete ? "concrete" : "wood", idx);
        float vol = m_sprinting ? 0.7f : (quiet ? 0.18f : 0.42f);
        eng.Audio().PlaySfx(path, vol);
        if (m_sprinting) {
            TriggerNoise(eng, feet, 6.0f, 0.8f);
        } else if (!quiet) {
            TriggerNoise(eng, feet, 1.8f, 0.4f);
        }
    }
}

void HouseWorld3D::InitRenderer(engine::core::Engine& eng) {
    Ch3Graphics& g = Ch3Graphics::Get();
    m_hd = false;
    m_appliedRevision = g.revision;
    m_appliedRenderer = g.renderer;
    m_appliedRt = g.rayTracing;
    m_appliedShadow = g.shadowQuality;
    if (g.renderer == 0) return;
    if (g.renderer == 2) {
        m_hd = m_hdBridge.Init(eng, m_level, engine::hd::HdApi::D3D12);
        if (!m_hd) {
            TraceLog(LOG_ERROR, "CH3 HD: D3D12 init failed (%s), falling back to D3D11", m_hdBridge.Info().c_str());
            ShowNotification("DirectX 12 недоступен, используется DirectX 11.", 5.0f);
        }
    }
    if (!m_hd) m_hd = m_hdBridge.Init(eng, m_level, engine::hd::HdApi::D3D11);
    if (!m_hd) {
        TraceLog(LOG_ERROR, "CH3 HD: init failed (%s), using classic renderer", m_hdBridge.Info().c_str());
        ShowNotification("HD-рендер не запустился, используется классический.", 5.0f);
    }
}

void HouseWorld3D::ApplyDisplay(engine::core::Engine& eng) {
    const Ch3Graphics& g = Ch3Graphics::Get();
    auto& s = eng.Settings().Get();
    if (m_savedVsync < 0) {
        m_savedVsync = s.vsync ? 1 : 0;
        m_savedFpsIndex = s.targetFpsIndex;
    }
    s.vsync = g.vsync;
    eng.Settings().ApplyWindowAndFps(false);
    SetTargetFPS(g.FpsLimitValue());
}

void HouseWorld3D::ApplyGraphics(engine::core::Engine& eng) {
    Ch3Graphics& g = Ch3Graphics::Get();
    if (g.revision == m_appliedRevision) return;
    ApplyDisplay(eng);
    bool reinit = g.renderer != m_appliedRenderer || g.rayTracing != m_appliedRt || g.shadowQuality != m_appliedShadow;
    m_appliedRevision = g.revision;
    if (!reinit) return;
    m_hdBridge.Shutdown(eng);
    InitRenderer(eng);
    ApplyDisplay(eng);
}

void HouseWorld3D::UpdateFear(engine::core::Engine& eng, float dt) {
    if (IsKeyPressed(KEY_TAB)) {
        m_invOpen = !m_invOpen;
        m_journalOpen = false;
    }
    if (IsKeyPressed(KEY_H)) {
        m_helpOpen = !m_helpOpen && m_helpTimer <= 0.0f;
        m_helpTimer = 0.0f;
    }
    if (m_helpTimer > 0.0f) m_helpTimer -= dt;
    if (IsKeyPressed(KEY_J)) {
        m_journalOpen = !m_journalOpen;
        m_invOpen = false;
    }
    if (m_invOpen) UpdateInventory(eng);

    Vector3 feet = m_physics.GetCharacterPosition();
    Vector3 gp = m_granny.GetPosition();
    float d = Vector3Distance(gp, feet);
    EntityState st = m_granny.GetState();
    float target = 0.0f;
    if (st == EntityState::Chase) {
        target = 1.0f;
    } else if (st != EntityState::Stunned && st != EntityState::Knockout) {
        float prox = std::clamp(1.0f - d / 15.0f, 0.0f, 1.0f);
        bool visible = !m_physics.IsStaticOccluded(m_viewCam.position, Vector3Add(gp, Vector3{0.0f, 1.3f, 0.0f}));
        target = prox * (visible ? 1.0f : 0.4f);
    }
    float rate = target > m_fear ? 2.6f : 0.7f;
    m_fear += (target - m_fear) * std::clamp(dt * rate, 0.0f, 1.0f);

    m_heartPulse = std::max(0.0f, m_heartPulse - dt * 3.5f);
    if (m_fear > 0.12f) {
        m_heartTimer -= dt;
        if (m_heartTimer <= 0.0f) {
            m_heartTimer = 1.15f - 0.75f * m_fear;
            m_heartPulse = 1.0f;
            eng.Audio().PlaySfx("audio/sfx/sfx_heartbeat_dread.wav", 0.25f + 0.7f * m_fear);
        }
    } else {
        m_heartTimer = 0.0f;
    }
    m_fps.SetJitter(0.0f);
}

void HouseWorld3D::UpdateFlashlight(float dt) {
    const Camera3D& cam = m_viewCam;
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float follow = 1.0f - std::exp(-dt * 15.0f);
    m_flashDir = Vector3Normalize(Vector3Lerp(m_flashDir, fwd, follow));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, Vector3{0.0f, 1.0f, 0.0f}));
    Vector3 up = Vector3CrossProduct(right, fwd);
    m_flash.position = Vector3Add(cam.position, Vector3Add(Vector3Add(Vector3Scale(right, 0.17f), Vector3Scale(up, -0.13f)), Vector3Scale(fwd, 0.1f)));
    m_flash.direction = m_inCutscene ? fwd : m_flashDir;

    auto flick = [](float t) {
        float s = std::sin(t * 12.9898f) * 43758.5453f;
        return s - std::floor(s);
    };
    float power = 1.0f;
    bool stutter = false;
    if (m_battery < 0.2f) {
        power = 0.35f + 0.65f * (m_battery / 0.2f);
        stutter = false;
    }
    if (!m_inCutscene && m_granny.GetState() != EntityState::Stunned) {
        float d = m_granny.GetDistanceToPlayer();
        (void)d;
    }
    m_flashPower = power;
    m_flash.enabled = m_hasFlashlight && m_flashlightOn && !m_isHiding && m_battery > 0.0f && !stutter;
    m_flash.innerCos = std::cos(15.0f * DEG2RAD);
    m_flash.outerCos = std::cos(29.0f * DEG2RAD);
    m_flash.range = 14.0f;
    m_flash.intensity = 1.5f * power;
    m_flash.color = Vector3{1.0f, 0.9f, 0.74f};
}

void HouseWorld3D::TakeSlotItem(engine::core::Engine& eng, SearchSlot& slot) {
    eng.Audio().PlaySfx("audio/sfx/sfx_door_creak_open.wav", 0.5f);
    if (slot.containedItem.empty()) {
        slot.searched = true;
        ShowNotification("Здесь ничего нет.");
        return;
    }
    const ItemDef* def = m_randomizer.GetItemDef(slot.containedItem);
    std::string iname = def ? def->name : slot.containedItem;
    std::string picked = slot.containedItem;
    if (slot.containedItem == "item_flashlight") {
        m_hasFlashlight = true;
        m_flashlightOn = true;
        m_battery = 1.0f;
        slot.containedItem.clear();
        slot.searched = true;
        eng.Audio().PlaySfx("audio/sfx/sfx_flashlight_click.wav", 0.7f);
        ShowNotification("Фонарик! [F] — включить и выключить. Батарейки садятся, ищите запасные ([Q] — заменить).", 6.0f);
        m_inspectMode = true;
        m_inspectItem = picked;
        m_inspectYaw = 0.4f;
        m_inspectPitch = 0.15f;
        m_inspectZoom = 1.0f;
        return;
    }
    if (slot.containedItem == "item_shotgun_ammo") {
        m_weapon.AddAmmo(2);
        slot.containedItem.clear();
        slot.searched = true;
        eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.6f);
        ShowNotification("Получено: " + iname + " (+2)");
        m_inspectMode = true;
        m_inspectItem = picked;
        m_inspectYaw = 0.4f;
        m_inspectPitch = 0.15f;
        m_inspectZoom = 1.0f;
        return;
    }
    if (static_cast<int>(m_inventory.size()) >= 8) {
        ShowNotification("Инвентарь полон! Нажмите [G], чтобы выбросить предмет.");
        return;
    }
    m_inventory.push_back(picked);
    eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.6f);
    if (picked == "item_code_note") {
        ShowNotification("Записка: код от двери — " + m_randomizer.GetDoorKeypadCode(), 5.0f);
    } else {
        ShowNotification("Получено: " + iname);
    }
    slot.containedItem.clear();
    slot.searched = true;
    m_inspectMode = true;
    m_inspectItem = picked;
    m_inspectYaw = 0.4f;
    m_inspectPitch = 0.15f;
    m_inspectZoom = 1.0f;
}


void HouseWorld3D::ConsumeActiveItem() {
    if (m_activeSlot < 0 || m_activeSlot >= static_cast<int>(m_inventory.size())) return;
    m_inventory.erase(m_inventory.begin() + m_activeSlot);
    if (m_activeSlot >= static_cast<int>(m_inventory.size()) && m_activeSlot > 0) m_activeSlot--;
}

void HouseWorld3D::CheckInteractions(engine::core::Engine& eng) {
    m_focusPrompt.clear();

    Vector3 origin = m_viewCam.position;
    Vector3 dir = Vector3Normalize(Vector3Subtract(m_viewCam.target, m_viewCam.position));
    RayHit hit = m_physics.CastRay(origin, dir, USE_RANGE + 0.8f, m_physics.GetCharacterInnerBody());
    float limit = hit.hit ? hit.distance : USE_RANGE + 0.8f;

    if (!m_isHiding) {
        Vector3 feet = m_physics.GetCharacterPosition();
        Vector3 fwd = m_fps.GetFlatForward();
        int bestHide = -1;
        float bestD = 1.7f;
        for (size_t i = 0; i < m_props.size(); ++i) {
            const InteractiveProp& p = m_props[i];
            if (p.kind != PropKind::Hide) continue;
            if (std::abs(p.center.y - feet.y) > 2.4f) continue;
            Vector3 to{p.center.x - feet.x, 0.0f, p.center.z - feet.z};
            float d = Vector3Length(to);
            if (d > bestD) continue;
            if (d > 0.9f && Vector3DotProduct(fwd, Vector3Scale(to, 1.0f / d)) < 0.25f) continue;
            bestD = d;
            bestHide = static_cast<int>(i);
        }
        if (bestHide >= 0) {
            m_searchProgress = 0.0f;
            m_searchProp = -1;
            m_focusPrompt = "[E] Спрятаться " + m_props[bestHide].name;
            if (IsKeyPressed(KEY_E)) {
                m_isHiding = true;
                m_hideProp = bestHide;
                m_fps.SetLookAngles(m_props[bestHide].hideYaw, 0.0f);
                eng.Audio().PlaySfx("audio/sfx/sfx_door_creak_open.wav", 0.55f);
                ShowNotification("Вы спрятались. [E] или движение — выйти. Не шумите!", 3.5f);
            }
            return;
        }
    }
    int bestDropped = -1;
    float bestDropDist = 2.4f;
    for (size_t i = 0; i < m_droppedItems.size(); ++i) {
        Vector3 toItem = Vector3Subtract(m_droppedItems[i].pos, origin);
        float t = Vector3DotProduct(toItem, dir);
        if (t < 0.1f || t > 2.8f) continue;
        float perp = Vector3Distance(Vector3Add(origin, Vector3Scale(dir, t)), m_droppedItems[i].pos);
        if (perp < 0.55f && t < bestDropDist) {
            bestDropDist = t;
            bestDropped = static_cast<int>(i);
        }
    }
    if (bestDropped >= 0) {
        std::string droppedId = m_droppedItems[bestDropped].itemId;
        const ItemDef* d = m_randomizer.GetItemDef(droppedId);
        std::string name = d ? d->name : droppedId;
        m_focusPrompt = "[E] Подобрать: " + name;
        if (IsKeyPressed(KEY_E)) {
            if (static_cast<int>(m_inventory.size()) < 8) {
                m_inventory.push_back(droppedId);
                m_droppedItems.erase(m_droppedItems.begin() + bestDropped);
                eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.6f);
                ShowNotification("Подобрано: " + name);
                m_inspectMode = true;
                m_inspectItem = droppedId;
                m_inspectYaw = 0.4f;
                m_inspectPitch = 0.15f;
                m_inspectZoom = 1.0f;
            } else {
                ShowNotification("Инвентарь полон! Выбросьте предмет на [G].");
            }
        }
        return;
    }
    int focused = -1;
    if (m_isHiding) {
        focused = m_hideProp;
    } else {
        float best = 1e9f;
        for (size_t i = 0; i < m_props.size(); ++i) {
            const InteractiveProp& p = m_props[i];
            Vector3 to = Vector3Subtract(p.center, origin);
            float t = Vector3DotProduct(to, dir);
            if (t < 0.15f || t > USE_RANGE) continue;
            float perp = Vector3Distance(Vector3Add(origin, Vector3Scale(dir, t)), p.center);
            if (perp > p.radius) continue;
            if (t > limit + p.radius + 0.15f) continue;
            float score = perp + t * 0.1f;
            if (score < best) {
                best = score;
                focused = static_cast<int>(i);
            }
        }
    }
    if (focused < 0) {
        m_searchProgress = 0.0f;
        m_searchProp = -1;
        return;
    }
    InteractiveProp& prop = m_props[focused];

    std::string activeItem;
    if (m_activeSlot >= 0 && m_activeSlot < static_cast<int>(m_inventory.size())) activeItem = m_inventory[m_activeSlot];
    bool use = IsKeyPressed(KEY_E);

    if (prop.kind != PropKind::Slot) {
        m_searchProgress = 0.0f;
        m_searchProp = -1;
    }
    switch (prop.kind) {
    case PropKind::Vent:
        m_focusPrompt = "Лаз: удерживайте [Ctrl] и проползите (маньяк здесь не пролезет)";
        return;
    case PropKind::Hide:
        m_focusPrompt = m_isHiding ? "[E] Выйти из укрытия" : "[E] Спрятаться " + prop.name;
        if (use) {
            m_isHiding = !m_isHiding;
            eng.Audio().PlaySfx(m_isHiding ? "audio/sfx/sfx_door_creak_open.wav" : "audio/sfx/sfx_door_close_latch.wav", 0.5f);
            if (m_isHiding) {
                m_hideProp = focused;
                m_fps.SetLookAngles(prop.hideYaw, 0.0f);
                ShowNotification("Вы спрятались. Не шумите!");
            } else {
                m_hideProp = -1;
            }
        }
        return;
    case PropKind::FlashlightPickup:
        if (m_hasFlashlight) return;
        m_focusPrompt = "[E] Взять фонарик";
        if (use) {
            m_hasFlashlight = true;
            m_flashlightOn = true;
            m_battery = 1.0f;
            m_inventory.push_back("item_flashlight");
            eng.Audio().PlaySfx("audio/sfx/sfx_flashlight_click.wav", 0.75f);
            ShowNotification("Фонарик подобран! [F] — включить/выключить. Батарейки можно менять на [Q].", 5.0f);
            m_inspectMode = true;
            m_inspectItem = "item_flashlight";
            m_inspectYaw = 0.4f;
            m_inspectPitch = 0.15f;
            m_inspectZoom = 1.0f;
        }
        return;
    case PropKind::Workbench:
        if (m_weapon.IsAssembled()) {
            m_focusPrompt = "Верстак: ружьё ТОЗ-34 собрано";
        } else if (m_weapon.CanAssemble()) {
            m_focusPrompt = "[E] Завершить сборку ружья ТОЗ-34";
            if (use) {
                m_weapon.CompleteAssembly(eng);
                ShowNotification("Ружьё собрано! [ЛКМ] выстрел, [R] перезарядка.");
            }
        } else {
            m_focusPrompt = "[E] Верстак (деталей: " + std::to_string(m_weapon.GetPartsAttachedCount()) + "/3)";
            if (use) {
                if (!activeItem.empty() && m_weapon.TryAttachPart(activeItem)) {
                    ConsumeActiveItem();
                    eng.Audio().PlaySfx("audio/sfx/sfx_ch3_shotgun_assemble.wav", 0.8f);
                    ShowNotification("Деталь установлена на верстак!");
                } else {
                    ShowNotification("Возьмите деталь ружья в руки (ствол, ложе или спуск).");
                }
            }
        }
        return;
    case PropKind::FrontDoor:
        m_focusPrompt = "[E] Осмотреть парадную дверь";
        if (use) {
            std::string code = m_randomizer.GetDoorKeypadCode();
            if (std::find(m_inventory.begin(), m_inventory.end(), "item_code_note") != m_inventory.end()) m_enteredKeypadCode = code;
            std::string msg;
            bool ok = m_escape.TryInteractFrontDoor(eng, activeItem, m_enteredKeypadCode, code, msg);
            ShowNotification(msg);
            if (ok && activeItem == "item_master_key") ConsumeActiveItem();
            if (m_escape.GetDoorLocks().IsFullyUnlocked()) PlayCutscene("front_door_escape");
        }
        return;
    case PropKind::Car:
        m_focusPrompt = "[E] Осмотреть автомобиль «Копейка»";
        if (use) {
            std::string msg;
            bool ok = m_escape.TryInteractCar(eng, activeItem, msg);
            ShowNotification(msg);
            if (ok && (activeItem == "item_car_battery" || activeItem == "item_car_sparkplug" || activeItem == "item_gas_canister" || activeItem == "item_car_key")) {
                ConsumeActiveItem();
            }
            if (m_escape.GetCarState().IsReady()) PlayCutscene("car_escape");
        }
        return;
    case PropKind::Sewer:
        m_focusPrompt = "[E] Дренажный коллектор";
        if (use) {
            std::string msg;
            bool ok = m_escape.TryInteractSewer(eng, activeItem, msg);
            ShowNotification(msg);
            if (ok && activeItem == "item_sewer_key") ConsumeActiveItem();
            if (m_escape.GetSewerState().IsReady()) PlayCutscene("sewer_escape");
        }
        return;
    case PropKind::Radio:
        m_focusPrompt = "[E] Радиостанция (94.5 FM)";
        if (use) {
            std::string msg;
            bool ok = m_escape.TryInteractRadio(eng, activeItem, 94.5f, msg);
            ShowNotification(msg);
            if (ok && (activeItem == "item_radio_fuse" || activeItem == "item_vacuum_tube")) ConsumeActiveItem();
            if (m_escape.GetRadioState().IsReady()) PlayCutscene("radio_ending");
        }
        return;
    case PropKind::Slot:
        break;
    }

    SearchSlot* slot = m_randomizer.GetSlotMut(prop.slotId);
    if (!slot) return;
    if (slot->searched) {
        m_focusPrompt = slot->name + " (пусто)";
        m_searchProgress = 0.0f;
        m_searchProp = -1;
        return;
    }
    if (slot->locked) {
        m_focusPrompt = "[E] " + slot->name + " (заперто)";
        m_searchProgress = 0.0f;
        if (use) {
            if (activeItem == slot->requiredTool) {
                slot->locked = false;
                eng.Audio().PlaySfx("audio/sfx/sfx_ch3_crowbar_pry.wav", 0.9f);
                TriggerNoise(eng, slot->position, 8.0f, 0.7f);
                ShowNotification("Крышка поддалась монтировке!");
            } else {
                ShowNotification("Заперто. Нужна монтировка.");
            }
        }
        return;
    }
    const ItemDef* def = m_randomizer.GetItemDef(slot->containedItem);
    if (slot->open) {
        if (slot->containedItem.empty()) {
            slot->searched = true;
            return;
        }
        m_focusPrompt = "[E] Взять: " + (def ? def->name : slot->containedItem);
        if (use) TakeSlotItem(eng, *slot);
        return;
    }
    m_focusPrompt = "[E] (удерживайте) Обыскать: " + slot->name;
    if (IsKeyDown(KEY_E)) {
        if (m_searchProp != focused) {
            m_searchProp = focused;
            m_searchProgress = 0.0f;
            eng.Audio().PlaySfx("audio/sfx/sfx_ch3_creak_wood_01.wav", 0.3f);
            TriggerNoise(eng, slot->position, 3.5f, 0.4f);
        }
        float before = m_searchProgress;
        m_searchProgress += m_frameDt / 1.1f;
        if (static_cast<int>(before * 3.0f) != static_cast<int>(m_searchProgress * 3.0f)) TriggerNoise(eng, slot->position, 3.0f, 0.3f);
        if (m_searchProgress >= 1.0f) {
            m_searchProgress = 0.0f;
            m_searchProp = -1;
            TakeSlotItem(eng, *slot);
        }
    } else {
        m_searchProgress = 0.0f;
        m_searchProp = -1;
    }
}

#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
void HouseWorld3D::StartTour() {
    m_hasFlashlight = true;
    m_flashlightOn = true;
    m_tour = {
        {"ch3_01_bedroom_bed", {-3.0f, 3.25f, 3.2f}, 46.4f, -18.0f},
        {"ch3_02_bedroom_door", {-3.0f, 3.25f, 3.2f}, 90.0f, -5.0f},
        {"ch3_03_hall2f", {0.0f, 3.25f, 4.8f}, 0.0f, -10.0f},
        {"ch3_04_hall1f", {-0.3f, 0.05f, 4.8f}, 0.0f, -5.0f},
        {"ch3_05_front_door", {0.0f, 0.05f, 3.6f}, 180.0f, -5.0f},
        {"ch3_06_living", {-2.2f, 0.05f, 2.4f}, -125.0f, -10.0f},
        {"ch3_07_kitchen", {3.0f, 0.05f, -0.4f}, 20.0f, -12.0f},
        {"ch3_08_dining", {2.2f, 0.05f, 5.3f}, 150.0f, -12.0f},
        {"ch3_09_garage", {-7.2f, 0.05f, 2.6f}, -150.0f, -8.0f},
        {"ch3_10_pantry", {-2.4f, 0.05f, -2.5f}, -90.0f, -18.0f},
        {"ch3_11_basement", {-3.5f, -2.95f, -2.2f}, 40.0f, -8.0f},
        {"ch3_12_boiler", {3.6f, -2.95f, 2.2f}, 160.0f, -8.0f},
        {"ch3_13_bathroom", {2.4f, 3.25f, 0.2f}, 60.0f, -10.0f},
        {"ch3_14_guest", {2.3f, 3.25f, 3.5f}, 120.0f, -10.0f},
        {"ch3_15_storage", {-2.2f, 3.25f, -2.5f}, -90.0f, -5.0f},
        {"ch3_16_attic", {0.8f, 6.45f, 1.5f}, 30.0f, -5.0f},
        {"ch3_17_outside", {0.0f, -0.25f, 15.0f}, 0.0f, 3.0f},
        {"ch3_18_walk_main_stairs", {0.85f, 0.05f, -5.6f}, 180.0f, 0.0f, 3.0f},
        {"ch3_19_walk_basement_stairs", {-5.4f, 0.05f, 0.7f}, 0.0f, 0.0f, 3.0f},
        {"ch3_20_walk_attic_stairs", {-5.4f, 3.25f, -5.2f}, 180.0f, 0.0f, 3.0f},
        {"ch3_21_walk_into_wall", {0.0f, 0.05f, 2.0f}, 90.0f, 0.0f, 2.5f},
        {"ch3_22_walk_through_door", {-0.3f, 0.05f, 3.5f}, -90.0f, 0.0f, 2.5f},
    };
    m_tourIndex = 0;
    m_tourFrames = 0;
    m_cutscene.Stop();
    m_inCutscene = false;
}

void HouseWorld3D::UpdateTour(engine::core::Engine& eng, float dt) {
    if (m_tourIndex >= static_cast<int>(m_tour.size())) {
        TraceLog(LOG_WARNING, "CH3 TOUR: avg frame %.2f ms (%.0f fps) over %d frames", m_tourTime / std::max(1, m_tourTotalFrames) * 1000.0f, static_cast<float>(m_tourTotalFrames) / std::max(0.001f, m_tourTime), m_tourTotalFrames);
        m_granny.ResetToDay(2);
        float minY = 99.0f;
        float maxY = -99.0f;
        for (int i = 0; i < 9000; ++i) {
            m_granny.Update(eng, 1.0f / 30.0f, Vector3{0.0f, 3.2f, 4.8f}, true, false, false);
            Vector3 g = m_granny.GetPosition();
            minY = std::min(minY, g.y);
            maxY = std::max(maxY, g.y);
            if (i % 300 == 0) TraceLog(LOG_WARNING, "CH3 AI: t=%d pos=(%.1f %.1f %.1f) state=%d", i / 30, g.x, g.y, g.z, static_cast<int>(m_granny.GetState()));
        }
        TraceLog(LOG_WARNING, "CH3 AI: y range %.1f..%.1f", minY, maxY);
        eng.RequestQuit();
        return;
    }
    const TourStop& stop = m_tour[m_tourIndex];
    if (m_tourFrames == 0) {
        m_physics.TeleportCharacter(stop.feet);
        m_fps.SetLookAngles(stop.yawDeg * DEG2RAD, stop.pitchDeg * DEG2RAD);
        m_flashDir = m_fps.GetForward();
        m_granny.ResetToDay(1);
        m_tourWalk = 0.0f;
    }
    ++m_tourFrames;
    m_tourTime += dt;
    ++m_tourTotalFrames;
    Vector3 vel{0.0f, 0.0f, 0.0f};
    bool walking = m_tourFrames > 10 && m_tourWalk < stop.walkSeconds;
    if (walking) {
        vel = Vector3Scale(m_fps.GetFlatForward(), WALK_SPEED);
        m_tourWalk += dt;
        m_tourFrames = 11;
    }
    m_physics.UpdateCharacter(dt, vel, false, INVALID_BODY);
    engine::camera3d::CameraMotion motion;
    motion.grounded = m_physics.IsCharacterOnGround();
    m_fps.Update(dt, m_physics.GetCharacterPosition(), motion);
    m_viewCam = m_fps.GetCamera();
    UpdateFlashlight(dt);
    int shotFrame = 40;
    if (m_tourFrames == shotFrame) {
        std::string file = stop.name + ".png";
        TakeScreenshot(file.c_str());
        Vector3 p = m_physics.GetCharacterPosition();
        TraceLog(LOG_WARNING, "CH3 TOUR: %s feet=(%.2f %.2f %.2f) ground=%d", file.c_str(), p.x, p.y, p.z, m_physics.IsCharacterOnGround() ? 1 : 0);
    }
    if (m_tourFrames > shotFrame) {
        ++m_tourIndex;
        m_tourFrames = 0;
    }
}
#endif

void HouseWorld3D::Update(engine::core::Engine& eng, float dt) {
    if (!m_ready) return;
    m_time += dt;
    m_frameDt = dt;
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    if (m_tourIndex >= 0) {
        UpdateTour(eng, dt);
        return;
    }
#endif

    if (m_inspectMode) {
        Vector2 md = GetMouseDelta();
        m_inspectYaw += md.x * 0.012f;
        m_inspectPitch = std::clamp(m_inspectPitch + md.y * 0.012f, -1.2f, 1.2f);
        m_inspectZoom = std::clamp(m_inspectZoom - GetMouseWheelMove() * 0.1f, 0.5f, 1.8f);
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            m_inspectMode = false;
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.55f);
        }
        return;
    }

    if (m_settingsUI.IsOpen()) {
        m_settingsUI.Update(eng);
        if (m_settingsUI.WantsQuitToMenu()) {
            m_settingsUI.ClearQuitToMenu();
            m_returnToMenu = true;
        }
        if (!m_settingsUI.IsOpen()) {
            DisableCursor();
            ApplyGraphics(eng);
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) && !m_inCutscene) {
        m_settingsUI.Open();
        EnableCursor();
        return;
    }

    if (m_inCutscene) {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) m_cutscene.Stop();
        m_cutscene.Update(eng, dt, m_viewCam);
        m_physics.UpdateCharacter(dt, Vector3{0.0f, 0.0f, 0.0f}, false, INVALID_BODY);
        UpdateFlashlight(dt);
        if (!m_cutscene.IsPlaying()) {
            m_inCutscene = false;
            std::string cname = m_cutscene.GetName();
            if (cname == "entity_knockout") {
                if (m_escape.GetCurrentDay() >= 5) {
                    PlayCutscene("game_over_day5");
                } else {
                    m_escape.AdvanceDay();
                    StartNewDay(eng, m_escape.GetCurrentDay());
                }
            } else if (cname == "game_over_day5") {
                m_returnToMenu = true;
            } else if (cname == "car_escape" || cname == "front_door_escape" || cname == "sewer_escape" || cname == "radio_ending") {
                m_chapterComplete = true;
            } else {
                m_viewCam = m_fps.GetCamera();
            }
        }
        return;
    }

    if (IsKeyPressed(KEY_F)) {
        if (!m_hasFlashlight) {
            ShowNotification("У вас нет фонарика. Поищите в спальне, на тумбочке.");
        } else if (m_battery <= 0.0f) {
            ShowNotification("Батарейки сели. Нужны новые ([Q] — заменить).");
        } else {
            m_flashlightOn = !m_flashlightOn;
            eng.Audio().PlaySfx("audio/sfx/sfx_flashlight_click.wav", 0.55f);
        }
    }
    if (IsKeyPressed(KEY_Q) && m_hasFlashlight) {
        auto it = std::find(m_inventory.begin(), m_inventory.end(), "item_batteries");
        if (it == m_inventory.end()) {
            ShowNotification("Нет запасных батареек.");
        } else if (m_battery > 0.9f) {
            ShowNotification("Батарейки ещё свежие.");
        } else {
            m_inventory.erase(it);
            if (m_activeSlot >= static_cast<int>(m_inventory.size()) && m_activeSlot > 0) m_activeSlot--;
            m_battery = 1.0f;
            m_flashlightOn = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.6f);
            ShowNotification("Батарейки заменены.");
        }
    }
    if (m_flash.enabled) m_battery = std::max(0.0f, m_battery - dt / 540.0f);

    if (IsKeyPressed(KEY_ONE)) m_activeSlot = 0;
    if (IsKeyPressed(KEY_TWO)) m_activeSlot = 1;
    if (IsKeyPressed(KEY_THREE)) m_activeSlot = 2;
    if (IsKeyPressed(KEY_FOUR)) m_activeSlot = 3;
    if (IsKeyPressed(KEY_FIVE)) m_activeSlot = 4;
    if (IsKeyPressed(KEY_SIX)) m_activeSlot = 5;
    if (IsKeyPressed(KEY_SEVEN)) m_activeSlot = 6;
    if (IsKeyPressed(KEY_EIGHT)) m_activeSlot = 7;
    float wheel = GetMouseWheelMove();
    if (wheel > 0.1f) m_activeSlot = (m_activeSlot + 1) % 8;
    else if (wheel < -0.1f) m_activeSlot = (m_activeSlot + 7) % 8;

    Vector3 feet = m_physics.GetCharacterPosition();

    if (IsKeyPressed(KEY_G) && m_activeSlot < static_cast<int>(m_inventory.size())) {
        std::string droppedId = m_inventory[m_activeSlot];
        m_inventory.erase(m_inventory.begin() + m_activeSlot);
        if (m_activeSlot >= static_cast<int>(m_inventory.size()) && m_activeSlot > 0) {
            m_activeSlot--;
        }
        Vector3 camFwd = Vector3Normalize(Vector3Subtract(m_viewCam.target, m_viewCam.position));
        camFwd.y = 0.0f;
        if (Vector3Length(camFwd) < 0.1f) camFwd = {0.0f, 0.0f, 1.0f};
        else camFwd = Vector3Normalize(camFwd);

        Vector3 dropPos = Vector3Add(feet, Vector3Scale(camFwd, 0.75f));
        dropPos.y = feet.y + 0.08f;
        m_droppedItems.push_back({droppedId, dropPos, m_fps.GetYaw()});

        eng.Audio().PlaySfx("audio/sfx/sfx_ch3_item_drop.wav", 0.8f);
        TriggerNoise(eng, feet, 10.0f, 0.8f);
        ShowNotification("Предмет лежит на полу! [G] (Сущность могла услышать звон)", 3.0f);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m_weapon.IsAssembled() && !m_isHiding && !m_invOpen) {
        Vector3 hitPos;
        bool hitEntity = false;
        Vector3 shootDir = Vector3Normalize(Vector3Subtract(m_viewCam.target, m_viewCam.position));
        if (m_weapon.Fire(eng, m_viewCam.position, shootDir, 25.0f, hitPos, hitEntity)) {
            TriggerNoise(eng, feet, 50.0f, 1.0f);
            Vector3 chest = Vector3Add(m_granny.GetPosition(), Vector3{0.0f, 1.2f, 0.0f});
            float dist = Vector3Distance(chest, m_viewCam.position);
            float dot = Vector3DotProduct(shootDir, Vector3Normalize(Vector3Subtract(chest, m_viewCam.position)));
            if (m_granny.GetState() != EntityState::Stunned && dot > 0.9f && dist < 20.0f &&
                !m_physics.IsStaticOccluded(m_viewCam.position, chest)) {
                m_granny.ApplyShotgunStun(eng);
                ShowNotification("Прямое попадание! Сущность повержена на 30 секунд!");
            }
        }
    }

    if (IsKeyPressed(KEY_R) && m_weapon.IsAssembled() && !m_weapon.Reload(eng)) {
        ShowNotification("Нет патронов для перезарядки!");
    }

    HandlePlayerMovement(eng, dt);
    m_viewCam = m_fps.GetCamera();
    m_viewCam.fovy = static_cast<float>(Ch3Graphics::Get().fov);
    UpdateFlashlight(dt);
    if (m_invOpen) m_focusPrompt.clear();
    else CheckInteractions(eng);

    m_granny.Update(eng, dt, m_physics.GetCharacterPosition(), m_isHiding, m_physics.IsCharacterCrouched(), m_flash.enabled);
    UpdateFear(eng, dt);
    int alert = m_granny.ConsumeAlert();
    if (alert != 0) {
        static const char* lines[6] = {"", "Ага! Попался!", "Нужно проверить, где был звук...", "Видимо, показалось...", "Где же ты?..", "Куда же ты делся? Пойду поищу тут..."};
        m_subText = lines[alert];
        m_subTimer = 3.2f;
        m_subRed = alert == 1 || alert == 4 || alert == 5;
        if (alert == 1) {
            m_alertText = "ОН ВАС ЗАМЕТИЛ! БЕГИТЕ!";
            m_alertTimer = 3.0f;
            m_alertRed = true;
        } else if (alert == 2) {
            m_alertText = "МАНЬЯК УСЛЫШАЛ ШУМ И ИДЕТ ПРОВЕРЯТЬ!";
            m_alertTimer = 3.0f;
            m_alertRed = false;
        } else if (alert == 3) {
            m_alertText = "Маньяк: «Видимо, показалось...»";
            m_alertTimer = 2.5f;
            m_alertRed = false;
        }
    }
    if (m_subTimer > 0.0f) m_subTimer -= dt;
    if (m_alertTimer > 0.0f) m_alertTimer -= dt;
    if (m_isHiding && m_granny.IsForceCatching()) {
        m_alertText = "ОН ВИДЕЛ, КУДА ВЫ СПРЯТАЛИСЬ!";
        m_alertTimer = 1.0f;
        m_alertRed = true;
    }
    if (m_granny.DidCatchPlayer()) {
        m_granny.ClearCaughtFlag();
        m_isHiding = false;
        m_hideProp = -1;
        PlayCutscene("entity_knockout");
    }

    m_weapon.Update(dt);

    if (m_notificationTimer > 0.0f) m_notificationTimer -= dt;
    if (m_dayTitleTimer > 0.0f) {
        m_dayTitleTimer -= dt;
        if (m_dayTitleTimer < 1.0f) m_dayTitleAlpha = std::max(0.0f, m_dayTitleTimer);
    }
}

void HouseWorld3D::UpdateInventory(engine::core::Engine& eng) {
    int n = 8;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) m_activeSlot = (m_activeSlot + n - 1) % n;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) m_activeSlot = (m_activeSlot + 1) % n;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) m_activeSlot = (m_activeSlot + n - 4) % n;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) m_activeSlot = (m_activeSlot + 4) % n;
    if (IsKeyPressed(KEY_ESCAPE)) m_invOpen = false;
    if (!IsKeyPressed(KEY_E) || m_activeSlot >= static_cast<int>(m_inventory.size())) return;

    const std::string id = m_inventory[m_activeSlot];
    if (id == "item_batteries") {
        if (!m_hasFlashlight) {
            ShowNotification("Сначала найдите фонарик.");
        } else if (m_battery > 0.9f) {
            ShowNotification("Батарейки ещё свежие.");
        } else {
            ConsumeActiveItem();
            m_battery = 1.0f;
            m_flashlightOn = true;
            eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.6f);
            ShowNotification("Батарейки заменены.");
        }
    } else if (id == "item_code_note") {
        ShowNotification("Код от двери: " + m_randomizer.GetDoorKeypadCode(), 5.0f);
    } else {
        ShowNotification("Возьмите предмет в руку (1-8), наведитесь на объект и нажмите [E].", 4.0f);
    }
}

void HouseWorld3D::RenderHelp(engine::core::Engine& eng) {
    int screenH = eng.Renderer().GetUIHeight();
    Font font = eng.Assets().GetMainFont();
    const char* lines[] = {
        "WASD — ходьба   Shift — бег (шумно)   Ctrl — присесть (тихо, лазы)",
        "E — взять / открыть / спрятаться (тайник: удерживать)   F — фонарик   Q — батарейки",
        "Tab — инвентарь   J — журнал целей   1-8 — предмет в руку   G — выбросить",
        "ЛКМ — выстрел   R — перезарядка   H — скрыть эту подсказку",
    };
    float x = 24.0f;
    float y = static_cast<float>(screenH) - 150.0f;
    DrawRectangleRounded({x - 10.0f, y - 8.0f, 820.0f, 100.0f}, 0.12f, 4, ColorAlpha(BLACK, 0.7f));
    for (const char* l : lines) {
        DrawTextEx(font, l, {x, y}, 16.0f, 1.0f, ColorAlpha(Color{225, 215, 195, 255}, 0.95f));
        y += 22.0f;
    }
}

void HouseWorld3D::RenderJournal(engine::core::Engine& eng) {
    int screenW = eng.Renderer().GetUIWidth();
    int screenH = eng.Renderer().GetUIHeight();
    Font font = eng.Assets().GetMainFont();
    DrawRectangle(0, 0, screenW, screenH, ColorAlpha(BLACK, 0.6f));
    float pw = 1080.0f;
    float ph = 640.0f;
    float px = (screenW - pw) * 0.5f;
    float py = (screenH - ph) * 0.5f;
    DrawRectangleRounded({px, py, pw, ph}, 0.03f, 6, Color{26, 20, 18, 248});
    DrawRectangleRoundedLinesEx({px, py, pw, ph}, 0.03f, 6, 2.0f, Color{196, 122, 59, 255});
    DrawTextEx(font, "ЖУРНАЛ: КАК ВЫБРАТЬСЯ  ([J] закрыть)", {px + 20.0f, py + 12.0f}, 24.0f, 1.0f, Color{244, 184, 96, 255});

    auto has = [&](const char* id) { return std::find(m_inventory.begin(), m_inventory.end(), id) != m_inventory.end(); };
    auto line = [&](float x, float& y, const std::string& text, bool done, const char* haveId) {
        Color c = done ? Color{130, 220, 130, 255} : ((haveId && has(haveId)) ? Color{240, 210, 110, 255} : Color{215, 205, 190, 255});
        std::string t = std::string(done ? "[x] " : "[ ] ") + text + ((!done && haveId && has(haveId)) ? "  (есть в инвентаре)" : "");
        DrawTextEx(font, t.c_str(), {x, y}, 16.0f, 1.0f, c);
        y += 21.0f;
    };
    const FrontDoorLocks& door = m_escape.GetDoorLocks();
    const CarEscapeState& car = m_escape.GetCarState();
    const SewerEscapeState& sewer = m_escape.GetSewerState();
    const RadioEscapeState& radio = m_escape.GetRadioState();

    float x1 = px + 20.0f;
    float y = py + 54.0f;
    DrawTextEx(font, "1. ПАРАДНАЯ ДВЕРЬ (прихожая, 1 этаж) — подходите и жмите [E]", {x1, y}, 18.0f, 1.0f, GOLD);
    y += 26.0f;
    line(x1, y, "Монтировка: сорвать 3 доски (" + std::to_string(door.planksRemoved) + "/3), держите её в руке", door.planksRemoved >= 3, "item_crowbar");
    line(x1, y, "Ключ от замка: открыть навесной замок", door.padlockOpen, "item_padlock_key");
    line(x1, y, "Кусачки: перекусить провод сигнализации", door.alarmCut, "item_wire_cutters");
    line(x1, y, "Записка с кодом: код вводится сам, когда она у вас", door.keypadUnlocked, "item_code_note");
    line(x1, y, "Главный ключ: последний ригель", door.masterKeyUsed, "item_master_key");
    y += 8.0f;
    DrawTextEx(font, "2. МАШИНА «КОПЕЙКА» (гараж, слева от дома) — [E] у машины", {x1, y}, 18.0f, 1.0f, GOLD);
    y += 26.0f;
    line(x1, y, "Аккумулятор", car.batteryInstalled, "item_car_battery");
    line(x1, y, "Свеча зажигания", car.sparkplugInstalled, "item_car_sparkplug");
    line(x1, y, "Канистра бензина", car.gasFilled, "item_gas_canister");
    line(x1, y, "Ключ от гаража (или монтировка): сбить засов ворот", car.garageDoorsOpen, "item_garage_key");
    line(x1, y, "Ключи от машины", car.keyInserted, "item_car_key");
    y += 8.0f;
    DrawTextEx(font, "3. КОЛЛЕКТОР (подвал, котельная справа) — [E] у решётки", {x1, y}, 18.0f, 1.0f, GOLD);
    y += 26.0f;
    line(x1, y, "Гаечный ключ: перекрыть вентиль пара", sewer.steamValveClosed, "item_wrench");
    line(x1, y, "Ключ от коллектора: открыть решётку", sewer.sewerGrateUnlocked, "item_sewer_key");
    y += 8.0f;
    DrawTextEx(font, "4. РАДИО (чердак, лестница из кладовой на 2 этаже) — [E] у передатчика", {x1, y}, 18.0f, 1.0f, GOLD);
    y += 26.0f;
    line(x1, y, "Предохранитель 25A: питание передатчика", radio.fuseInstalled, "item_radio_fuse");
    line(x1, y, "Радиолампа ГУ-50: усилитель (частота 94.5 настраивается сама)", radio.tubeInstalled, "item_vacuum_tube");

    float x2 = px + 560.0f;
    float y2 = py + 54.0f;
    DrawTextEx(font, "КАК ИГРАТЬ", {x2, y2}, 18.0f, 1.0f, GOLD);
    y2 += 26.0f;
    const char* tips[] = {
        "Предметы лежат в мебели случайно каждый раз.",
        "Тайники: удерживайте [E]. Это шумит.",
        "На открытых столах предмет светится — [E] взять.",
        "Заперты: чердачный ящик и багажник. Нужна монтировка.",
        "Ружьё: 3 детали принести на верстак в подвале",
        "  (ствол, ложе, спуск), патроны искать в доме.",
        "  Выстрел оглушает её на 30 секунд, но шумит на весь дом.",
        "Маньяк реагирует на шум: бег громкий, шаг тихий,",
        "  присед [Ctrl] бесшумен. Прячьтесь [E], пока она не видит.",
        "Лазы 1,2 м: проползти можно только на [Ctrl], она не пролезет.",
        "Фонарик [F], батарейки [Q]. 5 дней, потом конец.",
        "Отдельного рубильника нет: свет в доме случайный,",
        "  радио требует только предохранитель и лампу.",
        "Если вас поймали, наступает следующий день.",
    };
    for (const char* t : tips) {
        DrawTextEx(font, t, {x2, y2}, 16.0f, 1.0f, Color{215, 205, 190, 255});
        y2 += 21.0f;
    }
    y2 += 10.0f;
    std::string wp = m_weapon.IsAssembled() ? "Ружьё собрано" : "Ружьё: деталей на верстаке " + std::to_string(m_weapon.GetPartsAttachedCount()) + "/3";
    DrawTextEx(font, wp.c_str(), {x2, y2}, 17.0f, 1.0f, GOLD);
}

void HouseWorld3D::RenderInventory(engine::core::Engine& eng) {
    int screenW = eng.Renderer().GetUIWidth();
    int screenH = eng.Renderer().GetUIHeight();
    Font font = eng.Assets().GetMainFont();
    DrawRectangle(0, 0, screenW, screenH, ColorAlpha(BLACK, 0.55f));
    float pw = 760.0f;
    float ph = 430.0f;
    float px = (screenW - pw) * 0.5f;
    float py = (screenH - ph) * 0.5f;
    DrawRectangleRounded({px, py, pw, ph}, 0.04f, 6, Color{26, 20, 18, 245});
    DrawRectangleRoundedLinesEx({px, py, pw, ph}, 0.04f, 6, 2.0f, Color{196, 122, 59, 255});
    DrawTextEx(font, "ИНВЕНТАРЬ", {px + 24.0f, py + 16.0f}, 26.0f, 1.0f, Color{244, 184, 96, 255});
    DrawTextEx(font, "[Tab] закрыть   [E] использовать   [G] выбросить   стрелки — выбор", {px + 24.0f, py + ph - 34.0f}, 16.0f, 1.0f, GRAY);

    float cell = 84.0f;
    float gx = px + 24.0f;
    float gy = py + 64.0f;
    for (int i = 0; i < 8; ++i) {
        float x = gx + static_cast<float>(i % 4) * (cell + 10.0f);
        float y = gy + static_cast<float>(i / 4) * (cell + 10.0f);
        bool sel = i == m_activeSlot;
        DrawRectangleRounded({x, y, cell, cell}, 0.15f, 4, sel ? ColorAlpha(GOLD, 0.3f) : ColorAlpha(WHITE, 0.07f));
        DrawRectangleRoundedLinesEx({x, y, cell, cell}, 0.15f, 4, sel ? 2.5f : 1.0f, sel ? GOLD : DARKGRAY);
        DrawTextEx(font, std::to_string(i + 1).c_str(), {x + 6.0f, y + 4.0f}, 14.0f, 1.0f, sel ? GOLD : LIGHTGRAY);
        if (i >= static_cast<int>(m_inventory.size())) continue;
        const ItemDef* def = m_randomizer.GetItemDef(m_inventory[i]);
        if (!def) continue;
        Texture2D tex{};
        if (!def->iconPath.empty() && FileExists((eng.Assets().GetBaseDir() + "/" + def->iconPath).c_str())) tex = eng.Assets().GetTexture(def->iconPath);
        Rectangle r{x + 10.0f, y + 14.0f, cell - 20.0f, cell - 28.0f};
        if (tex.id != 0) {
            DrawTexturePro(tex, {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)}, r, {0, 0}, 0.0f, WHITE);
        } else {
            DrawRectangleRounded(r, 0.25f, 4, ColorAlpha(Color{120, 100, 80, 255}, 0.8f));
            std::string ini = def->name.substr(0, 2);
            DrawTextEx(font, ini.c_str(), {r.x + 12.0f, r.y + 10.0f}, 26.0f, 1.0f, WHITE);
        }
    }

    float dx = gx + 4.0f * (cell + 10.0f) + 14.0f;

    if (m_activeSlot < static_cast<int>(m_inventory.size())) {
        const ItemDef* def = m_randomizer.GetItemDef(m_inventory[m_activeSlot]);
        if (def) {
            DrawTextEx(font, def->name.c_str(), {dx, gy}, 22.0f, 1.0f, GOLD);
            std::string desc = def->description;
            float y = gy + 34.0f;
            DrawTextEx(font, desc.c_str(), {dx, y}, 16.0f, 1.0f, LIGHTGRAY);
        }
    } else {
        DrawTextEx(font, "Пусто", {dx, gy}, 20.0f, 1.0f, GRAY);
    }


    float ey = gy + 2.0f * (cell + 10.0f) + 12.0f;
    std::string flash = m_hasFlashlight ? "Фонарик: " + std::to_string(static_cast<int>(m_battery * 100.0f)) + "% заряда" : "Фонарик: не найден";
    DrawTextEx(font, flash.c_str(), {gx, ey}, 18.0f, 1.0f, m_hasFlashlight ? Color{150, 230, 150, 255} : Color{230, 120, 110, 255});
    std::string gun = m_weapon.IsAssembled() ? "ТОЗ-34: " + std::to_string(m_weapon.GetLoadedAmmo()) + " / " + std::to_string(m_weapon.GetReserveAmmo())
                                             : "Ружьё: деталей " + std::to_string(m_weapon.GetPartsAttachedCount()) + "/3 на верстаке";
    DrawTextEx(font, gun.c_str(), {gx, ey + 24.0f}, 18.0f, 1.0f, LIGHTGRAY);
    if (std::find(m_inventory.begin(), m_inventory.end(), "item_code_note") != m_inventory.end()) {
        std::string code = "Код от двери: " + m_randomizer.GetDoorKeypadCode();
        DrawTextEx(font, code.c_str(), {gx + 330.0f, ey}, 18.0f, 1.0f, GOLD);
    }
}

void HouseWorld3D::FixedUpdate(engine::core::Engine&, float fixedDt) {
    if (!m_ready) return;
    m_physics.Step(fixedDt);
}

bool HouseWorld3D::BatchVisible(const std::string& tag) const {
    if (tag.empty()) return true;
    const FrontDoorLocks& door = m_escape.GetDoorLocks();
    if (tag.rfind("plank", 0) == 0) return (tag[5] - '0') >= door.planksRemoved;
    if (tag == "padlock") return !door.padlockOpen;
    if (tag == "alarm") return !door.alarmCut;
    if (tag == "radio_led") return m_escape.GetRadioState().fuseInstalled;
    if (tag.rfind("lamp", 0) == 0) {
        size_t idx = static_cast<size_t>(std::atoi(tag.c_str() + 4));
        return idx < m_lampOn.size() ? m_lampOn[idx] != 0 : true;
    }
    if (tag == "gate") return !(m_inCutscene && m_cutscene.GetName() == "car_escape" && m_cutscene.GetCurrentTime() > 2.2f);
    if (tag == "front_door") return !(m_inCutscene && m_cutscene.GetName() == "front_door_escape" && m_cutscene.GetCurrentTime() > 1.6f);
    return true;
}

SceneEnvironment HouseWorld3D::BuildEnvironment() const {
    SceneEnvironment env;
    env.ambientSky = Vector3{0.045f, 0.05f, 0.07f};
    env.ambientGround = Vector3{0.025f, 0.025f, 0.03f};
    env.fogColor = Vector3{0.02f, 0.022f, 0.03f};
    env.fogStart = 7.0f;
    env.fogEnd = 32.0f;
    env.powerLevel = 0.0f;
    if (m_escape.GetCurrentDay() >= 3) {
        env.ambientSky = Vector3Scale(env.ambientSky, 0.8f);
        env.fogEnd = 26.0f;
    }
    return env;
}

void HouseWorld3D::BeginCamera(const Camera3D& camera) const {
    float aspect = UI_W / UI_H;
    if (m_eng != nullptr && m_eng->Settings().Get().stretchToUltrawide) {
        aspect = static_cast<float>(std::max(1, GetScreenWidth())) / static_cast<float>(std::max(1, GetScreenHeight()));
    }
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    double top = RL_CULL_DISTANCE_NEAR * std::tan(camera.fovy * 0.5 * DEG2RAD);
    double right = top * aspect;
    rlFrustum(-right, right, -top, top, RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    rlMultMatrixf(MatrixToFloat(view));
    rlEnableDepthTest();
}

void HouseWorld3D::DrawEntity(const Shader* overrideShader) {
    if (overrideShader != nullptr || m_phantomTex.id == 0) return;
    const Camera3D& cam = m_viewCam;
    SceneEnvironment env = BuildEnvironment();
    Vector3 feet = m_granny.GetPosition();
    EntityState state = m_granny.GetState();
    bool stunned = state == EntityState::Stunned;
    bool chasing = state == EntityState::Chase;

    float d = Vector3Distance(cam.position, feet);
    float fog = std::clamp(1.0f - (d - env.fogStart) / std::max(1.0f, env.fogEnd - env.fogStart), 0.08f, 1.0f);
    float jitter = std::sin(m_time * 55.0f) > 0.3f ? 0.03f : -0.03f;
    float scale = 1.0f;
    float rise = 1.0f;
    float alpha = 1.0f;
    if (stunned) {
        float left = m_granny.GetStunTimer();
        rise = 0.25f;
        alpha = std::clamp(left / 4.0f, 0.0f, 0.55f);
    }
    Vector2 size{1.25f * scale, 2.5f * scale * (0.3f + 0.7f * rise)};
    Vector3 center{feet.x + jitter, feet.y + size.y * 0.5f - (1.0f - rise) * 0.2f, feet.z};
    unsigned char a = static_cast<unsigned char>(245.0f * alpha * (0.35f + 0.65f * fog));
    unsigned char shade = static_cast<unsigned char>(70.0f + 180.0f * fog);
    Color tint = chasing ? Color{255, 120, 120, a} : Color{shade, static_cast<unsigned char>(shade * 0.9f), static_cast<unsigned char>(shade * 0.9f), a};
    Rectangle src{0.0f, 0.0f, static_cast<float>(m_phantomTex.width), static_cast<float>(m_phantomTex.height)};
    rlDrawRenderBatchActive();
    DrawBillboardRec(cam, m_phantomTex, src, center, size, tint);
    rlDrawRenderBatchActive();
    if (stunned) return;

    Vector3 head{center.x, center.y + size.y * 0.28f, center.z};
    Vector3 toCam = Vector3Normalize(Vector3Subtract(cam.position, head));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(cam.up, toCam));
    float pulse = 0.5f + 0.5f * std::sin(m_time * 24.0f);
    Color eye{255, static_cast<unsigned char>(30 + 40 * pulse), static_cast<unsigned char>(30 + 40 * pulse), a};
    float spread = 0.055f * scale;
    DrawSphereEx(Vector3Add(head, Vector3Add(Vector3Scale(right, -spread), Vector3Scale(toCam, 0.03f))), 0.02f * scale, 4, 4, eye);
    DrawSphereEx(Vector3Add(head, Vector3Add(Vector3Scale(right, spread), Vector3Scale(toCam, 0.03f))), 0.02f * scale, 4, 4, eye);
    rlDrawRenderBatchActive();
}

void HouseWorld3D::DrawScene(const Shader* overrideShader) {
    Matrix identity = MatrixIdentity();
    for (const HouseBatch& b : m_level.Batches()) {
        if (!BatchVisible(b.tag)) continue;
        Material m = b.material;
        if (overrideShader != nullptr) {
            m.shader = *overrideShader;
        } else {
            m_lighting.SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, b.emissive);
        }
        DrawMesh(b.mesh, m, identity);
    }

    if (overrideShader == nullptr) m_lighting.SetObjectParams(0.0f, Vector3{0.03f, 0.035f, 0.04f}, 0.0f);
    Material bark = m_level.BarkMaterial();
    Material pine = m_level.PineMaterial();
    if (overrideShader != nullptr) {
        bark.shader = *overrideShader;
        pine.shader = *overrideShader;
    }
    const float layers[3][2] = {{2.2f, 1.0f}, {3.6f, 0.78f}, {5.0f, 0.55f}};
    for (const HouseTree& t : m_level.Trees()) {
        Matrix world = MatrixMultiply(MatrixScale(t.scale, t.scale, t.scale), MatrixTranslate(t.position.x, t.position.y, t.position.z));
        DrawMesh(m_level.TrunkMesh(), bark, world);
        for (const auto& layer : layers) {
            Matrix local = MatrixMultiply(MatrixScale(layer[1], layer[1], layer[1]), MatrixTranslate(0.0f, layer[0], 0.0f));
            DrawMesh(m_level.CrownMesh(), pine, MatrixMultiply(local, world));
        }
    }

    DrawEntity(overrideShader);
    DrawDroppedItems(overrideShader);
    if (overrideShader == nullptr) m_lighting.SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, 0.0f);
}

void HouseWorld3D::DrawDroppedItems(const Shader* overrideShader) {
    if (!m_hasFlashlight) {
        DrawCube({-5.65f, 3.84f, 4.9f}, 0.12f, 0.08f, 0.28f, Color{220, 180, 70, 255});
        DrawCubeWires({-5.65f, 3.84f, 4.9f}, 0.12f, 0.08f, 0.28f, Color{255, 230, 140, 255});
    }
    for (const auto& it : m_droppedItems) {
        Vector3 p = it.pos;
        Color c = Color{200, 160, 60, 255};
        if (it.itemId == "item_flashlight") c = Color{220, 190, 70, 255};
        else if (it.itemId == "item_gas_canister") c = Color{210, 40, 40, 255};
        else if (it.itemId == "item_car_battery") c = Color{40, 40, 45, 255};
        else if (it.itemId == "item_code_note") c = Color{240, 235, 210, 255};
        else if (it.itemId == "item_crowbar") c = Color{180, 50, 45, 255};
        else if (it.itemId == "item_shotgun_ammo") c = Color{220, 50, 40, 255};
        DrawCube(p, 0.22f, 0.14f, 0.22f, c);
        DrawCubeWires(p, 0.22f, 0.14f, 0.22f, Color{255, 230, 180, 255});
    }
}

void HouseWorld3D::DrawInspectOverlay(engine::core::Engine& eng) {
    Font font = eng.Assets().GetMainFont();
    DrawRectangle(0, 0, static_cast<int>(UI_W), static_cast<int>(UI_H), Color{6, 5, 9, 235});

    Camera3D cam{};
    cam.position = Vector3{0.0f, 0.05f, 1.6f * m_inspectZoom};
    cam.target = Vector3{0.0f, 0.0f, 0.0f};
    cam.up = Vector3{0.0f, 1.0f, 0.0f};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(cam);
    rlPushMatrix();
    rlRotatef(m_inspectPitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    rlRotatef(m_inspectYaw * RAD2DEG, 0.0f, 1.0f, 0.0f);

    std::string id = m_inspectItem;
    if (id == "item_flashlight") {
        DrawCylinderEx({0.0f, -0.35f, 0.0f}, {0.0f, 0.25f, 0.0f}, 0.10f, 0.10f, 16, Color{45, 45, 50, 255});
        DrawCylinderEx({0.0f, 0.25f, 0.0f}, {0.0f, 0.45f, 0.0f}, 0.18f, 0.18f, 16, Color{220, 180, 70, 255});
        DrawSphere({0.0f, 0.45f, 0.0f}, 0.16f, Color{160, 230, 255, 220});
    } else if (id.find("key") != std::string::npos) {
        DrawCylinderEx({0.0f, 0.15f, 0.0f}, {0.0f, 0.35f, 0.0f}, 0.18f, 0.18f, 16, Color{212, 175, 55, 255});
        DrawCube({0.0f, -0.15f, 0.0f}, 0.06f, 0.42f, 0.04f, Color{200, 160, 40, 255});
        DrawCube({0.05f, -0.28f, 0.0f}, 0.06f, 0.05f, 0.04f, Color{200, 160, 40, 255});
        DrawCube({0.05f, -0.18f, 0.0f}, 0.05f, 0.04f, 0.04f, Color{200, 160, 40, 255});
    } else if (id == "item_crowbar") {
        DrawCube({0.0f, 0.0f, 0.0f}, 0.05f, 0.85f, 0.05f, Color{180, 40, 40, 255});
        DrawCube({0.10f, 0.38f, 0.0f}, 0.18f, 0.05f, 0.05f, Color{180, 40, 40, 255});
    } else if (id == "item_wire_cutters") {
        DrawCube({-0.08f, -0.15f, 0.0f}, 0.05f, 0.5f, 0.04f, Color{40, 70, 160, 255});
        DrawCube({0.08f, -0.15f, 0.0f}, 0.05f, 0.5f, 0.04f, Color{40, 70, 160, 255});
        DrawCube({0.0f, 0.15f, 0.0f}, 0.16f, 0.18f, 0.05f, Color{190, 190, 195, 255});
    } else if (id == "item_gas_canister") {
        DrawCube({0.0f, -0.05f, 0.0f}, 0.45f, 0.6f, 0.25f, Color{195, 35, 30, 255});
        DrawCube({0.0f, 0.3f, 0.0f}, 0.26f, 0.06f, 0.05f, Color{150, 25, 20, 255});
        DrawCylinderEx({0.12f, 0.26f, 0.0f}, {0.12f, 0.38f, 0.0f}, 0.05f, 0.05f, 12, Color{220, 200, 60, 255});
    } else if (id == "item_car_battery") {
        DrawCube({0.0f, 0.0f, 0.0f}, 0.52f, 0.38f, 0.28f, Color{35, 35, 40, 255});
        DrawCylinderEx({-0.16f, 0.19f, 0.0f}, {-0.16f, 0.25f, 0.0f}, 0.035f, 0.035f, 10, Color{220, 40, 40, 255});
        DrawCylinderEx({0.16f, 0.19f, 0.0f}, {0.16f, 0.25f, 0.0f}, 0.035f, 0.035f, 10, Color{40, 100, 220, 255});
    } else if (id == "item_code_note") {
        DrawCube({0.0f, 0.0f, 0.0f}, 0.45f, 0.6f, 0.01f, Color{240, 230, 200, 255});
        DrawCube({0.0f, 0.1f, 0.008f}, 0.35f, 0.02f, 0.01f, Color{40, 35, 30, 255});
        DrawCube({0.0f, 0.0f, 0.008f}, 0.35f, 0.02f, 0.01f, Color{40, 35, 30, 255});
        DrawCube({0.0f, -0.1f, 0.008f}, 0.28f, 0.02f, 0.01f, Color{40, 35, 30, 255});
    } else if (id == "item_shotgun_ammo") {
        DrawCylinderEx({-0.08f, -0.2f, 0.0f}, {-0.08f, 0.2f, 0.0f}, 0.07f, 0.07f, 14, Color{190, 40, 30, 255});
        DrawCylinderEx({0.08f, -0.2f, 0.0f}, {0.08f, 0.2f, 0.0f}, 0.07f, 0.07f, 14, Color{190, 40, 30, 255});
        DrawCylinderEx({-0.08f, -0.25f, 0.0f}, {-0.08f, -0.2f, 0.0f}, 0.075f, 0.075f, 14, Color{212, 175, 55, 255});
        DrawCylinderEx({0.08f, -0.25f, 0.0f}, {0.08f, -0.2f, 0.0f}, 0.075f, 0.075f, 14, Color{212, 175, 55, 255});
    } else {
        DrawCube({0.0f, 0.0f, 0.0f}, 0.35f, 0.35f, 0.35f, Color{180, 140, 90, 255});
        DrawCubeWires({0.0f, 0.0f, 0.0f}, 0.35f, 0.35f, 0.35f, Color{244, 184, 96, 255});
    }
    rlPopMatrix();
    EndMode3D();

    const ItemDef* def = m_randomizer.GetItemDef(m_inspectItem);
    std::string title = def ? def->name : m_inspectItem;
    std::string desc = def ? def->description : "";
    if (m_inspectItem == "item_code_note") {
        desc = "Шифр от электронного замка парадной двери: " + m_randomizer.GetDoorKeypadCode();
    }

    float panelX = UI_W * 0.5f - 340.0f;
    DrawRectangle(static_cast<int>(panelX), 520, 680, 145, Color{16, 12, 20, 240});
    DrawRectangleLines(static_cast<int>(panelX), 520, 680, 145, Color{244, 184, 96, 230});
    DrawTextEx(font, title.c_str(), Vector2{panelX + 24.0f, 532.0f}, 24.0f, 1.0f, Color{244, 184, 96, 255});
    DrawTextEx(font, desc.c_str(), Vector2{panelX + 24.0f, 566.0f}, 17.0f, 1.0f, Color{225, 220, 210, 240});
    DrawTextEx(font, "[Мышь] Вращать  |  [Колесо] Приблизить  |  [E / SPACE / ESC] Взять в руки", Vector2{panelX + 24.0f, 634.0f}, 15.0f, 1.0f, Color{180, 160, 140, 220});
}

void HouseWorld3D::RenderOffscreen(engine::core::Engine&) {
    if (!m_ready || m_hd || !m_flash.enabled) return;
    Shader depth = m_lighting.DepthShader();
    m_lighting.BeginShadowPass(m_flash);
    DrawScene(&depth);
    m_lighting.EndShadowPass();
}

void HouseWorld3D::Render3D(engine::core::Engine& eng) {
    if (!m_ready) return;
    if (m_hd) {
        HouseHdInput in;
        in.camera = m_viewCam;
        in.time = m_time;
        in.dt = GetFrameTime();
        in.flashOn = m_flash.enabled;
        in.flashPos = m_flash.position;
        in.flashDir = m_flash.direction;
        in.grannyPos = m_granny.GetPosition();
        in.grannyStunned = m_granny.GetState() == EntityState::Stunned;
        in.grannyChasing = m_granny.GetState() == EntityState::Chase;
        in.grannyStunLeft = m_granny.GetStunTimer();
        in.day = m_escape.GetCurrentDay();
        in.fear = m_fear;
        in.heartbeat = m_heartPulse;
        in.flashPower = m_flashPower;
        in.lampOn = m_lampOn;
        for (const SearchSlot& s : m_randomizer.GetAllSlots()) {
            if (!s.open || s.searched || s.containedItem.empty()) continue;
            HouseHdItem item;
            item.pos = s.position;
            if (s.containedItem == "item_flashlight") item.icon = "@flashlight";
            else if (s.containedItem == "item_batteries") item.icon = "@batteries";
            else if (const ItemDef* d = m_randomizer.GetItemDef(s.containedItem)) item.icon = d->iconPath;
            in.items.push_back(item);
        }
        in.batchVisible = [this](const std::string& tag) { return BatchVisible(tag); };
        m_hdBridge.Render(eng, in);
        return;
    }
    SceneEnvironment env = BuildEnvironment();
    ClearBackground(Color{static_cast<unsigned char>(env.fogColor.x * 255.0f), static_cast<unsigned char>(env.fogColor.y * 255.0f),
                          static_cast<unsigned char>(env.fogColor.z * 255.0f), 255});

    std::vector<DynamicLight> lights;
    if (m_granny.GetState() != EntityState::Stunned) {
        Vector3 head = Vector3Add(m_granny.GetPosition(), Vector3{0.0f, 1.75f, 0.0f});
        float pulse = 0.75f + 0.25f * std::sin(m_time * 6.0f);
        lights.push_back(DynamicLight{head, Vector3{0.55f * pulse, 0.04f, 0.03f}, 2.6f});
    }

    BeginCamera(m_viewCam);
    m_lighting.ApplyFrame(m_viewCam.position, m_flash, lights, env, m_flash.enabled);
    m_lighting.BindShadowMap();
    DrawScene(nullptr);
    rlDrawRenderBatchActive();
    EndMode3D();
}

void HouseWorld3D::RenderHUD(engine::core::Engine& eng) {
    int screenW = eng.Renderer().GetUIWidth();
    int screenH = eng.Renderer().GetUIHeight();
    Font font = eng.Assets().GetMainFont();
    if (Ch3Graphics::Get().showFps) {
        std::string fpsText = std::to_string(GetFPS()) + " FPS";
        DrawTextEx(font, fpsText.c_str(), {16.0f, 12.0f}, 18.0f, 1.0f, ColorAlpha(Color{150, 255, 150, 255}, 0.9f));
    }

    if (m_isHiding) {
        float sx = screenW * 0.14f;
        float sy = screenH * 0.36f;
        float sw = screenW - 2.0f * sx;
        float sh = screenH * 0.22f;
        Color frame = ColorAlpha(Color{4, 3, 3, 255}, 0.97f);
        DrawRectangle(0, 0, screenW, static_cast<int>(sy), frame);
        DrawRectangle(0, static_cast<int>(sy + sh), screenW, screenH - static_cast<int>(sy + sh), frame);
        DrawRectangle(0, static_cast<int>(sy), static_cast<int>(sx), static_cast<int>(sh), frame);
        DrawRectangle(static_cast<int>(sx + sw), static_cast<int>(sy), screenW - static_cast<int>(sx + sw), static_cast<int>(sh), frame);
        for (float y = sy + 6.0f; y < sy + sh; y += 16.0f) DrawRectangle(static_cast<int>(sx), static_cast<int>(y), static_cast<int>(sw), 4, ColorAlpha(BLACK, 0.55f));
        DrawRectangleLinesEx({sx, sy, sw, sh}, 3.0f, ColorAlpha(Color{60, 40, 30, 255}, 0.9f));
        DrawTextEx(font, "Вы в укрытии. [E] — выйти", {sx, sy + sh + 14.0f}, 18.0f, 1.0f, ColorAlpha(LIGHTGRAY, 0.7f));
    }

    DrawCircle(screenW / 2, screenH / 2, 2.5f, ColorAlpha(WHITE, 0.75f));

    if (!m_focusPrompt.empty()) {
        Vector2 size = MeasureTextEx(font, m_focusPrompt.c_str(), 20.0f, 1.0f);
        Vector2 pos = {(screenW - size.x) * 0.5f, screenH * 0.5f + 30.0f};
        DrawRectangleRounded({pos.x - 12.0f, pos.y - 4.0f, size.x + 24.0f, size.y + 8.0f}, 0.25f, 4, ColorAlpha(BLACK, 0.75f));
        DrawTextEx(font, m_focusPrompt.c_str(), pos, 20.0f, 1.0f, RAYWHITE);
    }

    if (m_notificationTimer > 0.0f && !m_notificationText.empty()) {
        Vector2 size = MeasureTextEx(font, m_notificationText.c_str(), 22.0f, 1.0f);
        Vector2 pos = {(screenW - size.x) * 0.5f, 50.0f};
        DrawRectangleRounded({pos.x - 16.0f, pos.y - 6.0f, size.x + 32.0f, size.y + 12.0f}, 0.3f, 4, ColorAlpha(BLACK, 0.85f));
        DrawTextEx(font, m_notificationText.c_str(), pos, 22.0f, 1.0f, GOLD);
    }

    if (m_dayTitleTimer > 0.0f && m_dayTitleAlpha > 0.01f) {
        std::string title = "ДЕНЬ " + std::to_string(m_escape.GetCurrentDay());
        Vector2 size = MeasureTextEx(font, title.c_str(), 48.0f, 2.0f);
        Vector2 pos = {(screenW - size.x) * 0.5f, screenH * 0.35f};
        DrawTextEx(font, title.c_str(), {pos.x + 2.0f, pos.y + 2.0f}, 48.0f, 2.0f, ColorAlpha(BLACK, m_dayTitleAlpha));
        DrawTextEx(font, title.c_str(), pos, 48.0f, 2.0f, ColorAlpha(RAYWHITE, m_dayTitleAlpha));
    }

    constexpr int INV_SLOTS = 8;
    int invSlotSize = 52;
    int invGap = 8;
    int totalInvW = INV_SLOTS * invSlotSize + (INV_SLOTS - 1) * invGap;
    int startX = (screenW - totalInvW) / 2;
    int invY = screenH - invSlotSize - 22;

    auto drawItemIcon = [&](const std::string& itemId, Rectangle r) {
        const ItemDef* def = m_randomizer.GetItemDef(itemId);
        if (!def) return;
        Texture2D tex{};
        if (!def->iconPath.empty() && FileExists((eng.Assets().GetBaseDir() + "/" + def->iconPath).c_str())) tex = eng.Assets().GetTexture(def->iconPath);
        if (tex.id != 0) {
            DrawTexturePro(tex, {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)}, r, {0, 0}, 0.0f, WHITE);
            return;
        }
        unsigned int h = 2166136261u;
        for (char ch : itemId) h = (h ^ static_cast<unsigned char>(ch)) * 16777619u;
        Color c{static_cast<unsigned char>(90 + (h & 0x7F)), static_cast<unsigned char>(90 + ((h >> 8) & 0x7F)), static_cast<unsigned char>(90 + ((h >> 16) & 0x7F)), 255};
        DrawRectangleRounded(r, 0.25f, 4, ColorAlpha(c, 0.85f));
        std::string initials = def->name.substr(0, 2);
        Vector2 ts = MeasureTextEx(font, initials.c_str(), r.height * 0.42f, 1.0f);
        DrawTextEx(font, initials.c_str(), {r.x + (r.width - ts.x) * 0.5f, r.y + (r.height - ts.y) * 0.5f}, r.height * 0.42f, 1.0f, WHITE);
    };

    for (int i = 0; i < INV_SLOTS; ++i) {
        int x = startX + i * (invSlotSize + invGap);
        Rectangle slotRect{static_cast<float>(x), static_cast<float>(invY), static_cast<float>(invSlotSize), static_cast<float>(invSlotSize)};
        Color slotBg = (i == m_activeSlot) ? ColorAlpha(GOLD, 0.35f) : ColorAlpha(BLACK, 0.65f);
        DrawRectangleRounded(slotRect, 0.2f, 4, slotBg);
        DrawRectangleRoundedLinesEx(slotRect, 0.2f, 4, (i == m_activeSlot ? 2.5f : 1.0f), (i == m_activeSlot ? GOLD : DARKGRAY));
        std::string num = std::to_string(i + 1);
        DrawTextEx(font, num.c_str(), {static_cast<float>(x + 5), static_cast<float>(invY + 3)}, 13.0f, 1.0f, (i == m_activeSlot ? GOLD : LIGHTGRAY));
        if (i < static_cast<int>(m_inventory.size())) {
            drawItemIcon(m_inventory[i], {static_cast<float>(x + 7), static_cast<float>(invY + 12), static_cast<float>(invSlotSize - 14), static_cast<float>(invSlotSize - 18)});
        }
    }

    if (m_activeSlot < static_cast<int>(m_inventory.size())) {
        const ItemDef* def = m_randomizer.GetItemDef(m_inventory[m_activeSlot]);
        if (def) {
            Vector2 size = MeasureTextEx(font, def->name.c_str(), 16.0f, 1.0f);
            DrawTextEx(font, def->name.c_str(), {(screenW - size.x) * 0.5f, static_cast<float>(invY - 22)}, 16.0f, 1.0f, LIGHTGRAY);
        }
    }

    if (m_alertTimer > 0.0f && !m_alertText.empty()) {
        Vector2 size = MeasureTextEx(font, m_alertText.c_str(), 26.0f, 1.0f);
        Vector2 pos = {(screenW - size.x) * 0.5f, 110.0f};
        float pulse = 0.75f + 0.25f * std::sin(m_time * 12.0f);
        DrawRectangleRounded({pos.x - 18.0f, pos.y - 8.0f, size.x + 36.0f, size.y + 16.0f}, 0.3f, 4, ColorAlpha(m_alertRed ? Color{90, 0, 0, 255} : Color{50, 40, 0, 255}, 0.85f));
        DrawTextEx(font, m_alertText.c_str(), pos, 26.0f, 1.0f, ColorAlpha(m_alertRed ? Color{255, 90, 80, 255} : Color{255, 210, 110, 255}, pulse));
    }
    if (m_subTimer > 0.0f && !m_subText.empty()) {
        std::string line = "Маньяк: «" + m_subText + "»";
        float a = std::min(1.0f, m_subTimer * 2.0f);
        Vector2 size = MeasureTextEx(font, line.c_str(), 26.0f, 1.0f);
        Vector2 pos = {(screenW - size.x) * 0.5f, static_cast<float>(screenH) - 150.0f};
        DrawRectangleRounded({pos.x - 18.0f, pos.y - 8.0f, size.x + 36.0f, size.y + 16.0f}, 0.3f, 4, ColorAlpha(BLACK, 0.78f * a));
        DrawTextEx(font, line.c_str(), {pos.x + 1.0f, pos.y + 1.0f}, 26.0f, 1.0f, ColorAlpha(BLACK, a));
        DrawTextEx(font, line.c_str(), pos, 26.0f, 1.0f, ColorAlpha(m_subRed ? Color{255, 120, 110, 255} : Color{235, 225, 200, 255}, a));
    }
    if (m_granny.GetState() == EntityState::Investigate && !m_isHiding) {
        Vector3 feet = m_physics.GetCharacterPosition();
        Vector3 to = Vector3Subtract(m_granny.GetTargetPos(), feet);
        float yaw = m_fps.GetYaw();
        Vector3 fwd{std::sin(yaw), 0.0f, -std::cos(yaw)};
        Vector3 right{std::cos(yaw), 0.0f, std::sin(yaw)};
        float fx = Vector3DotProduct(to, fwd);
        float rx = Vector3DotProduct(to, right);
        float ang = std::atan2(rx, fx);
        Vector2 c{static_cast<float>(screenW) * 0.5f, static_cast<float>(screenH) * 0.5f};
        Vector2 p{c.x + std::sin(ang) * 230.0f, c.y - std::cos(ang) * 150.0f};
        Vector2 tip{p.x + std::sin(ang) * 16.0f, p.y - std::cos(ang) * 16.0f};
        Vector2 l{p.x - std::cos(ang) * 11.0f, p.y - std::sin(ang) * 11.0f};
        Vector2 r{p.x + std::cos(ang) * 11.0f, p.y + std::sin(ang) * 11.0f};
        DrawTriangle(tip, l, r, ColorAlpha(Color{255, 190, 80, 255}, 0.85f));
        DrawTriangle(tip, r, l, ColorAlpha(Color{255, 190, 80, 255}, 0.85f));
        DrawTextEx(font, "шум", {p.x - 14.0f, p.y + 14.0f}, 13.0f, 1.0f, ColorAlpha(Color{255, 190, 80, 255}, 0.85f));
    }
    if (m_hasFlashlight) {
        float bx = 28.0f;
        float by = static_cast<float>(screenH) - 52.0f;
        Color bc = m_battery > 0.35f ? Color{120, 220, 120, 255} : (m_battery > 0.15f ? Color{240, 200, 90, 255} : Color{230, 70, 60, 255});
        DrawRectangleRounded({bx - 8.0f, by - 8.0f, 118.0f, 30.0f}, 0.3f, 4, ColorAlpha(BLACK, 0.7f));
        DrawTextEx(font, "F", {bx, by - 2.0f}, 18.0f, 1.0f, m_flashlightOn ? GOLD : GRAY);
        DrawRectangleLinesEx({bx + 22.0f, by + 1.0f, 70.0f, 14.0f}, 1.0f, LIGHTGRAY);
        DrawRectangle(static_cast<int>(bx + 24.0f), static_cast<int>(by + 3.0f), static_cast<int>(66.0f * m_battery), 10, bc);
        DrawRectangle(static_cast<int>(bx + 92.0f), static_cast<int>(by + 5.0f), 3, 6, LIGHTGRAY);
    }
    if (m_searchProgress > 0.01f) {
        Vector2 c{static_cast<float>(screenW) * 0.5f, static_cast<float>(screenH) * 0.5f};
        DrawRing(c, 20.0f, 25.0f, -90.0f, -90.0f + 360.0f * std::min(1.0f, m_searchProgress), 36, GOLD);
    }
    if (m_weapon.IsAssembled()) {
        std::string ammoStr = "ТОЗ-34: " + std::to_string(m_weapon.GetLoadedAmmo()) + " / " + std::to_string(m_weapon.GetReserveAmmo());
        Vector2 asize = MeasureTextEx(font, ammoStr.c_str(), 18.0f, 1.0f);
        DrawRectangleRounded({static_cast<float>(screenW - asize.x - 35), static_cast<float>(screenH - 55), asize.x + 20.0f, 32.0f}, 0.3f, 4, ColorAlpha(BLACK, 0.8f));
        DrawTextEx(font, ammoStr.c_str(), {static_cast<float>(screenW - asize.x - 25), static_cast<float>(screenH - 48)}, 18.0f, 1.0f, (m_weapon.GetLoadedAmmo() > 0 ? GREEN : RED));
    }

    const char* rName = "Рендерер: OpenGL 3.3 (Classic)";
    if (m_hd) {
        if (m_appliedRenderer == 2) {
            rName = (m_appliedRt > 0) ? "Рендерер: DirectX 12 (DXR On)" : "Рендерер: DirectX 12";
        } else {
            rName = "Рендерер: DirectX 11 (HD)";
        }
    }
    Vector2 rSize = MeasureTextEx(font, rName, 15.0f, 1.0f);
    DrawRectangleRounded({static_cast<float>(screenW) - rSize.x - 28.0f, 14.0f, rSize.x + 18.0f, 26.0f}, 0.3f, 4, ColorAlpha(BLACK, 0.75f));
    DrawTextEx(font, rName, {static_cast<float>(screenW) - rSize.x - 19.0f, 19.0f}, 15.0f, 1.0f, Color{180, 205, 235, 240});
}

void HouseWorld3D::RenderUI(engine::core::Engine& eng) {
    if (!m_ready) return;
    if (m_inCutscene) {
        m_cutscene.RenderUI(eng);
        return;
    }
    if (m_inspectMode) {
        DrawInspectOverlay(eng);
        return;
    }
    RenderHUD(eng);
    if (m_invOpen) RenderInventory(eng);
    if (m_journalOpen) RenderJournal(eng);
    if (m_helpOpen || m_helpTimer > 0.0f) RenderHelp(eng);
    if (m_settingsUI.IsOpen()) m_settingsUI.Render(eng);
}

void HouseWorld3D::Shutdown(engine::core::Engine& eng) {
    EnableCursor();
    if (m_savedVsync >= 0) {
        auto& s = eng.Settings().Get();
        s.vsync = m_savedVsync != 0;
        s.targetFpsIndex = m_savedFpsIndex;
        eng.Settings().ApplyWindowAndFps(false);
        m_savedVsync = -1;
    }
    m_hdBridge.Shutdown(eng);
    m_hd = false;
    if (!m_ready) return;
    m_ready = false;
    UnloadEntityMeshes();
    m_level.Unload();
    m_physics.Shutdown();
    m_lighting.Shutdown();
}

}
