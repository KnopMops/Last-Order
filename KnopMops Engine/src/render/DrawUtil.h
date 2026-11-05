#pragma once

#include "raylib.h"
#include "rlgl.h"

namespace knopmops::render {

// raylib 5.5 убрал DrawCubeTexture — рисуем текстурированный бокс сами (тайлинг: 1 текстура = 1 м).
inline void DrawTexturedBox(Texture2D tex, Vector3 c, float w, float h, float l, Color col) {
    float x = c.x, y = c.y, z = c.z;
    float hw = w * 0.5f, hh = h * 0.5f, hl = l * 0.5f;
    rlBegin(RL_QUADS);
    rlSetTexture(tex.id); // ПОСЛЕ rlBegin: смена режима в rlBegin сбрасывает текстуру на дефолтную
    rlColor4ub(col.r, col.g, col.b, col.a);
    rlNormal3f(0, 0, 1);
    rlTexCoord2f(0, 0); rlVertex3f(x - hw, y - hh, z + hl);
    rlTexCoord2f(w, 0); rlVertex3f(x + hw, y - hh, z + hl);
    rlTexCoord2f(w, h); rlVertex3f(x + hw, y + hh, z + hl);
    rlTexCoord2f(0, h); rlVertex3f(x - hw, y + hh, z + hl);
    rlNormal3f(0, 0, -1);
    rlTexCoord2f(w, 0); rlVertex3f(x - hw, y - hh, z - hl);
    rlTexCoord2f(w, h); rlVertex3f(x - hw, y + hh, z - hl);
    rlTexCoord2f(0, h); rlVertex3f(x + hw, y + hh, z - hl);
    rlTexCoord2f(0, 0); rlVertex3f(x + hw, y - hh, z - hl);
    rlNormal3f(0, 1, 0);
    rlTexCoord2f(0, l); rlVertex3f(x - hw, y + hh, z - hl);
    rlTexCoord2f(0, 0); rlVertex3f(x - hw, y + hh, z + hl);
    rlTexCoord2f(w, 0); rlVertex3f(x + hw, y + hh, z + hl);
    rlTexCoord2f(w, l); rlVertex3f(x + hw, y + hh, z - hl);
    rlNormal3f(0, -1, 0);
    rlTexCoord2f(w, l); rlVertex3f(x - hw, y - hh, z - hl);
    rlTexCoord2f(0, l); rlVertex3f(x + hw, y - hh, z - hl);
    rlTexCoord2f(0, 0); rlVertex3f(x + hw, y - hh, z + hl);
    rlTexCoord2f(w, 0); rlVertex3f(x - hw, y - hh, z + hl);
    rlNormal3f(1, 0, 0);
    rlTexCoord2f(l, 0); rlVertex3f(x + hw, y - hh, z - hl);
    rlTexCoord2f(l, h); rlVertex3f(x + hw, y + hh, z - hl);
    rlTexCoord2f(0, h); rlVertex3f(x + hw, y + hh, z + hl);
    rlTexCoord2f(0, 0); rlVertex3f(x + hw, y - hh, z + hl);
    rlNormal3f(-1, 0, 0);
    rlTexCoord2f(0, 0); rlVertex3f(x - hw, y - hh, z - hl);
    rlTexCoord2f(l, 0); rlVertex3f(x - hw, y - hh, z + hl);
    rlTexCoord2f(l, h); rlVertex3f(x - hw, y + hh, z + hl);
    rlTexCoord2f(0, h); rlVertex3f(x - hw, y + hh, z - hl);
    rlEnd();
    rlSetTexture(0);
}

}
