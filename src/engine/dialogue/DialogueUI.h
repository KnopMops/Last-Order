#pragma once
#include "engine/dialogue/DialogueSystem.h"
#include "raylib.h"

namespace engine::dialogue {

class DialogueUI {
public:
    void Render(const DialogueSystem& sys, Font font, int screenW, int screenH) const;
};

}
