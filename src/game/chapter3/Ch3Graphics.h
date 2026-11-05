#pragma once
#include "engine/core/Engine.h"
#include "engine/hd/HdRenderer.h"
#include <string>

namespace game::chapter3 {

enum class Ch3Renderer {
    Classic = 0,
    D3D11 = 1,
    D3D12 = 2
};

struct Ch3Graphics {
    int renderer = 1;
    int rayTracing = 0;
    int renderScale = 5;
    int shadowQuality = 2;
    bool ssao = true;
    int volumetrics = 2;
    bool bloom = true;
    int horror = 0;
    bool grain = true;
    bool chromatic = true;
    bool fxaa = true;
    int brightness = 3;
    int fov = 68;
    bool vsync = true;
    int fpsLimit = 1;
    int sharpen = 1;
    bool showFps = false;
    int revision = 0;

    static Ch3Graphics& Get();
    void Load();
    void Save() const;
    void Touch() { ++revision; }

    float RenderScaleValue() const;
    float BrightnessValue() const;
    float HorrorValue() const;
    int VolumetricSteps() const;
    int FpsLimitValue() const;
    float SharpenValue() const;
    engine::hd::HdQuality ToQuality() const;
};

class Ch3GraphicsPanel {
public:
    void Open(engine::core::Engine& eng);
    bool IsOpen() const { return m_open; }
    void Close();
    void Update(engine::core::Engine& eng);
    void Render(engine::core::Engine& eng) const;

private:
    void Adjust(int row, int dir);

    bool m_open = false;
    int m_row = 0;
    engine::hd::HdCapabilities m_caps;
};

}
