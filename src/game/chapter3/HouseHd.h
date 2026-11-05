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

struct HouseHdActor {
    Vector3 pos{};
    int kind = 0;
    float alpha = 1.0f;
    bool dead = false;
    float deadT = 0.0f;
};

struct HouseHdInput {
    Camera3D camera{};
    float time = 0.0f;
    float dt = 0.016f;
    bool flashOn = false;
    Vector3 flashPos{};
    Vector3 flashDir{0.0f, 0.0f, 1.0f};
    float flashPower = 1.0f;
    Vector3 prowlerPos{};
    float prowlerYaw = 0.0f;
    bool rtRoom = false;
    bool hideProwler = false;
    std::vector<HouseHdActor> actors;
    bool prowlerStunned = false;
    bool prowlerChasing = false;
    float prowlerStunLeft = 0.0f;
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
    bool Ready() const { return m_main.renderer != nullptr; }
    bool HasRtBackend() const { return m_rt.renderer != nullptr; }
    const std::string& Info() const { return m_info; }
    int LastDrawn() const { return m_cur->renderer ? m_cur->renderer->DrawnMeshes() : 0; }
    int MeshCount() const { return static_cast<int>(m_main.meshTags.size()); }

private:
    struct Backend {
        std::unique_ptr<engine::hd::IHdRenderer> renderer;
        std::vector<std::pair<int, std::string>> meshTags;
        std::vector<std::pair<std::string, int>> textureCache;
        int phantomTex = 0;
        int phantomBackTex = 0;
        int glowTex = 0;
        int flashlightTex = 0;
        int rescuerTex = 0;
        int leraTex = 0;
    };
    bool InitBackend(Backend& b, engine::core::Engine& eng, const HouseLevel& level, engine::hd::HdApi api, const engine::hd::HdQuality& quality);
    void ShutdownBackend(Backend& b);
    int TextureHandle(engine::core::Engine& eng, const std::string& name);
    int ItemTexture(engine::core::Engine& eng, const std::string& icon);
    void AddTrees(engine::core::Engine& eng, const HouseLevel& level);
    void EnsureOverlay(int w, int h);

    Backend m_main;
    Backend m_rt;
    bool m_forceRt = false;
    Backend* m_cur = &m_main;
    const HouseLevel* m_level = nullptr;
    float m_outside = 0.0f;
    Texture2D m_overlay{};
    std::string m_info;
};

}
