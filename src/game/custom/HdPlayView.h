#pragma once
#include "engine/core/Engine.h"
#include "engine/hd/HdRenderer.h"
#include "raylib.h"
#include "runtime/ChapterProject.h"
#include "runtime/PlaySession.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace game::custom {

class HdPlayView {
public:
    bool Init(engine::core::Engine& eng, knopmops::runtime::PlaySession& session, const knopmops::runtime::ChapterProject& project, engine::hd::HdApi api);
    void Shutdown(engine::core::Engine& eng);
    bool Ready() const { return m_renderer != nullptr; }
    bool NeedsRebuild(const knopmops::runtime::PlaySession& s) const { return m_renderer && s.GeometryRevision() != m_revision; }
    void Render(engine::core::Engine& eng, knopmops::runtime::PlaySession& session);
    const std::string& Info() const { return m_info; }

private:
    struct ObjMesh { int handle; uint32_t objId; bool door; };
    struct SlideGroup { uint32_t objId; float x0; float step; std::vector<int> handles; int shown = -1; };

    int Tex(const std::string& name);
    void EnsureOverlay(int w, int h);
    void AddBox(std::vector<engine::hd::HdVertex>& out, Vector3 c, Vector3 size, float yawDeg);
    void AddCylinder(std::vector<engine::hd::HdVertex>& out, Vector3 c, float radius, float height);

    std::unique_ptr<engine::hd::IHdRenderer> m_renderer;
    std::map<std::string, int> m_textures;
    std::vector<ObjMesh> m_objMeshes;
    std::vector<SlideGroup> m_slides;
    std::string m_texDir;
    std::string m_assetTexDir;
    std::string m_info;
    int m_revision = -1;
    int m_monsterTex = -1;
    int m_mimicTex = -1;
    int m_glowTex = -1;
    float m_outside = 0.0f;
    bool m_forceRt = false;
    Texture2D m_overlay{};
};

}
