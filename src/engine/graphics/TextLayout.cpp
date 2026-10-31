#include "engine/graphics/TextLayout.h"
#include <sstream>

namespace engine::graphics {

std::vector<std::string> TextLayout::WrapTextUTF8(Font font, const std::string& text,
                                                  float fontSize, float spacing, float maxWidth) {
    std::vector<std::string> lines;
    if (text.empty() || maxWidth <= 10.0f) return lines;

    std::stringstream paraStream(text);
    std::string paragraph;
    while (std::getline(paraStream, paragraph, '\n')) {
        if (paragraph.empty()) {
            lines.push_back("");
            continue;
        }

        std::string currentLine;
        size_t i = 0;
        while (i < paragraph.size()) {

            while (i < paragraph.size() && paragraph[i] == ' ') {
                if (!currentLine.empty()) currentLine += ' ';
                i++;
            }
            if (i >= paragraph.size()) break;

            size_t wordStart = i;
            while (i < paragraph.size() && paragraph[i] != ' ') {
                i++;
            }
            std::string word = paragraph.substr(wordStart, i - wordStart);

            std::string candidate = currentLine.empty() ? word : (currentLine + word);
            Vector2 sz = MeasureTextEx(font, candidate.c_str(), fontSize, spacing);

            if (sz.x <= maxWidth || currentLine.empty()) {
                currentLine = candidate;
            } else {

                while (!currentLine.empty() && currentLine.back() == ' ') {
                    currentLine.pop_back();
                }
                lines.push_back(currentLine);
                currentLine = word;
            }
        }

        if (!currentLine.empty()) {
            while (!currentLine.empty() && currentLine.back() == ' ') {
                currentLine.pop_back();
            }
            lines.push_back(currentLine);
        }
    }

    return lines;
}

float TextLayout::DrawWrapped(Font font, const std::string& text, math::Rect bounds,
                              float fontSize, float lineGap, Color color, float spacing) {
    auto lines = WrapTextUTF8(font, text, fontSize, spacing, bounds.w);
    float curY = bounds.y;
    float stepY = fontSize + lineGap;

    for (const auto& line : lines) {
        if (curY + fontSize > bounds.y + bounds.h + 4.0f) {
            break;
        }
        DrawTextEx(font, line.c_str(), Vector2{bounds.x, curY}, fontSize, spacing, color);
        curY += stepY;
    }
    return curY - bounds.y;
}

float TextLayout::MeasureWrappedHeight(Font font, const std::string& text,
                                       float fontSize, float lineGap, float maxWidth, float spacing) {
    auto lines = WrapTextUTF8(font, text, fontSize, spacing, maxWidth);
    if (lines.empty()) return 0.0f;
    return static_cast<float>(lines.size()) * (fontSize + lineGap) - lineGap;
}

void TextLayout::DrawCentered(Font font, const std::string& text,
                              float centerX, float y, float fontSize, Color color, float spacing) {
    Vector2 sz = MeasureTextEx(font, text.c_str(), fontSize, spacing);
    DrawTextEx(font, text.c_str(), Vector2{centerX - sz.x * 0.5f, y}, fontSize, spacing, color);
}

}
