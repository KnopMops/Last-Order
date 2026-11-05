#pragma once

#include "raylib.h"
#include "map/MapData.h"
#include "scene/Scene.h"
#include <string>
#include <vector>

namespace knopmops::render {

enum class ViewportShading {
    Lit,        // освещение как в игре (лампы + ambient)
    Textured,
    Wireframe,
    FlatLit
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
    void SetFont(Font f) { m_font = f; m_hasFont = true; }

    void Update(float dt, bool isHovered, bool isFocused);
    void Render(map::MapDocument& doc, scene::Scene& scene);

    const RenderTexture2D& GetRenderTexture() const { return m_target; }
    Camera3D& GetCamera() { return m_camera; }
    const Camera3D& GetCamera() const { return m_camera; }

    void FocusOn(Vector3 targetPos, float dist = 6.0f);
    void FrameAll(map::MapDocument& doc, scene::Scene& scene); // показать всю карту
    void SetTopView();
    Vector3 GetOrbitTarget() const { return m_cameraTarget; }
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
    bool IsShowColliders() const { return m_showColliders; }
    void SetShowColliders(bool v) { m_showColliders = v; }
    bool IsHideCeilings() const { return m_hideCeilings; }
    void SetHideCeilings(bool v) { m_hideCeilings = v; }

    // true, если пользователь тащит гизмо (чтобы UI не перехватывал клики)
    bool IsDragging() const { return m_isDraggingGizmo; }
    // Позиция выделенного (объект сцены, браш или сущность карты); false — ничего не выбрано.
    bool GetSelectionPosition(map::MapDocument& doc, scene::Scene& scene, Vector3* out) const;

    void HandleMousePicking(Vector2 mouseInViewport, map::MapDocument& doc, scene::Scene& scene, bool isMouseDown, bool isMousePressed);

private:
    RenderTexture2D m_target{};
    int m_width = 1280;
    int m_height = 720;
    Font m_font{};
    bool m_hasFont = false;

    Camera3D m_camera{};
    float m_cameraYaw = 45.0f;
    float m_cameraPitch = 30.0f;
    float m_cameraDist = 12.0f;
    Vector3 m_cameraTarget{0.0f, 1.0f, 0.0f};

    ViewportShading m_shading = ViewportShading::Lit;
    TransformMode m_transformMode = TransformMode::Translate;
    TransformSpace m_transformSpace = TransformSpace::Global;
    float m_gridSnap = 0.5f;
    bool m_showGrid = true;
    bool m_showColliders = true;
    bool m_hideCeilings = true;

    int m_activeGizmoAxis = -1;
    bool m_isDraggingGizmo = false;
    float m_dragStartT = 0.0f;
    Vector3 m_dragObjStartPos{};
    Vector3 m_dragLastPos{};
    Vector2 m_dragStartMouse{};
    float m_dragStartValue = 0.0f;
    Vector3 m_dragStartScale{1, 1, 1};

    void DrawGridFloor();
    void DrawBrushes(map::MapDocument& doc);
    void DrawEntities(map::MapDocument& doc);
    void DrawSceneObjects(scene::Scene& scene);
    void DrawGizmo(Vector3 position);
    void DrawLabels(scene::Scene& scene);
    void CollectLights(scene::Scene& scene);
    Color Shade(Vector3 p, Vector3 n) const;
    void DrawBrushesLit(map::MapDocument& doc);
    void DrawColliderShape(const scene::GameObject& o, Vector3 wp, Color c);

    struct LitVertex { Vector3 p; Vector2 uv; Vector3 n; };
    struct LitBatch { std::string texture; std::vector<LitVertex> verts; };
    struct LightS { Vector3 pos; Vector3 color; float radius; };
    std::vector<LitBatch> m_litBatches;
    std::vector<LightS> m_lights;
    float m_ambient = 0.1f;
    size_t m_litKey = 0;
    Vector3 m_dragOffset{};
};

}
