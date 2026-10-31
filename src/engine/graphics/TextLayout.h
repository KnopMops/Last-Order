#pragma once
#include "engine/math/Math2D.h"
#include "raylib.h"
#include <string>
#include <vector>

namespace engine::graphics {

class TextLayout {
public:

    static std::vector<std::string> WrapTextUTF8(Font font, const std::string& text,
                                                 float fontSize, float spacing, float maxWidth);

    static float DrawWrapped(Font font, const std::string& text, math::Rect bounds,
                             float fontSize, float lineGap, Color color, float spacing = 1.0f);

    static float MeasureWrappedHeight(Font font, const std::string& text,
                                      float fontSize, float lineGap, float maxWidth, float spacing = 1.0f);

    static void DrawCentered(Font font, const std::string& text,
                             float centerX, float y, float fontSize, Color color, float spacing = 1.0f);
};

}
