#include "engine/dialogue/DialogueUI.h"
#include "engine/graphics/TextLayout.h"
#include <algorithm>
#include <cstdio>

namespace engine::dialogue {

void DialogueUI::Render(const DialogueSystem& sys, Font font, int screenW, int screenH) const {
    if (!sys.IsActive()) return;

    const DialogueLine* line = sys.GetCurrentLine();
    if (!line) return;

    const int boxMarginX = 90;
    const int boxW = screenW - boxMarginX * 2;
    const float textMaxW = static_cast<float>(boxW - 56);

    float wrappedTextH = graphics::TextLayout::MeasureWrappedHeight(font, line->text, 21.0f, 5.0f, textMaxW);
    int minTextAreaH = std::max(72, static_cast<int>(wrappedTextH) + 24);

    int choicesAreaH = 0;
    if (sys.IsWaitingForChoice()) {
        choicesAreaH = static_cast<int>(sys.GetAvailableChoices().size()) * 38 + 20;
    } else {
        choicesAreaH = 34;
    }

    const int boxH = minTextAreaH + choicesAreaH + 28;
    const int boxX = boxMarginX;
    const int boxY = screenH - boxH - 36;

    DrawRectangle(boxX - 4, boxY - 4, boxW + 8, boxH + 8, Color{10, 13, 26, 245});
    DrawRectangle(boxX, boxY, boxW, boxH, Color{42, 23, 21, 246});
    DrawRectangleLinesEx(Rectangle{static_cast<float>(boxX), static_cast<float>(boxY),
                                   static_cast<float>(boxW), static_cast<float>(boxH)},
                         2.5f, Color{196, 122, 59, 255});

    if (!line->speaker.empty()) {
        Vector2 spkSize = MeasureTextEx(font, line->speaker.c_str(), 21.0f, 1.0f);
        int tagW = static_cast<int>(spkSize.x) + 32;
        int tagH = 34;
        DrawRectangle(boxX + 18, boxY - tagH + 4, tagW, tagH, Color{107, 51, 34, 255});
        DrawRectangleLinesEx(Rectangle{static_cast<float>(boxX + 18), static_cast<float>(boxY - tagH + 4),
                                       static_cast<float>(tagW), static_cast<float>(tagH)},
                             2.0f, Color{244, 184, 96, 255});
        DrawTextEx(font, line->speaker.c_str(),
                   Vector2{static_cast<float>(boxX + 34), static_cast<float>(boxY - tagH + 10)},
                   21.0f, 1.0f, Color{244, 184, 96, 255});
    }

    int contentTopY = boxY + 20;
    if (sys.IsWaitingForChoice() && sys.HasActiveTimeout()) {
        float ratio = sys.GetTimeoutRemaining() / sys.GetTimeoutTotal();
        int barW = static_cast<int>((boxW - 48) * ratio);
        DrawRectangle(boxX + 24, boxY + 12, boxW - 48, 8, Color{10, 13, 26, 255});
        DrawRectangle(boxX + 24, boxY + 12, barW, 8, Color{214, 69, 69, 255});
        contentTopY += 8;
    }

    std::string visText = sys.GetVisibleTextUTF8();
    graphics::TextLayout::DrawWrapped(
        font, visText,
        math::Rect{static_cast<float>(boxX + 28), static_cast<float>(contentTopY), textMaxW, static_cast<float>(minTextAreaH)},
        21.0f, 5.0f, Color{232, 227, 217, 255});

    if (sys.IsWaitingForChoice()) {
        const auto& choices = sys.GetAvailableChoices();
        int startY = contentTopY + minTextAreaH;
        for (size_t i = 0; i < choices.size(); ++i) {
            bool selected = (static_cast<int>(i) == sys.GetSelectedChoiceIndex());
            int cy = startY + static_cast<int>(i) * 36;

            if (selected) {
                DrawRectangle(boxX + 22, cy - 3, boxW - 44, 32, Color{107, 51, 34, 225});
                DrawRectangleLinesEx(Rectangle{static_cast<float>(boxX + 22), static_cast<float>(cy - 3),
                                               static_cast<float>(boxW - 44), 32.0f},
                                     1.5f, Color{244, 184, 96, 255});
            }

            char prefix[32];
            std::snprintf(prefix, sizeof(prefix), "%s [%zu] ", selected ? "►" : " ", i + 1);
            std::string fullChoice = std::string(prefix) + choices[i].text;
            Color col = selected ? Color{244, 184, 96, 255} : Color{188, 178, 162, 255};
            graphics::TextLayout::DrawWrapped(
                font, fullChoice,
                math::Rect{static_cast<float>(boxX + 30), static_cast<float>(cy + 3), textMaxW - 12.0f, 28.0f},
                19.5f, 2.0f, col);
        }
    } else {
        const char* prompt = sys.IsLineFullyRevealed()
            ? "[E / ПРОБЕЛ / ЛКМ] Далее ►"
            : "[E / ПРОБЕЛ / ЛКМ] Показать сразу ►";
        Vector2 pSize = MeasureTextEx(font, prompt, 18.0f, 1.0f);
        DrawTextEx(font, prompt,
                   Vector2{static_cast<float>(boxX + boxW - pSize.x - 24), static_cast<float>(boxY + boxH - 28)},
                   18.0f, 1.0f, Color{244, 184, 96, 235});
    }
}

}
