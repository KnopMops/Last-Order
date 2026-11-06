-- dinner_trap.lua — Сцена ловушки с обеденным столом (Baker dinner scene)

local triggered = false

function on_interact()
  if triggered then
    game.notify("На столе остатки гниющей плоти и черные свечи.")
    return
  end
  triggered = true

  game.start(function()
    game.freeze_movement(true)
    game.play_sfx("audio/sfx/sfx_radio_tune.wav", 0.9)
    game.subtitle("На стуле сидит манекен в желтом дождевике Леры. В кармане крутится магнитофон...", 5)
    game.wait(5.2)

    game.play_sfx("audio/sfx/sfx_cat_hiss.wav", 0.8)
    game.subtitle("Голос из динамика искажается: «И ты поверила, Тамара?! Ты сама пришла прямо в пасть!»", 6)
    game.wait(6.2)

    game.play_sfx("audio/sfx/sfx_door_smash.wav", 1.0)
    game.shake(0.8, 0.6)

    local boss = game.find("Игнатов_Босс")
    if boss then
      boss:set_position(14.0, 0.4, 0.0)
      boss:set_mode("chase")
    end

    game.subtitle("ИГНАТОВ ВЫЛАМЫВАЕТ ДВЕРЬ С КРИКОМ: «ДОБРО ПОЖАЛОВАТЬ К ОБЕДУ, ТАМАРОЧКА!»", 6)
    game.wait(2.0)
    game.freeze_movement(false)
    game.notify("ПВП С БОССОМ НАЧАЛОСЬ! ЦЕЛЬТЕСЬ В ГОЛОВУ ИЗ ТОЗ-34 [ЛКМ]!")
  end)
end
