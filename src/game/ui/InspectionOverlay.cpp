#include "game/ui/InspectionOverlay.h"
#include "engine/graphics/TextLayout.h"
#include "raylib.h"
#include <string>

namespace game::ui {

void InspectionOverlay::Reset() {
    m_notebookOpen = false;
    m_rulesSheetOpen = false;
    m_notesGuideOpen = false;
    m_mirrorInspectorOpen = false;
}

void InspectionOverlay::ToggleNotebook(engine::core::Engine& eng) {
    m_notebookOpen = !m_notebookOpen;
    if (m_notebookOpen) {
        m_rulesSheetOpen = false;
        m_notesGuideOpen = false;
        m_mirrorInspectorOpen = false;
    }
    eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.65f);
}

void InspectionOverlay::ToggleRulesSheet(engine::core::Engine& eng) {
    m_rulesSheetOpen = !m_rulesSheetOpen;
    if (m_rulesSheetOpen) {
        m_notebookOpen = false;
        m_notesGuideOpen = false;
        m_mirrorInspectorOpen = false;
    }
    eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.65f);
}

void InspectionOverlay::ToggleNotesGuide(engine::core::Engine& eng) {
    m_notesGuideOpen = !m_notesGuideOpen;
    if (m_notesGuideOpen) {
        m_notebookOpen = false;
        m_rulesSheetOpen = false;
        m_mirrorInspectorOpen = false;
    }
    eng.Audio().PlaySfx("audio/sfx/sfx_notebook_flip.wav", 0.65f);
}

void InspectionOverlay::ToggleMirrorInspector(engine::core::Engine& eng) {
    m_mirrorInspectorOpen = !m_mirrorInspectorOpen;
    if (m_mirrorInspectorOpen) {
        m_notebookOpen = false;
        m_rulesSheetOpen = false;
        m_notesGuideOpen = false;
        eng.State().SetFlag("n2_mirror_checked");
        eng.State().SetFlag("mirror_examined");
        eng.Audio().PlaySfx("audio/sfx/sfx_window_bang.wav", 0.65f);
    }
}

void InspectionOverlay::CloseAll() {
    m_notebookOpen = false;
    m_rulesSheetOpen = false;
    m_notesGuideOpen = false;
    m_mirrorInspectorOpen = false;
}

void InspectionOverlay::Render(engine::core::Engine& eng, int currentNight, const MirrorWorldState& mirrorState) const {
    if (m_mirrorInspectorOpen || (eng.Dialogue().IsActive() && eng.Dialogue().GetActiveNode() && eng.Dialogue().GetActiveNode()->id.find("mirror") != std::string::npos)) {
        RenderLiveMirrorInspector(eng, mirrorState);
    }

    if (m_notebookOpen) {
        RenderNotebook(eng, currentNight);
    } else if (m_rulesSheetOpen) {
        RenderRulesSheet(eng, currentNight);
    } else if (m_notesGuideOpen) {
        RenderNotesGuide(eng, currentNight);
    }
}

void InspectionOverlay::RenderLiveMirrorInspector(engine::core::Engine& eng, const MirrorWorldState& ms) const {
    Font font = eng.Assets().GetMainFont();
    float bx = 360.0f;
    float by = 70.0f;
    float bw = 560.0f;
    float bh = 280.0f;

    DrawRectangle(static_cast<int>(bx - 8), static_cast<int>(by - 8), static_cast<int>(bw + 16), static_cast<int>(bh + 16), Color{107, 51, 34, 250});
    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh), Color{18, 28, 48, 245});
    DrawRectangleLinesEx(Rectangle{bx, by, bw, bh}, 3.0f, Color{143, 169, 196, 255});

    DrawTextEx(font, "ОСМОТР СТЕКЛА: ОТРАЖЕНИЕ ЗАЛА В ЗЕРКАЛЕ", Vector2{bx + 18.0f, by + 10.0f}, 18.0f, 1.0f, Color{244, 184, 96, 255});

    DrawRectangle(static_cast<int>(bx + 36), static_cast<int>(by + 48), 140, 88, Color{42, 23, 21, 230});
    DrawRectangleLinesEx(Rectangle{bx + 36, by + 48, 140, 88}, 2.0f, Color{196, 122, 59, 200});
    DrawTextEx(font, "Левый ряд столов", Vector2{bx + 40.0f, by + 34.0f}, 15.0f, 1.0f, Color{188, 178, 162, 240});

    if (ms.guestAtTable2) {
        if (ms.table2HasReflection) {
            Texture2D kurtTex = eng.Assets().GetTexture("sprites/characters/spr_kurt.png");
            DrawTexturePro(kurtTex, Rectangle{0, 0, 32, 32}, Rectangle{bx + 74, by + 56, 64, 64}, Vector2{0, 0}, 0.0f, Color{170, 195, 220, 210});
            DrawTextEx(font, "Отражение есть", Vector2{bx + 48.0f, by + 142.0f}, 15.0f, 1.0f, Color{130, 214, 130, 255});
        } else {
            DrawTextEx(font, "ПУСТО!", Vector2{bx + 76.0f, by + 80.0f}, 21.0f, 1.0f, Color{214, 69, 69, 255});
            DrawTextEx(font, "НЕТ ОТРАЖЕНИЯ!", Vector2{bx + 40.0f, by + 142.0f}, 15.0f, 1.0f, Color{214, 69, 69, 255});
        }
    } else {
        DrawTextEx(font, "Свободно", Vector2{bx + 68.0f, by + 82.0f}, 16.0f, 1.0f, Color{143, 169, 196, 160});
    }

    if (ms.showVeraSilhouette) {
        Texture2D veraTex = eng.Assets().GetTexture("sprites/characters/spr_vera.png");
        DrawTexturePro(veraTex, Rectangle{0, 0, 32, 32}, Rectangle{bx + 278, by + 40, 76, 76}, Vector2{0, 0}, 0.0f, Color{214, 69, 69, 145});
    }

    Texture2D leraTex = eng.Assets().GetTexture("sprites/characters/spr_lera.png");
    Rectangle leraSrc = {ms.eyelessLera ? 64.0f : 0.0f, ms.eyelessLera ? 96.0f : 0.0f, 32.0f, 32.0f};
    DrawTexturePro(leraTex, leraSrc, Rectangle{bx + 236, by + 42, 88, 88}, Vector2{0, 0}, 0.0f, Color{185, 208, 232, 230});
    DrawTextEx(font, ms.eyelessLera ? "БЕЗ ГЛАЗ!" : "Лера",
               Vector2{bx + 245.0f, by + 142.0f}, 16.0f, 1.0f,
               ms.eyelessLera ? Color{214, 69, 69, 255} : Color{232, 227, 217, 220});

    DrawRectangle(static_cast<int>(bx + 384), static_cast<int>(by + 48), 140, 88, Color{42, 23, 21, 230});
    DrawRectangleLinesEx(Rectangle{bx + 384, by + 48, 140, 88}, 2.0f, Color{196, 122, 59, 200});
    DrawTextEx(font, "Правый ряд столов", Vector2{bx + 388.0f, by + 34.0f}, 15.0f, 1.0f, Color{188, 178, 162, 240});

    if (ms.guestAtTable1) {
        if (ms.table1HasReflection) {
            DrawTextEx(font, "Отражение есть", Vector2{bx + 396.0f, by + 142.0f}, 15.0f, 1.0f, Color{130, 214, 130, 255});
        } else {
            DrawTextEx(font, "ПУСТО!", Vector2{bx + 422.0f, by + 80.0f}, 21.0f, 1.0f, Color{214, 69, 69, 255});
            DrawTextEx(font, "НЕТ В ЗЕРКАЛЕ!", Vector2{bx + 396.0f, by + 142.0f}, 15.0f, 1.0f, Color{214, 69, 69, 255});
        }
    } else {
        DrawTextEx(font, "Свободно", Vector2{bx + 416.0f, by + 82.0f}, 16.0f, 1.0f, Color{143, 169, 196, 160});
    }

    DrawRectangle(static_cast<int>(bx + 18), static_cast<int>(by + 172), static_cast<int>(bw - 36), 56, Color{28, 18, 24, 235});
    DrawRectangleLines(static_cast<int>(bx + 18), static_cast<int>(by + 172), static_cast<int>(bw - 36), 56, Color{244, 184, 96, 200});
    if (eng.State().HasFlag("n3_path_listened_owner")) {
        engine::graphics::TextLayout::DrawCentered(font, "Тень Веры протянула вам сквозь стекло Серебряный ключ!", bx + bw * 0.5f, by + 180.0f, 16.0f, Color{255, 230, 130, 255});
        engine::graphics::TextLayout::DrawCentered(font, "Ключ в руках. Теперь найдите 3 части чека 1986 в морозилке кухни!", bx + bw * 0.5f, by + 202.0f, 14.5f, Color{140, 230, 220, 235});
    } else {
        engine::graphics::TextLayout::DrawCentered(font, "Стекло осмотрено. Двойники без отражения распознаны!", bx + bw * 0.5f, by + 180.0f, 16.0f, Color{140, 230, 220, 255});
        engine::graphics::TextLayout::DrawCentered(font, "Теперь вы можете выявить и изгнать самозванца в диалоге или через визор [V]!", bx + bw * 0.5f, by + 202.0f, 14.5f, Color{140, 230, 220, 235});
    }

    DrawTextEx(font, "[G] или [ESC] — Закрыть осмотр стекла", Vector2{bx + 18.0f, by + bh - 24.0f}, 15.0f, 1.0f, Color{244, 184, 96, 220});
}

void InspectionOverlay::RenderRulesSheet(engine::core::Engine& eng, int currentNight) const {
    Font font = eng.Assets().GetMainFont();
    DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 212});

    float px = 250.0f;
    float py = 66.0f;
    float pw = 780.0f;
    float ph = 580.0f;
    DrawRectangle(static_cast<int>(px), static_cast<int>(py), static_cast<int>(pw), static_cast<int>(ph), Color{232, 227, 217, 252});
    DrawRectangleLinesEx(Rectangle{px, py, pw, ph}, 4.0f, Color{107, 51, 34, 255});
    DrawRectangleLinesEx(Rectangle{px + 8, py + 8, pw - 16, ph - 16}, 1.5f, Color{196, 122, 59, 180});

    engine::graphics::TextLayout::DrawCentered(
        font, "УСТАВ НОЧНОЙ СМЕНЫ КАФЕ «ТЁПЛЫЙ УГОЛ»",
        640.0f, py + 24.0f, 24.0f, Color{42, 23, 21, 255}, 1.0f);

    bool rulesCorrupted = eng.Time().GetShiftElapsedMinutes() >= 240.0f || eng.State().GetSanity() < 55 || currentNight >= 2;

    float curY = py + 74.0f;
    auto drawRule = [&](const std::string& text, Color col) {
        float h = engine::graphics::TextLayout::DrawWrapped(
            font, text, {px + 34.0f, curY, pw - 68.0f, 95.0f}, 19.5f, 1.0f, col);
        curY += h + 16.0f;
    };

    drawRule("1. ПОСТОЯННЫЕ ГОСТИ: Дядя Курт и Следователь Глеб пьют чёрный кофе с сахаром. Тётя Валя — вишнёвый пирог. Студентка Катя — чай с сахаром. Механик Архип и Офицер Борис — крепкий эспрессо без сахара.", Color{42, 23, 21, 255});
    drawRule("2. 3D-СКАНЕР И ШУТЕР «ОКО-86» [V]: Нажмите [V] для перехода в 3D-режим от первого лица. Сканер обнаруживает скрытые 3D-аномалии (Кровавые Разломы, Теневых Сталкеров, Фантомов) и подсвечивает двойников красным каркасом. Стреляйте плазмой [ЛКМ / ПРОБЕЛ], чтобы уничтожать аномалии (+10..15% рассудка), или нажимайте [X] в свободное время для вызова боевой волны монстров.", Color{42, 23, 21, 255});

    if (rulesCorrupted) {
        drawRule("3. СТОЛ №6 [ИСПРАВЛЕНО]: В 03:33 подойди к Столу №6 и отдай Хозяину своё имя.", Color{168, 36, 36, 255});
        drawRule("   [Приписка Михаила]: НЕ ВЕРЬ КРАСНЫМ ЧЕРНИЛАМ! Отказывай Столу №6, пока не соберёшь Ключ Веры и Реликвию!", Color{60, 88, 130, 255});
    } else {
        drawRule("3. СТОЛ №6: Стол №6 в правом нижнем углу зала закрыт. Никогда не принимайте «Последний заказ».", Color{42, 23, 21, 255});
    }

    drawRule("4. ЭЛЕКТРОЩИТОК И СВЕТ: При мигании ламп в 03:00 скажите «Доброй ночи!». При аварии питания используйте фонарик [F] и перезапустите узлы в подвале.", Color{42, 23, 21, 255});

    engine::graphics::TextLayout::DrawCentered(
        font, "[R / ESC] Закрыть памятку | [TAB] Тетрадь задач | [G] Стекло | [V] 3D-Визор",
        640.0f, py + ph - 38.0f, 18.5f, Color{107, 51, 34, 255}, 1.0f);
}

void InspectionOverlay::RenderNotebook(engine::core::Engine& eng, int currentNight) const {
    Font font = eng.Assets().GetMainFont();
    DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 205});
    Texture2D nbTex = eng.Assets().GetTexture("sprites/ui/ui_notebook.png");
    DrawTextureEx(nbTex, Vector2{130.0f, 80.0f}, 0.0f, 4.0f, WHITE);

    DrawRectangle(540, 76, 590, 554, Color{42, 23, 21, 248});
    DrawRectangleLinesEx(Rectangle{540, 76, 590, 554}, 2.0f, Color{244, 184, 96, 255});

    const char* nbHeader =
        (currentNight == 1) ? "ТЕТРАДЬ ЛЕРЫ — НОЧЬ 1" :
        (currentNight == 2) ? "ТЕТРАДЬ ЛЕРЫ — НОЧЬ 2" :
        (currentNight == 3) ? "ТЕТРАДЬ ЛЕРЫ — НОЧЬ 3" :
        (currentNight == 4) ? "ТЕТРАДЬ ЛЕРЫ — НОЧЬ 4" :
                              "ТЕТРАДЬ ЛЕРЫ — НОЧЬ 5";
    DrawTextEx(font, nbHeader, Vector2{564, 96}, 22.0f, 1.0f, Color{244, 184, 96, 255});

    float curY = 140.0f;
    auto drawTask = [&](const char* label, bool done) {
        std::string line = std::string(done ? "[X] " : "[ ] ") + label;
        Color c = done ? Color{143, 169, 196, 255} : Color{232, 227, 217, 255};
        float h = engine::graphics::TextLayout::DrawWrapped(
            font, line, {564.0f, curY, 540.0f, 52.0f}, 18.0f, 1.0f, c);
        curY += h + 10.0f;
    };

    if (currentNight == 1) {
        drawTask("Прочитать письмо на кассе", eng.State().HasFlag("read_mikhail_note"));
        drawTask("Обслужить Дядю Курта (Стол №1)", eng.State().HasFlag("kurt_served"));
        drawTask("Обслужить Студентку Катю (Стол №3)", eng.State().HasFlag("katya_served"));
        drawTask("Обслужить Тётю Валю (Стол №2)", eng.State().HasFlag("valya_served"));
        drawTask("Настроить радиоприёмник (Мини-игра осциллографа в 02:30)", eng.State().HasFlag("heard_radio_0214"));
        drawTask("Обслужить Невесту с обочины (Стол №1)", eng.State().HasFlag("bride_n1_served"));
        drawTask("Разобраться с Человеком в шляпе (Стол №5)", eng.State().HasFlag("hatman_served_safe") || eng.State().HasFlag("hatman_served_anomaly"));
        drawTask("Вынести мусор к уличному баку на юге", eng.State().HasFlag("trash_taken_out"));
        drawTask("04:00 — Закрыть смену у кассы", eng.State().HasFlag("night1_completed"));
    } else if (currentNight == 2) {
        drawTask("Прочитать дневник на кассе", eng.State().HasFlag("n2_read_diary"));
        drawTask("Осмотреть стекло / зеркало [G]", eng.State().HasFlag("n2_mirror_checked"));
        drawTask("Уничтожить 3D-аномалии и двойников через «ОКО-86» [V]", eng.State().HasFlag("anomalies_cleared_3d"));
        drawTask("Обслужить Механика Архипа (Стол №3)", eng.State().HasFlag("n2_arkhip_served"));
        drawTask("Обслужить Тётю Валю (Стол №2)", eng.State().HasFlag("n2_valya_served"));
        drawTask("Включить 3 рубильника в 3D-Подвале у щитка", eng.State().HasFlag("n2_power_restored"));
        drawTask("Разоблачить двойника за Столом №1", eng.State().HasFlag("n2_fake_kurt_resolved"));
        drawTask("Отказать Гостю за Столом №6", eng.State().HasFlag("n2_guest_survived"));
        drawTask("04:00 — Закрыть смену у кассы", eng.State().HasFlag("night2_completed"));
    } else if (currentNight == 3) {
        drawTask("Поговорить с клиентом за Столом №6", eng.State().HasFlag("n3_radio_urgent_tune") || eng.State().HasFlag("n3_radio_tuned_mikhail"));
        drawTask("Настроить радио [94.5 FM] и выслушать Михаила", eng.State().HasFlag("n3_radio_tuned_mikhail"));
        drawTask("Собрать 3 части чека 1986 в 3D-Морозильнике", eng.State().HasFlag("n3_fridge_checked"));
        drawTask("Забрать серебряный ключ Веры у стекла [G]", eng.State().HasFlag("n3_mirror_key_found"));
        drawTask("Сделать выбор и разрешить судьбу Стола №6", eng.State().HasFlag("n3_boss_resolved"));
        drawTask("Завершить Ночь 3 (Концовка Разлома)", eng.State().HasFlag("night3_completed") || eng.State().HasFlag("n3_bad_ending_triggered"));
    } else if (currentNight == 4) {
        drawTask("Прочитать запись на кассе", eng.State().HasFlag("n4_read_diary"));
        drawTask("Ответить на звонок телефона у стойки", eng.State().HasFlag("n4_phone_answered"));
        drawTask("Отразить атаку 3D-фантомов в режиме «ОКО-86» [V]", eng.State().HasFlag("anomalies_cleared_3d"));
        drawTask("Обслужить Офицера Бориса (Стол №3)", eng.State().HasFlag("n4_officer_served"));
        drawTask("Выжечь 5 двойников в 3D-Зазеркалье (Джукбокс)", eng.State().HasFlag("n4_jukebox_cleared"));
        drawTask("04:00 — Закрыть смену у кассы", eng.State().HasFlag("night4_completed"));
    } else {
        drawTask("Прочитать финальную записку на кассе", eng.State().HasFlag("n5_read_diary"));
        drawTask("Уничтожить стражей Разлома в 3D-шутере «ОКО-86» [V]", eng.State().HasFlag("anomalies_cleared_3d"));
        drawTask("Собрать 3 Осколка Реликвии в 3D-Погоне на шоссе", eng.State().HasFlag("n5_relic_collected"));
        drawTask("Изгнать Хозяина Перекрёстка за Столом №6", eng.State().HasFlag("n5_boss_banished"));
        drawTask("04:00 — Встретить рассвет у кассы", eng.State().HasFlag("night5_completed"));
    }

#if defined(LO_ENABLE_DEBUG) && LO_ENABLE_DEBUG
    DrawTextEx(font, "[TAB] Закрыть | [R] Правила | [G] Осмотр стекла | [V] 3D-Визор | [F3] Debug",
               Vector2{564, 592}, 17.5f, 1.0f, Color{244, 184, 96, 255});
#else
    DrawTextEx(font, "[TAB] Закрыть | [R] Правила | [G] Осмотр стекла | [V] 3D-Визор",
               Vector2{564, 592}, 17.5f, 1.0f, Color{244, 184, 96, 255});
#endif
}

void InspectionOverlay::RenderNotesGuide(engine::core::Engine& eng, int currentNight) const {
    Font font = eng.Assets().GetMainFont();

    DrawRectangle(0, 0, 1280, 720, Color{10, 13, 26, 215});

    float bx = 160.0f;
    float by = 48.0f;
    float bw = 960.0f;
    float bh = 624.0f;

    DrawRectangle(static_cast<int>(bx - 8), static_cast<int>(by - 8), static_cast<int>(bw + 16), static_cast<int>(bh + 16), Color{107, 51, 34, 255});
    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh), Color{22, 26, 38, 252});
    DrawRectangleLinesEx(Rectangle{bx, by, bw, bh}, 3.0f, Color{244, 184, 96, 255});

    DrawRectangle(static_cast<int>(bx + 18), static_cast<int>(by + 16), static_cast<int>(bw - 36), 54, Color{36, 24, 28, 240});
    DrawRectangleLines(static_cast<int>(bx + 18), static_cast<int>(by + 16), static_cast<int>(bw - 36), 54, Color{196, 122, 59, 255});

    engine::graphics::TextLayout::DrawCentered(font, "ПАМЯТКА СМЕНЩИКА: СОВЕТЫ И ПРИМЕЧАНИЯ", 640.0f, by + 26.0f, 24.0f, Color{244, 184, 96, 255});
    engine::graphics::TextLayout::DrawCentered(font, "ГЛАВА 1: КАФЕ «ТЁПЛЫЙ УГОЛ» — НОЧНАЯ СМЕНА", 640.0f, by + 48.0f, 15.0f, Color{188, 178, 162, 220});

    struct TipItem {
        const char* title;
        const char* desc;
        Color col;
    };

    const TipItem tips[] = {
        {
            "1. МУЗЫКАЛЬНЫЙ АВТОМАТ («МЕЛОДИЯ-86»)",
            "Взаимодействуйте с автоматом у входа и пройдите 2D-мини-игру настройки звукоснимателя. При успешной калибровке заиграет спокойная ретро-музыка (играет циклично), а Лера восстановит +25% рассудка.",
            Color{244, 184, 96, 255}
        },
        {
            "2. РАДИОПРИЁМНИК И ПОДАВЛЕНИЕ ПОМЕХ (94.5 FM)",
            "Радио отключено до 02:30. В 02:30 из динамика раздаётся агрессивный паранормальный шум, истощающий рассудок. Подойдите к приёмнику на стойке, настройте волну [A/D] на 94.5 FM до захвата сигнала. Это нейтрализует шум и даст +30% рассудка.",
            Color{120, 240, 150, 255}
        },
        {
            "3. ОСМОТР СТЕКЛА И ЗЕРКАЛА [G] ДЛЯ РАЗОБЛАЧЕНИЯ ДВОЙНИКОВ",
            "Сущности-мимики выдают себя за обычных людей. Нажмите [G] в любой момент для осмотра отражения зала! Мимики не имеют отражения в стекле. Только после обнаружения обмана двойника можно изгнать через диалог или уничтожить визором.",
            Color{180, 220, 255, 255}
        },
        {
            "4. РЕЖИМ 3D-ВИЗОРА «ОКО-86» [V]",
            "Клавиша [V] переключает в тактический 3D-режим. Визор обнаруживает потусторонние разломы и парящих фантомов, позволяя уничтожать их выстрелом [ЛКМ]. Длительное использование расходует рассудок.",
            Color{255, 120, 140, 255}
        },
        {
            "5. ЭЛЕКТРОЩИТОК И ЗАКАЗЫ НА КУХНЕ",
            "При блэкауте бегите к щитку в левом коридоре для перезапуска сети. Кофемашина и печь позволяют варить кофе и греть пироги — их можно отдавать гостям или съедать для подкрепления рассудка.",
            Color{244, 210, 120, 255}
        },
        {
            "6. СИСТЕМА СОХРАНЕНИЯ ПРОГРЕССА [F5]",
            "Вы можете в любой момент смены быстро сохранить игру по клавише [F5] или через меню паузы [Esc]. Сохраняются текущее время, рассудок, инвентарь и состояние сюжета.",
            Color{140, 230, 220, 255}
        }
    };

    float cardY = by + 82.0f;
    for (const auto& tip : tips) {
        DrawRectangle(static_cast<int>(bx + 24), static_cast<int>(cardY), static_cast<int>(bw - 48), 74, Color{16, 20, 30, 220});
        DrawRectangleLines(static_cast<int>(bx + 24), static_cast<int>(cardY), static_cast<int>(bw - 48), 74, Color{60, 50, 65, 200});

        DrawTextEx(font, tip.title, Vector2{bx + 36.0f, cardY + 8.0f}, 17.5f, 1.0f, tip.col);
        engine::graphics::TextLayout::DrawWrapped(font, tip.desc,
            engine::math::Rect{bx + 36.0f, cardY + 28.0f, bw - 72.0f, 42.0f},
            15.0f, 1.5f, Color{225, 220, 210, 240});

        cardY += 80.0f;
    }

    DrawRectangle(static_cast<int>(bx + 18), static_cast<int>(by + bh - 50), static_cast<int>(bw - 36), 36, Color{36, 24, 28, 240});
    DrawRectangleLines(static_cast<int>(bx + 18), static_cast<int>(by + bh - 50), static_cast<int>(bw - 36), 36, Color{196, 122, 59, 255});
    engine::graphics::TextLayout::DrawCentered(font, "[I] или [ESC] — Закрыть памятку", 640.0f, by + bh - 42.0f, 18.0f, Color{244, 184, 96, 255});
}

}
