#pragma once
#include "engine/core/Engine.h"
#include "engine/hd/HdRenderer.h"
#include "game/chapter3/HouseLevel.h"
#include "raylib.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace game::chapter3 {

struct HouseHdItem {
    Vector3 pos{};
    std::string icon;
};

struct HouseHdInput {
    Camera3D camera{};
    float time = 0.0f;
    float dt = 0.016f;
    bool flashOn = false;
    Vector3 flashPos{};
    Vector3 flashDir{0.0f, 0.0f, 1.0f};
    float flashPower = 1.0f;
    Vector3 grannyPos{};
    bool grannyStunned = false;
    bool grannyChasing = false;
    float grannyStunLeft = 0.0f;
    int day = 1;
    float fear = 0.0f;
    float heartbeat = 0.0f;
    std::vector<unsigned char> lampOn;
    std::vector<HouseHdItem> items;
    std::function<bool(const std::string&)> batchVisible;
};

class HouseHd {
public:
    bool Init(engine::core::Engine& eng, const HouseLevel& level, engine::hd::HdApi api);
    void Shutdown(engine::core::Engine& eng);
    void Render(engine::core::Engine& eng, const HouseHdInput& in);
    bool Ready() const { return m_renderer != nullptr; }
    const std::string& Info() const { return m_info; }
    int LastDrawn() const { return m_renderer ? m_renderer->DrawnMeshes() : 0; }
    int MeshCount() const { return static_cast<int>(m_meshTags.size()); }

private:
    int TextureHandle(engine::core::Engine& eng, const std::string& name);
    int ItemTexture(engine::core::Engine& eng, const std::string& icon);
    void AddTrees(engine::core::Engine& eng, const HouseLevel& level);
    void EnsureOverlay(int w, int h);

    std::unique_ptr<engine::hd::IHdRenderer> m_renderer;
    std::vector<std::pair<int, std::string>> m_meshTags;
    std::vector<std::pair<std::string, int>> m_textureCache;
    const HouseLevel* m_level = nullptr;
    int m_phantomTex = 0;
    int m_glowTex = 0;
    int m_flashlightTex = 0;
    float m_outside = 0.0f;
    Texture2D m_overlay{};
    std::string m_info;
};

}
