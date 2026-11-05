#pragma once

#include "raylib.h"
#include "map/MapData.h"

namespace knopmops::render {

enum class ViewportShading {
    Textured,
    Wireframe,
    FlatLit,
    CollisionHulls
};

enum class TransformMode {
    Select,
    Translate,
    Rotate,
    Scale
};

enum class TransformSpace {
    Global,
    Local
};

class ViewportRenderer {
public:
    ViewportRenderer();
    ~ViewportRenderer();

    void Init(int width, int height);
    void Resize(int width, int height);
    void Shutdown();

    void Update(float dt, bool isHovered, bool isFocused);
    void Render(map::MapDocument& doc);

    const RenderTexture2D& GetRenderTexture() const { return m_target; }
    Camera3D& GetCamera() { return m_camera; }
    const Camera3D& GetCamera() const { return m_camera; }

    void FocusOn(Vector3 targetPos, float dist = 6.0f);
    void ResetCamera();

    ViewportShading GetShadingMode() const { return m_shading; }
    void SetShadingMode(ViewportShading mode) { m_shading = mode; }

    TransformMode GetTransformMode() const { return m_transformMode; }
    void SetTransformMode(TransformMode mode) { m_transformMode = mode; }

    TransformSpace GetTransformSpace() const { return m_transformSpace; }
    void SetTransformSpace(TransformSpace space) { m_transformSpace = space; }

    float GetGridSnap() const { return m_gridSnap; }
    void SetGridSnap(float snap) { m_gridSnap = snap; }

    bool IsShowGrid() const { return m_showGrid; }
    void SetShowGrid(bool show) { m_showGrid = show; }

    void HandleMousePicking(Vector2 mouseInViewport, map::MapDocument& doc, bool isMouseDown, bool isMousePressed);

private:
    RenderTexture2D m_target{};
    int m_width = 1280;
    int m_height = 720;

    Camera3D m_camera{};
    float m_cameraYaw = 45.0f;
    float m_cameraPitch = 30.0f;
    float m_cameraDist = 12.0f;
    Vector3 m_cameraTarget{0.0f, 1.0f, 0.0f};

    ViewportShading m_shading = ViewportShading::Textured;
    TransformMode m_transformMode = TransformMode::Translate;
    TransformSpace m_transformSpace = TransformSpace::Global;
    float m_gridSnap = 0.5f;
    bool m_showGrid = true;

    int m_activeGizmoAxis = -1;
    bool m_isDraggingGizmo = false;
    Vector3 m_dragStartPos{};
    Vector3 m_dragObjStartPos{};

    void DrawGridFloor();
    void DrawBrushes(map::MapDocument& doc);
    void DrawEntities(map::MapDocument& doc);
    void DrawGizmo(Vector3 position);
    void DrawCompassOverlay();
};

}

