-- main.lua — Глава 4 «Дом на сопке: Финал»
-- Механики в стиле Resident Evil 7 Biohazard:
-- - Сталкер-босс (дежурный Игнатов) с патрулями, ломанием дверей, монологами
-- - PvP-сражения из двустволки ТОЗ-34 (ЛКМ - огонь, R - перезарядка)
-- - Умные часы CODEX RE7 (FINE / CAUTION / DANGER кардиограмма, [H] - медпакет)
-- - Сейфзона с магнитофоном и расслабляющей музыкой
-- - Параллельное прохождение: Тамара (верх) и Лера (подземный сектор)

local TAMARA_INTRO = "Гроза над сопкой не утихает. Старый УАЗ заглох перед черными коваными воротами. Тамара проверяет патронник ТОЗ-34: два патрона с картечью. Рация хрипит голосом Леры... но что-то в этом голосе звучит неправильно."

game.on("level_loaded", function(name)
  if name == "mansion_ground" then
    game.log("Загружен уровень: Усадьба Заслона (Наземный сектор)")
    game.enable_health(true, 100)
    game.play_ambient("audio/bgm/amb_motel_storm_lp.wav", 0.6)

    game.start(function()
      game.fade(1, 0)
      game.freeze_movement(true)
      game.wait(0.3)
      game.fade(0, 2.5)
      game.subtitle(TAMARA_INTRO, 8)
      game.wait(8.2)

      game.freeze_movement(false)
      game.notify("Тамара: [ЛКМ] Залп из ТОЗ-34 | [R] Перезарядка | [H] Медпакет")
      game.notify("Исследуйте усадьбу и найдите источник радиосигнала.")
    end)
  elseif name == "mansion_sub" then
    game.log("Загружен уровень: Подземелье Усадьбы (Сектор Леры)")
    game.enable_health(true, 75)
    game.play_ambient("audio/bgm/amb_basement_lp.ogg", 0.7)

    game.start(function()
      game.fade(1, 0)
      game.freeze_movement(true)
      game.wait(0.3)
      game.fade(0, 2.0)
      game.subtitle("Лера очнулась на холодном кафеле. Голова раскалывается. Где-то наверху раздаются тяжелые шаги и треск дерева...", 7)
      game.wait(7.2)
      game.freeze_movement(false)
      game.notify("Лера: скрытность — единственный шанс. Найдите инструмент и подъемник к Тамаре.")
    end)
  end
end)

game.on("player_damaged", function(dmg, hp)
  game.shake(0.35, 0.3)
end)
