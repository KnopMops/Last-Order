#include "game/chapter3/Ch3Graphics.h"
#include "engine/core/PathUtils.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace game::chapter3 {

namespace {

std::string ConfigPath() {
    return (engine::core::GetUserStorageDir() / "ch3_graphics.cfg").string();
}

constexpr int ROW_COUNT = 16;
constexpr int FPS_LIMITS[8] = {30, 60, 90, 120, 144, 165, 240, 0};

}

Ch3Graphics& Ch3Graphics::Get() {
    static Ch3Graphics instance;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        instance.Load();
    }
    return instance;
}

void Ch3Graphics::Load() {
    std::ifstream f(ConfigPath());
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        int v = std::atoi(line.c_str() + eq + 1);
        if (k == "renderer") renderer = std::clamp(v, 0, 2);
        else if (k == "rayTracing") rayTracing = std::clamp(v, 0, 3);
        else if (k == "renderScale") renderScale = std::clamp(v, 0, 5);
        else if (k == "shadowQuality") shadowQuality = std::clamp(v, 0, 2);
        else if (k == "ssao") ssao = v != 0;
        else if (k == "volumetrics") volumetrics = std::clamp(v, 0, 3);
        else if (k == "bloom") bloom = v != 0;
        else if (k == "grain") grain = v != 0;
        else if (k == "fxaa") fxaa = v != 0;
        else if (k == "brightness") brightness = std::clamp(v, 0, 6);
        else if (k == "fov") fov = std::clamp(v, 55, 95);
        else if (k == "vsync") vsync = v != 0;
        else if (k == "fpsLimit") fpsLimit = std::clamp(v, 0, 7);
        else if (k == "sharpen") sharpen = std::clamp(v, 0, 3);
        else if (k == "showFps") showFps = v != 0;
    }
    horror = 0;
    chromatic = false;
}

void Ch3Graphics::Save() const {
    std::ofstream f(ConfigPath());
    if (!f) return;
    f << "renderer=" << renderer << "\nrayTracing=" << rayTracing << "\nrenderScale=" << renderScale << "\nshadowQuality=" << shadowQuality
      << "\nssao=" << ssao << "\nvolumetrics=" << volumetrics << "\nbloom=" << bloom << "\ngrain=" << grain << "\nfxaa=" << fxaa
      << "\nbrightness=" << brightness << "\nfov=" << fov << "\nvsync=" << vsync << "\nfpsLimit=" << fpsLimit << "\nsharpen=" << sharpen
      << "\nshowFps=" << showFps << "\n";
}

float Ch3Graphics::RenderScaleValue() const {
    static const float v[6] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    return v[std::clamp(renderScale, 0, 5)];
}

float Ch3Graphics::BrightnessValue() const {
    return 0.7f + 0.1f * static_cast<float>(std::clamp(brightness, 0, 6));
}

float Ch3Graphics::HorrorValue() const {
    return 0.0f;
}

int Ch3Graphics::VolumetricSteps() const {
    static const int v[4] = {0, 14, 24, 40};
    return v[std::clamp(volumetrics, 0, 3)];
}

int Ch3Graphics::FpsLimitValue() const {
    return FPS_LIMITS[std::clamp(fpsLimit, 0, 7)];
}

float Ch3Graphics::SharpenValue() const {
    static const float v[4] = {0.0f, 0.25f, 0.5f, 0.8f};
    return v[std::clamp(sharpen, 0, 3)];
}

engine::hd::HdQuality Ch3Graphics::ToQuality() const {
    engine::hd::HdQuality q;
    q.shadowQuality = shadowQuality;
    q.rayTracing = renderer == 2 ? rayTracing : 0;
    q.ssao = ssao;
    q.bloom = bloom;
    q.volumetrics = volumetrics > 0;
    q.volumetricSteps = std::max(8, VolumetricSteps());
    q.fxaa = fxaa;
    return q;
}

void Ch3GraphicsPanel::Open(engine::core::Engine& eng) {
    m_open = true;
    m_row = 0;
    m_caps = engine::hd::ProbeCapabilities(eng.Assets().GetBaseDir());
}

void Ch3GraphicsPanel::Close() {
    m_open = false;
    Ch3Graphics::Get().Save();
}

void Ch3GraphicsPanel::Adjust(int row, int dir) {
    Ch3Graphics& g = Ch3Graphics::Get();
    auto cycle = [dir](int v, int count) { return (v + dir + count) % count; };
    switch (row) {
        case 0:
            g.renderer = cycle(g.renderer, 3);
            if (g.renderer != 2) g.rayTracing = 0;
            break;
        case 1:
            if (g.renderer == 2 && m_caps.dxr && m_caps.dxc) g.rayTracing = cycle(g.rayTracing, 4);
            else g.rayTracing = 0;
            break;
        case 2: g.renderScale = std::clamp(g.renderScale + dir, 0, 5); break;
        case 3: g.shadowQuality = std::clamp(g.shadowQuality + dir, 0, 2); break;
        case 4: g.ssao = !g.ssao; break;
        case 5: g.volumetrics = std::clamp(g.volumetrics + dir, 0, 3); break;
        case 6: g.bloom = !g.bloom; break;
        case 7: g.grain = !g.grain; break;
        case 8: g.fxaa = !g.fxaa; break;
        case 9: g.sharpen = std::clamp(g.sharpen + dir, 0, 3); break;
        case 10: g.vsync = !g.vsync; break;
        case 11: g.fpsLimit = cycle(g.fpsLimit, 8); break;
        case 12: g.showFps = !g.showFps; break;
        case 13: g.brightness = std::clamp(g.brightness + dir, 0, 6); break;
        case 14: g.fov = std::clamp(g.fov + dir * 3, 55, 95); break;
        default: break;
    }
    g.Touch();
}

void Ch3GraphicsPanel::Update(engine::core::Engine& eng) {
    if (!m_open) return;
    if (IsKeyPressed(KEY_ESCAPE)) {
        eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.5f);
        Close();
        return;
    }
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
        m_row = (m_row + ROW_COUNT - 1) % ROW_COUNT;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
    }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
        m_row = (m_row + 1) % ROW_COUNT;
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.4f);
    }
    int dir = 0;
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) dir = -1;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) dir = 1;

    engine::math::Vec2 mpos = eng.Renderer().GetVirtualMousePos();
    bool click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool rclick = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
    for (int r = 0; r < ROW_COUNT; ++r) {
        engine::math::Rect rect = {260.0f, 72.0f + r * 32.0f, 760.0f, 30.0f};
        if (rect.Contains(mpos)) {
            m_row = r;
            if (click) dir = 1;
            if (rclick) dir = -1;
        }
    }
    if (dir != 0) {
        if (m_row == ROW_COUNT - 1) {
            Close();
            return;
        }
        Adjust(m_row, dir);
        eng.Audio().PlaySfx("audio/sfx/sfx_clock_tick.wav", 0.5f);
    }
}

void Ch3GraphicsPanel::Render(engine::core::Engine& eng) const {
    if (!m_open) return;
    Font font = eng.Assets().GetMainFont();
    const Ch3Graphics& g = Ch3Graphics::Get();

    DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 238});
    DrawRectangle(240, 16, 800, 690, Color{42, 23, 21, 250});
    DrawRectangleLinesEx(Rectangle{240, 16, 800, 690}, 3.0f, Color{196, 122, 59, 255});
    engine::graphics::TextLayout::DrawCentered(font, "ГРАФИКА ГЛАВЫ 3", 640.0f, 28.0f, 24.0f, Color{244, 184, 96, 255});

    static const char* rendererNames[3] = {"Classic (OpenGL)", "DirectX 11", "DirectX 12"};
    static const char* rtNames[4] = {"Выкл", "Тени", "Тени + AO", "Тени + AO + отражения"};
    static const char* scaleNames[6] = {"50%", "60%", "70%", "80%", "90%", "100%"};
    static const char* shadowNames[3] = {"Низкое", "Среднее", "Высокое"};
    static const char* volNames[4] = {"Выкл", "Низкое", "Среднее", "Высокое"};
    static const char* sharpNames[4] = {"Выкл", "Слабая", "Средняя", "Сильная"};

    bool hdAvailable = g.renderer != 0;
    bool rtAvailable = g.renderer == 2 && m_caps.dxr && m_caps.dxc;
    char bright[32];
    std::snprintf(bright, sizeof(bright), "%d%%", static_cast<int>(g.BrightnessValue() * 100.0f + 0.5f));
    std::string fov = std::to_string(g.fov) + "°";
    std::string fps = g.FpsLimitValue() == 0 ? "Без лимита" : std::to_string(g.FpsLimitValue()) + " FPS";

    struct Row {
        const char* label;
        std::string value;
        bool enabled;
    };
    Row rows[ROW_COUNT] = {
        {"Движок отрисовки", rendererNames[g.renderer], true},
        {"Трассировка лучей (DXR)", rtAvailable ? rtNames[g.rayTracing] : "Недоступно (нужен DX12 + DXR)", rtAvailable},
        {"Масштаб рендера", scaleNames[g.renderScale], hdAvailable},
        {"Качество теней", shadowNames[g.shadowQuality], hdAvailable},
        {"SSAO (затенение углов)", g.ssao ? "Вкл" : "Выкл", hdAvailable},
        {"Объёмный свет и пыль", volNames[g.volumetrics], hdAvailable},
        {"Bloom (свечение)", g.bloom ? "Вкл" : "Выкл", hdAvailable},
        {"Зерно", g.grain ? "Вкл" : "Выкл", hdAvailable},
        {"Сглаживание FXAA", g.fxaa ? "Вкл" : "Выкл", hdAvailable},
        {"Резкость (апскейл)", sharpNames[g.sharpen], hdAvailable},
        {"Вертикальная синхронизация", g.vsync ? "Вкл" : "Выкл", true},
        {"Ограничение кадров", fps, true},
        {"Показывать FPS", g.showFps ? "Вкл" : "Выкл", true},
        {"Яркость", bright, hdAvailable},
        {"Поле зрения", fov, true},
        {"Назад (ESC)", "", true},
    };

    for (int r = 0; r < ROW_COUNT; ++r) {
        float y = 72.0f + r * 32.0f;
        bool sel = m_row == r;
        if (sel) {
            DrawRectangle(260, static_cast<int>(y), 760, 30, Color{107, 51, 34, 200});
            DrawRectangleLinesEx(Rectangle{260, y, 760, 30}, 1.5f, Color{244, 184, 96, 255});
        }
        Color base = rows[r].enabled ? (sel ? Color{244, 184, 96, 255} : Color{232, 227, 217, 255}) : Color{130, 118, 110, 200};
        DrawTextEx(font, rows[r].label, Vector2{276, y + 5}, 19.0f, 1.0f, base);
        if (!rows[r].value.empty()) {
            std::string v = "< " + rows[r].value + " >";
            DrawTextEx(font, v.c_str(), Vector2{620, y + 5}, 19.0f, 1.0f, rows[r].enabled ? Color{244, 184, 96, 255} : Color{130, 118, 110, 200});
        }
    }

    std::string gpu = "Видеокарта: " + (m_caps.adapter.empty() ? std::string("не определена") : m_caps.adapter);
    DrawTextEx(font, gpu.c_str(), Vector2{262, 590}, 16.0f, 1.0f, Color{188, 178, 162, 255});
    std::string caps = std::string("DX11: ") + (m_caps.d3d11 ? "да" : "нет") + "   DX12: " + (m_caps.d3d12 ? "да" : "нет") +
                       "   Трассировка DXR: " + (m_caps.dxr ? "да" : "нет") + "   DXC: " + (m_caps.dxc ? "да" : "нет");
    DrawTextEx(font, caps.c_str(), Vector2{262, 612}, 16.0f, 1.0f, Color{188, 178, 162, 255});
    DrawTextEx(font, "Движок, трассировка и тени применяются после закрытия настроек.", Vector2{262, 636}, 15.0f, 1.0f, Color{150, 140, 128, 255});
    DrawTextEx(font, "DLSS/FSR: требуют SDK производителя, не подключены. Замена: масштаб рендера + резкость.", Vector2{262, 656}, 15.0f, 1.0f,
               Color{150, 140, 128, 255});
}

}
