#include "render/ViewportRenderer.h"
#include "render/TextureManager.h"
#include "core/Math3D.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>

namespace knopmops::render {

ViewportRenderer::ViewportRenderer() {
    ResetCamera();
}

ViewportRenderer::~ViewportRenderer() {
    Shutdown();
}

void ViewportRenderer::Init(int width, int height) {
    m_width = width > 0 ? width : 1280;
    m_height = height > 0 ? height : 720;
    m_target = LoadRenderTexture(m_width, m_height);
    SetTextureFilter(m_target.texture, TEXTURE_FILTER_BILINEAR);
}

void ViewportRenderer::Resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;

    if (m_target.id != 0) UnloadRenderTexture(m_target);
    m_width = width;
    m_height = height;
    m_target = LoadRenderTexture(m_width, m_height);
    SetTextureFilter(m_target.texture, TEXTURE_FILTER_BILINEAR);
}

void ViewportRenderer::Shutdown() {
    if (m_target.id != 0) {
        UnloadRenderTexture(m_target);
        m_target = RenderTexture2D{};
    }
}

void ViewportRenderer::ResetCamera() {
    m_camera.position = Vector3{8.0f, 6.0f, 8.0f};
    m_camera.target = Vector3{0.0f, 1.0f, 0.0f};
    m_camera.up = Vector3{0.0f, 1.0f, 0.0f};
    m_camera.fovy = 55.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;

    m_cameraYaw = 45.0f;
    m_cameraPitch = 30.0f;
    m_cameraDist = 12.0f;
    m_cameraTarget = Vector3{0.0f, 1.0f, 0.0f};
}

void ViewportRenderer::FocusOn(Vector3 targetPos, float dist) {
    m_cameraTarget = targetPos;
    m_cameraDist = dist;

    float yawRad = m_cameraYaw * DEG2RAD;
    float pitchRad = m_cameraPitch * DEG2RAD;

    m_camera.target = m_cameraTarget;
    m_camera.position = Vector3{
        m_cameraTarget.x + m_cameraDist * std::cos(pitchRad) * std::sin(yawRad),
        m_cameraTarget.y + m_cameraDist * std::sin(pitchRad),
        m_cameraTarget.z + m_cameraDist * std::cos(pitchRad) * std::cos(yawRad)
    };
}

void ViewportRenderer::Update(float dt, bool isHovered, bool isFocused) {
    if (!isHovered && !isFocused) return;

    Vector2 mouseDelta = GetMouseDelta();
    float wheel = GetMouseWheelMove();

    if (wheel != 0.0f && isHovered) {
        m_cameraDist = std::clamp(m_cameraDist - wheel * 1.5f, 1.0f, 150.0f);
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        float rotSpeed = 0.25f;
        m_cameraYaw -= mouseDelta.x * rotSpeed;
        m_cameraPitch = std::clamp(m_cameraPitch - mouseDelta.y * rotSpeed, -85.0f, 85.0f);

        float yawRad = m_cameraYaw * DEG2RAD;
        float pitchRad = m_cameraPitch * DEG2RAD;

        Vector3 forward{
            -std::sin(yawRad) * std::cos(pitchRad),
            -std::sin(pitchRad),
            -std::cos(yawRad) * std::cos(pitchRad)
        };
        Vector3 right{
            std::cos(yawRad),
            0.0f,
            -std::sin(yawRad)
        };

        float flySpeed = 8.0f * dt;
        if (IsKeyDown(KEY_LEFT_SHIFT)) flySpeed *= 3.0f;

        Vector3 moveDelta{0.0f, 0.0f, 0.0f};
        if (IsKeyDown(KEY_W)) moveDelta = core::Vec3Add(moveDelta, core::Vec3Scale(forward, flySpeed));
        if (IsKeyDown(KEY_S)) moveDelta = core::Vec3Sub(moveDelta, core::Vec3Scale(forward, flySpeed));
        if (IsKeyDown(KEY_D)) moveDelta = core::Vec3Add(moveDelta, core::Vec3Scale(right, flySpeed));
        if (IsKeyDown(KEY_A)) moveDelta = core::Vec3Sub(moveDelta, core::Vec3Scale(right, flySpeed));
        if (IsKeyDown(KEY_E)) moveDelta.y += flySpeed;
        if (IsKeyDown(KEY_Q)) moveDelta.y -= flySpeed;

        m_camera.position = core::Vec3Add(m_camera.position, moveDelta);
        m_camera.target = core::Vec3Add(m_camera.target, moveDelta);
        m_cameraTarget = m_camera.target;
        return;
    }

    if (IsKeyDown(KEY_LEFT_ALT) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float rotSpeed = 0.35f;
        m_cameraYaw -= mouseDelta.x * rotSpeed;
        m_cameraPitch = std::clamp(m_cameraPitch + mouseDelta.y * rotSpeed, -85.0f, 85.0f);
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        float panSpeed = 0.015f * (m_cameraDist * 0.1f + 0.5f);
        float yawRad = m_cameraYaw * DEG2RAD;

        Vector3 right{std::cos(yawRad), 0.0f, -std::sin(yawRad)};
        Vector3 up{0.0f, 1.0f, 0.0f};

        Vector3 panOffset = core::Vec3Add(
            core::Vec3Scale(right, -mouseDelta.x * panSpeed),
            core::Vec3Scale(up, mouseDelta.y * panSpeed)
        );

        m_cameraTarget = core::Vec3Add(m_cameraTarget, panOffset);
    }

    float yawRad = m_cameraYaw * DEG2RAD;
    float pitchRad = m_cameraPitch * DEG2RAD;

    m_camera.target = m_cameraTarget;
    m_camera.position = Vector3{
        m_cameraTarget.x + m_cameraDist * std::cos(pitchRad) * std::sin(yawRad),
        m_cameraTarget.y + m_cameraDist * std::sin(pitchRad),
        m_cameraTarget.z + m_cameraDist * std::cos(pitchRad) * std::cos(yawRad)
    };
}

void ViewportRenderer::DrawGridFloor() {
    if (!m_showGrid) return;

    int slices = 40;
    float spacing = 1.0f;
    float extent = (slices * spacing) * 0.5f;

    rlBegin(RL_LINES);
    for (int i = -slices / 2; i <= slices / 2; ++i) {
        float pos = i * spacing;
        Color col = (i % 5 == 0) ? Color{70, 75, 85, 255} : Color{45, 48, 55, 180};
        if (i == 0) col = Color{90, 100, 120, 255};

        rlColor4ub(col.r, col.g, col.b, col.a);
        rlVertex3f(pos, 0.0f, -extent);
        rlVertex3f(pos, 0.0f, extent);

        rlVertex3f(-extent, 0.0f, pos);
        rlVertex3f(extent, 0.0f, pos);
    }

    rlColor4ub(220, 50, 50, 255);
    rlVertex3f(0.0f, 0.01f, 0.0f);
    rlVertex3f(2.0f, 0.01f, 0.0f);

    rlColor4ub(50, 100, 240, 255);
    rlVertex3f(0.0f, 0.01f, 0.0f);
    rlVertex3f(0.0f, 0.01f, 2.0f);
    rlEnd();
}

void ViewportRenderer::DrawBrushes(map::MapDocument& doc) {
    auto& texMgr = TextureManager::Instance();

    auto renderBrushList = [&](std::vector<map::MapBrush>& brushes, bool isSelectedContainer) {
        for (auto& brush : brushes) {
            if (!brush.visible) continue;

            for (const auto& face : brush.faces) {
                if (face.builtPoly.vertices.size() < 3) continue;

                Texture2D tex = texMgr.GetTexture(face.texture);
                const auto& verts = face.builtPoly.vertices;

                if (m_shading == ViewportShading::Textured) {
                    rlSetTexture(tex.id);
                    rlBegin(RL_TRIANGLES);
                    rlColor4ub(255, 255, 255, 255);

                    for (size_t k = 1; k + 1 < verts.size(); ++k) {
                        Vector3 v0 = verts[0];
                        Vector3 v1 = verts[k];
                        Vector3 v2 = verts[k + 1];

                        Vector2 uv0 = face.CalculateUV(v0);
                        Vector2 uv1 = face.CalculateUV(v1);
                        Vector2 uv2 = face.CalculateUV(v2);

                        rlTexCoord2f(uv0.x, uv0.y); rlVertex3f(v0.x, v0.y, v0.z);
                        rlTexCoord2f(uv1.x, uv1.y); rlVertex3f(v1.x, v1.y, v1.z);
                        rlTexCoord2f(uv2.x, uv2.y); rlVertex3f(v2.x, v2.y, v2.z);
                    }
                    rlEnd();
                    rlSetTexture(0);
                } else if (m_shading == ViewportShading::FlatLit) {
                    rlBegin(RL_TRIANGLES);
                    float light = std::max(0.2f, core::Vec3Dot(face.builtPoly.normal, core::Vec3Normalize({0.5f, 1.0f, 0.3f})));
                    unsigned char c = static_cast<unsigned char>(light * 220);
                    rlColor4ub(c, c, c, 255);

                    for (size_t k = 1; k + 1 < verts.size(); ++k) {
                        rlVertex3f(verts[0].x, verts[0].y, verts[0].z);
                        rlVertex3f(verts[k].x, verts[k].y, verts[k].z);
                        rlVertex3f(verts[k + 1].x, verts[k + 1].y, verts[k + 1].z);
                    }
                    rlEnd();
                }

                rlBegin(RL_LINES);
                Color wireCol = (brush.selected || isSelectedContainer) ? Color{255, 215, 0, 255} : Color{25, 25, 30, 160};
                rlColor4ub(wireCol.r, wireCol.g, wireCol.b, wireCol.a);
                for (size_t k = 0; k < verts.size(); ++k) {
                    Vector3 p1 = verts[k];
                    Vector3 p2 = verts[(k + 1) % verts.size()];
                    rlVertex3f(p1.x, p1.y, p1.z);
                    rlVertex3f(p2.x, p2.y, p2.z);
                }
                rlEnd();
            }

            if (brush.selected || isSelectedContainer) {
                DrawBoundingBox(brush.bounds, Color{255, 200, 0, 255});
            }
        }
    };

    renderBrushList(doc.GetWorldBrushes(), false);

    for (size_t i = 0; i < doc.GetEntities().size(); ++i) {
        auto& ent = doc.GetEntities()[i];
        renderBrushList(ent.brushes, ent.selected);
    }
}

void ViewportRenderer::DrawEntities(map::MapDocument& doc) {
    for (const auto& ent : doc.GetEntities()) {
        Vector3 pos = ent.origin;
        bool sel = ent.selected;

        if (ent.classname == "info_player_start") {
            Color col = sel ? Color{255, 220, 50, 255} : Color{60, 220, 100, 255};
            DrawCylinderWires(pos, 0.35f, 0.35f, 1.75f, 12, col);

            float yawRad = ent.yaw * DEG2RAD;
            Vector3 forward{std::sin(yawRad) * 0.8f, 0.0f, std::cos(yawRad) * 0.8f};
            DrawLine3D(pos, core::Vec3Add(pos, forward), Color{255, 255, 255, 255});
        } else if (ent.classname == "light") {
            Color col = sel ? Color{255, 220, 50, 255} : Color{255, 240, 100, 255};
            DrawSphereWires(pos, 0.25f, 8, 8, col);
            DrawCircle3D(pos, 0.5f, {1, 0, 0}, 90.0f, Color{col.r, col.g, col.b, 100});
        } else if (ent.classname == "prop_search_slot") {
            Color col = sel ? Color{255, 220, 50, 255} : Color{60, 140, 250, 255};
            DrawCubeWires(pos, 0.5f, 0.5f, 0.5f, col);
        } else if (ent.classname == "prop_hide_wardrobe") {
            Color col = sel ? Color{255, 220, 50, 255} : Color{180, 80, 230, 255};
            DrawCubeWires(Vector3{pos.x, pos.y + 1.0f, pos.z}, 0.9f, 2.0f, 0.7f, col);
        } else if (ent.classname == "prop_vent") {
            Color col = sel ? Color{255, 220, 50, 255} : Color{250, 130, 40, 255};
            DrawCubeWires(Vector3{pos.x, pos.y + 0.4f, pos.z}, 0.9f, 0.8f, 1.2f, col);
        } else {
            Color col = sel ? Color{255, 220, 50, 255} : Color{200, 200, 200, 255};
            DrawCubeWires(pos, 0.4f, 0.4f, 0.4f, col);
        }

        if (sel) {
            DrawSphereWires(pos, 0.45f, 6, 6, Color{255, 220, 0, 255});
        }
    }
}

void ViewportRenderer::DrawGizmo(Vector3 position) {
    if (m_transformMode == TransformMode::Select) return;

    float len = 1.3f;
    float arrowHead = 0.25f;

    rlDisableDepthTest();

    DrawLine3D(position, Vector3{position.x + len, position.y, position.z}, Color{235, 50, 50, 255});
    DrawCylinder(Vector3{position.x + len, position.y, position.z}, 0.07f, 0.0f, arrowHead, 8, Color{235, 50, 50, 255});

    DrawLine3D(position, Vector3{position.x, position.y + len, position.z}, Color{50, 225, 50, 255});
    DrawCylinder(Vector3{position.x, position.y + len, position.z}, 0.07f, 0.0f, arrowHead, 8, Color{50, 225, 50, 255});

    DrawLine3D(position, Vector3{position.x, position.y, position.z + len}, Color{50, 100, 245, 255});
    DrawCylinder(Vector3{position.x, position.y, position.z + len}, 0.07f, 0.0f, arrowHead, 8, Color{50, 100, 245, 255});

    DrawCube(position, 0.12f, 0.12f, 0.12f, Color{240, 240, 240, 255});

    rlEnableDepthTest();
}

void ViewportRenderer::Render(map::MapDocument& doc) {
    BeginTextureMode(m_target);
    ClearBackground(Color{30, 32, 38, 255});

    BeginMode3D(m_camera);

    DrawGridFloor();
    DrawBrushes(doc);
    DrawEntities(doc);

    int selBrush = doc.GetSelectedBrushIndex();
    int selEnt = doc.GetSelectedEntityIndex();

    if (selBrush >= 0 && selBrush < static_cast<int>(doc.GetWorldBrushes().size())) {
        DrawGizmo(doc.GetWorldBrushes()[selBrush].center);
    } else if (selEnt >= 0 && selEnt < static_cast<int>(doc.GetEntities().size())) {
        DrawGizmo(doc.GetEntities()[selEnt].origin);
    }

    EndMode3D();

    DrawText("KnopMops 3D Viewport", 12, 12, 14, Color{160, 170, 185, 200});

    std::string modeText = "Tool: ";
    switch (m_transformMode) {
    case TransformMode::Select: modeText += "Select [Q]"; break;
    case TransformMode::Translate: modeText += "Move [W]"; break;
    case TransformMode::Rotate: modeText += "Rotate [E]"; break;
    case TransformMode::Scale: modeText += "Scale [R]"; break;
    }
    DrawText(modeText.c_str(), 12, 30, 13, Color{220, 220, 220, 255});

    std::string snapText = "Snap: " + (m_gridSnap > 0.0f ? std::to_string(m_gridSnap).substr(0, 4) + "m" : "Off");
    DrawText(snapText.c_str(), 12, 46, 13, Color{180, 180, 180, 255});

    EndTextureMode();
}

void ViewportRenderer::HandleMousePicking(Vector2 mouseInViewport, map::MapDocument& doc, bool isMouseDown, bool isMousePressed) {
    if (mouseInViewport.x < 0 || mouseInViewport.y < 0 || mouseInViewport.x >= m_width || mouseInViewport.y >= m_height) {
        m_isDraggingGizmo = false;
        return;
    }

    Ray ray = GetScreenToWorldRay(mouseInViewport, m_camera);

    int selBrush = doc.GetSelectedBrushIndex();
    int selEnt = doc.GetSelectedEntityIndex();
    Vector3 gizmoPos{0, 0, 0};
    bool hasSelection = false;

    if (selBrush >= 0 && selBrush < static_cast<int>(doc.GetWorldBrushes().size())) {
        gizmoPos = doc.GetWorldBrushes()[selBrush].center;
        hasSelection = true;
    } else if (selEnt >= 0 && selEnt < static_cast<int>(doc.GetEntities().size())) {
        gizmoPos = doc.GetEntities()[selEnt].origin;
        hasSelection = true;
    }

    if (hasSelection && isMousePressed) {
        BoundingBox boxX{gizmoPos, Vector3{gizmoPos.x + 1.4f, gizmoPos.y + 0.2f, gizmoPos.z + 0.2f}};
        BoundingBox boxY{gizmoPos, Vector3{gizmoPos.x + 0.2f, gizmoPos.y + 1.4f, gizmoPos.z + 0.2f}};
        BoundingBox boxZ{gizmoPos, Vector3{gizmoPos.x + 0.2f, gizmoPos.y + 0.2f, gizmoPos.z + 1.4f}};

        if (core::RayIntersectsBox(ray, boxX)) {
            m_activeGizmoAxis = 0;
            m_isDraggingGizmo = true;
            m_dragStartPos = gizmoPos;
            m_dragObjStartPos = gizmoPos;
            return;
        } else if (core::RayIntersectsBox(ray, boxY)) {
            m_activeGizmoAxis = 1;
            m_isDraggingGizmo = true;
            m_dragStartPos = gizmoPos;
            m_dragObjStartPos = gizmoPos;
            return;
        } else if (core::RayIntersectsBox(ray, boxZ)) {
            m_activeGizmoAxis = 2;
            m_isDraggingGizmo = true;
            m_dragStartPos = gizmoPos;
            m_dragObjStartPos = gizmoPos;
            return;
        }
    }

    if (m_isDraggingGizmo && isMouseDown) {
        RayCollision col = GetRayCollisionQuad(ray,
            Vector3{gizmoPos.x - 50, gizmoPos.y, gizmoPos.z - 50},
            Vector3{gizmoPos.x + 50, gizmoPos.y, gizmoPos.z - 50},
            Vector3{gizmoPos.x + 50, gizmoPos.y, gizmoPos.z + 50},
            Vector3{gizmoPos.x - 50, gizmoPos.y, gizmoPos.z + 50}
        );

        if (col.hit) {
            Vector3 newPos = m_dragObjStartPos;
            if (m_activeGizmoAxis == 0) newPos.x = core::SnapValue(col.point.x, m_gridSnap);
            else if (m_activeGizmoAxis == 1) newPos.y = core::SnapValue(col.point.y, m_gridSnap);
            else if (m_activeGizmoAxis == 2) newPos.z = core::SnapValue(col.point.z, m_gridSnap);

            Vector3 delta = core::Vec3Sub(newPos, gizmoPos);

            if (selBrush >= 0 && selBrush < static_cast<int>(doc.GetWorldBrushes().size())) {
                auto& b = doc.GetWorldBrushes()[selBrush];
                Vector3 qDelta = core::WorldToQuake(delta);
                for (auto& f : b.faces) {
                    for (int k = 0; k < 3; ++k) f.planePoints[k] = core::Vec3Add(f.planePoints[k], qDelta);
                }
                b.BuildPolygons();
                doc.SetModified(true);
            } else if (selEnt >= 0 && selEnt < static_cast<int>(doc.GetEntities().size())) {
                doc.GetEntities()[selEnt].origin = newPos;
                doc.GetEntities()[selEnt].SyncPropertiesFromOrigin();
                doc.SetModified(true);
            }
        }
        return;
    } else {
        m_isDraggingGizmo = false;
        m_activeGizmoAxis = -1;
    }

    if (isMousePressed && !m_isDraggingGizmo) {
        float closestDist = 1e9f;
        int hitBrush = -1;
        int hitEntity = -1;

        for (size_t i = 0; i < doc.GetWorldBrushes().size(); ++i) {
            float dist = 0.0f;
            if (core::RayIntersectsBox(ray, doc.GetWorldBrushes()[i].bounds, &dist)) {
                if (dist < closestDist) {
                    closestDist = dist;
                    hitBrush = static_cast<int>(i);
                    hitEntity = -1;
                }
            }
        }

        for (size_t i = 0; i < doc.GetEntities().size(); ++i) {
            BoundingBox box{
                core::Vec3Sub(doc.GetEntities()[i].origin, {0.35f, 0.35f, 0.35f}),
                core::Vec3Add(doc.GetEntities()[i].origin, {0.35f, 0.35f, 0.35f})
            };
            float dist = 0.0f;
            if (core::RayIntersectsBox(ray, box, &dist)) {
                if (dist < closestDist) {
                    closestDist = dist;
                    hitEntity = static_cast<int>(i);
                    hitBrush = -1;
                }
            }
        }

        if (hitBrush >= 0) {
            doc.SelectBrush(hitBrush);
        } else if (hitEntity >= 0) {
            doc.SelectEntity(hitEntity);
        } else {
            doc.DeselectAll();
        }
    }
}

}

