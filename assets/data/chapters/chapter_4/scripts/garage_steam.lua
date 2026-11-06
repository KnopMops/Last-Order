-- garage_steam.lua — Загадка с паровой магистралью в мастерской

local valve_fixed = false

function on_interact()
  if valve_fixed then
    game.notify("Вентиль повернут: паровая завеса сжигает щупальца Сущности у спуска в бункер.")
    return
  end

  if game.has_item("item_valve_wheel") then
    valve_fixed = true
    game.take_item("item_valve_wheel")
    game.play_sfx("audio/sfx/sfx_pipe_valve.wav", 1.0)
    game.notify("Чугунный маховик установлен и повернут до упора!")
    game.subtitle("С шипением вырывается раскаленный пар! Черная биомасса, блокировавшая лестницу, сгорает и испаряется!", 6)
    game.play_sfx("audio/sfx/sfx_chase_flash_burn.wav", 1.0)
    game.shake(0.5, 0.4)
  else
    game.play_sfx("audio/sfx/sfx_door_locked_rattle.wav", 0.7)
    game.notify("Шток парового клапана сорван. Нужен чугунный маховик [item_valve_wheel].")
  end
end
