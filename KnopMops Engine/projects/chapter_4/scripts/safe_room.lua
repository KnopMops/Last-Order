-- safe_room.lua — Механика Сейфзоны (Safe Haven / Cassette Save)

local in_safe = false

game.on("trigger_enter", function(trigger, player)
  if trigger:name() == "Trigger_SafeRoom" then
    in_safe = true
    game.set("in_safe_room", true)
    game.play_ambient("audio/bgm/amb_cafe_calm_lp.ogg", 0.7)
    game.notify("БЕЗОПАСНАЯ ЗОНА: Здесь враги не могут достать вас.")

    local boss = game.find("Игнатов_Босс")
    if boss then
      boss:set_mode("patrol")
    end
  end
end)

game.on("trigger_exit", function(trigger, player)
  if trigger:name() == "Trigger_SafeRoom" then
    in_safe = false
    game.set("in_safe_room", false)
    game.play_ambient("audio/bgm/amb_motel_storm_lp.wav", 0.6)
  end
end)

function on_interact()
  game.play_sfx("audio/sfx/sfx_radio_click.wav", 0.9)
  game.play_sfx("audio/sfx/sfx_coffee_sip.wav", 0.8)
  game.heal_player(25.0)
  game.notify("Диктофонная запись сохранена. Рассудок восстановлен (+25 HP).")
  game.subtitle("Тамара: «Запись номер 14. Я в усадьбе. Игнатов обезумел или это больше не он. Нужно вытащить Леру...»", 6)
end
