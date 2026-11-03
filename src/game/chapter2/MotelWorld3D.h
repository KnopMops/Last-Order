#pragma once
#include "engine/core/Engine.h"
#include "game/chapter2/FPSCamera.h"
#include "raylib.h"
#include <string>
#include <vector>

namespace game::chapter2 {

enum class InteractiveType {
    None,
    EntranceDoor,
    DeskBell,
    RadioTransistor,
    Key101Tag,
    DoorRoom101,
    WardrobeCloset,
    BathroomMirror,
    HighwayCar
};

struct InteractiveObject3D {
    InteractiveType type = InteractiveType::None;
    BoundingBox bounds{};
    std::string label;
    bool active = true;
};

struct RainDrop3D {
    Vector3 pos{};
    float speed = 12.0f;
    float len = 0.5f;
};

class MotelWorld3D {
public:
    void Init(engine::core::Engine& eng, int nightNumber);
    void Update(engine::core::Engine& eng, float dt);
    void Render3D(engine::core::Engine& eng);
    void RenderUI(engine::core::Engine& eng) const;
    void Shutdown(engine::core::Engine& eng);

    bool WantsReturnToMenu() const { return m_returnToMenu; }

private:
    void BuildCollidersAndInteractives();
    void UpdateRain(float dt);
    void HandleInteraction(engine::core::Engine& eng);
    void RenderInspectOverlay(Font font) const;

    void DrawFloorQuad(Texture2D tex, float x0, float z0, float x1, float z1, float y, float uTile, float vTile, Color tint) const;
    void DrawCeilingQuad(Texture2D tex, float x0, float z0, float x1, float z1, float y, float uTile, float vTile, Color tint) const;
    void DrawWallQuadX(Texture2D tex, float x, float y0, float z0, float y1, float z1, float uTile, float vTile, Color tint) const;
    void DrawWallQuadZ(Texture2D tex, float z, float x0, float y0, float x1, float y1, float uTile, float vTile, Color tint) const;
    void DrawBoxTextured(Texture2D sideTex, Texture2D topTex, Vector3 min, Vector3 max, Color tint) const;

    FPSCamera m_camera{};
    std::vector<BoundingBox> m_colliders;
    std::vector<InteractiveObject3D> m_interactives;
    std::vector<RainDrop3D> m_rain;

    Texture2D m_texWall{};
    Texture2D m_texCarpet{};
    Texture2D m_texTile{};
    Texture2D m_texConcrete{};
    Texture2D m_texDoor{};
    Texture2D m_texCeiling{};
    Texture2D m_texNeon{};
    Texture2D m_texKeyTag{};
    Texture2D m_texMirror{};
    Texture2D m_texRadio{};

    Shader m_litShader{};
    int m_uViewPosLoc = -1;
    int m_uFlashlightPosLoc = -1;
    int m_uFlashlightDirLoc = -1;
    int m_uFlashlightColorLoc = -1;
    int m_uFlashlightCutoffLoc = -1;
    int m_uFlashlightOuterCutoffLoc = -1;
    int m_uFlashlightIntensityLoc = -1;
    int m_uAmbientColorLoc = -1;
    int m_uFogColorLoc = -1;
    int m_uFogStartLoc = -1;
    int m_uFogEndLoc = -1;
    bool m_shaderLoaded = false;

    InteractiveType m_focusedType = InteractiveType::None;
    std::string m_focusPrompt;

    bool m_flashlightOn = true;
    float m_flashlightBattery = 100.0f;
    bool m_flashlightFocus = false;
    float m_flashlightFlickerTimer = 0.0f;

    bool m_hasKey101 = false;
    bool m_door101Open = false;
    bool m_inWardrobe = false;
    int m_bellRings = 0;
    bool m_radioPlaying = false;
    float m_radioPlaybackTimer = 0.0f;
    bool m_mirrorTriggered = false;
    float m_mirrorAnomalyAlpha = 0.0f;

    bool m_inspectMode = false;
    InteractiveType m_inspectedItem = InteractiveType::None;
    float m_inspectRotX = 0.0f;
    float m_inspectRotY = 0.0f;
    std::string m_inspectTitle;
    std::string m_inspectDesc;

    std::string m_bannerText;
    float m_bannerTimer = 0.0f;

    int m_stepSoundIndex = 0;
    bool m_returnToMenu = false;
    int m_nightNumber = 1;
    float m_globalTime = 0.0f;
};

}
