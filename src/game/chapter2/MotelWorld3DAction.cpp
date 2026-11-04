#include "game/chapter2/MotelWorld3D.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace game::chapter2 {

using engine::render3d::DynamicLight;
using engine::render3d::SceneEnvironment;

namespace {

constexpr float UI_W = 1280.0f;
constexpr float UI_H = 720.0f;
constexpr float FOOT_CRUMB = 0.7f;
constexpr float CAR_CRUMB = 2.0f;
constexpr size_t TRAIL_MAX = 3000;
constexpr float STALKER_REACH = 0.95f;
constexpr float CAR_EYE_HEIGHT = 1.3f;
constexpr int MAG_SIZE = 8;
constexpr float DEATH_TIME = 4.2f;
constexpr float KO_FAKE_DEATH_TIME = 2.6f;
constexpr float KO_TURN_START = 1.1f;
constexpr float KO_TURN_END = 1.75f;
constexpr float KO_STRIKE = 2.05f;
constexpr float KO_BLACKOUT = 0.3f;
constexpr float KO_END = 3.4f;
constexpr const char* ENGINE_LOOP = "audio/sfx/sfx_car_engine_loop.wav";
constexpr const char* HEART_LOOP = "audio/sfx/sfx_heartbeat_dread.wav";
constexpr const char* SND_SHOT = "audio/sfx/sfx_pistol_shot.wav";
constexpr const char* SND_RELOAD = "audio/sfx/sfx_pistol_reload.wav";
constexpr const char* SND_DRY = "audio/sfx/sfx_flashlight_click.wav";
constexpr const char* SND_ROAR = "audio/screamers/sfx_sc_09_shadow_roar.wav";
constexpr const char* SND_SCREECH = "audio/screamers/sfx_sc_03_string_screech.wav";
constexpr const char* SND_BREATH = "audio/screamers/sfx_sc_12_chase_breath.wav";
constexpr const char* SND_VISCERAL = "audio/screamers/sfx_screamer_visceral.wav";
constexpr const char* SND_DEATH = "audio/screamers/sfx_sc_13_blood_curdle.wav";
constexpr const char* SND_STEPS = "audio/sfx/sfx_phantom_steps.wav";
constexpr const char* SND_HIT = "audio/screamers/sfx_sc_04_meat_drop.wav";
constexpr const char* TEX_CAUGHT = "sprites/screamers/sc_night12_corridor_rush.png";

float Rand01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

float FlatDistance(Vector3 a, Vector3 b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

float RaySphereT(Vector3 origin, Vector3 dir, Vector3 center, float radius) {
    Vector3 oc = Vector3Subtract(origin, center);
    float b = Vector3DotProduct(oc, dir);
    float c = Vector3DotProduct(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0f) return -1.0f;
    float t = -b - std::sqrt(h);
    return t >= 0.0f ? t : -1.0f;
}

Matrix BasisMatrix(Vector3 x, Vector3 y, Vector3 z, Vector3 pos) {
    Matrix m{};
    m.m0 = x.x; m.m1 = x.y; m.m2 = x.z;
    m.m4 = y.x; m.m5 = y.y; m.m6 = y.z;
    m.m8 = z.x; m.m9 = z.y; m.m10 = z.z;
    m.m12 = pos.x; m.m13 = pos.y; m.m14 = pos.z;
    m.m15 = 1.0f;
    return m;
}

void DrawLabel(Font font, const std::string& text, float x, float y, float size, Color color) {
    DrawTextEx(font, text.c_str(), Vector2{x, y}, size, 1.0f, color);
}

}


void MotelWorld3D::SetupAction() {
    m_mode = PlayerMode::OnFoot;
    m_carReady = false;
    m_carHealth = 100.0f;
    m_headlights = true;
    m_driveView = 0;
    m_driveCamInit = false;
    m_camOrbit = 0.0f;
    m_camLift = 0.0f;
    m_skidCooldown = 0.0f;
    m_crashCooldown = 0.0f;
    m_deadInCar = false;
    const VehicleObject& v = m_level.Vehicle();
    if (v.present) {
        m_car.Setup(v.spawnOrigin, v.spawnHeading, v.localMin, v.localMax, m_physics);
        m_carReady = true;
        m_carCheckpointPos = v.spawnOrigin;
        m_carCheckpointHeading = v.spawnHeading;
    }
    m_stalker = StalkerData{};
    m_trail.clear();
    m_trailBase = 0;
    m_ambushes.clear();
    for (int i = 1; i <= 9; ++i) {
        const MarkerObject* m = m_level.FindMarker("ambush_" + std::to_string(i));
        if (m == nullptr) continue;
        AmbushData a;
        a.pos = m->position;
        m_ambushes.push_back(a);
    }
    m_checkpoint.clear();
    m_deathTimer = 0.0f;
    m_ammoMag = 0;
    m_ammoReserve = 0;
    m_fireCooldown = m_reloadTimer = m_muzzleTimer = m_recoil = 0.0f;
    m_powerSwitchTimer = -1.0f;
    m_chapterComplete = false;
    m_completeTimer = -1.0f;
    m_damageFlash = 0.0f;
    m_extraLights.clear();
    m_treeVisual = &m_itemVisuals.Get("tree_pine", WHITE);
    const ItemDefinition* pistol = m_script.FindItem("pistol");
    m_pistolVisual = &m_itemVisuals.Get("pistol", pistol ? pistol->color : WHITE);
}

void MotelWorld3D::ShutdownAction() {
    if (m_eng == nullptr) return;
    m_eng->Audio().StopLoop("engine");
    m_eng->Audio().StopLoop("heart");
}

Vector3 MotelWorld3D::PlayerAnchor() const {
    bool inCar = m_mode == PlayerMode::Driving || (m_mode == PlayerMode::Dead && m_deadInCar);
    if (inCar && m_carReady) return m_car.GroundPosition();
    return m_physics.GetCharacterPosition();
}

const Camera3D& MotelWorld3D::ViewCamera() const {
    bool inCar = m_mode == PlayerMode::Driving || (m_mode == PlayerMode::Dead && m_deadInCar);
    return (inCar && m_driveCamInit) ? m_driveCam : m_camera.GetCamera();
}

Vector3 MotelWorld3D::ViewRight() const {
    const Camera3D& cam = ViewCamera();
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 right = Vector3CrossProduct(fwd, Vector3{0.0f, 1.0f, 0.0f});
    float len = Vector3Length(right);
    return len > 0.001f ? Vector3Scale(right, 1.0f / len) : Vector3{1.0f, 0.0f, 0.0f};
}

bool MotelWorld3D::HasPistol() const {
    return m_script.HasItem("pistol");
}


void MotelWorld3D::SetPowerDelayed(bool on, float delay) {
    m_powerSwitchTarget = on;
    m_powerSwitchTimer = std::max(0.01f, delay);
}

void MotelWorld3D::LockDoor(const std::string& name, const std::string& item, const std::string& lockedEvent,
                            const std::string& unlockEvent) {
    int idx = m_level.FindDoorByName(name);
    if (idx < 0) return;
    DoorObject& door = m_level.Doors()[idx];
    door.locked = true;
    door.lockItem = item;
    if (!lockedEvent.empty()) door.lockedEvent = lockedEvent;
    if (!unlockEvent.empty()) door.unlockEvent = unlockEvent;
    door.open = false;
    m_physics.SetDoorTarget(door.body, 0.0f);
    m_physics.SetDoorLimits(door.body, 0.0f, 0.0f);
    PlaySound(m_script.Sound("door_close", "audio/sfx/sfx_door_close_latch.wav"), 1.0f, door.restCenter);
    PlaySound("audio/sfx/sfx_heavy_knock.wav", 0.8f, door.restCenter);
}

void MotelWorld3D::SetStalkerMode(const std::string& mode) {
    if (mode == "hunt") {
        const MarkerObject* m = m_level.FindMarker("stalker_spawn_yard");
        SpawnStalker(m ? m->position : StalkerSpawnBehindPlayer(30), StalkerState::Emerging, 0.4f);
    } else if (mode == "final") {
        const MarkerObject* m = m_level.FindMarker("stalker_spawn_post");
        SpawnStalker(m ? m->position : StalkerSpawnBehindPlayer(40), StalkerState::Emerging, 1.2f);
        m_stalker.finalPhase = true;
    } else if (mode == "lurk") {
        m_stalker.state = StalkerState::Lurking;
    } else if (mode == "stop") {
        m_stalker.state = StalkerState::Inactive;
    }
}

void MotelWorld3D::SetCheckpoint(const std::string& name) {
    m_checkpoint = name;
    if (name == "car" && m_carReady) {
        m_carCheckpointPos = m_car.Position();
        m_carCheckpointHeading = m_car.Heading();
    }
}

void MotelWorld3D::EnterVehicle() {
    if (!m_carReady || m_mode != PlayerMode::OnFoot) return;
    m_mode = PlayerMode::Driving;
    m_hiddenSpot = -1;
    m_carriedProp = -1;
    m_inspectMode = false;
    m_driveCamInit = false;
    m_camOrbit = 0.0f;
    m_camLift = 0.0f;
    m_physics.TeleportCharacter(Vector3Subtract(m_car.GroundPosition(), Vector3{0.0f, 40.0f, 0.0f}));
    if (m_eng) {
        m_eng->Audio().PlaySfx("audio/sfx/sfx_door_close_latch.wav", 0.7f);
        if (m_carHealth > 0.0f) {
            m_eng->Audio().PlaySfx("audio/sfx/sfx_car_engine_start.wav", 0.9f);
            m_eng->Audio().SetLoop("engine", ENGINE_LOOP, 0.25f, 0.7f);
        }
    }
    m_script.SetFlag("car_started");
    UpdateDriveCamera(0.0f);
}

void MotelWorld3D::AddAmmo(int rounds) {
    int total = std::max(0, rounds);
    int toMag = std::min(total, MAG_SIZE - m_ammoMag);
    m_ammoMag += toMag;
    m_ammoReserve += total - toMag;
}

void MotelWorld3D::CompleteChapter(float delay) {
    m_completeTimer = std::max(0.1f, delay);
}

void MotelWorld3D::UpdateActionTimers(float dt) {
    if (m_powerSwitchTimer > 0.0f) {
        m_powerSwitchTimer -= dt;
        if (m_powerSwitchTimer <= 0.0f) {
            m_powerSwitchTimer = -1.0f;
            m_power = m_powerSwitchTarget;
            if (!m_power && m_eng) m_eng->Audio().PlaySfx("audio/sfx/sfx_fuse_insert.wav", 0.9f);
        }
    }
    if (m_completeTimer > 0.0f) {
        m_completeTimer -= dt;
        if (m_completeTimer <= 0.0f) m_chapterComplete = true;
    }
    m_damageFlash = std::max(0.0f, m_damageFlash - dt * 1.8f);
    m_muzzleTimer = std::max(0.0f, m_muzzleTimer - dt);
    m_recoil = std::max(0.0f, m_recoil - dt * 7.0f);
    m_stalker.hitFlash = std::max(0.0f, m_stalker.hitFlash - dt);

    m_extraLights.clear();
    if (m_carReady) {
        float hl = m_car.HalfLength();
        float off = m_car.GroundOffset();
        bool engineOn = m_mode == PlayerMode::Driving && m_carHealth > 0.0f;
        if (!m_script.HasFlag("car_started")) {
            if (std::fmod(m_time, 1.0f) < 0.5f) {
                for (float side : {-0.75f, 0.75f}) {
                    m_extraLights.push_back(DynamicLight{m_car.LocalToWorld(Vector3{-hl - 0.25f, 0.75f - off, side}), Vector3{1.3f, 0.7f, 0.13f}, 4.0f});
                }
            }
        } else if (engineOn) {
            bool braking = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_SPACE);
            float tail = braking ? 1.5f : 0.6f;
            m_extraLights.push_back(DynamicLight{m_car.LocalToWorld(Vector3{-hl - 0.3f, 0.7f - off, 0.0f}), Vector3{tail, 0.05f, 0.03f}, 3.2f});
            if (m_headlights) {
                m_extraLights.push_back(DynamicLight{m_car.LocalToWorld(Vector3{hl + 1.2f, 0.5f - off, 0.0f}), Vector3{0.55f, 0.5f, 0.42f}, 4.5f});
            }
        }
    }
    if (m_muzzleTimer > 0.0f) {
        const Camera3D& cam = m_camera.GetCamera();
        m_extraLights.push_back(DynamicLight{Vector3Add(cam.position, Vector3Scale(m_camera.GetForward(), 0.6f)), Vector3{3.2f, 2.4f, 1.4f}, 8.0f});
    }
}


void MotelWorld3D::UpdateHeadlights() {
    float hl = m_car.HalfLength();
    float off = m_car.GroundOffset();
    m_flash.position = m_car.LocalToWorld(Vector3{hl + 0.15f, 0.72f - off, 0.0f});
    m_flash.direction = Vector3Normalize(Vector3Subtract(m_car.Forward(), Vector3Scale(m_car.Up(), 0.08f)));
    m_flash.innerCos = std::cos(17.0f * DEG2RAD);
    m_flash.outerCos = std::cos(33.0f * DEG2RAD);
    m_flash.range = 42.0f;
    m_flash.intensity = 2.3f;
    m_flash.color = Vector3{1.0f, 0.93f, 0.78f};
    bool stutter = m_carHealth < 30.0f && std::fmod(m_time * 5.3f, 1.0f) < 0.1f;
    m_flash.enabled = m_headlights && m_carHealth > 0.0f && !stutter;
    m_flashFocus = false;
}

void MotelWorld3D::UpdateDriveCamera(float dt) {
    Vector3 ground = m_car.GroundPosition();
    float h = m_car.Heading();
    Vector3 flatFwd{std::cos(h), 0.0f, std::sin(h)};
    m_driveCam.projection = CAMERA_PERSPECTIVE;
    float speed = m_car.Speed();
    if (m_driveView == 1) {
        float off = m_car.GroundOffset();
        Vector3 eye = m_car.LocalToWorld(Vector3{-0.2f, CAR_EYE_HEIGHT - off, -0.42f});
        float yaw = m_camOrbit;
        Vector3 dir = Vector3Add(Vector3Scale(m_car.Forward(), std::cos(yaw)), Vector3Scale(m_car.Right(), std::sin(yaw)));
        dir = Vector3Normalize(Vector3Add(dir, Vector3Scale(m_car.Up(), m_camLift - 0.06f)));
        m_driveCam.position = eye;
        m_driveCam.target = Vector3Add(eye, dir);
        m_driveCam.up = m_car.Up();
        m_driveCam.fovy = 74.0f;
        m_driveCamInit = true;
        return;
    }
    float yaw = h + m_camOrbit;
    Vector3 back{-std::cos(yaw), 0.0f, -std::sin(yaw)};
    Vector3 desired = Vector3Add(Vector3Add(ground, Vector3Scale(back, 6.4f)), Vector3{0.0f, 2.35f + m_camLift * 3.0f, 0.0f});
    Vector3 focus = Vector3Add(Vector3Add(ground, Vector3Scale(flatFwd, 2.0f)), Vector3{0.0f, 1.0f, 0.0f});
    if (!m_driveCamInit || dt <= 0.0f) {
        m_driveCam.position = desired;
    } else {
        float k = 1.0f - std::exp(-dt * 7.0f);
        m_driveCam.position = Vector3Lerp(m_driveCam.position, desired, k);
    }
    Vector3 toCam = Vector3Subtract(m_driveCam.position, focus);
    float len = Vector3Length(toCam);
    if (len > 0.01f) {
        Vector3 dir = Vector3Scale(toCam, 1.0f / len);
        float hit = m_physics.CastStaticDistance(focus, dir, len);
        if (hit < len) m_driveCam.position = Vector3Add(focus, Vector3Scale(dir, std::max(0.6f, hit - 0.3f)));
    }
    m_driveCam.target = focus;
    m_driveCam.up = Vector3{0.0f, 1.0f, 0.0f};
    m_driveCam.fovy = std::min(84.0f, 66.0f + speed * 0.4f);
    m_driveCamInit = true;
}

void MotelWorld3D::UpdateDriving(float dt) {
    VehicleInput in;
    in.throttle = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) ? 1.0f : 0.0f;
    in.brake = (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) ? 1.0f : 0.0f;
    in.steer = ((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) ? 1.0f : 0.0f) - ((IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) ? 1.0f : 0.0f);
    in.handbrake = IsKeyDown(KEY_SPACE);
    in.engineAlive = m_carHealth > 0.0f;
#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    if (m_autoActive) {
        in = m_autoInput;
        in.engineAlive = m_carHealth > 0.0f;
    }
#endif

    if (IsKeyPressed(KEY_V)) {
        m_driveView = 1 - m_driveView;
        m_camOrbit = 0.0f;
        m_camLift = 0.0f;
        m_driveCamInit = false;
    }
    if (IsKeyPressed(KEY_F)) {
        m_headlights = !m_headlights;
        m_eng->Audio().PlaySfx("audio/sfx/sfx_flashlight_click.wav", 0.6f);
    }
    if (IsKeyPressed(KEY_E) && ExitVehicle()) return;

    Vector2 md = GetMouseDelta();
    m_camOrbit = std::clamp(m_camOrbit + md.x * 0.004f, m_driveView == 1 ? -1.4f : -PI, m_driveView == 1 ? 1.4f : PI);
    m_camLift = std::clamp(m_camLift - md.y * 0.003f, -0.3f, 0.6f);
    if (std::abs(md.x) < 0.5f && std::abs(md.y) < 0.5f && m_car.Speed() > 3.0f) {
        float k = std::exp(-dt * 1.4f);
        m_camOrbit *= k;
        m_camLift *= k;
    }

    m_car.Update(dt, in, m_physics);
    float impact = m_car.TakeImpact();
    m_crashCooldown -= dt;
    m_skidCooldown -= dt;
    if (impact > 4.5f && m_crashCooldown <= 0.0f) {
        PlaySound("audio/sfx/sfx_car_crash.wav", std::clamp(impact / 14.0f, 0.3f, 1.0f), std::nullopt);
        Shake(std::min(10.0f, impact * 0.55f), 0.45f);
        m_crashCooldown = 0.5f;
        if (impact > 9.0f) DamageCar((impact - 9.0f) * 2.5f);
    }
    if (m_car.IsSkidding() && m_car.Speed() > 5.0f && m_skidCooldown <= 0.0f) {
        PlaySound("audio/sfx/sfx_car_skid.wav", 0.55f, std::nullopt);
        m_skidCooldown = 1.1f;
    }
    if (m_carHealth > 0.0f) {
        float pitch = 0.62f + 0.9f * m_car.Rpm() + 0.12f * m_car.EngineLoad();
        m_eng->Audio().SetLoop("engine", ENGINE_LOOP, 0.2f + 0.22f * m_car.EngineLoad(), pitch);
    } else {
        m_eng->Audio().StopLoop("engine");
    }

    UpdateHeadlights();
    UpdateDriveCamera(dt);
    UpdateTrail();
    UpdateTriggers();
    UpdateStalker(dt);
    UpdateAmbushes(dt);
    UpdateRain(dt);
    UpdateSanity(dt);
}

bool MotelWorld3D::ExitVehicle() {
    if (m_car.Speed() > 1.5f) {
        ShowMessage("Сначала остановите машину.", 1.5f);
        return false;
    }
    float hw = m_car.HalfWidth();
    float hl = m_car.HalfLength();
    const Vector3 candidates[4] = {
        {-0.2f, 0.0f, -(hw + 0.85f)}, {-0.2f, 0.0f, hw + 0.85f}, {-(hl + 1.0f), 0.0f, 0.0f}, {hl + 1.0f, 0.0f, 0.0f},
    };
    Vector3 center = Vector3Add(m_car.GroundPosition(), Vector3{0.0f, 1.0f, 0.0f});
    for (const Vector3& local : candidates) {
        Vector3 w = m_car.LocalToWorld(local);
        Vector3 spot{w.x, m_car.GroundPosition().y + 1.0f, w.z};
        if (m_physics.IsStaticOccluded(center, spot)) continue;
        float down = m_physics.CastStaticDistance(spot, Vector3{0.0f, -1.0f, 0.0f}, 3.0f);
        if (down >= 2.99f) continue;
        m_physics.TeleportCharacter(Vector3{spot.x, spot.y - down + 0.05f, spot.z});
        m_camera.SetLookAngles(m_car.Heading() + PI * 0.5f, 0.0f);
        m_flashDir = m_camera.GetForward();
        m_moveVelocity = Vector3{0.0f, 0.0f, 0.0f};
        m_camera.Update(0.0f, m_physics.GetCharacterPosition(), engine::camera3d::CameraMotion{});
        m_mode = PlayerMode::OnFoot;
        m_eng->Audio().StopLoop("engine");
        m_eng->Audio().PlaySfx("audio/sfx/sfx_door_close_latch.wav", 0.8f);
        return true;
    }
    ShowMessage("Дверь не открыть — снаружи мешает препятствие.", 2.0f);
    return false;
}

void MotelWorld3D::DamageCar(float amount) {
    if (m_carHealth <= 0.0f) return;
    m_carHealth = std::max(0.0f, m_carHealth - amount);
    m_damageFlash = std::max(m_damageFlash, 0.5f);
    if (m_carHealth <= 0.0f) {
        m_eng->Audio().StopLoop("engine");
        m_eng->Audio().PlaySfx("audio/sfx/sfx_car_starter_dead.wav", 0.9f);
        ShowMessage("Мотор заглох! Машина не заводится... ОНО приближается!", 4.0f);
    }
}

void MotelWorld3D::UpdateAmbushes(float dt) {
    if (m_mode != PlayerMode::Driving) return;
    Vector3 car = m_car.GroundPosition();
    Vector3 fwd = m_car.Forward();
    for (AmbushData& a : m_ambushes) {
        if (a.done) continue;
        Vector3 to = Vector3Subtract(a.pos, car);
        float d = FlatDistance(a.pos, car);
        if (!a.active) {
            if (d < 55.0f && Vector3DotProduct(fwd, to) > 0.0f) {
                a.active = true;
                a.timer = 0.0f;
                PlaySound(SND_BREATH, 1.0f, a.pos);
            }
            continue;
        }
        a.timer += dt;
        if (d < m_car.HalfWidth() + 1.1f) {
            a.done = true;
            float side = Vector3DotProduct(to, m_car.Right()) > 0.0f ? -1.0f : 1.0f;
            m_car.Kick(Vector3Scale(m_car.Right(), side * 4.5f), side * 0.9f);
            DamageCar(22.0f);
            TriggerScreamer(TEX_CAUGHT, 0.7f, 9.0f);
            PlaySound(SND_VISCERAL, 1.0f, std::nullopt);
            AddSanity(-10.0f);
        } else if (Vector3DotProduct(fwd, to) < -3.0f || a.timer > 14.0f) {
            a.done = true;
            PlaySound(SND_SCREECH, 0.7f, a.pos);
        }
    }
}


void MotelWorld3D::UpdateTrail() {
    if (m_mode == PlayerMode::Dead) return;
    Vector3 p = PlayerAnchor();
    float spacing = m_mode == PlayerMode::Driving ? CAR_CRUMB : FOOT_CRUMB;
    if (m_trail.empty() || FlatDistance(m_trail.back(), p) >= spacing) {
        m_trail.push_back(p);
        if (m_trail.size() > TRAIL_MAX) {
            m_trail.pop_front();
            ++m_trailBase;
        }
    }
}

Vector3 MotelWorld3D::StalkerSpawnBehindPlayer(int crumbsBack) const {
    if (m_trail.empty()) return Vector3Add(PlayerAnchor(), Vector3{0.0f, 0.0f, -12.0f});
    size_t idx = m_trail.size() > static_cast<size_t>(crumbsBack) ? m_trail.size() - 1 - static_cast<size_t>(crumbsBack) : 0;
    return m_trail[idx];
}

void MotelWorld3D::SpawnStalker(Vector3 pos, StalkerState state, float delay) {
    m_stalker = StalkerData{};
    m_stalker.pos = pos;
    m_stalker.state = state;
    m_stalker.delay = delay;
    m_stalker.growlTimer = 2.0f;
    m_stalker.lungeTimer = 4.0f;
    m_stalker.follow = m_trailBase + static_cast<long long>(m_trail.size()) - 1;
    float best = 1e9f;
    size_t start = m_trail.size() > 900 ? m_trail.size() - 900 : 0;
    for (size_t i = start; i < m_trail.size(); ++i) {
        float d = FlatDistance(m_trail[i], pos);
        if (d < best) {
            best = d;
            m_stalker.follow = m_trailBase + static_cast<long long>(i);
        }
    }
}

bool MotelWorld3D::MoveStalker(Vector3 target, float speed, float dt) {
    Vector3 delta{target.x - m_stalker.pos.x, 0.0f, target.z - m_stalker.pos.z};
    float dist = Vector3Length(delta);
    float step = speed * dt;
    if (dist <= step || dist < 0.001f) {
        m_stalker.pos.x = target.x;
        m_stalker.pos.z = target.z;
    } else {
        m_stalker.pos = Vector3Add(m_stalker.pos, Vector3Scale(delta, step / dist));
    }
    Vector3 probe = Vector3Add(m_stalker.pos, Vector3{0.0f, 1.3f, 0.0f});
    float down = m_physics.CastStaticDistance(probe, Vector3{0.0f, -1.0f, 0.0f}, 4.0f);
    if (down < 3.99f) {
        float groundY = probe.y - down;
        m_stalker.pos.y += (groundY - m_stalker.pos.y) * std::min(1.0f, dt * 10.0f);
    }
    return dist <= step;
}

void MotelWorld3D::UpdateStalker(float dt) {
    StalkerData& st = m_stalker;
    if (st.state == StalkerState::Inactive || st.state == StalkerState::Dead || st.state == StalkerState::Lurking ||
        st.state == StalkerState::Ambush) {
        UpdateStalkerAudio(dt);
        return;
    }
    if (st.delay > 0.0f) {
        st.delay -= dt;
        UpdateStalkerAudio(dt);
        return;
    }
    st.timer += dt;
    st.attackCooldown -= dt;
    st.stagger -= dt;
    st.losCheck -= dt;

    if (st.state == StalkerState::Emerging) {
        if (st.timer > 2.2f) {
            st.state = st.finalPhase ? StalkerState::Final : StalkerState::Hunting;
            st.timer = 0.0f;
            PlaySound(SND_ROAR, 1.0f, st.pos);
        }
        UpdateStalkerAudio(dt);
        return;
    }
    if (st.state == StalkerState::Dying) {
        if (st.timer > KO_FAKE_DEATH_TIME) {
            if (m_mode == PlayerMode::OnFoot) {
                StartKnockout();
            } else {
                st.state = StalkerState::Dead;
                m_eng->Audio().StopLoop("heart");
                m_script.RunEvent("stalker_killed", *this);
            }
        }
        return;
    }

    long long latest = m_trailBase + static_cast<long long>(m_trail.size()) - 1;
    auto crumb = [&](long long g) { return m_trail[static_cast<size_t>(std::clamp(g - m_trailBase, 0LL, static_cast<long long>(m_trail.size()) - 1))]; };
    Vector3 player = PlayerAnchor();

    if (m_mode == PlayerMode::Driving && !st.finalPhase) {
        if (!m_trail.empty()) {
            long long gap = latest - st.follow;
            if (static_cast<float>(gap) * CAR_CRUMB > 50.0f) {
                st.follow = latest - 21;
                st.pos = crumb(st.follow);
            }
            Vector3 target = st.follow >= latest ? player : crumb(st.follow);
            if (FlatDistance(st.pos, target) < 1.4f && st.follow < latest) ++st.follow;
            MoveStalker(target, 12.5f, dt);
        } else {
            MoveStalker(player, 12.5f, dt);
        }
        float reach = m_car.HalfLength() + 1.1f;
        if (FlatDistance(st.pos, player) < reach && st.attackCooldown <= 0.0f) {
            st.attackCooldown = 2.6f;
            if (m_carHealth <= 0.0f) {
                KillPlayer();
                return;
            }
            Vector3 push = Vector3Normalize(Vector3Subtract(player, st.pos));
            m_car.Kick(Vector3Scale(push, 5.0f), (Rand01() - 0.5f) * 1.6f);
            DamageCar(30.0f);
            Shake(9.0f, 0.6f);
            PlaySound(SND_SCREECH, 1.0f, st.pos);
            PlaySound("audio/sfx/sfx_chase_debris_hit.wav", 1.0f, std::nullopt);
            m_screamerRedFlash = 1.0f;
            AddSanity(-8.0f);
            st.follow = std::max(m_trailBase, st.follow - 5);
            if (!m_trail.empty()) st.pos = crumb(st.follow);
        }
        UpdateStalkerAudio(dt);
        return;
    }

    Vector3 eyeSelf = Vector3Add(st.pos, Vector3{0.0f, 1.5f, 0.0f});
    Vector3 playerEye = m_camera.GetCamera().position;
    bool hidden = m_hiddenSpot >= 0;
    if (st.losCheck <= 0.0f) {
        st.losCheck = 0.15f;
        st.sees = !hidden && m_mode == PlayerMode::OnFoot && !m_physics.IsStaticOccluded(eyeSelf, playerEye);
        if (!st.sees && !m_trail.empty()) {
            for (long long k = std::min(latest, st.follow + 8); k > st.follow; --k) {
                if (!m_physics.IsStaticOccluded(eyeSelf, Vector3Add(crumb(k), Vector3{0.0f, 1.3f, 0.0f}))) {
                    st.follow = k;
                    break;
                }
            }
        }
    }
    float distToPlayer = FlatDistance(st.pos, player);

    if (st.state == StalkerState::Searching) {
        if (!hidden) {
            st.state = st.finalPhase ? StalkerState::Final : StalkerState::Hunting;
            st.follow = latest;
        } else if (distToPlayer < 2.2f && !m_holdingBreath && Rand01() < dt * 0.9f) {
            KillPlayer();
            return;
        } else if (st.timer > 7.5f) {
            SpawnStalker(StalkerSpawnBehindPlayer(45), st.finalPhase ? StalkerState::Final : StalkerState::Hunting, 4.0f);
            m_stalker.finalPhase = st.finalPhase;
            PlaySound(SND_STEPS, 0.6f, std::nullopt);
            return;
        } else {
            Vector3 wander = Vector3Add(crumb(latest), Vector3{std::sin(st.timer * 0.9f) * 1.4f, 0.0f, std::cos(st.timer * 0.7f) * 1.4f});
            MoveStalker(wander, 0.9f, dt);
        }
        UpdateStalkerAudio(dt);
        return;
    }

    Vector3 target = player;
    if (!st.sees && !m_trail.empty()) {
        if (st.follow > latest) st.follow = latest;
        target = crumb(st.follow);
        if (FlatDistance(st.pos, target) < 0.6f && st.follow < latest) ++st.follow;
        if (hidden && st.follow >= latest && FlatDistance(st.pos, target) < 0.9f) {
            st.state = StalkerState::Searching;
            st.timer = 0.0f;
            PlaySound(SND_BREATH, 0.9f, st.pos);
            UpdateStalkerAudio(dt);
            return;
        }
    } else if (st.sees) {
        st.follow = latest;
    }

    float speed;
    if (st.finalPhase) {
        st.lungeTimer -= dt;
        st.lunge -= dt;
        if (st.lungeTimer <= 0.0f && st.sees && distToPlayer < 15.0f) {
            st.lunge = 0.9f;
            st.lungeTimer = 3.5f + Rand01() * 3.0f;
            PlaySound(SND_ROAR, 0.9f, st.pos);
        }
        speed = st.lunge > 0.0f ? 6.0f : 2.9f;
    } else {
        speed = st.timer < 3.0f ? 2.4f : ((st.sees && distToPlayer > 10.0f) ? 4.0f : 3.3f);
    }
    if (st.stagger > 0.0f) speed = 0.0f;
    MoveStalker(target, speed, dt);

    if (m_mode == PlayerMode::OnFoot && !hidden && distToPlayer < STALKER_REACH && std::abs(st.pos.y - player.y) < 1.6f) {
        KillPlayer();
        return;
    }
    if (m_mode == PlayerMode::Driving && st.finalPhase && distToPlayer < m_car.HalfLength() + 1.0f) {
        KillPlayer();
        return;
    }
    UpdateStalkerAudio(dt);
}

void MotelWorld3D::UpdateStalkerAudio(float dt) {
    StalkerData& st = m_stalker;
    bool active = (st.state == StalkerState::Hunting || st.state == StalkerState::Final || st.state == StalkerState::Searching ||
                   st.state == StalkerState::Emerging) && st.delay <= 0.0f && m_mode != PlayerMode::Dead;
    if (!active) {
        m_eng->Audio().StopLoop("heart");
        return;
    }
    float d = FlatDistance(st.pos, PlayerAnchor());
    float closeness = std::clamp(1.0f - d / 26.0f, 0.0f, 1.0f);
    m_eng->Audio().SetLoop("heart", HEART_LOOP, 0.15f + 0.75f * closeness, 0.9f + 0.45f * closeness);
    st.stepTimer -= dt;
    if (st.stepTimer <= 0.0f && d < 28.0f && st.state != StalkerState::Emerging) {
        st.stepTimer = m_mode == PlayerMode::Driving ? 0.5f : 0.9f;
        PlaySound(SND_STEPS, 0.55f * (0.3f + closeness), st.pos);
    }
    st.growlTimer -= dt;
    if (st.growlTimer <= 0.0f) {
        st.growlTimer = 5.0f + Rand01() * 6.0f;
        PlaySound(SND_BREATH, 0.5f + 0.5f * closeness, st.pos);
    }
}


void MotelWorld3D::UpdatePistol(float dt) {
    if (!HasPistol() || m_mode != PlayerMode::OnFoot) return;
    m_fireCooldown -= dt;
    if (m_reloadTimer > 0.0f) {
        m_reloadTimer -= dt;
        if (m_reloadTimer <= 0.0f) {
            int take = std::min(MAG_SIZE - m_ammoMag, m_ammoReserve);
            m_ammoMag += take;
            m_ammoReserve -= take;
        }
        return;
    }
    if (IsKeyPressed(KEY_R) && m_ammoMag < MAG_SIZE && m_ammoReserve > 0) {
        m_reloadTimer = 1.3f;
        m_eng->Audio().PlaySfx(SND_RELOAD, 0.85f);
        return;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m_carriedProp < 0 && m_hiddenSpot < 0 && m_fireCooldown <= 0.0f) {
        if (m_ammoMag <= 0) {
            m_eng->Audio().PlaySfx(SND_DRY, 0.8f);
            m_fireCooldown = 0.25f;
            if (m_ammoReserve > 0) {
                m_reloadTimer = 1.3f;
                m_eng->Audio().PlaySfx(SND_RELOAD, 0.85f);
            } else {
                ShowMessage("Патроны кончились! Коробка с патронами лежала на столе в будке.", 2.5f);
            }
            return;
        }
        FirePistol();
    }
}

void MotelWorld3D::FirePistol() {
    --m_ammoMag;
    m_fireCooldown = 0.24f;
    m_muzzleTimer = 0.06f;
    m_recoil = 1.0f;
    m_eng->Audio().PlaySfx(SND_SHOT, 1.0f);
    Shake(1.8f, 0.12f);
    const Camera3D& cam = m_camera.GetCamera();
    Vector3 origin = cam.position;
    Vector3 dir = m_camera.GetForward();
    m_camera.SetLookAngles(m_camera.GetYaw() + (Rand01() - 0.5f) * 0.01f, m_camera.GetPitch() + 0.035f);

    StalkerData& st = m_stalker;
    bool targetable = st.delay <= 0.0f && (st.state == StalkerState::Hunting || st.state == StalkerState::Final ||
                                            st.state == StalkerState::Emerging || st.state == StalkerState::Searching);
    float wall = m_physics.CastStaticDistance(origin, dir, 80.0f);
    if (targetable) {
        float tHead = RaySphereT(origin, dir, Vector3Add(st.pos, Vector3{0.0f, 1.95f, 0.0f}), 0.27f);
        float tBody = RaySphereT(origin, dir, Vector3Add(st.pos, Vector3{0.0f, 1.15f, 0.0f}), 0.48f);
        bool head = tHead >= 0.0f && (tBody < 0.0f || tHead <= tBody);
        float t = head ? tHead : tBody;
        if (t >= 0.0f && t < wall + 0.05f) {
            float damage = head ? 38.0f : 21.0f;
            st.health -= damage;
            st.stagger = 0.45f;
            st.hitFlash = 0.25f;
            Vector3 push = Vector3Normalize(Vector3{dir.x, 0.0f, dir.z});
            st.pos = Vector3Add(st.pos, Vector3Scale(push, head ? 1.3f : 0.9f));
            PlaySound(SND_HIT, 0.9f, st.pos);
            if (st.health <= 0.0f) {
                st.state = StalkerState::Dying;
                st.timer = 0.0f;
                PlaySound(SND_DEATH, 1.0f, std::nullopt);
                PlaySound(SND_VISCERAL, 0.8f, std::nullopt);
                Shake(6.0f, 1.2f);
                ShowMessage("ОНО рассыпается чёрным пеплом... Тишина.", 3.0f);
            } else {
                PlaySound(SND_SCREECH, 0.85f, st.pos);
                if (head) ShowMessage("В голову! ОНО шатается!", 1.4f);
            }
            return;
        }
    }
    if (wall < 79.0f) PlaySound("audio/sfx/sfx_chase_debris_hit.wav", 0.35f, Vector3Add(origin, Vector3Scale(dir, wall)));
}


void MotelWorld3D::KillPlayer() {
    if (m_mode == PlayerMode::Dead) return;
    m_deadInCar = m_mode == PlayerMode::Driving;
    m_mode = PlayerMode::Dead;
    m_deathTimer = 0.0f;
    m_hiddenSpot = -1;
    m_carriedProp = -1;
    TriggerScreamer(TEX_CAUGHT, 1.1f, 11.0f);
    PlaySound(SND_VISCERAL, 1.0f, std::nullopt);
    PlaySound(SND_ROAR, 1.0f, std::nullopt);
    m_eng->Audio().StopLoop("engine");
    m_eng->Audio().StopLoop("heart");
    AddSanity(-20.0f);
}

void MotelWorld3D::StartKnockout() {
    StalkerData& st = m_stalker;
    st.state = StalkerState::Ambush;
    st.timer = 0.0f;
    st.hitFlash = 0.0f;
    m_knockoutTime = 0.0f;
    m_knockoutBlack = 0.0f;
    m_hiddenSpot = -1;
    m_carriedProp = -1;
    m_reloadTimer = 0.0f;
    m_focusKind = FocusKind::None;
    m_focusLabel.clear();
    m_moveVelocity = Vector3{0.0f, 0.0f, 0.0f};

    Vector3 feet = m_physics.GetCharacterPosition();
    Vector3 back = Vector3Negate(m_camera.GetFlatForward());
    float room = m_physics.CastStaticDistance(Vector3Add(feet, Vector3{0.0f, 1.2f, 0.0f}), back, 1.6f);
    float dist = std::clamp(room - 0.35f, 0.6f, 1.25f);
    st.pos = Vector3Add(feet, Vector3Scale(back, dist));

    m_knockoutYawFrom = m_camera.GetYaw();
    m_knockoutPitchFrom = m_camera.GetPitch();
    m_knockoutYawTo = std::atan2(back.x, -back.z);
    m_eng->Audio().StopLoop("heart");
    PlaySound(SND_BREATH, 1.0f, st.pos);
}

void MotelWorld3D::UpdateKnockout(float dt) {
    float prev = m_knockoutTime;
    m_knockoutTime += dt;
    float t = m_knockoutTime;
    auto crossed = [&](float mark) { return prev < mark && t >= mark; };
    StalkerData& st = m_stalker;
    st.timer = t;

    float yaw = m_knockoutYawFrom;
    float pitch = m_knockoutPitchFrom;
    if (t >= KO_TURN_START) {
        float k = std::clamp((t - KO_TURN_START) / (KO_TURN_END - KO_TURN_START), 0.0f, 1.0f);
        k = k * k * (3.0f - 2.0f * k);
        float delta = std::remainder(m_knockoutYawTo - m_knockoutYawFrom, 2.0f * PI);
        yaw = m_knockoutYawFrom + delta * k;
        pitch = m_knockoutPitchFrom + (0.12f - m_knockoutPitchFrom) * k;
    }
    if (crossed(KO_TURN_START)) {
        PlaySound("audio/sfx/sfx_sting_dread.wav", 0.9f, std::nullopt);
        Shake(2.0f, 0.5f);
    }
    if (t >= KO_TURN_END && t < KO_STRIKE) {
        Vector3 player = m_physics.GetCharacterPosition();
        Vector3 toPlayer = Vector3Subtract(player, st.pos);
        toPlayer.y = 0.0f;
        float d = Vector3Length(toPlayer);
        if (d > 0.55f) st.pos = Vector3Add(st.pos, Vector3Scale(toPlayer, std::min(d - 0.55f, dt * 2.5f) / d));
    }
    if (crossed(KO_TURN_END)) PlaySound(SND_ROAR, 1.0f, st.pos);
    if (crossed(KO_STRIKE)) {
        PlaySound(SND_HIT, 1.0f, std::nullopt);
        PlaySound(SND_VISCERAL, 1.0f, std::nullopt);
        TriggerScreamer(TEX_CAUGHT, 0.35f, 14.0f);
        Shake(14.0f, 0.7f);
        m_flashOn = false;
    }
    if (t >= KO_STRIKE) {
        float fall = std::clamp((t - KO_STRIKE) / 0.5f, 0.0f, 1.0f);
        pitch = 0.12f + (-1.1f - 0.12f) * fall;
        yaw += 0.35f * fall;
        m_knockoutBlack = std::clamp((t - KO_STRIKE) / KO_BLACKOUT, 0.0f, 1.0f);
    }
    if (crossed(KO_END)) {
        st.state = StalkerState::Dead;
        m_script.RunEvent("stalker_killed", *this);
    }

    m_camera.SetLookAngles(yaw, pitch);
    UpdateFlashlight(dt);
    m_physics.UpdateCharacter(dt, Vector3{0.0f, 0.0f, 0.0f}, false, INVALID_BODY);
    m_camera.Update(dt, m_physics.GetCharacterPosition(), engine::camera3d::CameraMotion{});
}

void MotelWorld3D::UpdateDeath(float dt) {
    m_deathTimer += dt;
    if (m_deathTimer >= DEATH_TIME) Respawn();
}

void MotelWorld3D::Respawn() {
    m_mode = PlayerMode::OnFoot;
    m_deathTimer = 0.0f;
    m_deadInCar = false;
    m_sanity = std::max(m_sanity, 65.0f);
    m_battery = std::max(m_battery, 55.0f);
    m_stamina = 100.0f;
    m_flashOn = true;
    m_screamerActive = false;
    m_screamerTimer = 0.0f;
    m_knockoutTime = -1.0f;
    m_knockoutBlack = 0.0f;
    m_trail.clear();
    m_trailBase = 0;
    m_moveVelocity = Vector3{0.0f, 0.0f, 0.0f};

    auto placePlayer = [&](const MarkerObject* m) {
        Vector3 pos = m ? m->position : m_level.PlayerStart();
        float yaw = m ? m->yaw : m_level.PlayerStartYaw();
        m_physics.TeleportCharacter(Vector3Add(pos, Vector3{0.0f, 0.05f, 0.0f}));
        m_camera.SetLookAngles(yaw, 0.0f);
        m_flashDir = m_camera.GetForward();
        m_camera.Update(0.0f, m_physics.GetCharacterPosition(), engine::camera3d::CameraMotion{});
        UpdateTrail();
    };

    if (m_checkpoint == "post") {
        placePlayer(m_level.FindMarker("checkpoint_post"));
        m_ammoMag = MAG_SIZE;
        m_ammoReserve = std::max(m_ammoReserve, 16);
        m_reloadTimer = 0.0f;
        const MarkerObject* s = m_level.FindMarker("stalker_spawn_post");
        SpawnStalker(s ? s->position : StalkerSpawnBehindPlayer(40), StalkerState::Emerging, 2.5f);
        m_stalker.finalPhase = true;
        ShowMessage("Вы снова в будке поста. Магазин полон. ОНО возвращается...", 4.0f);
    } else if (m_checkpoint == "car" && m_carReady) {
        m_car.Reset(m_carCheckpointPos, m_carCheckpointHeading, m_physics);
        m_physics.TeleportBody(m_level.Vehicle().body, m_car.Transform());
        m_carHealth = 100.0f;
        for (AmbushData& a : m_ambushes) a = AmbushData{a.pos};
        EnterVehicle();
        UpdateTrail();
        const MarkerObject* s = m_level.FindMarker("stalker_spawn_porch");
        SpawnStalker(s ? s->position : StalkerSpawnBehindPlayer(10), StalkerState::Hunting, 2.5f);
        ShowMessage("Вы снова за рулём. Жмите газ — ОНО уже на крыльце!", 4.0f);
    } else if (m_checkpoint == "generator") {
        placePlayer(m_level.FindMarker("checkpoint_generator"));
        const MarkerObject* s = m_level.FindMarker("stalker_spawn_yard");
        SpawnStalker(s ? s->position : StalkerSpawnBehindPlayer(30), StalkerState::Emerging, 2.0f);
        ShowMessage("Вы снова у генератора. Бегите в холл — ключ на доске, ключи от машины в сейфе!", 4.5f);
    } else {
        placePlayer(nullptr);
        m_stalker.state = StalkerState::Inactive;
    }
}


void MotelWorld3D::DrawVehicle(const Shader* overrideShader) {
    if (!m_carReady) return;
    Matrix t = m_car.Transform();
    if (overrideShader == nullptr) m_lighting.SetObjectParams(1.5f, Vector3{0.03f, 0.03f, 0.035f}, 0.0f);
    for (const RenderBatch& b : m_level.Vehicle().batches) {
        Material m = b.material;
        if (overrideShader != nullptr) m.shader = *overrideShader;
        DrawMesh(b.mesh, m, t);
    }
    if (overrideShader == nullptr) m_lighting.SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, 0.0f);
}

void MotelWorld3D::DrawTrees(const Shader* overrideShader, Vector3 viewPos) {
    if (m_treeVisual == nullptr || m_level.Trees().empty()) return;
    float maxD = BuildEnvironment().fogEnd + 8.0f;
    float maxD2 = maxD * maxD;
    if (overrideShader == nullptr) m_lighting.SetObjectParams(0.0f, Vector3{0.045f, 0.055f, 0.075f}, 0.0f);
    for (const TreeObject& tree : m_level.Trees()) {
        float dx = tree.position.x - viewPos.x;
        float dz = tree.position.z - viewPos.z;
        if (dx * dx + dz * dz > maxD2) continue;
        Matrix world = MatrixMultiply(MatrixMultiply(MatrixScale(tree.scale, tree.scale, tree.scale), MatrixRotateY(tree.yaw)),
                                      MatrixTranslate(tree.position.x, tree.position.y, tree.position.z));
        m_itemVisuals.Draw(*m_treeVisual, world, overrideShader);
    }
    if (overrideShader == nullptr) m_lighting.SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, 0.0f);
}

void MotelWorld3D::DrawStalker(const Camera3D& cam) const {
    if (m_phantomTex.id == 0) return;
    SceneEnvironment env = BuildEnvironment();
    auto drawFigure = [&](Vector3 feet, float scale, float rise, float alpha, bool hurt) {
        float d = Vector3Distance(cam.position, feet);
        float fog = std::clamp(1.0f - (d - env.fogStart) / std::max(1.0f, env.fogEnd - env.fogStart), 0.08f, 1.0f);
        float jitter = std::sin(m_time * 55.0f) > 0.3f ? 0.03f : -0.03f;
        Vector2 size{1.25f * scale, 2.5f * scale * (0.3f + 0.7f * rise)};
        Vector3 center{feet.x + jitter, feet.y + size.y * 0.5f - (1.0f - rise) * 0.2f, feet.z};
        unsigned char a = static_cast<unsigned char>(245.0f * alpha * (0.35f + 0.65f * fog));
        unsigned char shade = static_cast<unsigned char>(60.0f + 190.0f * fog);
        Color tint = hurt ? Color{255, 70, 70, a} : Color{shade, static_cast<unsigned char>(shade * 0.9f), static_cast<unsigned char>(shade * 0.9f), a};
        Rectangle src{0.0f, 0.0f, static_cast<float>(m_phantomTex.width), static_cast<float>(m_phantomTex.height)};
        rlDrawRenderBatchActive();
        DrawBillboardRec(cam, m_phantomTex, src, center, size, tint);
        rlDrawRenderBatchActive();
        Vector3 head{center.x, center.y + size.y * 0.28f, center.z};
        Vector3 toCam = Vector3Normalize(Vector3Subtract(cam.position, head));
        Vector3 right = Vector3Normalize(Vector3CrossProduct(cam.up, toCam));
        float pulse = 0.5f + 0.5f * std::sin(m_time * 24.0f);
        Color eye{255, static_cast<unsigned char>(30 + 40 * pulse), static_cast<unsigned char>(30 + 40 * pulse), a};
        float spread = 0.055f * scale;
        DrawSphereEx(Vector3Add(head, Vector3Add(Vector3Scale(right, -spread), Vector3Scale(toCam, 0.03f))), 0.02f * scale, 4, 4, eye);
        DrawSphereEx(Vector3Add(head, Vector3Add(Vector3Scale(right, spread), Vector3Scale(toCam, 0.03f))), 0.02f * scale, 4, 4, eye);
    };

    const StalkerData& st = m_stalker;
    bool visible = st.delay <= 0.0f && (st.state == StalkerState::Emerging || st.state == StalkerState::Hunting ||
                                         st.state == StalkerState::Searching || st.state == StalkerState::Final ||
                                         st.state == StalkerState::Dying || st.state == StalkerState::Ambush);
    if (visible) {
        float rise = st.state == StalkerState::Emerging ? std::clamp(st.timer / 2.2f, 0.0f, 1.0f) : 1.0f;
        float alpha = 1.0f;
        Vector3 feet = st.pos;
        if (st.state == StalkerState::Dying) {
            float k = std::clamp(st.timer / KO_FAKE_DEATH_TIME, 0.0f, 1.0f);
            alpha = 1.0f - k;
            feet.y -= k * 1.2f;
        }
        bool chasingCar = m_mode == PlayerMode::Driving && !st.finalPhase;
        drawFigure(feet, chasingCar ? 1.2f : (st.finalPhase ? 1.15f : 1.0f), rise, alpha, st.hitFlash > 0.0f || st.state == StalkerState::Dying);
    }
    for (const AmbushData& a : m_ambushes) {
        if (!a.active || a.done) continue;
        drawFigure(a.pos, 1.35f, std::clamp(a.timer / 0.6f, 0.0f, 1.0f), 1.0f, false);
    }
}

void MotelWorld3D::DrawPistolViewmodel() {
    if (m_pistolVisual == nullptr) return;
    const Camera3D& cam = m_camera.GetCamera();
    Vector3 fwd = m_camera.GetForward();
    Vector3 right = m_camera.GetRight();
    Vector3 up = m_camera.GetUp();
    float phase = m_camera.GetBobPhase();
    float reload = m_reloadTimer > 0.0f ? std::sin(PI * std::clamp(1.0f - m_reloadTimer / 1.3f, 0.0f, 1.0f)) : 0.0f;
    Vector3 aimFwd = Vector3Normalize(Vector3Add(fwd, Vector3Scale(up, m_recoil * 0.22f - reload * 0.5f)));
    Vector3 aimRight = Vector3Normalize(Vector3CrossProduct(aimFwd, up));
    Vector3 aimUp = Vector3CrossProduct(aimRight, aimFwd);
    Vector3 pos = cam.position;
    pos = Vector3Add(pos, Vector3Scale(fwd, 0.3f - m_recoil * 0.05f));
    pos = Vector3Add(pos, Vector3Scale(right, 0.13f + std::cos(phase) * 0.005f));
    pos = Vector3Add(pos, Vector3Scale(up, -0.115f - reload * 0.08f + std::abs(std::sin(phase)) * 0.006f));
    Matrix world = BasisMatrix(aimFwd, aimUp, aimRight, pos);
    m_lighting.SetObjectParams(0.0f, Vector3{0.22f, 0.2f, 0.19f}, 0.0f);
    rlDrawRenderBatchActive();
    m_itemVisuals.Draw(*m_pistolVisual, world);
    m_lighting.SetObjectParams(2.0f, Vector3{0.0f, 0.0f, 0.0f}, 0.0f);
    if (m_muzzleTimer > 0.0f) {
        Vector3 muzzle = Vector3Add(pos, Vector3Add(Vector3Scale(aimFwd, 0.12f), Vector3Scale(aimUp, 0.03f)));
        rlDisableDepthTest();
        DrawSphereEx(muzzle, 0.035f, 5, 5, Color{255, 230, 150, 255});
        DrawSphereEx(Vector3Add(muzzle, Vector3Scale(aimFwd, 0.03f)), 0.022f, 5, 5, Color{255, 255, 220, 255});
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
    }
}

void MotelWorld3D::DrawCockpit() const {
    if (!m_carReady) return;
    float off = m_car.GroundOffset();
    auto L = [&](float x, float y, float z) { return m_car.LocalToWorld(Vector3{x, y - off, z}); };
    float hw = m_car.HalfWidth() * 0.92f;
    Color dash{24, 20, 18, 255};
    Color dashFace{34, 28, 24, 255};
    rlDrawRenderBatchActive();
    DrawTriangle3D(L(0.45f, 1.06f, -hw), L(0.45f, 1.06f, hw), L(0.85f, 1.02f, hw), dash);
    DrawTriangle3D(L(0.45f, 1.06f, -hw), L(0.85f, 1.02f, hw), L(0.85f, 1.02f, -hw), dash);
    DrawTriangle3D(L(0.45f, 0.82f, -hw), L(0.45f, 0.82f, hw), L(0.45f, 1.06f, hw), dashFace);
    DrawTriangle3D(L(0.45f, 0.82f, -hw), L(0.45f, 1.06f, hw), L(0.45f, 1.06f, -hw), dashFace);
    Color pillar{16, 14, 14, 255};
    DrawCylinderEx(L(0.8f, 1.02f, -hw), L(0.05f, 1.62f, -hw + 0.08f), 0.045f, 0.04f, 6, pillar);
    DrawCylinderEx(L(0.8f, 1.02f, hw), L(0.05f, 1.62f, hw - 0.08f), 0.045f, 0.04f, 6, pillar);
    DrawCylinderEx(L(0.05f, 1.62f, -hw + 0.08f), L(0.05f, 1.62f, hw - 0.08f), 0.05f, 0.05f, 6, pillar);
    Vector3 hub = L(0.3f, 1.06f, -0.42f);
    Vector3 axis = Vector3Normalize(Vector3Add(Vector3Scale(m_car.Forward(), -0.75f), Vector3Scale(m_car.Up(), 0.66f)));
    Vector3 a = Vector3Normalize(Vector3CrossProduct(axis, m_car.Up()));
    Vector3 b = Vector3CrossProduct(axis, a);
    float spin = -m_car.SteerInput() * 2.2f;
    const int segments = 18;
    const float radius = 0.17f;
    Color rim{40, 34, 30, 255};
    for (int i = 0; i < segments; ++i) {
        float t0 = spin + 2.0f * PI * static_cast<float>(i) / segments;
        float t1 = spin + 2.0f * PI * static_cast<float>(i + 1) / segments;
        Vector3 p0 = Vector3Add(hub, Vector3Add(Vector3Scale(a, std::cos(t0) * radius), Vector3Scale(b, std::sin(t0) * radius)));
        Vector3 p1 = Vector3Add(hub, Vector3Add(Vector3Scale(a, std::cos(t1) * radius), Vector3Scale(b, std::sin(t1) * radius)));
        DrawCylinderEx(p0, p1, 0.016f, 0.016f, 4, rim);
    }
    for (int i = 0; i < 3; ++i) {
        float t = spin + PI * 0.5f + 2.0f * PI * static_cast<float>(i) / 3.0f;
        Vector3 p = Vector3Add(hub, Vector3Add(Vector3Scale(a, std::cos(t) * radius), Vector3Scale(b, std::sin(t) * radius)));
        DrawCylinderEx(hub, p, 0.01f, 0.01f, 4, rim);
    }
    Vector3 gauge = L(0.47f, 1.0f, -0.42f);
    DrawSphereEx(gauge, 0.035f, 6, 6, Color{200, 150, 70, 255});
    rlDrawRenderBatchActive();
}

void MotelWorld3D::RenderActionUI(engine::core::Engine& eng) {
    Font font = eng.Assets().GetMainFont();
    float cx = UI_W * 0.5f;

    float danger = 0.0f;
    if (m_stalker.state == StalkerState::Hunting || m_stalker.state == StalkerState::Final || m_stalker.state == StalkerState::Searching) {
        float d = FlatDistance(m_stalker.pos, PlayerAnchor());
        danger = std::clamp(1.0f - d / (m_mode == PlayerMode::Driving ? 14.0f : 9.0f), 0.0f, 1.0f);
    }
    if (danger > 0.0f || m_damageFlash > 0.0f) {
        float pulse = 0.6f + 0.4f * std::sin(m_time * 9.0f);
        unsigned char a = static_cast<unsigned char>(std::clamp(danger * 120.0f * pulse + m_damageFlash * 140.0f, 0.0f, 200.0f));
        DrawRectangleGradientV(0, 0, static_cast<int>(UI_W), 120, Color{150, 0, 0, a}, Color{150, 0, 0, 0});
        DrawRectangleGradientV(0, static_cast<int>(UI_H) - 120, static_cast<int>(UI_W), 120, Color{150, 0, 0, 0}, Color{150, 0, 0, a});
        DrawRectangleGradientH(0, 0, 140, static_cast<int>(UI_H), Color{150, 0, 0, a}, Color{150, 0, 0, 0});
        DrawRectangleGradientH(static_cast<int>(UI_W) - 140, 0, 140, static_cast<int>(UI_H), Color{150, 0, 0, 0}, Color{150, 0, 0, a});
    }

    if (m_mode == PlayerMode::Driving) {
        float kmh = std::abs(m_car.ForwardSpeed()) * 3.6f;
        DrawRectangle(static_cast<int>(UI_W) - 250, 600, 230, 100, Color{12, 9, 14, 200});
        DrawRectangleLines(static_cast<int>(UI_W) - 250, 600, 230, 100, Color{244, 184, 96, 160});
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%3.0f", kmh);
        DrawLabel(font, buf, UI_W - 236.0f, 606.0f, 52.0f, Color{240, 220, 170, 255});
        DrawLabel(font, "км/ч", UI_W - 140.0f, 630.0f, 18.0f, Color{200, 180, 140, 230});
        std::string gear = m_car.Gear() < 0 ? "R" : std::to_string(m_car.Gear());
        DrawLabel(font, gear, UI_W - 70.0f, 612.0f, 36.0f, m_car.Gear() < 0 ? Color{240, 90, 80, 255} : Color{140, 210, 160, 255});
        DrawLabel(font, "КУЗОВ", UI_W - 240.0f, 672.0f, 14.0f, Color{190, 170, 150, 220});
        Color hp = m_carHealth > 50.0f ? Color{90, 200, 120, 230} : (m_carHealth > 20.0f ? Color{230, 180, 50, 230} : Color{230, 60, 50, 230});
        DrawRectangle(static_cast<int>(UI_W) - 180, 676, static_cast<int>(150.0f * m_carHealth / 100.0f), 8, hp);
        DrawRectangleLines(static_cast<int>(UI_W) - 180, 676, 150, 8, Color{170, 160, 150, 180});
        DrawLabel(font, "[W/S] Газ/Тормоз  [A/D] Руль  [ПРОБЕЛ] Ручник  [V] Камера  [F] Фары  [E] Выйти  [Мышь] Обзор",
                  24.0f, 700.0f, 14.0f, Color{170, 160, 150, 190});
    } else if (m_mode == PlayerMode::OnFoot && HasPistol() && m_knockoutTime < 0.0f) {
        DrawRectangle(static_cast<int>(UI_W) - 200, 640, 180, 56, Color{12, 9, 14, 200});
        char buf[48];
        std::snprintf(buf, sizeof(buf), "ПМ  %d / %d", m_ammoMag, m_ammoReserve);
        DrawLabel(font, buf, UI_W - 188.0f, 646.0f, 24.0f, m_ammoMag > 0 ? Color{240, 220, 170, 255} : Color{240, 90, 80, 255});
        DrawLabel(font, m_reloadTimer > 0.0f ? "ПЕРЕЗАРЯДКА..." : "[ЛКМ] Огонь  [R] Перезарядка", UI_W - 188.0f, 674.0f, 13.0f,
                  Color{190, 170, 150, 220});
        float cy = UI_H * 0.5f;
        Color ch{240, 220, 190, 210};
        DrawRectangle(static_cast<int>(cx) - 12, static_cast<int>(cy), 7, 2, ch);
        DrawRectangle(static_cast<int>(cx) + 6, static_cast<int>(cy), 7, 2, ch);
        DrawRectangle(static_cast<int>(cx), static_cast<int>(cy) - 12, 2, 7, ch);
        DrawRectangle(static_cast<int>(cx), static_cast<int>(cy) + 6, 2, 7, ch);
    }

    if (m_mode == PlayerMode::Dead) {
        float fade = std::clamp((m_deathTimer - 0.9f) / 1.2f, 0.0f, 1.0f);
        DrawRectangle(0, 0, static_cast<int>(UI_W), static_cast<int>(UI_H), Color{0, 0, 0, static_cast<unsigned char>(255.0f * fade)});
        if (fade > 0.3f) {
            unsigned char a = static_cast<unsigned char>(255.0f * std::clamp((fade - 0.3f) / 0.7f, 0.0f, 1.0f));
            const char* title = "ОНО ВАС НАСТИГЛО";
            Vector2 size = MeasureTextEx(font, title, 40.0f, 1.0f);
            DrawLabel(font, title, cx - size.x * 0.5f, 300.0f, 40.0f, Color{200, 30, 30, a});
            const char* sub = "Возврат к контрольной точке...";
            Vector2 s2 = MeasureTextEx(font, sub, 18.0f, 1.0f);
            DrawLabel(font, sub, cx - s2.x * 0.5f, 356.0f, 18.0f, Color{200, 190, 180, a});
        }
    }
}

}

#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
namespace game::chapter2 {

void MotelWorld3D::UpdateDriveTest(engine::core::Engine& eng) {
    ++m_driveFrames;
    auto shot = [&](const char* name) {
        TakeScreenshot(name);
        TraceLog(LOG_INFO, "CH2 DRIVE: screenshot %s", name);
    };
    auto log = [&](const char* tag) {
        Vector3 p = PlayerAnchor();
        TraceLog(LOG_INFO, "CH2 DRIVE[%s] f=%d mode=%d pos=(%.1f %.1f %.1f) kmh=%.0f gear=%d car=%.0f stalker=%d d=%.1f hp=%.0f ammo=%d/%d",
                 tag, m_driveFrames, static_cast<int>(m_mode), p.x, p.y, p.z, std::abs(m_car.ForwardSpeed()) * 3.6f, m_car.Gear(),
                 m_carHealth, static_cast<int>(m_stalker.state), FlatDistance(m_stalker.pos, p), m_stalker.health, m_ammoMag, m_ammoReserve);
    };
    if (m_driveFrames == 5) {
        m_script.GiveItem("car_keys");
        m_script.SetFlag("tape_played");
        m_power = true;
        m_script.RunEvent("car", *this);
        const MarkerObject* porch = m_level.FindMarker("stalker_spawn_porch");
        SpawnStalker(porch ? porch->position : Vector3{1.5f, 0.3f, 2.5f}, StalkerState::Hunting, 0.5f);
    }
    if (m_driveFrames == 60) shot("drive_01_chase_cam.png");
    if (m_driveFrames == 400) m_driveView = 1;
    if (m_driveFrames == 430) shot("drive_02_cockpit.png");
    if (m_driveFrames == 440) {
        m_driveView = 0;
        m_driveCamInit = false;
    }
    if (m_driveFrames == 700) shot("drive_03_highway.png");
    if (m_driveFrames % 90 == 0) log("tick");

    const MarkerObject* post = m_level.FindMarker("checkpoint_post");
    if (m_drivePhase == 0 && m_mode == PlayerMode::Driving) {
        Vector3 p = m_car.GroundPosition();
        const auto& road = m_level.Road();
        size_t nearest = 0;
        float bestD = 1e9f;
        for (size_t i = 0; i < road.size(); ++i) {
            float d = FlatDistance(road[i].position, p);
            if (d < bestD) {
                bestD = d;
                nearest = i;
            }
        }
        Vector3 target = road.empty() ? Vector3Add(p, m_car.Forward()) : road.back().position;
        for (size_t i = nearest; i < road.size(); ++i) {
            if (FlatDistance(road[i].position, p) > 16.0f) {
                target = road[i].position;
                break;
            }
        }
        if (p.x < 26.0f) target = Vector3{34.0f, 0.0f, 20.0f};
        float desired = std::atan2(target.z - p.z, target.x - p.x);
        float diff = std::remainder(desired - m_car.Heading(), 2.0f * PI);
        m_autoInput = VehicleInput{};
        m_autoInput.steer = std::clamp(diff * 2.5f, -1.0f, 1.0f);
        float speed = m_car.Speed();
        bool nearPost = post && FlatDistance(post->position, p) < 30.0f;
        m_autoInput.throttle = (!nearPost && speed < (std::abs(diff) > 0.3f ? 12.0f : 26.0f)) ? 1.0f : 0.0f;
        m_autoInput.brake = nearPost ? 1.0f : 0.0f;
        m_autoActive = true;
        if (nearPost && speed < 0.5f) {
            log("post");
            shot("drive_04_post_arrival.png");
            m_autoActive = false;
            if (ExitVehicle()) m_drivePhase = 1;
        }
    } else if (m_drivePhase == 1) {
        if (post) {
            m_physics.TeleportCharacter(Vector3Add(post->position, Vector3{0.7f, 0.0f, -5.0f}));
            m_camera.SetLookAngles(PI, -0.2f);
        }
        m_script.GiveItem("pistol");
        m_script.RunEvent("got_pistol", *this);
        m_drivePhase = 2;
        m_driveFrames = 100000;
    } else if (m_drivePhase == 2) {
        if (m_stalker.state == StalkerState::Final || m_stalker.state == StalkerState::Emerging) {
            Vector3 eye = m_camera.GetCamera().position;
            Vector3 aim = Vector3Subtract(Vector3Add(m_stalker.pos, Vector3{0.0f, 1.3f, 0.0f}), eye);
            float yaw = std::atan2(aim.x, -aim.z);
            float pitch = std::atan2(aim.y, std::sqrt(aim.x * aim.x + aim.z * aim.z));
            m_camera.SetLookAngles(yaw, pitch);
            if (m_driveFrames % 40 == 0 && m_stalker.state == StalkerState::Final) {
                if (m_ammoMag <= 0) AddAmmo(8);
                FirePistol();
                log("shot");
            }
        }
        if (m_driveFrames == 100150) shot("drive_05_final_fight.png");
        if (m_stalker.state == StalkerState::Dead) {
            log("killed");
            shot("drive_06_victory.png");
            m_drivePhase = 3;
        }
        if (m_mode == PlayerMode::Dead) log("died");
    }
    if (m_drivePhase == 3 || m_driveFrames > 100000 + 2400 || (m_driveFrames > 16000 && m_drivePhase == 0)) {
        log("end");
        eng.RequestQuit();
        m_driveTest = false;
    }
}

}
#endif
