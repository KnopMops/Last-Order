#include "game/chapter2/MotelWorld3D.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace game::chapter2 {

using engine::physics3d::CharacterConfig;
using engine::physics3d::RayHit;
using engine::render3d::DynamicLight;
using engine::render3d::SceneEnvironment;

namespace {

constexpr float UI_W = 1280.0f;
constexpr float UI_H = 720.0f;
constexpr float USE_DISTANCE = 2.3f;
constexpr float WALK_SPEED = 2.4f;
constexpr float RUN_SPEED = 4.4f;
constexpr float CROUCH_SPEED = 1.25f;
constexpr int RAIN_DROPS = 420;
constexpr float RAIN_RADIUS = 13.0f;

float Rand01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

float RaySphere(Vector3 origin, Vector3 dir, Vector3 center, float radius) {
    Vector3 oc = Vector3Subtract(origin, center);
    float b = Vector3DotProduct(oc, dir);
    float c = Vector3DotProduct(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0f) return -1.0f;
    float t = -b - std::sqrt(h);
    return t >= 0.0f ? t : (c < 0.0f ? 0.0f : -1.0f);
}

bool PointInBox(Vector3 p, const BoundingBox& b) {
    return p.x >= b.min.x && p.x <= b.max.x && p.y >= b.min.y && p.y <= b.max.y && p.z >= b.min.z && p.z <= b.max.z;
}

Vector3 RotateYVec(Vector3 v, float angle) {
    float c = std::cos(angle);
    float s = std::sin(angle);
    return Vector3{v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

bool IsSoftSurface(const std::string& tex) {
    return tex.find("carpet") != std::string::npos || tex.find("fabric") != std::string::npos;
}

bool IsWoodSurface(const std::string& tex) {
    return tex.find("wood") != std::string::npos || tex.find("trim") != std::string::npos || IsSoftSurface(tex) ||
           tex.find("door") != std::string::npos;
}

void DrawLabel(Font font, const std::string& text, float x, float y, float size, Color color) {
    DrawTextEx(font, text.c_str(), Vector2{x, y}, size, 1.0f, color);
}

}

void MotelWorld3D::Init(engine::core::Engine& eng, int) {
    Shutdown(eng);
    m_eng = &eng;
    m_returnToMenu = false;
    m_ready = false;
    m_loadError.clear();
    m_time = 0.0f;

    const std::string& assetDir = eng.Assets().GetBaseDir();
    if (!m_lighting.Init(assetDir, 1024)) {
        m_loadError = "Не удалось загрузить шейдеры 3D (assets/shaders/lit3d.*)";
        EnableCursor();
        return;
    }
    m_physics.Init();
    if (!m_level.Load(eng, m_physics, m_lighting, assetDir + "/data/maps/ch2_motel86.map")) {
        m_loadError = "Не удалось загрузить карту assets/data/maps/ch2_motel86.map";
        EnableCursor();
        return;
    }
    if (!m_script.Load(assetDir + "/" + m_level.ScriptPath())) {
        m_loadError = "Не удалось загрузить сценарий " + m_level.ScriptPath();
        EnableCursor();
        return;
    }

    m_itemVisuals.Init(m_lighting.LitShader(), eng.Assets().GetTexture("sprites/3d/motel_key_tag.png"));
    m_itemVisuals.SetTexture("bark", eng.Assets().GetTexture("sprites/3d/motel_bark.png"));
    m_itemVisuals.SetTexture("pine", eng.Assets().GetTexture("sprites/3d/motel_pine.png"));
    CharacterConfig cfg;
    m_physics.CreateCharacter(Vector3Add(m_level.PlayerStart(), Vector3{0.0f, 0.05f, 0.0f}), cfg);
    m_camera.Init(m_level.PlayerStartYaw(), -0.05f, 72.0f);
    m_flashDir = m_camera.GetForward();
    m_camera.Update(0.0f, m_physics.GetCharacterPosition(), engine::camera3d::CameraMotion{});

    m_reflectionRT = LoadRenderTexture(640, 360);
    SetTextureFilter(m_reflectionRT.texture, TEXTURE_FILTER_BILINEAR);

    m_flashOn = true;
    m_flashFocus = false;
    m_battery = 100.0f;
    m_flickerTimer = 0.0f;
    m_power = false;
    m_sanity = 100.0f;
    m_stamina = 100.0f;
    m_whisperTimer = 14.0f;
    m_focusKind = FocusKind::None;
    m_carriedProp = -1;
    m_hiddenSpot = -1;
    m_breath = 100.0f;
    m_inspectMode = false;
    m_message.clear();
    m_messageTimer = 0.0f;
    m_objective.clear();
    m_endActText.clear();
    m_endActTimer = 0.0f;
    m_moveVelocity = Vector3{0.0f, 0.0f, 0.0f};

    m_rain.assign(RAIN_DROPS, RainDrop3D{});
    m_ready = true;
    UpdateRain(0.0f);
    for (auto& d : m_rain) RespawnDrop(d, true);

    if (!m_script.AmbientPath().empty()) eng.Audio().PlayAmbient(m_script.AmbientPath(), m_script.AmbientVolume());
    m_rainAmbientVolume = m_script.AmbientVolume();
    m_phantomTex = eng.Assets().GetTexture("sprites/3d/motel_phantom.png");
    eng.Renderer().SetSanityStress(0.0f);
    SetupAction();
    DisableCursor();
    m_script.RunEvent(m_script.StartEvent(), *this);
}

void MotelWorld3D::Shutdown(engine::core::Engine& eng) {
    ShutdownAction();
    if (m_reflectionRT.id != 0) {
        UnloadRenderTexture(m_reflectionRT);
        m_reflectionRT = RenderTexture2D{};
    }
    m_itemVisuals.Shutdown();
    m_level.Unload();
    m_physics.Shutdown();
    m_lighting.Shutdown();
    if (m_ready) eng.Renderer().SetSanityStress(0.0f);
    m_ready = false;
    EnableCursor();
}

void MotelWorld3D::ShowMessage(const std::string& text, float seconds) {
    m_message = text;
    m_messageTimer = seconds;
}

void MotelWorld3D::SetObjective(const std::string& text) {
    if (text == m_objective) return;
    m_objective = text;
    m_objectiveFlash = 3.0f;
    if (m_eng) m_eng->Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.45f);
}

void MotelWorld3D::PlaySound(const std::string& path, float volume, std::optional<Vector3> at) {
    if (!m_eng || path.empty()) return;
    if (!at.has_value()) {
        m_eng->Audio().PlaySfx(path, volume);
        return;
    }
    Vector3 listener = ViewCamera().position;
    Vector3 delta = Vector3Subtract(*at, listener);
    float dist = Vector3Length(delta);
    float att = std::clamp(1.0f - dist / 22.0f, 0.08f, 1.0f);
    float side = dist > 0.2f ? Vector3DotProduct(Vector3Scale(delta, 1.0f / dist), ViewRight()) : 0.0f;
    m_eng->Audio().PlaySfxSpatial(path, volume * att, 0.5f - 0.42f * side);
}

void MotelWorld3D::Flicker(float seconds) {
    m_flickerTimer = std::max(m_flickerTimer, seconds);
}

void MotelWorld3D::SetPower(bool on) {
    m_power = on;
}

void MotelWorld3D::UnlockDoor(const std::string& name) {
    int idx = m_level.FindDoorByName(name);
    if (idx < 0) return;
    DoorObject& door = m_level.Doors()[idx];
    if (!door.locked) return;
    door.locked = false;
    float a = std::abs(door.openAngle);
    float minA = door.bothWays ? -a : std::min(0.0f, door.openAngle);
    float maxA = door.bothWays ? a : std::max(0.0f, door.openAngle);
    m_physics.SetDoorLimits(door.body, minA, maxA);
}

void MotelWorld3D::OpenDoor(const std::string& name) {
    int idx = m_level.FindDoorByName(name);
    if (idx < 0) return;
    UnlockDoor(name);
    if (!m_level.Doors()[idx].open) UseDoor(idx, true);
}

void MotelWorld3D::CloseDoor(const std::string& name) {
    int idx = m_level.FindDoorByName(name);
    if (idx < 0) return;
    DoorObject& door = m_level.Doors()[idx];
    door.open = false;
    m_physics.SetDoorTarget(door.body, 0.0f);
    PlaySound(m_script.Sound("door_close", "audio/sfx/sfx_door_close_latch.wav"), 0.9f, door.restCenter);
}

void MotelWorld3D::Shake(float amount, float seconds) {
    if (m_eng) m_eng->Renderer().TriggerShake(amount, seconds);
}

void MotelWorld3D::AddSanity(float delta) {
    m_sanity = std::clamp(m_sanity + delta, 0.0f, 100.0f);
}

void MotelWorld3D::AddBattery(float amount) {
    m_battery = std::clamp(m_battery + amount, 0.0f, 100.0f);
}

void MotelWorld3D::SetAmbient(const std::string& path, float volume) {
    m_rainAmbientVolume = volume;
    if (m_eng && !path.empty()) m_eng->Audio().PlayAmbient(path, volume);
}

void MotelWorld3D::TriggerScreamer(const std::string& texture, float duration, float shake) {
    m_screamerActive = true;
    m_screamerTimer = duration;
    m_screamerDuration = duration;
    m_screamerRedFlash = 1.0f;
    if (m_eng && !texture.empty()) {
        m_screamerTex = m_eng->Assets().GetTexture(texture);
    }
    if (m_eng) m_eng->Renderer().TriggerShake(shake, duration);
}

void MotelWorld3D::EndAct(const std::string& text) {
    m_endActText = text;
    m_endActTimer = 9.0f;
    if (m_eng) m_eng->Audio().PlaySfx("audio/sfx/sfx_sting_dread.wav", 0.8f);
}

void MotelWorld3D::Update(engine::core::Engine& eng, float dt) {
    m_time += dt;
    if (m_screamerTimer > 0.0f) {
        m_screamerTimer -= dt;
        if (m_screamerTimer <= 0.0f) m_screamerActive = false;
    }
    if (m_screamerRedFlash > 0.0f) {
        m_screamerRedFlash = std::max(0.0f, m_screamerRedFlash - dt * 2.0f);
    }
    if (!m_ready) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) m_returnToMenu = true;
        return;
    }
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    if (m_driveTest) UpdateDriveTest(eng);
    if (m_tourIndex >= 0) {
        UpdateTour(eng);
        UpdateFlashlight(dt);
        m_physics.UpdateCharacter(dt, Vector3{0.0f, 0.0f, 0.0f}, false, INVALID_BODY);
        m_camera.Update(dt, m_physics.GetCharacterPosition(), engine::camera3d::CameraMotion{});
        UpdateRain(dt);
        return;
    }
#endif
    if (m_messageTimer > 0.0f) m_messageTimer -= dt;
    if (m_objectiveFlash > 0.0f) m_objectiveFlash -= dt;
    if (m_endActTimer > 0.0f) m_endActTimer -= dt;
    UpdateActionTimers(dt);
    {
        Vector3 anchor = PlayerAnchor();
        bool indoor = m_mode != PlayerMode::Driving && anchor.z <= 0.0f && anchor.z >= -13.8f && anchor.x >= -21.0f && anchor.x <= 8.5f;
        float targetRainVol = indoor ? 0.0f : 0.28f;
        m_rainAmbientVolume += (targetRainVol - m_rainAmbientVolume) * std::clamp(dt * 3.5f, 0.0f, 1.0f);
        eng.Audio().SetAmbientBaseVolume(m_rainAmbientVolume);
    }

    if (m_settingsUI.IsOpen()) {
        m_settingsUI.Update(eng);
        if (m_settingsUI.WantsQuitToMenu()) {
            m_returnToMenu = true;
            m_settingsUI.ClearQuitToMenu();
        }
        if (!m_settingsUI.IsOpen()) {
            DisableCursor();
        }
        return;
    }

    if (m_inspectMode) {
        Vector2 md = GetMouseDelta();
        m_inspectYaw += md.x * 0.012f;
        m_inspectPitch = std::clamp(m_inspectPitch + md.y * 0.012f, -1.5f, 1.5f);
        m_inspectZoom = std::clamp(m_inspectZoom - GetMouseWheelMove() * 0.1f, 0.6f, 1.6f);
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_E) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            m_inspectMode = false;
            eng.Audio().PlaySfx(m_script.Sound("inspect_close", "audio/sfx/sfx_notebook_flip.wav"), 0.55f);
            const ItemDefinition* def = m_script.FindItem(m_inspectItem);
            if (def != nullptr) m_script.RunEvent(def->event, *this);
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        m_settingsUI.Open();
        EnableCursor();
        return;
    }

    if (m_mode == PlayerMode::Dead) {
        UpdateDeath(dt);
        UpdateRain(dt);
        return;
    }
    if (m_mode == PlayerMode::Driving) {
        UpdateDriving(dt);
        return;
    }

    m_camera.ApplyMouseLook(GetMouseDelta(), 0.0022f);
    if (m_hiddenSpot >= 0) {
        const HideSpot& spot = m_level.HideSpots()[m_hiddenSpot];
        m_camera.ClampYawAround(spot.yaw, 0.55f, -0.35f, 0.3f);
    }

    UpdateFlashlight(dt);
    UpdateMovement(dt);
    UpdateFootsteps();
    UpdateFocus();
    HandleUse();
    UpdateTriggers();
    UpdateMirrorEvents();
    UpdateSoundEmitters(dt);
    UpdateRain(dt);
    UpdateSanity(dt);
    UpdatePistol(dt);
    UpdateTrail();
    UpdateStalker(dt);
}

void MotelWorld3D::UpdateFlashlight(float dt) {
    if (IsKeyPressed(KEY_F)) {
        m_flashOn = !m_flashOn;
        m_eng->Audio().PlaySfx(m_script.Sound("flashlight", "audio/sfx/sfx_flashlight_click.wav"), 0.55f);
    }
    m_flashFocus = m_flashOn && m_carriedProp < 0 && IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (m_flashOn) m_battery = std::max(0.0f, m_battery - dt * (m_flashFocus ? 0.75f : 0.22f));

    if (m_flickerTimer > 0.0f) {
        m_flickerTimer -= dt;
        m_flickerSwitch -= dt;
        if (m_flickerSwitch <= 0.0f) {
            m_flickerOn = !m_flickerOn || Rand01() < 0.25f;
            m_flickerSwitch = 0.03f + Rand01() * (m_flickerOn ? 0.18f : 0.09f);
        }
    } else {
        m_flickerOn = true;
    }

    Vector3 fwd = m_camera.GetForward();
    float follow = 1.0f - std::exp(-dt * 15.0f);
    m_flashDir = Vector3Normalize(Vector3Lerp(m_flashDir, fwd, follow));

    const Camera3D& cam = m_camera.GetCamera();
    Vector3 right = m_camera.GetRight();
    Vector3 up = m_camera.GetUp();
    m_flash.position = Vector3Add(cam.position, Vector3Add(Vector3Add(Vector3Scale(right, 0.17f), Vector3Scale(up, -0.13f)), Vector3Scale(fwd, 0.1f)));
    m_flash.direction = m_flashDir;
    float lowBattery = m_battery < 15.0f ? (0.35f + 0.65f * m_battery / 15.0f) : 1.0f;
    bool batteryStutter = m_battery < 8.0f && std::fmod(m_time * 7.3f, 1.0f) < 0.12f;
    m_flash.enabled = m_flashOn && m_battery > 0.0f && m_flickerOn && !batteryStutter && m_hiddenSpot < 0;
    if (m_flashFocus) {
        m_flash.innerCos = std::cos(10.0f * DEG2RAD);
        m_flash.outerCos = std::cos(16.0f * DEG2RAD);
        m_flash.range = 19.0f;
        m_flash.intensity = 2.4f * lowBattery;
        m_flash.color = Vector3{1.0f, 0.97f, 0.9f};
    } else {
        m_flash.innerCos = std::cos(15.0f * DEG2RAD);
        m_flash.outerCos = std::cos(29.0f * DEG2RAD);
        m_flash.range = 13.0f;
        m_flash.intensity = 1.55f * lowBattery;
        m_flash.color = Vector3{1.0f, 0.9f, 0.74f};
    }
}

void MotelWorld3D::UpdateMovement(float dt) {
    BodyHandle ignore = m_carriedProp >= 0 ? m_level.Props()[m_carriedProp].body : INVALID_BODY;
    if (m_hiddenSpot >= 0) {
        m_holdingBreath = IsKeyDown(KEY_SPACE) && m_breath > 0.0f;
        if (m_holdingBreath) {
            m_breath = std::max(0.0f, m_breath - dt * 18.0f);
            if (m_breath <= 0.0f) {
                PlaySound(m_script.Sound("breath", "audio/sfx/sfx_chase_stamina_pant.wav"), 0.8f, std::nullopt);
                AddSanity(-4.0f);
            }
        } else {
            m_breath = std::min(100.0f, m_breath + dt * 14.0f);
        }
        m_physics.UpdateCharacter(dt, Vector3{0.0f, 0.0f, 0.0f}, false, ignore);
        const HideSpot& spot = m_level.HideSpots()[m_hiddenSpot];
        engine::camera3d::CameraMotion motion;
        m_camera.Update(dt, Vector3Subtract(spot.eye, Vector3{0.0f, m_camera.GetEyeHeight(), 0.0f}), motion);
        return;
    }

    Vector3 fwd = m_camera.GetFlatForward();
    Vector3 right = m_camera.GetRight();
    Vector3 input{0.0f, 0.0f, 0.0f};
    float strafe = 0.0f;
    if (IsKeyDown(KEY_W)) input = Vector3Add(input, fwd);
    if (IsKeyDown(KEY_S)) input = Vector3Subtract(input, fwd);
    if (IsKeyDown(KEY_D)) { input = Vector3Add(input, right); strafe += 1.0f; }
    if (IsKeyDown(KEY_A)) { input = Vector3Subtract(input, right); strafe -= 1.0f; }
    float len = Vector3Length(input);
    if (len > 0.001f) input = Vector3Scale(input, 1.0f / len);

    bool wantCrouch = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C);
    bool crouched = m_physics.IsCharacterCrouched();
    bool wantSprint = IsKeyDown(KEY_LEFT_SHIFT) && len > 0.001f && !crouched && m_carriedProp < 0;
    if (wantSprint && m_stamina > 1.0f) {
        m_sprinting = true;
        m_stamina = std::max(0.0f, m_stamina - dt * 17.0f);
    } else {
        m_sprinting = false;
        m_stamina = std::min(100.0f, m_stamina + dt * (len > 0.001f ? 9.0f : 15.0f));
    }
    float speed = crouched ? CROUCH_SPEED : (m_sprinting ? RUN_SPEED : WALK_SPEED);
    if (m_stamina < 15.0f && !m_sprinting) speed *= 0.85f;
    Vector3 target = Vector3Scale(input, speed);
    float accel = m_physics.IsCharacterOnGround() ? 11.0f : 2.0f;
    m_moveVelocity = Vector3Lerp(m_moveVelocity, target, std::min(1.0f, dt * accel));

    m_physics.UpdateCharacter(dt, m_moveVelocity, wantCrouch, ignore);

    engine::camera3d::CameraMotion motion;
    Vector3 vel = m_physics.GetCharacterVelocity();
    motion.horizontalSpeed = std::sqrt(vel.x * vel.x + vel.z * vel.z);
    motion.grounded = m_physics.IsCharacterOnGround();
    motion.crouched = m_physics.IsCharacterCrouched();
    motion.sprinting = m_sprinting;
    motion.strafe = strafe;
    float stress = 1.0f - m_sanity / 100.0f;
    m_camera.SetJitter(stress > 0.5f ? (stress - 0.5f) * 1.6f : 0.0f);
    m_camera.Update(dt, m_physics.GetCharacterPosition(), motion);
}

void MotelWorld3D::UpdateFootsteps() {
    if (!m_camera.PopFootstep() || m_hiddenSpot >= 0) return;
    BodyHandle ground = m_physics.GetCharacterGroundBody();
    std::string surface = m_level.SurfaceForBody(ground);
    if (surface.empty() && ground != INVALID_BODY) surface = "wood";
    bool wood = IsWoodSurface(surface);
    int idx = (m_stepIndex++ % 4) + 1;
    char path[80];
    std::snprintf(path, sizeof(path), "audio/sfx/sfx_footstep_%s_%02d.wav", wood ? "wood" : "concrete", idx);
    float vol = m_sprinting ? 0.7f : (m_physics.IsCharacterCrouched() ? 0.2f : 0.42f);
    if (IsSoftSurface(surface)) vol *= 0.6f;
    m_eng->Audio().PlaySfx(path, vol);
}

void MotelWorld3D::UpdateFocus() {
    m_focusKind = FocusKind::None;
    m_focusIndex = -1;
    m_focusLabel.clear();
    if (m_hiddenSpot >= 0) {
        m_focusKind = FocusKind::Hide;
        m_focusIndex = m_hiddenSpot;
        m_focusLabel = "Выйти из укрытия";
        return;
    }
    if (m_carriedProp >= 0) {
        m_focusLabel = "Отпустить  [ЛКМ] Бросить";
        return;
    }
    const Camera3D& cam = m_camera.GetCamera();
    Vector3 dir = m_camera.GetForward();
    RayHit hit = m_physics.CastRay(cam.position, dir, USE_DISTANCE + 0.8f);
    float limit = hit.hit ? hit.distance : USE_DISTANCE + 0.8f;
    float best = USE_DISTANCE;

    if (hit.hit && hit.distance <= USE_DISTANCE + 0.4f && m_carReady && hit.body == m_level.Vehicle().body) {
        m_focusKind = FocusKind::Vehicle;
        m_focusIndex = 0;
        m_focusLabel = "Сесть за руль: " + m_level.Vehicle().label;
        best = hit.distance;
    } else if (hit.hit && hit.distance <= USE_DISTANCE) {
        int door = m_level.FindDoorByBody(hit.body);
        int prop = m_level.FindPropByBody(hit.body);
        int inter = m_level.FindInteractByBody(hit.body);
        if (door >= 0) {
            const DoorObject& d = m_level.Doors()[door];
            m_focusKind = FocusKind::Door;
            m_focusIndex = door;
            m_focusLabel = d.label + (d.open ? " — закрыть" : " — открыть");
            best = hit.distance;
        } else if (prop >= 0 && m_level.Props()[prop].mass <= 30.0f) {
            m_focusKind = FocusKind::Prop;
            m_focusIndex = prop;
            m_focusLabel = "Взять: " + m_level.Props()[prop].label;
            best = hit.distance;
        } else if (inter >= 0) {
            m_focusKind = FocusKind::Interact;
            m_focusIndex = inter;
            m_focusLabel = m_level.Interacts()[inter].label;
            best = hit.distance;
        }
    }

    auto& interacts = m_level.Interacts();
    for (size_t i = 0; i < interacts.size(); ++i) {
        if (!interacts[i].bodies.empty()) continue;
        RayCollision rc = GetRayCollisionBox(Ray{cam.position, dir}, interacts[i].bounds);
        if (rc.hit && rc.distance < best && rc.distance <= limit + 0.05f) {
            best = rc.distance;
            m_focusKind = FocusKind::Interact;
            m_focusIndex = static_cast<int>(i);
            m_focusLabel = interacts[i].label;
        }
    }

    auto& items = m_level.Items();
    for (size_t i = 0; i < items.size(); ++i) {
        if (!ItemVisible(items[i])) continue;
        float t = RaySphere(cam.position, dir, items[i].position, 0.13f);
        if (t >= 0.0f && t < best + 0.15f && t <= limit + 0.12f) {
            best = t;
            m_focusKind = FocusKind::Item;
            m_focusIndex = static_cast<int>(i);
            const ItemDefinition* def = m_script.FindItem(items[i].item);
            m_focusLabel = "Взять: " + (def ? def->name : items[i].item);
        }
    }

    const auto& spots = m_level.HideSpots();
    for (size_t i = 0; i < spots.size(); ++i) {
        float t = RaySphere(cam.position, dir, spots[i].eye, 0.75f);
        float d = Vector3Distance(cam.position, spots[i].eye);
        if (t >= 0.0f && d < 2.4f && t <= limit + 0.8f && (m_focusKind == FocusKind::None || m_focusKind == FocusKind::Prop)) {
            m_focusKind = FocusKind::Hide;
            m_focusIndex = static_cast<int>(i);
            m_focusLabel = "Спрятаться: " + spots[i].label;
        }
    }

    for (size_t i = 0; i < m_level.Mirrors().size(); ++i) {
        MirrorObject& mir = m_level.Mirrors()[i];
        if (mir.fired || mir.lookEvent.empty()) continue;
        Vector3 toM = Vector3Subtract(mir.center, cam.position);
        float d = Vector3Length(toM);
        if (d <= 4.2f && Vector3DotProduct(dir, Vector3Scale(toM, 1.0f / d)) > 0.72f && d < best) {
            m_focusKind = FocusKind::Interact;
            m_focusIndex = -999;
            m_focusLabel = "Осмотреть зеркало";
            best = d;
        }
    }
}

void MotelWorld3D::UseDoor(int index, bool forceOpen) {
    DoorObject& door = m_level.Doors()[index];
    if (door.locked) {
        if (!door.lockItem.empty() && m_script.HasItem(door.lockItem)) {
            UnlockDoor(door.name);
            m_script.RunEvent(door.unlockEvent, *this, door.restCenter);
        } else {
            PlaySound(m_script.Sound("door_locked", "audio/sfx/sfx_door_locked_rattle.wav"), 0.7f, door.restCenter);
            m_physics.AddImpulse(door.body, Vector3Scale(m_camera.GetFlatForward(), 8.0f));
            m_script.RunEvent(door.lockedEvent, *this, door.restCenter);
            return;
        }
    }
    if (door.open && !forceOpen) {
        door.open = false;
        m_physics.SetDoorTarget(door.body, 0.0f);
        PlaySound(m_script.Sound("door_open", "audio/sfx/sfx_door_creak_open.wav"), 0.35f, door.restCenter);
        return;
    }
    Vector3 player = m_physics.GetCharacterPosition();
    Vector3 rel = Vector3Subtract(door.restCenter, door.hinge);
    float a = std::abs(door.openAngle);
    float chosen = door.openAngle;
    if (door.bothWays) {
        Vector3 p1 = Vector3Add(door.hinge, RotateYVec(rel, a));
        Vector3 p2 = Vector3Add(door.hinge, RotateYVec(rel, -a));
        float d1 = Vector2Distance(Vector2{p1.x, p1.z}, Vector2{player.x, player.z});
        float d2 = Vector2Distance(Vector2{p2.x, p2.z}, Vector2{player.x, player.z});
        chosen = d1 >= d2 ? a : -a;
    }
    door.open = true;
    m_physics.SetDoorTarget(door.body, chosen);
    PlaySound(m_script.Sound("door_open", "audio/sfx/sfx_door_creak_open.wav"), 0.55f, door.restCenter);
}

void MotelWorld3D::HandleUse() {
    if (m_carriedProp >= 0) {
        PropObject& prop = m_level.Props()[m_carriedProp];
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector3 v = Vector3Add(m_physics.GetBodyVelocity(prop.body), Vector3Scale(m_camera.GetForward(), 7.5f / std::max(1.0f, prop.mass * 0.25f)));
            m_physics.SetBodyVelocity(prop.body, v, m_physics.GetBodyAngularVelocity(prop.body));
            m_carriedProp = -1;
            return;
        }
        if (IsKeyPressed(KEY_E)) m_carriedProp = -1;
        return;
    }
    if (!IsKeyPressed(KEY_E)) return;

    switch (m_focusKind) {
        case FocusKind::Door:
            UseDoor(m_focusIndex, false);
            break;
        case FocusKind::Prop:
            m_carriedProp = m_focusIndex;
            m_physics.WakeBody(m_level.Props()[m_carriedProp].body);
            break;
        case FocusKind::Interact: {
            if (m_focusIndex == -999) {
                for (auto& mir : m_level.Mirrors()) {
                    if (!mir.fired && !mir.lookEvent.empty()) {
                        mir.fired = true;
                        m_script.RunEvent(mir.lookEvent, *this, mir.center);
                        break;
                    }
                }
                break;
            }
            const InteractObject& obj = m_level.Interacts()[m_focusIndex];
            m_script.RunEvent(obj.event, *this, obj.center);
            break;
        }
        case FocusKind::Item: {
            ItemObject& item = m_level.Items()[m_focusIndex];
            item.taken = true;
            const ItemDefinition* def = m_script.FindItem(item.item);
            if (def == nullptr || !def->consumable) m_script.GiveItem(item.item);
            m_eng->Audio().PlaySfx(m_script.Sound("pickup", "audio/sfx/sfx_key_pickup.wav"), 0.75f);
            m_inspectMode = true;
            m_inspectItem = item.item;
            m_inspectYaw = 0.4f;
            m_inspectPitch = 0.15f;
            m_inspectZoom = 1.0f;
            break;
        }
        case FocusKind::Vehicle:
            m_script.RunEvent(m_level.Vehicle().event, *this, m_car.Position());
            break;
        case FocusKind::Hide:
            if (m_hiddenSpot >= 0) {
                m_hiddenSpot = -1;
            } else {
                m_hiddenSpot = m_focusIndex;
                m_camera.SetLookAngles(m_level.HideSpots()[m_hiddenSpot].yaw, 0.0f);
                m_breath = 100.0f;
            }
            PlaySound(m_script.Sound("hide", "audio/sfx/sfx_wardrobe_hide.wav"), 0.6f, std::nullopt);
            break;
        default:
            break;
    }
}

void MotelWorld3D::UpdateTriggers() {
    Vector3 p = Vector3Add(PlayerAnchor(), Vector3{0.0f, 0.9f, 0.0f});
    for (TriggerObject& t : m_level.Triggers()) {
        bool inside = PointInBox(p, t.bounds);
        if (inside && !t.inside && !(t.once && t.fired)) {
            t.fired = true;
            m_script.RunEvent(t.event, *this);
        }
        t.inside = inside;
    }
}

void MotelWorld3D::UpdateMirrorEvents() {
    const Camera3D& cam = m_camera.GetCamera();
    Vector3 fwd = m_camera.GetForward();
    for (MirrorObject& m : m_level.Mirrors()) {
        if (m.fired || m.lookEvent.empty()) continue;
        Vector3 toMirror = Vector3Subtract(m.center, cam.position);
        float dist = Vector3Length(toMirror);
        if (dist > 4.8f || dist < 0.1f) continue;
        if (Vector3DotProduct(Vector3Subtract(cam.position, m.center), m.normal) <= 0.0f) continue;
        if (Vector3DotProduct(fwd, Vector3Scale(toMirror, 1.0f / dist)) < 0.65f) continue;
        if (m_physics.IsStaticOccluded(cam.position, Vector3Add(m.center, Vector3Scale(m.normal, 0.08f)))) continue;
        m.fired = true;
        m_script.RunEvent(m.lookEvent, *this, m.center);
    }
}

void MotelWorld3D::UpdateSoundEmitters(float dt) {
    Vector3 listener = m_camera.GetCamera().position;
    for (SoundEmitter& s : m_level.Sounds()) {
        s.timer -= dt;
        if (s.timer > 0.0f) continue;
        s.timer = s.interval * (0.8f + Rand01() * 0.4f);
        if (!s.flag.empty() && !m_script.HasFlag(s.flag)) continue;
        if (!s.hideFlag.empty() && m_script.HasFlag(s.hideFlag)) continue;
        float d = Vector3Distance(listener, s.position);
        if (d > s.radius) continue;
        PlaySound(s.sound, s.volume * (1.0f - d / s.radius) * 1.6f, s.position);
    }
}

void MotelWorld3D::RespawnDrop(RainDrop3D& drop, bool anyHeight) {
    Vector3 c = ViewCamera().position;
    float angle = Rand01() * 2.0f * PI;
    float r = std::sqrt(Rand01()) * RAIN_RADIUS;
    drop.pos = Vector3{c.x + std::cos(angle) * r, c.y + (anyHeight ? (Rand01() * 9.0f - 2.0f) : (5.0f + Rand01() * 3.0f)), c.z + std::sin(angle) * r};
    drop.speed = 14.0f + Rand01() * 5.0f;
    drop.len = 0.35f + Rand01() * 0.25f;
    drop.visible = m_physics.CastStaticDistance(drop.pos, Vector3{0.0f, 1.0f, 0.0f}, 30.0f) >= 29.9f;
    drop.stopY = drop.pos.y - m_physics.CastStaticDistance(drop.pos, Vector3{0.0f, -1.0f, 0.0f}, 40.0f);
}

void MotelWorld3D::UpdateRain(float dt) {
    Vector3 c = ViewCamera().position;
    for (RainDrop3D& d : m_rain) {
        d.pos.y -= d.speed * dt;
        d.pos.x -= d.speed * 0.1f * dt;
        float dx = d.pos.x - c.x;
        float dz = d.pos.z - c.z;
        if (d.pos.y < c.y - 3.0f || d.pos.y < d.stopY || dx * dx + dz * dz > RAIN_RADIUS * RAIN_RADIUS * 1.3f) {
            RespawnDrop(d, false);
        }
    }
}

void MotelWorld3D::UpdateSanity(float dt) {
    bool dark = !m_flash.enabled && !m_power;
    if (dark) {
        m_sanity -= dt * 0.9f;
    } else if (m_power && m_flash.enabled) {
        m_sanity += dt * 0.35f;
    }
    if (m_hiddenSpot >= 0) m_sanity += dt * 0.6f;
    m_sanity = std::clamp(m_sanity, 0.0f, 100.0f);
    float stress = 1.0f - m_sanity / 100.0f;
    m_eng->Renderer().SetSanityStress(stress * stress);

    if (m_sanity < 40.0f) {
        m_whisperTimer -= dt;
        if (m_whisperTimer <= 0.0f) {
            m_whisperTimer = 16.0f + Rand01() * 18.0f;
        }
    }
}

void MotelWorld3D::FixedUpdate(engine::core::Engine&, float fixedDt) {
    if (!m_ready || m_inspectMode || m_settingsUI.IsOpen()) return;
    if (m_carReady && m_mode == PlayerMode::Driving) m_physics.MoveKinematic(m_level.Vehicle().body, m_car.Transform(), fixedDt);
    if (m_carriedProp >= 0) {
        PropObject& prop = m_level.Props()[m_carriedProp];
        const Camera3D& cam = m_camera.GetCamera();
        Vector3 target = Vector3Add(cam.position, Vector3Scale(m_camera.GetForward(), 1.35f));
        Vector3 pos = m_physics.GetBodyPosition(prop.body);
        Vector3 delta = Vector3Subtract(target, pos);
        if (Vector3Length(delta) > 2.4f) {
            m_carriedProp = -1;
        } else {
            Vector3 vel = Vector3Scale(delta, 12.0f);
            float speed = Vector3Length(vel);
            if (speed > 7.0f) vel = Vector3Scale(vel, 7.0f / speed);
            Vector3 ang = Vector3Scale(m_physics.GetBodyAngularVelocity(prop.body), 0.6f);
            m_physics.SetBodyVelocity(prop.body, vel, ang);
        }
    }
    m_physics.Step(fixedDt);

    for (PropObject& p : m_level.Props()) {
        Vector3 v = m_physics.GetBodyVelocity(p.body);
        float dv = Vector3Length(Vector3Subtract(v, p.lastVelocity));
        p.lastVelocity = v;
        if (p.impactCooldown > 0.0f) p.impactCooldown -= fixedDt;
        if (dv > 2.6f && p.impactCooldown <= 0.0f && m_level.FindPropByBody(p.body) != m_carriedProp) {
            p.impactCooldown = 0.3f;
            PlaySound(m_script.Sound("throw", "audio/sfx/sfx_chase_debris_hit.wav"), std::min(0.9f, dv / 9.0f), m_physics.GetBodyPosition(p.body));
        }
    }
}

float MotelWorld3D::LightStyleFactor(const LightObject& light) const {
    float t = m_time + light.phase;
    if (light.style == "blink") return std::fmod(t, 1.0f) < 0.5f ? 1.0f : 0.0f;
    if (light.style == "pulse") return 0.6f + 0.4f * std::sin(t * 2.2f);
    if (light.style == "flicker") {
        float n = std::sin(t * 23.0f) * std::sin(t * 7.1f + 1.3f) * std::sin(t * 2.3f);
        return n > 0.82f ? 0.15f : 0.85f + 0.15f * std::sin(t * 31.0f);
    }
    return 1.0f;
}

float MotelWorld3D::CurrentPowerLevel() const {
    if (!m_power) return 0.0f;
    if (m_flickerTimer > 0.0f) return m_flickerOn ? 1.0f : 0.08f;
    return 1.0f;
}

std::vector<DynamicLight> MotelWorld3D::GatherLights(Vector3 viewPos) const {
    struct Candidate {
        float score;
        DynamicLight light;
    };
    std::vector<Candidate> candidates;
    float power = CurrentPowerLevel();
    for (const LightObject& l : m_level.Lights()) {
        if (!l.dynamic) continue;
        float level = l.intensity * LightStyleFactor(l);
        if (l.group == "power") level *= power;
        if (level <= 0.01f) continue;
        float d = Vector3Distance(viewPos, l.position) - l.radius;
        if (d > 24.0f) continue;
        candidates.push_back({d, DynamicLight{l.position, Vector3Scale(l.color, level), l.radius}});
    }
    for (const DynamicLight& l : m_extraLights) {
        candidates.push_back({Vector3Distance(viewPos, l.position) - l.radius - 4.0f, l});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score < b.score; });
    std::vector<DynamicLight> out;
    for (const Candidate& c : candidates) {
        if (static_cast<int>(out.size()) >= engine::render3d::Lighting3D::MAX_LIGHTS) break;
        out.push_back(c.light);
    }
    return out;
}

SceneEnvironment MotelWorld3D::BuildEnvironment() const {
    const LevelEnvironment& le = m_level.Environment();
    SceneEnvironment env;
    env.ambientSky = le.ambientSky;
    env.ambientGround = le.ambientGround;
    env.fogColor = le.fogColor;
    env.fogStart = le.fogStart;
    env.fogEnd = le.fogEnd;
    env.powerLevel = CurrentPowerLevel();
    float highway = std::clamp((PlayerAnchor().x - 12.0f) / 20.0f, 0.0f, 1.0f);
    env.fogStart = Lerp(env.fogStart, 9.0f, highway);
    env.fogEnd = Lerp(env.fogEnd, 44.0f, highway);
    if (m_screamerActive) {
        float f = std::clamp(m_screamerTimer / std::max(0.01f, m_screamerDuration), 0.0f, 1.0f);
        env.ambientSky = Vector3Add(env.ambientSky, Vector3{1.4f * f, 1.5f * f, 1.8f * f});
        env.fogColor = Vector3Add(env.fogColor, Vector3{0.35f * f, 0.4f * f, 0.5f * f});
    }
    return env;
}

float MotelWorld3D::ViewAspect() const {
    if (m_eng != nullptr && m_eng->Settings().Get().stretchToUltrawide) {
        float w = static_cast<float>(std::max(1, GetScreenWidth()));
        float h = static_cast<float>(std::max(1, GetScreenHeight()));
        return w / h;
    }
    return UI_W / UI_H;
}

void MotelWorld3D::BeginCamera(const Camera3D& camera) const {
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    double top = RL_CULL_DISTANCE_NEAR * std::tan(camera.fovy * 0.5 * DEG2RAD);
    double right = top * ViewAspect();
    rlFrustum(-right, right, -top, top, RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    rlMultMatrixf(MatrixToFloat(view));
    rlEnableDepthTest();
}

bool MotelWorld3D::ItemVisible(const ItemObject& item) const {
    if (item.taken) return false;
    if (!item.requireFlag.empty() && !m_script.HasFlag(item.requireFlag)) return false;
    return true;
}

void MotelWorld3D::DrawScene(const Shader* overrideShader, bool reflectionPass) {
    Matrix identity = MatrixIdentity();
    for (const RenderBatch& b : m_level.StaticBatches()) {
        Material m = b.material;
        if (overrideShader != nullptr) {
            m.shader = *overrideShader;
        } else {
            m_lighting.SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, b.emissive ? 1.0f : 0.0f);
        }
        DrawMesh(b.mesh, m, identity);
    }
    if (overrideShader == nullptr) m_lighting.SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, 0.0f);

    for (const DoorObject& d : m_level.Doors()) {
        Matrix t = m_physics.GetBodyTransform(d.body);
        for (const RenderBatch& b : d.batches) {
            Material m = b.material;
            if (overrideShader != nullptr) m.shader = *overrideShader;
            DrawMesh(b.mesh, m, t);
        }
    }
    for (const PropObject& p : m_level.Props()) {
        Matrix t = m_physics.GetBodyTransform(p.body);
        for (const RenderBatch& b : p.batches) {
            Material m = b.material;
            if (overrideShader != nullptr) m.shader = *overrideShader;
            DrawMesh(b.mesh, m, t);
        }
    }

    float power = CurrentPowerLevel();
    for (const ItemObject& item : m_level.Items()) {
        if (!ItemVisible(item)) continue;
        const ItemDefinition* def = m_script.FindItem(item.item);
        const ItemVisual& visual = m_itemVisuals.Get(def ? def->model : "box", def ? def->color : WHITE);
        if (overrideShader == nullptr) {
            Vector3 extra = Vector3Add(item.bakedLight, Vector3Scale(item.bakedPower, power));
            float glint = 0.04f + 0.03f * std::sin(m_time * 3.0f);
            m_lighting.SetObjectParams(0.0f, Vector3AddValue(extra, glint), 0.0f);
        }
        Matrix world = MatrixMultiply(MatrixRotateY(item.yaw), MatrixTranslate(item.position.x, item.position.y, item.position.z));
        m_itemVisuals.Draw(visual, world, overrideShader);
    }
    if (overrideShader == nullptr) m_lighting.SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, 0.0f);

    DrawVehicle(overrideShader);
    DrawTrees(overrideShader, ViewCamera().position);

    if (reflectionPass) {
        DrawPhantoms(true);
    } else {
        DrawPhantoms(false);
        if (overrideShader == nullptr) DrawStalker(ViewCamera());
    }
}

void MotelWorld3D::DrawPhantoms(bool reflectionPass) const {
    const Camera3D& currentCam = reflectionPass ? m_reflectionCam : ViewCamera();
    for (const Phantom& p : m_level.Phantoms()) {
        if (!p.flag.empty() && !m_script.HasFlag(p.flag)) continue;
        if (!p.hideFlag.empty() && m_script.HasFlag(p.hideFlag)) continue;
        bool isMirrorPhantom = (p.position.x > -19.0f && p.position.z > -3.0f);
        if (reflectionPass != isMirrorPhantom) continue;
        Vector3 base = p.position;
        float sway = std::sin(m_time * 1.1f) * 0.02f;
        bool horrific = m_script.HasFlag("mirror_seen") || m_script.HasFlag("corridor_whisper_done");
        float jitter = horrific ? (std::sin(m_time * 55.0f) > 0.35f ? 0.035f : -0.035f) : 0.0f;
        Vector3 center{base.x + sway + jitter, base.y + 1.15f, base.z + jitter * 0.5f};
        if (m_phantomTex.id != 0) {
            rlDrawRenderBatchActive();
            Rectangle src{0.0f, 0.0f, static_cast<float>(m_phantomTex.width), static_cast<float>(m_phantomTex.height)};
            Vector2 size{1.15f, 2.3f};
            Color tint = horrific ? Color{255, 230, 230, 250} : Color{215, 220, 230, 240};
            DrawBillboardRec(currentCam, m_phantomTex, src, center, size, tint);
            rlDrawRenderBatchActive();
            if (horrific) {
                Vector3 headPos{center.x, center.y + 0.65f, center.z};
                Vector3 toCam = Vector3Normalize(Vector3Subtract(currentCam.position, headPos));
                Vector3 right = Vector3Normalize(Vector3CrossProduct(currentCam.up, toCam));
                Vector3 eyeL = Vector3Add(headPos, Vector3Add(Vector3Scale(right, -0.052f), Vector3Scale(toCam, 0.02f)));
                Vector3 eyeR = Vector3Add(headPos, Vector3Add(Vector3Scale(right, 0.052f), Vector3Scale(toCam, 0.02f)));
                float pulse = 0.5f + 0.5f * std::sin(m_time * 24.0f);
                Color eyeCol{255, static_cast<unsigned char>(30 + 40 * pulse), static_cast<unsigned char>(30 + 40 * pulse), 255};
                DrawSphereEx(eyeL, 0.018f, 4, 4, eyeCol);
                DrawSphereEx(eyeR, 0.018f, 4, 4, eyeCol);
            }
        }
    }
}

void MotelWorld3D::DrawViewmodel() const {
    if (m_hiddenSpot >= 0) return;
    const Camera3D& cam = m_camera.GetCamera();
    Vector3 fwd = m_camera.GetForward();
    Vector3 right = m_camera.GetRight();
    Vector3 up = m_camera.GetUp();
    float phase = m_camera.GetBobPhase();
    float swayX = std::cos(phase) * 0.006f;
    float swayY = std::abs(std::sin(phase)) * 0.008f;
    Vector3 lag = Vector3Subtract(m_flashDir, fwd);
    float focusShift = m_flashFocus ? -0.05f : 0.0f;
    Vector3 back = Vector3Add(cam.position, Vector3Add(Vector3Add(Vector3Scale(right, 0.15f + swayX + focusShift), Vector3Scale(up, -0.14f + swayY)), Vector3Scale(fwd, 0.2f)));
    back = Vector3Add(back, Vector3Scale(lag, 0.08f));
    Vector3 dir = m_flashDir;
    Vector3 front = Vector3Add(back, Vector3Scale(dir, 0.13f));
    Vector3 bezel = Vector3Add(front, Vector3Scale(dir, 0.03f));
    Vector3 sleeve = Vector3Subtract(back, Vector3Scale(dir, 0.16f));

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    DrawCylinderEx(Vector3Subtract(sleeve, Vector3Scale(up, 0.02f)), Vector3Subtract(back, Vector3Scale(dir, 0.02f)), 0.045f, 0.038f, 8, Color{18, 16, 22, 255});
    DrawCylinderEx(Vector3Subtract(back, Vector3Scale(dir, 0.03f)), Vector3Add(back, Vector3Scale(dir, 0.035f)), 0.03f, 0.028f, 8, Color{34, 26, 24, 255});
    DrawCylinderEx(back, front, 0.019f, 0.024f, 8, Color{38, 40, 46, 255});
    DrawCylinderEx(front, bezel, 0.027f, 0.031f, 8, Color{26, 28, 32, 255});
    Color lens = m_flash.enabled ? (m_flashFocus ? Color{255, 255, 230, 255} : Color{255, 238, 190, 255}) : Color{45, 40, 35, 255};
    DrawSphereEx(Vector3Add(bezel, Vector3Scale(dir, -0.006f)), 0.017f, 6, 6, lens);
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

void MotelWorld3D::RenderOffscreen(engine::core::Engine&) {
    if (!m_ready) return;
    const Camera3D& cam = ViewCamera();
    SceneEnvironment env = BuildEnvironment();
    Shader depth = m_lighting.DepthShader();

    if (m_flash.enabled) {
        m_lighting.BeginShadowPass(m_flash);
        DrawScene(&depth, false);
        m_lighting.EndShadowPass();
    }

    m_activeMirror = -1;
    if (m_mode != PlayerMode::OnFoot) return;
    float bestDist = 8.0f;
    Vector3 fwd = m_camera.GetForward();
    const auto& mirrors = m_level.Mirrors();
    for (size_t i = 0; i < mirrors.size(); ++i) {
        Vector3 toMirror = Vector3Subtract(mirrors[i].center, cam.position);
        float d = Vector3Length(toMirror);
        if (d > bestDist) continue;
        if (Vector3DotProduct(Vector3Subtract(cam.position, mirrors[i].center), mirrors[i].normal) <= 0.02f) continue;
        if (d > 1.0f && Vector3DotProduct(fwd, Vector3Scale(toMirror, 1.0f / d)) < 0.2f) continue;
        bestDist = d;
        m_activeMirror = static_cast<int>(i);
    }
    if (m_activeMirror < 0 || m_reflectionRT.id == 0) return;

    const MirrorObject& mirror = mirrors[m_activeMirror];
    auto reflectPoint = [&](Vector3 p) {
        float dist = Vector3DotProduct(mirror.normal, p) - mirror.planeDistance;
        return Vector3Subtract(p, Vector3Scale(mirror.normal, 2.0f * dist));
    };
    auto reflectDir = [&](Vector3 v) {
        return Vector3Subtract(v, Vector3Scale(mirror.normal, 2.0f * Vector3DotProduct(mirror.normal, v)));
    };
    Camera3D refl = cam;
    refl.position = reflectPoint(cam.position);
    refl.target = reflectPoint(cam.target);
    refl.up = reflectDir(cam.up);
    m_reflectionCam = refl;

    BeginTextureMode(m_reflectionRT);
    ClearBackground(Color{static_cast<unsigned char>(env.fogColor.x * 255.0f), static_cast<unsigned char>(env.fogColor.y * 255.0f),
                          static_cast<unsigned char>(env.fogColor.z * 255.0f), 255});
    BeginCamera(refl);
    m_lighting.ApplyFrame(refl.position, m_flash, GatherLights(refl.position), env, m_flash.enabled);
    m_lighting.BindShadowMap();
    m_lighting.SetClipPlane(true, Vector4{mirror.normal.x, mirror.normal.y, mirror.normal.z, -mirror.planeDistance - 0.01f});
    DrawScene(nullptr, true);
    m_lighting.SetClipPlane(false, Vector4{0.0f, 1.0f, 0.0f, 0.0f});
    EndMode3D();
    EndTextureMode();
}

void MotelWorld3D::Render3D(engine::core::Engine&) {
    if (!m_ready) return;
    const Camera3D& cam = ViewCamera();
    SceneEnvironment env = BuildEnvironment();
    ClearBackground(Color{static_cast<unsigned char>(env.fogColor.x * 255.0f), static_cast<unsigned char>(env.fogColor.y * 255.0f),
                          static_cast<unsigned char>(env.fogColor.z * 255.0f), 255});

    BeginCamera(cam);
    m_lighting.ApplyFrame(cam.position, m_flash, GatherLights(cam.position), env, m_flash.enabled);
    m_lighting.BindShadowMap();
    DrawScene(nullptr, false);

    float distort = m_script.HasFlag("mirror_seen") && !m_script.HasFlag("phantom_gone") ? 1.0f : 0.15f;
    m_lighting.SetMirrorParams(cam.position, env, m_time, Vector2{UI_W, UI_H}, distort);
    const auto& mirrors = m_level.Mirrors();
    for (size_t i = 0; i < mirrors.size(); ++i) {
        Material m = mirrors[i].surface.material;
        m.maps[MATERIAL_MAP_DIFFUSE].texture = (static_cast<int>(i) == m_activeMirror) ? m_reflectionRT.texture
                                                                                       : m.maps[MATERIAL_MAP_SPECULAR].texture;
        DrawMesh(mirrors[i].surface.mesh, m, MatrixIdentity());
    }

    rlDrawRenderBatchActive();
    Color rainColor{150, 175, 210, 120};
    for (const RainDrop3D& d : m_rain) {
        if (!d.visible) continue;
        DrawLine3D(d.pos, Vector3{d.pos.x - 0.04f, d.pos.y - d.len, d.pos.z}, rainColor);
    }

    if (m_screamerActive && m_screamerTex.id != 0 && m_script.HasFlag("corridor_whisper_done")) {
        rlDrawRenderBatchActive();
        DrawBillboard(cam, m_screamerTex, Vector3{-19.92f, 1.6f, -6.75f}, 1.5f, Color{255, 255, 255, 240});
        rlDrawRenderBatchActive();
    }
    if (m_mode == PlayerMode::OnFoot) {
        if (HasPistol()) {
            DrawPistolViewmodel();
        } else {
            DrawViewmodel();
        }
    } else if (m_mode == PlayerMode::Driving && m_driveView == 1) {
        DrawCockpit();
    }
    EndMode3D();
}

void MotelWorld3D::DrawInspectOverlay(engine::core::Engine& eng) {
    Font font = eng.Assets().GetMainFont();
    DrawRectangle(0, 0, static_cast<int>(UI_W), static_cast<int>(UI_H), Color{6, 5, 9, 225});
    const ItemDefinition* def = m_script.FindItem(m_inspectItem);
    const ItemVisual& visual = m_itemVisuals.Get(def ? def->model : "box", def ? def->color : WHITE);

    Camera3D cam{};
    float dist = visual.inspectDistance * m_inspectZoom;
    cam.position = Vector3{0.0f, 0.035f, dist};
    cam.target = Vector3{0.0f, 0.0f, 0.0f};
    cam.up = Vector3{0.0f, 1.0f, 0.0f};
    cam.fovy = 40.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    engine::render3d::Flashlight light;
    light.enabled = true;
    light.position = Vector3{0.12f, 0.18f, dist + 0.1f};
    light.direction = Vector3Normalize(Vector3Subtract(Vector3{0.0f, 0.0f, 0.0f}, light.position));
    light.innerCos = std::cos(25.0f * DEG2RAD);
    light.outerCos = std::cos(45.0f * DEG2RAD);
    light.range = 5.0f;
    light.intensity = 1.9f;
    SceneEnvironment env;
    env.fogStart = 100.0f;
    env.fogEnd = 200.0f;
    env.ambientSky = Vector3{0.22f, 0.22f, 0.26f};
    env.ambientGround = Vector3{0.08f, 0.07f, 0.07f};
    env.dither = false;
    env.snapResolution = Vector2{0.0f, 0.0f};

    BeginCamera(cam);
    m_lighting.ApplyFrame(cam.position, light, {}, env, false);
    m_lighting.SetObjectParams(0.0f, Vector3{0.12f, 0.11f, 0.1f}, 0.0f);
    Matrix world = MatrixMultiply(MatrixRotateY(m_inspectYaw), MatrixRotateX(m_inspectPitch));
    m_itemVisuals.Draw(visual, world);
    EndMode3D();
    m_lighting.SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, 0.0f);

    float panelX = UI_W * 0.5f - 330.0f;
    DrawRectangle(static_cast<int>(panelX), 520, 660, 140, Color{16, 12, 20, 235});
    DrawRectangleLines(static_cast<int>(panelX), 520, 660, 140, Color{244, 184, 96, 230});
    if (def != nullptr) {
        DrawLabel(font, def->inspectTitle, panelX + 20.0f, 532.0f, 22.0f, Color{244, 184, 96, 255});
        DrawLabel(font, def->inspectText, panelX + 20.0f, 566.0f, 17.0f, Color{225, 220, 210, 240});
    }
    DrawLabel(font, "[Мышь] Вращать  |  [Колесо] Приблизить  |  [E / ПКМ] Убрать", panelX + 20.0f, 634.0f, 15.0f, Color{180, 160, 140, 210});
}

void MotelWorld3D::RenderUI(engine::core::Engine& eng) {
    Font font = eng.Assets().GetMainFont();
    float cx = UI_W * 0.5f;
    float cy = UI_H * 0.5f;

    if (!m_ready) {
        DrawRectangle(0, 0, static_cast<int>(UI_W), static_cast<int>(UI_H), Color{10, 8, 12, 255});
        DrawLabel(font, "ГЛАВА 2 НЕ ЗАГРУЖЕНА", cx - 150.0f, cy - 40.0f, 26.0f, Color{230, 80, 70, 255});
        DrawLabel(font, m_loadError, cx - 300.0f, cy, 18.0f, Color{220, 210, 200, 255});
        DrawLabel(font, "[ESC] Вернуться в меню", cx - 110.0f, cy + 40.0f, 18.0f, Color{180, 170, 160, 255});
        return;
    }

    if (m_settingsUI.IsOpen()) {
        m_settingsUI.Render(eng);
        return;
    }

    if (m_inspectMode) {
        DrawInspectOverlay(eng);
        return;
    }

    if (m_hiddenSpot >= 0) {
        int slitW = 90;
        DrawRectangle(0, 0, static_cast<int>(cx) - slitW, static_cast<int>(UI_H), Color{4, 3, 5, 250});
        DrawRectangle(static_cast<int>(cx) + slitW, 0, static_cast<int>(cx) - slitW, static_cast<int>(UI_H), Color{4, 3, 5, 250});
        DrawRectangleGradientH(static_cast<int>(cx) - slitW, 0, 40, static_cast<int>(UI_H), Color{4, 3, 5, 250}, Color{4, 3, 5, 0});
        DrawRectangleGradientH(static_cast<int>(cx) + slitW - 40, 0, 40, static_cast<int>(UI_H), Color{4, 3, 5, 0}, Color{4, 3, 5, 250});
        DrawRectangle(static_cast<int>(cx) - 120, 640, 240, 10, Color{20, 20, 26, 230});
        DrawRectangle(static_cast<int>(cx) - 120, 640, static_cast<int>(240.0f * m_breath / 100.0f), 10,
                      m_holdingBreath ? Color{120, 190, 240, 240} : Color{90, 110, 130, 200});
        DrawLabel(font, "[ПРОБЕЛ] Задержать дыхание   [E] Выйти", cx - 170.0f, 656.0f, 17.0f, Color{196, 178, 156, 230});
    } else if (m_mode == PlayerMode::OnFoot && !HasPistol()) {
        DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 2.0f, m_focusKind != FocusKind::None ? Color{255, 220, 150, 240} : Color{220, 210, 200, 150});
    }

    if (!m_focusLabel.empty() && m_mode == PlayerMode::OnFoot) {
        std::string prompt = "[E] " + m_focusLabel;
        Vector2 size = MeasureTextEx(font, prompt.c_str(), 19.0f, 1.0f);
        float px = cx - size.x * 0.5f;
        float py = cy + 30.0f;
        DrawRectangle(static_cast<int>(px - 12.0f), static_cast<int>(py - 5.0f), static_cast<int>(size.x + 24.0f), static_cast<int>(size.y + 10.0f), Color{14, 10, 18, 200});
        DrawLabel(font, prompt, px, py, 19.0f, Color{244, 196, 120, 255});
    }

    if (m_messageTimer > 0.0f && !m_message.empty()) {
        float alpha = std::clamp(m_messageTimer, 0.0f, 1.0f);
        Vector2 size = MeasureTextEx(font, m_message.c_str(), 19.0f, 1.0f);
        float bx = cx - size.x * 0.5f;
        DrawRectangle(static_cast<int>(bx - 16.0f), 560, static_cast<int>(size.x + 32.0f), static_cast<int>(size.y + 16.0f),
                      Color{12, 9, 14, static_cast<unsigned char>(210 * alpha)});
        DrawLabel(font, m_message, bx, 568.0f, 19.0f, Color{235, 228, 215, static_cast<unsigned char>(255 * alpha)});
    }

    if (!m_objective.empty() && eng.Settings().Get().showObjectiveLine) {
        float flash = m_objectiveFlash > 0.0f ? (0.5f + 0.5f * std::sin(m_time * 10.0f)) : 0.0f;
        Color c{static_cast<unsigned char>(200 + 55 * flash), static_cast<unsigned char>(180 + 40 * flash), 140, 235};
        DrawLabel(font, "ЦЕЛЬ: " + m_objective, 24.0f, 20.0f, 18.0f, c);
    }

    if (m_mode == PlayerMode::OnFoot) {
        DrawRectangle(20, 646, 250, 56, Color{12, 9, 14, 190});
        std::string flashLabel = m_flashOn ? "Фонарик [F]" : "Фонарик выкл [F]";
        DrawLabel(font, flashLabel, 30.0f, 652.0f, 15.0f, m_flashOn ? Color{244, 220, 140, 240} : Color{140, 130, 120, 200});
        Color batCol = m_battery > 40.0f ? Color{70, 200, 120, 230} : (m_battery > 15.0f ? Color{230, 180, 50, 230} : Color{230, 60, 50, 230});
        DrawRectangle(150, 656, static_cast<int>(108.0f * m_battery / 100.0f), 8, batCol);
        DrawRectangleLines(150, 656, 108, 8, Color{170, 160, 150, 180});
        DrawRectangle(30, 676, static_cast<int>(228.0f * m_stamina / 100.0f), 4, Color{80, 160, 220, 210});
        DrawRectangle(30, 686, static_cast<int>(228.0f * m_sanity / 100.0f), 4, Color{190, 80, 120, 210});
    }

    float ix = 290.0f;
    for (const std::string& id : m_script.Inventory()) {
        const ItemDefinition* def = m_script.FindItem(id);
        std::string name = def ? def->name : id;
        Vector2 size = MeasureTextEx(font, name.c_str(), 15.0f, 1.0f);
        DrawRectangle(static_cast<int>(ix), 670, static_cast<int>(size.x + 18.0f), 26, Color{12, 9, 14, 190});
        DrawRectangle(static_cast<int>(ix), 670, 4, 26, def ? def->color : WHITE);
        DrawLabel(font, name, ix + 10.0f, 675.0f, 15.0f, Color{220, 210, 196, 230});
        ix += size.x + 26.0f;
    }

    if (m_mode == PlayerMode::OnFoot && !HasPistol()) {
        DrawLabel(font, "[WASD] Шаг  [SHIFT] Бег  [CTRL] Присед  [ПКМ] Фокус луча  [E] Действие  [ESC] Меню",
                  UI_W - 640.0f, 700.0f, 14.0f, Color{170, 160, 150, 170});
    }

    if (m_endActTimer > 0.0f && !m_endActText.empty()) {
        float a = std::clamp(std::min(m_endActTimer, 9.0f - m_endActTimer), 0.0f, 1.0f);
        DrawRectangle(0, 250, static_cast<int>(UI_W), 120, Color{0, 0, 0, static_cast<unsigned char>(200 * a)});
        Vector2 size = MeasureTextEx(font, m_endActText.c_str(), 30.0f, 1.0f);
        DrawLabel(font, m_endActText, cx - size.x * 0.5f, 292.0f, 30.0f, Color{230, 200, 160, static_cast<unsigned char>(255 * a)});
    }

    if (m_screamerRedFlash > 0.0f) {
        DrawRectangle(0, 0, static_cast<int>(UI_W), static_cast<int>(UI_H), Color{210, 15, 15, static_cast<unsigned char>(85 * m_screamerRedFlash)});
    }

    if (m_screamerTimer > 0.0f) {
        float norm = m_screamerTimer / std::max(0.01f, m_screamerDuration);
        float alpha = std::clamp(norm, 0.0f, 1.0f);
        DrawRectangle(0, 0, static_cast<int>(UI_W), static_cast<int>(UI_H), Color{140, 10, 10, static_cast<unsigned char>(130 * alpha)});
        if (m_screamerTex.id != 0) {
            float jx = (std::sin(m_time * 80.0f) > 0.0f ? 8.0f : -8.0f) * alpha;
            float jy = (std::cos(m_time * 70.0f) > 0.0f ? 6.0f : -6.0f) * alpha;
            Rectangle src{0.0f, 0.0f, static_cast<float>(m_screamerTex.width), static_cast<float>(m_screamerTex.height)};
            Rectangle dst{jx, jy, UI_W, UI_H};
            DrawTexturePro(m_screamerTex, src, dst, Vector2{0.0f, 0.0f}, 0.0f, Color{255, 255, 255, static_cast<unsigned char>(255 * alpha)});
        }
    }
    RenderActionUI(eng);
}


#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
void MotelWorld3D::StartTour(const std::string& outputDir) {
    if (outputDir == "@drive") {
        m_driveTest = true;
        m_driveFrames = 0;
        m_drivePhase = 0;
        return;
    }
    m_tourDir = outputDir;
    m_tour = {
        {"tour_01_start", {-3.7f, 0.0f, 12.2f}, 20.0f, 2.0f},
        {"tour_02_porch", {2.0f, 0.25f, 3.2f}, 0.0f, 0.0f},
        {"tour_03_lobby_dark", {1.0f, 0.25f, -1.2f}, 20.0f, -8.0f},
        {"tour_04_lobby_door_open", {2.0f, 0.25f, -2.2f}, 180.0f, -5.0f, false, "door_lobby"},
        {"tour_05_keys", {4.3f, 0.25f, -6.4f}, 0.0f, 0.0f},
        {"tour_06_corridor", {-4.6f, 0.25f, -7.0f}, -90.0f, 0.0f},
        {"tour_07_room101", {-6.0f, 0.25f, -4.6f}, -120.0f, -10.0f, false, "door_101"},
        {"tour_08_backyard", {-1.0f, 0.0f, -17.0f}, 0.0f, -5.0f},
        {"tour_09_power_lobby", {1.0f, 0.25f, -1.2f}, 20.0f, -8.0f, true},
        {"tour_10_power_corridor", {-4.6f, 0.25f, -7.0f}, -90.0f, 0.0f, true},
        {"tour_11_mirror", {-18.6f, 0.25f, -1.6f}, -90.0f, 0.0f, true, "door_bath102"},
        {"tour_12_room101_power", {-6.0f, 0.25f, -4.6f}, -120.0f, -10.0f, true},
        {"tour_13_no_flash", {-4.6f, 0.25f, -7.0f}, -90.0f, 0.0f, false, "", false},
        {"tour_14_car", {-3.7f, 0.05f, 15.5f}, 0.0f, -8.0f, true},
        {"tour_15_highway_start", {36.0f, 0.05f, 20.0f}, 90.0f, 0.0f, true},
    };
    const auto& road = m_level.Road();
    for (size_t k : {road.size() / 4, road.size() / 2, (road.size() * 3) / 4}) {
        if (k + 1 >= road.size()) continue;
        Vector3 a = road[k].position;
        Vector3 b = road[k + 1].position;
        float yaw = std::atan2(b.x - a.x, -(b.z - a.z)) * RAD2DEG;
        m_tour.push_back({"tour_16_road_" + std::to_string(k), Vector3Add(a, Vector3{0.0f, 0.05f, 0.0f}), yaw, -2.0f, true});
    }
    if (const MarkerObject* post = m_level.FindMarker("checkpoint_post")) {
        m_tour.push_back({"tour_17_post_inside", post->position, 180.0f, -12.0f, true});
        m_tour.push_back({"tour_18_post_outside", Vector3Add(post->position, Vector3{-14.0f, -0.05f, -11.0f}), 55.0f, 2.0f, true});
    }
    m_tourIndex = 0;
    m_tourFrames = 0;
}

void MotelWorld3D::UpdateTour(engine::core::Engine& eng) {
    if (m_tourIndex >= static_cast<int>(m_tour.size())) {
        eng.RequestQuit();
        return;
    }
    const TourStop& stop = m_tour[m_tourIndex];
    if (m_tourFrames == 0) {
        m_physics.TeleportCharacter(stop.feet);
        m_camera.SetLookAngles(stop.yawDeg * DEG2RAD, stop.pitchDeg * DEG2RAD);
        m_flashDir = m_camera.GetForward();
        m_power = stop.power;
        m_flashOn = stop.flashlight;
        if (!stop.openDoor.empty()) {
            int idx = m_level.FindDoorByName(stop.openDoor);
            if (idx >= 0) {
                UnlockDoor(stop.openDoor);
                m_level.Doors()[idx].open = true;
                m_physics.SetDoorTarget(m_level.Doors()[idx].body, m_level.Doors()[idx].openAngle);
            }
        }
    }
    ++m_tourFrames;
    if (m_tourFrames == stop.frames) {
        std::string file = stop.name + ".png";
        TakeScreenshot(file.c_str());
        Vector3 p = m_physics.GetCharacterPosition();
        TraceLog(LOG_INFO, "CH2 TOUR: %s feet=(%.2f %.2f %.2f) ground=%d", file.c_str(), p.x, p.y, p.z, m_physics.IsCharacterOnGround() ? 1 : 0);
    }
    if (m_tourFrames > stop.frames) {
        ++m_tourIndex;
        m_tourFrames = 0;
    }
}
#endif

}