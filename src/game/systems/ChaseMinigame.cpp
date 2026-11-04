#include "game/systems/ChaseMinigame.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game::systems {

void ChaseMinigame::Start(MinigameType type) {
    m_type = type;
    Reset();
}

void ChaseMinigame::Reset() {
    m_state = ChaseState::Briefing;
    m_timer = 0.0f;
    m_progress = 0.0f;
    m_radioTargetFreq = 0.5f;
    m_radioPlayerFreq = 0.5f;
    m_vinylRotation = 0.0f;

    m_camX = 0.0f;
    m_camZ = 0.0f;
    m_camYaw = 0.0f;
    m_stalkerX = 10.0f;
    m_stalkerZ = 10.0f;
    m_nodes3D.clear();

    m_playerDist = 0.0f;
    m_playerLane = 0.0f;
    m_targetLane = 0.0f;
    m_playerSpeed = 28.0f;
    m_stamina = 100.0f;
    m_isSprinting = false;
    m_monsterDist = -24.0f;
    m_flashlightCooldown = 0.0f;
    m_flashlightActive = 0.0f;
    m_shardsCollected = 0;
    m_shardsTotal = (m_type == MinigameType::Night4_MirrorHunt3D) ? 5 : 3;
    m_stumbleTimer = 0.0f;
    m_roadScroll = 0.0f;
    m_pantTimer = 0.0f;
    m_carIntegrity = 100.0f;
    m_isBraking = false;
    m_skidCooldown = 0.0f;

    if (m_type == MinigameType::Night5_HighwayChase3D) {
        m_playerSpeed = 46.0f;
        m_monsterDist = -34.0f;
        m_shardsTotal = 5;
    } else if (m_type == MinigameType::Night1_RadioTuner2D) {
        m_shardsTotal = 1;
        m_radioPlayerFreq = 0.12f;
        m_radioTargetFreq = 0.325f;
    } else if (m_type == MinigameType::Night_JukeboxTuner2D) {
        m_shardsTotal = 1;
        m_radioPlayerFreq = 0.2f;
        m_radioTargetFreq = 0.65f;
    } else if (m_type == MinigameType::Night2_BasementFuse3D || m_type == MinigameType::Night3_FreezerMaze3D) {
        m_nodes3D.push_back({ 6.5f,  4.0f, true, false});
        m_nodes3D.push_back({-5.5f,  7.5f, true, false});
        m_nodes3D.push_back({ 3.0f, -7.0f, true, false});
        m_stalkerX = -8.0f;
        m_stalkerZ = -8.0f;
    } else if (m_type == MinigameType::Night4_MirrorHunt3D) {
        m_nodes3D.push_back({ 5.0f,  0.0f, true, false});
        m_nodes3D.push_back({-4.0f,  4.0f, true, false});
        m_nodes3D.push_back({ 0.0f, -5.5f, true, false});
        m_nodes3D.push_back({ 4.2f, -4.2f, true, false});
        m_nodes3D.push_back({-4.5f, -3.5f, true, false});
        m_nodes3D.push_back({ 0.0f,  5.0f, true, true});
    } else if (m_type == MinigameType::Night5_HighwayChase3D) {
        SpawnObstacles();
    }
}

void ChaseMinigame::SpawnObstacles() {
    m_obstacles.clear();
    const float lanes[3] = {-1.0f, 0.0f, 1.0f};

    if (m_type == MinigameType::Night5_HighwayChase3D) {
        m_obstacles.push_back({160.0f,  0.0f, 2, false, false});
        m_obstacles.push_back({360.0f, -1.0f, 2, false, false});
        m_obstacles.push_back({560.0f,  1.0f, 2, false, false});
        m_obstacles.push_back({760.0f,  0.0f, 2, false, false});
        m_obstacles.push_back({920.0f, -1.0f, 2, false, false});

        for (float d = 45.0f; d < 980.0f; d += 24.0f) {
            if (std::abs(d - 160.0f) < 12.0f || std::abs(d - 360.0f) < 12.0f ||
                std::abs(d - 560.0f) < 12.0f || std::abs(d - 760.0f) < 12.0f ||
                std::abs(d - 920.0f) < 12.0f) {
                continue;
            }
            int laneIdx = static_cast<int>(d * 13) % 3;
            int type = (static_cast<int>(d) % 3 == 0) ? 1 : 0;
            m_obstacles.push_back({d, lanes[laneIdx], type, false, false});
        }
    } else {
        m_obstacles.push_back({55.0f,   0.0f, 2, false, false});
        m_obstacles.push_back({130.0f, -1.0f, 2, false, false});
        m_obstacles.push_back({195.0f,  1.0f, 2, false, false});

        for (float d = 25.0f; d < 235.0f; d += 17.0f) {
            if (std::abs(d - 55.0f) < 8.0f || std::abs(d - 130.0f) < 8.0f || std::abs(d - 195.0f) < 8.0f) {
                continue;
            }
            int laneIdx = static_cast<int>(d * 7) % 3;
            int type = (static_cast<int>(d) % 2 == 0) ? 0 : 1;
            m_obstacles.push_back({d, lanes[laneIdx], type, false, false});
        }
    }
}

void ChaseMinigame::Update(engine::core::Engine& eng, float dt) {
    m_timer += dt;

    if (m_state == ChaseState::Briefing) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E)) {
            m_state = ChaseState::Running;
            eng.Audio().PlaySfx("audio/sfx/sfx_visor_toggle.wav", 0.8f);
        }
        return;
    }

    if (m_state == ChaseState::Caught) {
        if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            Reset();
            m_state = ChaseState::Running;
        }
        return;
    }

    if (m_state == ChaseState::Victory) {
        return;
    }

    switch (m_type) {
        case MinigameType::Night1_RadioTuner2D:
            UpdateRadio2D(eng, dt);
            break;
        case MinigameType::Night_JukeboxTuner2D:
            UpdateJukebox2D(eng, dt);
            break;
        case MinigameType::Night2_BasementFuse3D:
        case MinigameType::Night3_FreezerMaze3D:
            UpdateExploration3D(eng, dt);
            break;
        case MinigameType::Night4_MirrorHunt3D:
            UpdateMirrorHunt3D(eng, dt);
            break;
        case MinigameType::Night5_HighwayChase3D:
            UpdateHighwayChase3D(eng, dt);
            break;
    }
}

void ChaseMinigame::UpdateRadio2D(engine::core::Engine& eng, float dt) {
    m_radioTargetFreq = 0.325f + 0.006f * std::sin(m_timer * 0.8f);
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  m_radioPlayerFreq -= dt * 0.22f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) m_radioPlayerFreq += dt * 0.22f;
    m_radioPlayerFreq = std::clamp(m_radioPlayerFreq, 0.02f, 0.98f);

    float diff = std::abs(m_radioTargetFreq - m_radioPlayerFreq);
    if (diff < 0.055f) {
        m_progress += dt * 38.0f;
    } else {
        m_progress = std::max(0.0f, m_progress - dt * 10.0f);
    }

    if (m_progress >= 100.0f) {
        m_progress = 100.0f;
        m_shardsCollected = 3;
        m_state = ChaseState::Victory;
        eng.Audio().PlaySfx("audio/sfx/sfx_pie_microwave_bell.wav", 1.0f);
    }
}

void ChaseMinigame::UpdateJukebox2D(engine::core::Engine& eng, float dt) {
    m_vinylRotation += dt * 4.2f;
    m_radioTargetFreq = 0.55f + 0.25f * std::sin(m_timer * 1.4f) + 0.08f * std::cos(m_timer * 2.8f);
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  m_radioPlayerFreq -= dt * 0.65f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) m_radioPlayerFreq += dt * 0.65f;
    m_radioPlayerFreq = std::clamp(m_radioPlayerFreq, 0.05f, 0.95f);

    float diff = std::abs(m_radioTargetFreq - m_radioPlayerFreq);
    if (diff < 0.13f) {
        m_progress += dt * 32.0f;
    } else {
        m_progress = std::max(0.0f, m_progress - dt * 12.0f);
    }

    if (m_progress >= 100.0f) {
        m_progress = 100.0f;
        m_shardsCollected = 1;
        m_state = ChaseState::Victory;
        eng.Audio().PlaySfx("audio/sfx/sfx_chase_gate_escape.wav", 1.0f);
    }
}

void ChaseMinigame::UpdateExploration3D(engine::core::Engine& eng, float dt) {
    if (m_flashlightCooldown > 0.0f) m_flashlightCooldown = std::max(0.0f, m_flashlightCooldown - dt);
    if (m_flashlightActive > 0.0f)   m_flashlightActive   = std::max(0.0f, m_flashlightActive - dt);

    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  m_camYaw -= dt * 2.4f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) m_camYaw += dt * 2.4f;

    float moveSpd = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_SPACE)) ? 5.2f : 3.4f;
    float fx = std::cos(m_camYaw);
    float fz = std::sin(m_camYaw);
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
        m_camX += fx * moveSpd * dt;
        m_camZ += fz * moveSpd * dt;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        m_camX -= fx * moveSpd * dt * 0.7f;
        m_camZ -= fz * moveSpd * dt * 0.7f;
    }
    m_camX = std::clamp(m_camX, -9.5f, 9.5f);
    m_camZ = std::clamp(m_camZ, -9.5f, 9.5f);

    float toPlayerX = m_camX - m_stalkerX;
    float toPlayerZ = m_camZ - m_stalkerZ;
    float distToPlayer = std::sqrt(toPlayerX * toPlayerX + toPlayerZ * toPlayerZ);

    if (distToPlayer > 0.01f) {
        float stalkerSpd = (m_flashlightActive > 0.0f) ? -2.2f : 2.1f;
        m_stalkerX += (toPlayerX / distToPlayer) * stalkerSpd * dt;
        m_stalkerZ += (toPlayerZ / distToPlayer) * stalkerSpd * dt;
    }

    if (IsKeyPressed(KEY_F) && m_flashlightCooldown <= 0.0f) {
        m_flashlightActive = 1.0f;
        m_flashlightCooldown = 2.4f;
        eng.Audio().PlaySfx("audio/sfx/sfx_chase_flash_burn.wav", 0.9f);
        eng.Renderer().TriggerFlash(Color{244, 184, 96, 210}, 0.25f);
    }

    if (distToPlayer < 1.35f) {
        m_state = ChaseState::Caught;
        eng.Audio().PlayScreamerStinger("sfx_sc_03_string_screech.wav");
        eng.Renderer().TriggerFlash(Color{180, 20, 30, 240}, 0.8f);
        eng.Renderer().TriggerShake(7.0f, 0.9f);
        return;
    }

    for (auto& node : m_nodes3D) {
        if (!node.active) continue;
        float dx = m_camX - node.x;
        float dz = m_camZ - node.z;
        if (std::sqrt(dx * dx + dz * dz) < 1.55f) {
            node.active = false;
            m_shardsCollected++;
            eng.Audio().PlaySfx("audio/sfx/sfx_chase_relic_pickup.wav", 0.95f);
            eng.Renderer().TriggerFlash(Color{244, 184, 96, 220}, 0.25f);
            break;
        }
    }

    if (m_shardsCollected >= m_shardsTotal) {
        m_state = ChaseState::Victory;
        eng.Audio().PlaySfx("audio/sfx/sfx_chase_gate_escape.wav", 1.0f);
    }
}

void ChaseMinigame::UpdateMirrorHunt3D(engine::core::Engine& eng, float dt) {
    if (m_flashlightCooldown > 0.0f) m_flashlightCooldown = std::max(0.0f, m_flashlightCooldown - dt);
    if (m_flashlightActive > 0.0f)   m_flashlightActive   = std::max(0.0f, m_flashlightActive - dt);

    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  m_camYaw -= dt * 2.8f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) m_camYaw += dt * 2.8f;

    if ((IsKeyPressed(KEY_F) || IsKeyPressed(KEY_SPACE)) && m_flashlightCooldown <= 0.0f) {
        m_flashlightActive = 0.55f;
        m_flashlightCooldown = 1.1f;
        eng.Audio().PlaySfx("audio/sfx/sfx_chase_flash_burn.wav", 0.85f);
        eng.Renderer().TriggerFlash(Color{244, 184, 96, 175}, 0.20f);

        float fx = std::cos(m_camYaw);
        float fz = std::sin(m_camYaw);

        for (auto& node : m_nodes3D) {
            if (!node.active) continue;
            float nx = node.x;
            float nz = node.z;
            float dist = std::sqrt(nx * nx + nz * nz);
            if (dist < 0.1f) continue;
            float dirX = nx / dist;
            float dirZ = nz / dist;
            float dot = dirX * fx + dirZ * fz;

            if (dot >= 0.88f) {
                if (node.isFriendly) {
                    m_state = ChaseState::Caught;
                    eng.Audio().PlayScreamerStinger("sfx_sc_06_mirror_crack.wav");
                    eng.Renderer().TriggerFlash(Color{214, 69, 69, 255}, 0.7f);
                    return;
                } else {
                    node.active = false;
                    m_shardsCollected++;
                    eng.Audio().PlaySfx("audio/sfx/sfx_chase_relic_pickup.wav", 0.9f);
                }
                break;
            }
        }
    }

    if (m_shardsCollected >= m_shardsTotal) {
        m_state = ChaseState::Victory;
        eng.Audio().PlaySfx("audio/sfx/sfx_chase_gate_escape.wav", 1.0f);
    }
}

void ChaseMinigame::UpdateHighwayChase3D(engine::core::Engine& eng, float dt) {
    if (m_stumbleTimer > 0.0f) m_stumbleTimer = std::max(0.0f, m_stumbleTimer - dt);
    if (m_flashlightCooldown > 0.0f) m_flashlightCooldown = std::max(0.0f, m_flashlightCooldown - dt);
    if (m_flashlightActive > 0.0f) m_flashlightActive = std::max(0.0f, m_flashlightActive - dt);
    if (m_skidCooldown > 0.0f) m_skidCooldown = std::max(0.0f, m_skidCooldown - dt);

    bool steerLeft = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    bool steerRight = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
    bool tapLeft = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT);
    bool tapRight = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT);

    if (tapLeft) {
        m_targetLane = std::min(1.0f, m_targetLane + 1.0f);
        if (m_skidCooldown <= 0.0f) {
            m_skidCooldown = 0.35f;
            eng.Audio().PlaySfx("audio/sfx/sfx_car_skid.wav", 0.45f);
        }
    } else if (tapRight) {
        m_targetLane = std::max(-1.0f, m_targetLane - 1.0f);
        if (m_skidCooldown <= 0.0f) {
            m_skidCooldown = 0.35f;
            eng.Audio().PlaySfx("audio/sfx/sfx_car_skid.wav", 0.45f);
        }
    } else if (steerLeft) {
        m_targetLane = std::min(1.0f, m_targetLane + 5.5f * dt);
    } else if (steerRight) {
        m_targetLane = std::max(-1.0f, m_targetLane - 5.5f * dt);
    }

    float mouseDeltaX = GetMouseDelta().x;
    if (std::abs(mouseDeltaX) > 1.2f) {
        m_targetLane = std::clamp(m_targetLane - mouseDeltaX * 0.008f, -1.0f, 1.0f);
    }

    m_playerLane += (m_targetLane - m_playerLane) * std::min(1.0f, dt * 16.0f);

    bool wantsGas = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool wantsBrake = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
    m_isBraking = wantsBrake;

    float targetSpeed = 46.0f;
    if (m_stumbleTimer > 0.0f) {
        targetSpeed = 24.0f;
    } else if (wantsGas) {
        targetSpeed = 76.0f;
        m_pantTimer += dt;
        if (m_pantTimer >= 2.0f) {
            m_pantTimer = 0.0f;
            eng.Audio().PlaySfx("audio/sfx/sfx_car_engine_start.wav", 0.35f);
        }
    } else if (wantsBrake) {
        targetSpeed = 22.0f;
    }
    m_playerSpeed += (targetSpeed - m_playerSpeed) * std::min(1.0f, dt * 3.5f);

    if (IsKeyPressed(KEY_F) && m_flashlightCooldown <= 0.0f) {
        m_flashlightActive = 0.9f;
        m_flashlightCooldown = 2.2f;
        eng.Audio().PlaySfx("audio/sfx/sfx_flashlight_click.wav", 0.85f);
        eng.Renderer().TriggerFlash(Color{255, 240, 180, 200}, 0.25f);
        for (auto& ob : m_obstacles) {
            if (ob.type == 1 && !ob.burned && ob.x >= m_playerDist && ob.x <= m_playerDist + 50.0f) {
                if (std::abs(ob.y - m_playerLane) < 0.75f) {
                    ob.burned = true;
                    eng.Audio().PlaySfx("audio/sfx/sfx_chase_flash_burn.wav", 0.85f);
                }
            }
        }
    }

    m_playerDist += m_playerSpeed * dt;
    m_roadScroll += m_playerSpeed * dt;

    float monsterSpeed = (m_monsterDist < -22.0f) ? 52.0f : 44.0f;
    m_monsterDist += (monsterSpeed - m_playerSpeed) * dt;

    for (auto& ob : m_obstacles) {
        if (ob.collected || ob.burned) continue;
        if (std::abs(ob.x - m_playerDist) < 2.8f && std::abs(ob.y - m_playerLane) < 0.45f) {
            if (ob.type == 2) {
                ob.collected = true;
                m_shardsCollected++;
                m_carIntegrity = std::min(100.0f, m_carIntegrity + 25.0f);
                eng.Audio().PlaySfx("audio/sfx/sfx_chase_relic_pickup.wav", 0.95f);
                eng.Renderer().TriggerFlash(Color{244, 184, 96, 210}, 0.25f);
                m_monsterDist -= 8.0f;
            } else if (m_stumbleTimer <= 0.0f) {
                m_stumbleTimer = 0.9f;
                m_carIntegrity = std::max(0.0f, m_carIntegrity - 30.0f);
                m_monsterDist += 10.0f;
                eng.Audio().PlaySfx("audio/sfx/sfx_car_crash.wav", 1.0f);
                eng.Renderer().TriggerShake(6.5f, 0.5f);
                if (m_carIntegrity <= 0.0f) {
                    m_state = ChaseState::Caught;
                    eng.Audio().PlayScreamerStinger("sfx_sc_10_mirror_agony.wav");
                    eng.Renderer().TriggerFlash(Color{168, 36, 36, 240}, 0.8f);
                    eng.Renderer().TriggerShake(7.0f, 1.0f);
                    return;
                }
            }
        }
    }

    if (m_monsterDist >= 0.0f) {
        m_state = ChaseState::Caught;
        eng.Audio().PlayScreamerStinger("sfx_sc_10_mirror_agony.wav");
        eng.Renderer().TriggerFlash(Color{168, 36, 36, 240}, 0.8f);
        eng.Renderer().TriggerShake(7.0f, 1.0f);
        return;
    }

    if (m_playerDist >= 1000.0f) {
        m_state = ChaseState::Victory;
        eng.Audio().PlaySfx("audio/sfx/sfx_chase_gate_escape.wav", 1.0f);
        eng.Audio().PlaySfx("audio/sfx/sfx_radio_click.wav", 0.9f);
        eng.Audio().PlaySfx("audio/sfx/sfx_phone_whisper.wav", 0.65f);
        eng.Renderer().TriggerFlash(Color{255, 250, 240, 255}, 1.0f);
    }
}

void ChaseMinigame::RenderWorld(engine::core::Engine& eng) {
    switch (m_type) {
        case MinigameType::Night1_RadioTuner2D:
            RenderRadio2D(eng);
            break;
        case MinigameType::Night_JukeboxTuner2D:
            RenderJukebox2D(eng);
            break;
        case MinigameType::Night2_BasementFuse3D:
        case MinigameType::Night3_FreezerMaze3D:
            RenderExploration3D(eng);
            break;
        case MinigameType::Night4_MirrorHunt3D:
            RenderMirrorHunt3D(eng);
            break;
        case MinigameType::Night5_HighwayChase3D:
            RenderHighwayChase3D(eng);
            break;
    }
}

void ChaseMinigame::RenderRadio2D(engine::core::Engine& eng) const {
    DrawRectangle(0, 0, 320, 180, Color{16, 12, 16, 255});
    DrawRectangle(12, 12, 296, 156, Color{45, 24, 20, 255});
    DrawRectangleLinesEx(Rectangle{12, 12, 296, 156}, 2.0f, Color{196, 122, 59, 255});

    for (int sx = 22; sx <= 78; sx += 5) {
        DrawLine(sx, 24, sx, 156, Color{28, 14, 12, 255});
    }

    DrawRectangle(88, 20, 212, 140, Color{22, 18, 24, 255});
    DrawRectangleLines(88, 20, 212, 140, Color{107, 51, 34, 255});

    DrawRectangle(96, 26, 196, 40, Color{28, 22, 18, 255});
    DrawRectangleLines(96, 26, 196, 40, Color{244, 184, 96, 220});

    for (int ti = 0; ti <= 10; ++ti) {
        float tx = 102.0f + static_cast<float>(ti) * 18.4f;
        int tickH = (ti % 5 == 0) ? 14 : 7;
        DrawLine(static_cast<int>(tx), 28, static_cast<int>(tx), 28 + tickH, Color{196, 122, 59, 200});
    }

    float targetLineX = 102.0f + m_radioTargetFreq * 184.0f;
    DrawRectangle(static_cast<int>(targetLineX - 6), 28, 12, 34, Color{80, 240, 120, 50});
    DrawLine(static_cast<int>(targetLineX), 28, static_cast<int>(targetLineX), 62, Color{244, 184, 96, 255});

    float needleX = 102.0f + m_radioPlayerFreq * 184.0f;
    DrawLineEx(Vector2{needleX, 26}, Vector2{needleX, 66}, 2.0f, Color{244, 60, 60, 255});
    DrawCircle(static_cast<int>(needleX), 27, 2.5f, RED);

    float diff = std::abs(m_radioTargetFreq - m_radioPlayerFreq);
    bool locked = (diff < 0.055f);

    DrawRectangle(96, 72, 196, 16, Color{14, 14, 18, 255});
    DrawRectangleLines(96, 72, 196, 16, locked ? Color{80, 240, 120, 255} : Color{220, 60, 60, 255});

    Font font = eng.Assets().GetMainFont();
    float mhz = 88.0f + m_radioPlayerFreq * 20.0f;
    char freqBuf[64];
    if (locked) {
        std::snprintf(freqBuf, sizeof(freqBuf), "94.5 FM [СИГНАЛ МИХАИЛА ЗАХВАЧЕН]");
        DrawTextEx(font, freqBuf, Vector2{102, 74}, 11.0f, 1.0f, Color{80, 240, 120, 255});
    } else {
        std::snprintf(freqBuf, sizeof(freqBuf), "%.1f FM: ПОИСК (ПОМЕХИ ЭФИРА)", mhz);
        DrawTextEx(font, freqBuf, Vector2{102, 74}, 11.0f, 1.0f, Color{230, 80, 80, 255});
    }

    DrawRectangle(96, 92, 196, 38, Color{12, 14, 20, 255});
    DrawRectangleLines(96, 92, 196, 38, Color{70, 80, 95, 200});
    for (int x = 98; x < 290; x += 2) {
        float waveY = 111.0f;
        if (locked) {
            float phase = (x - 98) * 0.14f + m_timer * 12.0f;
            waveY += std::sin(phase) * 14.0f;
            DrawPixel(x, static_cast<int>(waveY), Color{80, 240, 120, 255});
            DrawPixel(x, static_cast<int>(waveY + 1.0f), Color{80, 240, 120, 160});
        } else {
            float n = std::sin((x * 17.13f) + m_timer * 45.0f) * 12.0f + std::cos((x * 31.4f) - m_timer * 33.0f) * 4.0f;
            DrawPixel(x, static_cast<int>(111.0f + n), Color{214, 69, 69, 220});
        }
    }

    DrawRectangle(96, 134, 196, 14, Color{14, 14, 18, 255});
    DrawRectangleLines(96, 134, 196, 14, Color{244, 184, 96, 255});
    int fillW = static_cast<int>((m_progress / 100.0f) * 192.0f);
    DrawRectangle(98, 136, fillW, 10, locked ? Color{80, 240, 120, 230} : Color{244, 184, 96, 200});
}

void ChaseMinigame::RenderJukebox2D(engine::core::Engine&) const {
    DrawRectangle(0, 0, 320, 180, Color{12, 10, 18, 255});
    DrawRectangle(14, 14, 292, 152, Color{28, 18, 24, 255});
    DrawRectangleLinesEx(Rectangle{14, 14, 292, 152}, 3.0f, Color{244, 184, 96, 255});

    DrawRectangle(22, 22, 170, 136, Color{18, 12, 16, 255});
    DrawRectangleLines(22, 22, 170, 136, Color{107, 51, 34, 255});

    Vector2 center = {107.0f, 90.0f};
    DrawCircleV(center, 58.0f, Color{14, 14, 16, 255});
    DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), 58.0f, Color{45, 45, 55, 255});
    DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), 48.0f, Color{25, 25, 32, 255});
    DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), 38.0f, Color{35, 35, 42, 255});
    DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), 28.0f, Color{25, 25, 32, 255});

    DrawCircleV(center, 18.0f, Color{180, 36, 42, 255});
    DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), 18.0f, Color{244, 184, 96, 255});
    DrawCircleV(center, 3.5f, BLACK);

    float nx = center.x + std::cos(m_vinylRotation) * 12.0f;
    float ny = center.y + std::sin(m_vinylRotation) * 12.0f;
    DrawCircle(static_cast<int>(nx), static_cast<int>(ny), 2.0f, Color{244, 184, 96, 200});

    float armAngle = -0.35f + m_radioPlayerFreq * 0.70f;
    Vector2 pivot = {165.0f, 40.0f};
    Vector2 armEnd = {pivot.x - std::cos(armAngle) * 62.0f, pivot.y + std::sin(armAngle) * 62.0f};
    DrawLineEx(pivot, armEnd, 3.0f, Color{180, 190, 205, 255});
    DrawCircleV(pivot, 5.0f, Color{120, 130, 145, 255});
    DrawCircleV(armEnd, 3.5f, Color{244, 184, 96, 255});

    DrawRectangle(200, 22, 100, 136, Color{18, 22, 28, 255});
    DrawRectangleLines(200, 22, 100, 136, Color{143, 169, 196, 255});

    DrawRectangle(208, 30, 84, 18, Color{10, 12, 16, 255});
    float targetX = 208.0f + m_radioTargetFreq * 80.0f;
    float playerX = 208.0f + m_radioPlayerFreq * 80.0f;
    DrawLine(static_cast<int>(targetX), 30, static_cast<int>(targetX), 48, Color{244, 60, 60, 255});
    DrawLine(static_cast<int>(playerX), 30, static_cast<int>(playerX), 48, Color{80, 240, 120, 255});

    float diff = std::abs(m_radioTargetFreq - m_radioPlayerFreq);
    Color meterCol = (diff < 0.13f) ? Color{80, 240, 120, 255} : Color{220, 60, 60, 255};
    DrawRectangle(208, 56, 84, 60, Color{14, 16, 22, 255});
    DrawRectangleLines(208, 56, 84, 60, meterCol);

    for (int x = 212; x < 288; x += 2) {
        float waveT = (x - 212) * 0.15f + m_timer * 10.0f;
        float amp = (diff < 0.13f) ? 18.0f : 6.0f;
        float y = 86.0f + std::sin(waveT) * amp;
        DrawPixel(x, static_cast<int>(y), meterCol);
    }

    DrawRectangle(208, 126, 84, 16, Color{10, 12, 16, 255});
    DrawRectangleLines(208, 126, 84, 16, Color{244, 184, 96, 255});
    int fillW = static_cast<int>((m_progress / 100.0f) * 82.0f);
    DrawRectangle(209, 127, fillW, 14, Color{244, 184, 96, 230});
}

void ChaseMinigame::RenderExploration3D(engine::core::Engine&) const {
    bool isFreezer = (m_type == MinigameType::Night3_FreezerMaze3D);
    ClearBackground(isFreezer ? Color{12, 22, 34, 255} : Color{14, 10, 12, 255});

    Camera3D cam{};
    cam.position = {m_camX, 1.2f, m_camZ};
    cam.target = {m_camX + std::cos(m_camYaw), 1.15f, m_camZ + std::sin(m_camYaw)};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 65.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(cam);
    Color floorCol = isFreezer ? Color{40, 62, 82, 255} : Color{38, 26, 22, 255};
    Color wallCol  = isFreezer ? Color{85, 115, 145, 255} : Color{68, 36, 28, 255};
    DrawPlane(Vector3{0.0f, 0.0f, 0.0f}, Vector2{22.0f, 22.0f}, floorCol);

    DrawCube(Vector3{0.0f,  1.5f, -10.5f}, 21.0f, 3.0f, 1.0f, wallCol);
    DrawCube(Vector3{0.0f,  1.5f,  10.5f}, 21.0f, 3.0f, 1.0f, wallCol);
    DrawCube(Vector3{-10.5f,1.5f,   0.0f}, 1.0f,  3.0f, 21.0f, wallCol);
    DrawCube(Vector3{ 10.5f,1.5f,   0.0f}, 1.0f,  3.0f, 21.0f, wallCol);

    for (int px = -6; px <= 6; px += 6) {
        for (int pz = -4; pz <= 4; pz += 8) {
            DrawCube(Vector3{static_cast<float>(px), 1.5f, static_cast<float>(pz)}, 1.2f, 3.0f, 1.2f, wallCol);
            DrawCubeWires(Vector3{static_cast<float>(px), 1.5f, static_cast<float>(pz)}, 1.2f, 3.0f, 1.2f, BLACK);
        }
    }

    for (const auto& node : m_nodes3D) {
        if (!node.active) continue;
        Color c = isFreezer ? Color{180, 230, 255, 255} : Color{244, 184, 96, 255};
        DrawSphere(Vector3{node.x, 1.1f, node.z}, 0.35f, c);
        DrawSphereWires(Vector3{node.x, 1.1f, node.z}, 0.50f, 6, 6, WHITE);
    }

    float tNow = static_cast<float>(GetTime());
    float pulse = 0.55f + 0.15f * std::sin(tNow * 6.0f);
    DrawSphere(Vector3{m_stalkerX, 1.2f, m_stalkerZ}, pulse, Color{195, 20, 28, 240});
    DrawCubeWires(Vector3{m_stalkerX, 1.2f, m_stalkerZ}, 0.8f, 1.8f, 0.8f, RED);

    if (m_flashlightActive > 0.0f) {
        Vector3 flashTgt = {m_camX + std::cos(m_camYaw) * 12.0f, 1.0f, m_camZ + std::sin(m_camYaw) * 12.0f};
        DrawLine3D(cam.position, flashTgt, Color{255, 245, 200, 220});
    }

    EndMode3D();
}

void ChaseMinigame::RenderMirrorHunt3D(engine::core::Engine&) const {
    ClearBackground(Color{16, 12, 18, 255});

    Camera3D cam{};
    cam.position = {0.0f, 1.15f, 0.0f};
    cam.target = {std::cos(m_camYaw), 1.15f, std::sin(m_camYaw)};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 65.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(cam);
    DrawPlane(Vector3{0.0f, 0.0f, 0.0f}, Vector2{16.0f, 16.0f}, Color{32, 22, 28, 255});

    for (const auto& node : m_nodes3D) {
        if (!node.active) continue;
        Vector3 pos = {node.x, 1.1f, node.z};
        if (node.isFriendly) {
            DrawSphere(pos, 0.40f, Color{140, 200, 255, 255});
            DrawCubeWires(pos, 0.7f, 1.6f, 0.7f, Color{180, 230, 255, 220});
        } else {
            DrawSphere(pos, 0.42f, Color{214, 45, 45, 255});
            DrawCubeWires(pos, 0.8f, 1.7f, 0.8f, RED);
        }
    }

    if (m_flashlightActive > 0.0f) {
        Vector3 flashTgt = {std::cos(m_camYaw) * 10.0f, 1.15f, std::sin(m_camYaw) * 10.0f};
        DrawLine3D(cam.position, flashTgt, Color{244, 184, 96, 220});
    }

    EndMode3D();
}

void ChaseMinigame::RenderHighwayChase3D(engine::core::Engine&) const {
    ClearBackground(Color{6, 8, 14, 255});

    float carX = m_playerLane * 2.4f;
    Camera3D cam{};
    cam.position = {carX * 0.85f, 2.1f, -4.6f};
    cam.target = {carX * 0.95f, 1.35f, 20.0f};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 62.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(cam);
    DrawPlane(Vector3{0.0f, 0.0f, 40.0f}, Vector2{14.0f, 110.0f}, Color{20, 18, 22, 255});
    DrawPlane(Vector3{-8.5f, -0.05f, 40.0f}, Vector2{4.0f, 110.0f}, Color{10, 9, 12, 255});
    DrawPlane(Vector3{ 8.5f, -0.05f, 40.0f}, Vector2{4.0f, 110.0f}, Color{10, 9, 12, 255});

    float zOffset = std::fmod(m_roadScroll, 8.0f);
    for (float z = -8.0f; z < 80.0f; z += 8.0f) {
        float sz = z - zOffset;
        DrawCube(Vector3{-2.4f, 0.02f, sz}, 0.16f, 0.02f, 3.4f, Color{244, 184, 96, 210});
        DrawCube(Vector3{ 2.4f, 0.02f, sz}, 0.16f, 0.02f, 3.4f, Color{244, 184, 96, 210});
        DrawCube(Vector3{-6.8f, 0.8f, sz}, 0.22f, 1.1f, 7.5f, Color{65, 70, 85, 255});
        DrawCube(Vector3{ 6.8f, 0.8f, sz}, 0.22f, 1.1f, 7.5f, Color{65, 70, 85, 255});
    }

    float poleOffset = std::fmod(m_roadScroll, 24.0f);
    for (float z = -8.0f; z < 80.0f; z += 24.0f) {
        float pz = z - poleOffset;
        DrawCube(Vector3{-7.6f, 2.5f, pz}, 0.35f, 5.0f, 0.35f, Color{45, 32, 24, 255});
        DrawCube(Vector3{-7.6f, 4.6f, pz}, 2.2f, 0.22f, 0.22f, Color{55, 40, 30, 255});
    }

    DrawCube(Vector3{carX, 0.45f, 0.0f}, 1.95f, 0.55f, 3.8f, Color{24, 26, 32, 255});
    DrawCubeWires(Vector3{carX, 0.45f, 0.0f}, 1.96f, 0.56f, 3.82f, Color{50, 55, 65, 255});
    DrawCube(Vector3{carX, 0.88f, -0.2f}, 1.6f, 0.45f, 2.1f, Color{16, 18, 22, 255});
    DrawCube(Vector3{carX, 0.90f, 0.55f}, 1.5f, 0.38f, 0.35f, Color{70, 100, 120, 220});
    DrawCube(Vector3{carX, 0.90f, -0.95f}, 1.5f, 0.38f, 0.35f, Color{40, 50, 60, 200});

    DrawCube(Vector3{carX - 0.7f, 0.35f, 14.0f}, 0.85f, 0.25f, 24.0f, Color{255, 240, 180, 50});
    DrawCube(Vector3{carX + 0.7f, 0.35f, 14.0f}, 0.85f, 0.25f, 24.0f, Color{255, 240, 180, 50});

    Color tailCol = m_isBraking ? Color{255, 40, 40, 255} : Color{180, 20, 20, 220};
    DrawCube(Vector3{carX - 0.75f, 0.48f, -1.92f}, 0.35f, 0.22f, 0.08f, tailCol);
    DrawCube(Vector3{carX + 0.75f, 0.48f, -1.92f}, 0.35f, 0.22f, 0.08f, tailCol);

    for (const auto& ob : m_obstacles) {
        if (ob.collected || ob.burned) continue;
        float relZ = ob.x - m_playerDist;
        if (relZ < -4.0f || relZ > 65.0f) continue;
        float wx = ob.y * 2.4f;

        if (ob.type == 2) {
            DrawSphere(Vector3{wx, 0.85f, relZ}, 0.55f, Color{244, 184, 96, 255});
            DrawSphereWires(Vector3{wx, 0.85f, relZ}, 0.75f, 6, 6, WHITE);
        } else if (ob.type == 0) {
            DrawCube(Vector3{wx, 0.85f, relZ}, 1.8f, 1.4f, 1.8f, Color{105, 42, 38, 255});
            DrawCubeWires(Vector3{wx, 0.85f, relZ}, 1.82f, 1.42f, 1.82f, Color{224, 75, 75, 255});
        } else {
            DrawCube(Vector3{wx, 1.35f, relZ}, 1.1f, 2.7f, 1.1f, Color{14, 10, 18, 250});
            DrawSphere(Vector3{wx - 0.2f, 2.2f, relZ - 0.4f}, 0.16f, Color{255, 50, 50, 255});
            DrawSphere(Vector3{wx + 0.2f, 2.2f, relZ - 0.4f}, 0.16f, Color{255, 50, 50, 255});
        }
    }

    if (m_monsterDist > -45.0f) {
        float monX = m_playerLane * 1.8f;
        DrawCube(Vector3{monX, 2.6f, m_monsterDist}, 2.6f, 4.8f, 1.4f, Color{8, 6, 12, 250});
        DrawSphere(Vector3{monX, 5.2f, m_monsterDist}, 1.1f, Color{12, 8, 16, 255});
        DrawSphere(Vector3{monX - 0.35f, 5.4f, m_monsterDist + 0.9f}, 0.18f, Color{255, 40, 40, 255});
        DrawSphere(Vector3{monX + 0.35f, 5.4f, m_monsterDist + 0.9f}, 0.18f, Color{255, 40, 40, 255});
        DrawCube(Vector3{monX - 1.8f, 3.2f, m_monsterDist + 1.6f}, 0.45f, 0.45f, 3.0f, Color{10, 8, 14, 255});
        DrawCube(Vector3{monX + 1.8f, 3.2f, m_monsterDist + 1.6f}, 0.45f, 0.45f, 3.0f, Color{10, 8, 14, 255});
    }

    if (m_playerDist >= 880.0f) {
        float gateZ = 1000.0f - m_playerDist;
        DrawCube(Vector3{-6.8f, 4.0f, gateZ}, 0.6f, 8.0f, 0.6f, Color{244, 184, 96, 255});
        DrawCube(Vector3{ 6.8f, 4.0f, gateZ}, 0.6f, 8.0f, 0.6f, Color{244, 184, 96, 255});
        DrawCube(Vector3{ 0.0f, 7.8f, gateZ}, 14.2f, 0.6f, 0.6f, Color{244, 184, 96, 255});
    }
    EndMode3D();
}

void ChaseMinigame::RenderUI(engine::core::Engine& eng) const {
    Font font = eng.Assets().GetMainFont();

    const char* title =
        (m_type == MinigameType::Night1_RadioTuner2D)   ? "НОЧЬ 1: РАДИОПРИЁМНИК (ЭФИР МИХАИЛА 94.5 FM)" :
        (m_type == MinigameType::Night2_BasementFuse3D) ? "НОЧЬ 2: КОТЕЛЬНАЯ И ФАЗЫ (3D)" :
        (m_type == MinigameType::Night3_FreezerMaze3D)  ? "НОЧЬ 3: МОРОЗИЛЬНИК (3D)" :
        (m_type == MinigameType::Night4_MirrorHunt3D)   ? "НОЧЬ 4: ЗАЗЕРКАЛЬЕ (3D)" :
        (m_type == MinigameType::Night_JukeboxTuner2D)  ? "МУЗЫКАЛЬНЫЙ АВТОМАТ «МЕЛОДИЯ-86»" :
                                                          "ГЛАВА 2: ПОГОНЯ НА МАШИНЕ ПО ШОССЕ 86";

    const char* rules =
        (m_type == MinigameType::Night1_RadioTuner2D)   ? "Поворачивайте ручку частоты [A / D] или [Стрелки], чтобы поймать 94.5 FM и подавить паранормальный шум. Удерживайте захват волны." :
        (m_type == MinigameType::Night2_BasementFuse3D) ? "В 3D-подвале найдите 3 золотых узла питания [WASD]. Вспышка [F] отгоняет тень." :
        (m_type == MinigameType::Night3_FreezerMaze3D)  ? "В 3D-морозильнике соберите 3 замёрзших улики [WASD]. Свет [F] слепит Краулера." :
        (m_type == MinigameType::Night4_MirrorHunt3D)   ? "Поворачивайтесь [A / D] и сжигайте вспышкой [F / ПРОБЕЛ] 5 красных двойников." :
        (m_type == MinigameType::Night_JukeboxTuner2D)  ? "Удерживайте иглу звукоснимателя [A / D] на дорожке пластинки до 100%." :
                                                          "Управляйте машиной: [A / D] Руль, [W / Shift / ЛКМ] Газ, [S / Пробел] Тормоз, [F] Дальний свет против теней. Не дайте твари догнать машину!";

    if (m_state == ChaseState::Briefing) {
        DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 225});
        DrawRectangle(280, 200, 720, 300, Color{42, 23, 21, 250});
        DrawRectangleLinesEx(Rectangle{280, 200, 720, 300}, 3.0f, Color{196, 122, 59, 255});
        engine::graphics::TextLayout::DrawCentered(font, title, 640.0f, 240.0f, 26.0f, Color{244, 184, 96, 255}, 1.0f);
        engine::graphics::TextLayout::DrawWrapped(font, rules, engine::math::Rect{320.0f, 300.0f, 640.0f, 95.0f}, 20.0f, 2.0f, Color{232, 227, 217, 255});
        engine::graphics::TextLayout::DrawCentered(font, "[ENTER / ПРОБЕЛ / КЛИК] — Завести мотор и жать газ", 640.0f, 435.0f, 22.0f, Color{244, 184, 96, 255}, 1.0f);
        return;
    }

    if (m_state == ChaseState::Caught) {
        DrawRectangle(0, 0, 1280, 720, Color{105, 18, 22, 225});
        engine::graphics::TextLayout::DrawCentered(font, "МАШИНА РАЗБИТА В ТЕМНОТЕ ШОССЕ!", 640.0f, 290.0f, 32.0f, Color{244, 184, 96, 255}, 1.0f);
        engine::graphics::TextLayout::DrawCentered(font, "Тварь настигла вас в ливне...", 640.0f, 340.0f, 22.0f, Color{220, 200, 200, 255}, 1.0f);
        engine::graphics::TextLayout::DrawCentered(font, "[ПРОБЕЛ / R] Попробовать снова  |  [ESC] Меню", 640.0f, 410.0f, 22.0f, WHITE, 1.0f);
        return;
    }

    if (m_state == ChaseState::Victory) {
        if (m_type == MinigameType::Night5_HighwayChase3D) {
            DrawRectangle(0, 0, 1280, 720, Color{8, 10, 16, 245});
            DrawRectangle(140, 45, 1000, 630, Color{22, 17, 22, 252});
            DrawRectangleLinesEx(Rectangle{140, 45, 1000, 630}, 2.5f, Color{196, 122, 59, 255});

            engine::graphics::TextLayout::DrawCentered(font, "ГЛАВА 2: МОТЕЛЬ «86 КМ» — ФИНАЛ", 640.0f, 75.0f, 26.0f, Color{244, 184, 96, 255}, 1.0f);
            engine::graphics::TextLayout::DrawCentered(font, "МОСТ ЧЕРЕЗ РЕКУ БЕЗДНЫ ПРЕОДОЛЁН", 640.0f, 106.0f, 18.0f, Color{180, 200, 220, 240}, 1.0f);

            DrawRectangle(180, 135, 920, 250, Color{12, 15, 20, 255});
            DrawRectangleLinesEx(Rectangle{180, 135, 920, 250}, 1.5f, Color{80, 240, 120, 220});
            DrawTextEx(font, "[РАДИОПРИЁМНИК «ВОЛГИ» — 94.5 FM — ЭФИР МИХАИЛА]", Vector2{200, 148}, 18.0f, 1.0f, Color{80, 240, 120, 255});

            const char* radioMsg = "«...Ты слышишь меня?! Слава богу, ты прорвался через мост! Твари привязаны к аномалии мотеля — текущую воду они пересечь не способны. В зеркале заднего вида тень уже растаяла в утреннем тумане.\n\nНо кошмар ещё не окончен. Передатчик, сводящий с ума весь район и поднявший мертвецов... он на вершине сопки. Старая ретрансляционная станция «Маяк-86». Пока её генераторы вещают в эфир — мы в ловушке. Шоссе поднимается прямо к сопке. Я жду тебя у развилки к станции...»";
            engine::graphics::TextLayout::DrawWrapped(font, radioMsg, engine::math::Rect{200.0f, 182.0f, 880.0f, 185.0f}, 17.0f, 2.0f, Color{225, 235, 225, 255});

            DrawRectangle(180, 400, 920, 95, Color{28, 20, 24, 255});
            DrawRectangleLinesEx(Rectangle{180, 400, 920, 95}, 1.5f, Color{244, 184, 96, 255});
            DrawTextEx(font, "СЛЕДУЮЩАЯ ЧАСТЬ: ГЛАВА 3 — РАДИОСТАНЦИЯ «МАЯК-86»", Vector2{200, 412}, 20.0f, 1.0f, Color{244, 184, 96, 255});
            DrawTextEx(font, "Впереди: заброшенные аппаратные залы, тайна советского эксперимента и глушение сигнала.", Vector2{200, 442}, 16.0f, 1.0f, Color{210, 200, 190, 240});
            DrawTextEx(font, "СТАТУС: ГЛАВА 2 УСПЕШНО ЗАВЕРШЕНА", Vector2{200, 468}, 16.0f, 1.0f, Color{80, 240, 120, 255});

            DrawRectangle(180, 510, 920, 60, Color{14, 12, 18, 255});
            DrawRectangleLines(180, 510, 920, 60, Color{107, 51, 34, 255});
            char statsBuf[160];
            std::snprintf(statsBuf, sizeof(statsBuf), "Состояние машины: %d%%  |  Пройдено: 1000 м  |  Собрано канистр топлива: %d / 5", static_cast<int>(m_carIntegrity), m_shardsCollected);
            engine::graphics::TextLayout::DrawCentered(font, statsBuf, 640.0f, 530.0f, 18.0f, Color{244, 184, 96, 240}, 1.0f);

            engine::graphics::TextLayout::DrawCentered(font, "[ENTER / ПРОБЕЛ / КЛИК] — Сохранить прогресс и перейти в главное меню", 640.0f, 625.0f, 21.0f, Color{244, 184, 96, 255}, 1.0f);
            return;
        }

        DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 235});
        DrawRectangle(260, 180, 760, 340, Color{36, 22, 20, 250});
        DrawRectangleLinesEx(Rectangle{260, 180, 760, 340}, 3.0f, Color{196, 122, 59, 255});
        engine::graphics::TextLayout::DrawCentered(font, "МОСТ ПРЕОДОЛЁН! ТРАССА 86 СВОБОДНА!", 640.0f, 230.0f, 28.0f, Color{244, 184, 96, 255}, 1.0f);
        engine::graphics::TextLayout::DrawCentered(font, "Тварь с яростным воплем растворилась в стене бури позади.", 640.0f, 290.0f, 20.0f, Color{232, 227, 217, 255}, 1.0f);
        engine::graphics::TextLayout::DrawCentered(font, "ГЛАВА 2 УСПЕШНО ЗАВЕРШЕНА", 640.0f, 350.0f, 24.0f, Color{80, 240, 120, 255}, 1.0f);
        engine::graphics::TextLayout::DrawCentered(font, "[ENTER / ПРОБЕЛ] — Завершить", 640.0f, 440.0f, 22.0f, Color{244, 184, 96, 255}, 1.0f);
        return;
    }

    if (m_type == MinigameType::Night5_HighwayChase3D) {
        DrawRectangle(470, 12, 340, 60, Color{12, 10, 16, 235});
        DrawRectangleLinesEx(Rectangle{470, 12, 340, 60}, 2.0f, Color{196, 122, 59, 255});
        float monDistVal = std::abs(m_monsterDist);
        bool danger = (monDistVal < 14.0f);
        char mirrorBuf[96];
        std::snprintf(mirrorBuf, sizeof(mirrorBuf), danger ? "! ТВАРЬ НА ХВОСТЕ: %.0f М !" : "ЗЕРКАЛО: ТЕНЬ ПОЗАДИ %.0f М", monDistVal);
        engine::graphics::TextLayout::DrawCentered(font, mirrorBuf, 640.0f, 22.0f, 18.0f, danger ? Color{255, 60, 60, 255} : Color{244, 184, 96, 255}, 1.0f);
        float barNorm = std::clamp(1.0f - monDistVal / 35.0f, 0.0f, 1.0f);
        DrawRectangle(485, 48, static_cast<int>(310 * barNorm), 14, danger ? Color{255, 40, 40, 240} : Color{244, 184, 96, 200});

        DrawRectangle(16, 610, 360, 96, Color{12, 10, 16, 235});
        DrawRectangleLinesEx(Rectangle{16, 610, 360, 96}, 2.0f, Color{196, 122, 59, 255});
        char spdBuf[64];
        std::snprintf(spdBuf, sizeof(spdBuf), "СКОРОСТЬ: %d КМ/Ч", static_cast<int>(m_playerSpeed * 1.8f));
        DrawTextEx(font, spdBuf, Vector2{30, 622}, 20.0f, 1.0f, Color{244, 184, 96, 255});
        char integBuf[64];
        std::snprintf(integBuf, sizeof(integBuf), "ПРОЧНОСТЬ: %d%%", static_cast<int>(m_carIntegrity));
        DrawTextEx(font, integBuf, Vector2{30, 650}, 19.0f, 1.0f, (m_carIntegrity > 40.0f) ? Color{80, 240, 120, 255} : Color{255, 60, 60, 255});
        DrawRectangle(30, 678, static_cast<int>(330 * (m_carIntegrity / 100.0f)), 16, (m_carIntegrity > 40.0f) ? Color{80, 240, 120, 220} : Color{255, 40, 40, 220});

        DrawRectangle(880, 610, 384, 96, Color{12, 10, 16, 235});
        DrawRectangleLinesEx(Rectangle{880, 610, 384, 96}, 2.0f, Color{196, 122, 59, 255});
        char distBuf[64];
        float remainDist = std::max(0.0f, 1000.0f - m_playerDist);
        std::snprintf(distBuf, sizeof(distBuf), "ДО МОСТА: %.0f М", remainDist);
        DrawTextEx(font, distBuf, Vector2{896, 622}, 20.0f, 1.0f, Color{244, 184, 96, 255});
        DrawTextEx(font, "[A / D] Руль  |  [W] Газ  |  [S] Тормоз", Vector2{896, 650}, 17.0f, 1.0f, Color{220, 210, 200, 240});
        DrawTextEx(font, "[F] Дальний свет против теней", Vector2{896, 674}, 16.0f, 1.0f, Color{244, 184, 96, 220});
        return;
    }

    DrawRectangle(0, 0, 1280, 50, Color{10, 13, 26, 220});
    DrawTextEx(font, title, Vector2{24, 14}, 20.0f, 1.0f, Color{244, 184, 96, 255});

    char statBuf[128];
    if (m_type == MinigameType::Night1_RadioTuner2D || m_type == MinigameType::Night_JukeboxTuner2D) {
        std::snprintf(statBuf, sizeof(statBuf), "СИГНАЛ: %.0f%%", m_progress);
    } else {
        std::snprintf(statBuf, sizeof(statBuf), "ВЫПОЛНЕНО: %d / %d  |  [F] Вспышка фонаря", m_shardsCollected, m_shardsTotal);
    }
    DrawTextEx(font, statBuf, Vector2{540, 15}, 19.0f, 1.0f, Color{232, 227, 217, 255});
}

}