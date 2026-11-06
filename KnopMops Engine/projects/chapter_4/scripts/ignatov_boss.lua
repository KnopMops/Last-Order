-- ignatov_boss.lua — AI Босса Дежурного Игнатова (Jack Baker archetype)

local boss = nil
local boss_hp = 200.0
local is_stunned = false
local downed_count = 0

local VOICE_CHASE = {
  "Игнатов: «Тамара! Брось ружьишко! Семья за столом заждалась!»",
  "Игнатов: «Ты закон здесь?! Закон теперь Я, Волкова!»",
  "Игнатов: «От судьбы не убежишь, капитан! Стол уже накрыт!»",
  "Игнатов: «Куда ты прячешься?! Я чую твой страх!»"
}

local VOICE_STUN = {
  "Игнатов: «Ах ты сука!.. Больно же!.. Но меня этим не убить!»",
  "Игнатов: «Хороший выстрел... А теперь МОЯ ОЧЕРЕДЬ!»",
  "Игнатов: «Ты только злишь меня, девка!»"
}

game.on("level_loaded", function(name)
  if name ~= "mansion_ground" then return end
  boss = game.find("Игнатов_Босс")
  boss_hp = 200.0
  is_stunned = false
end)

game.on("monster_mode", function(obj, mode)
  if obj:name() ~= "Игнатов_Босс" then return end
  if mode == "chase" then
    local phrase = VOICE_CHASE[math.random(#VOICE_CHASE)]
    game.subtitle(phrase, 4.5)
    game.play_sfx("audio/sfx/sfx_stalker_breathe.wav", 1.0)
  end
end)

game.on("monster_hit", function(obj, is_headshot, damage)
  if obj:name() ~= "Игнатов_Босс" then return end

  boss_hp = boss_hp - damage
  game.log("Босс Игнатов получил урон: " .. tostring(damage) .. " (Осталось HP: " .. tostring(boss_hp) .. ")")

  if is_headshot then
    game.subtitle(VOICE_STUN[math.random(#VOICE_STUN)], 4)
    game.shake(0.4, 0.4)
    obj:freeze(true)
    is_stunned = true

    game.after(4.5, function()
      if boss and boss_hp > 0 then
        obj:freeze(false)
        obj:set_mode("chase")
        is_stunned = false
        game.subtitle("Игнатов: «Я ЕЩЕ НЕ ЗАКОНЧИЛ!»", 3)
      end
    end)
  end

  if boss_hp <= 0 then
    downed_count = downed_count + 1
    game.subtitle("Игнатов с глухим стоном падает на колени и растворяется в черной слизи...", 6)
    game.play_sfx("audio/sfx/sfx_body_thud.wav", 1.0)
    obj:freeze(true)
    obj:set_position(999, -50, 999)

    game.notify("Босс временно повержен! Выиграно драгоценное время!")
    game.after(45.0, function()
      game.subtitle("Из трещин в стенах раздается глухой рык: Игнатов вернулся!", 5)
      game.play_sfx("audio/sfx/sfx_door_smash.wav", 1.0)
      obj:set_position(14.0, 0.4, 0.0)
      obj:freeze(false)
      boss_hp = 180.0
      obj:set_mode("investigate")
    end)
  end
end)
