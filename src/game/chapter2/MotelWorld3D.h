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

    bool m_returnToMenu = false;
    int m_nightNumber = 1;
    float m_globalTime = 0.0f;
};

}
