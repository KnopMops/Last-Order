-- dumbwaiter.lua — Бельевой подъемник (связь между этажами)

function on_interact()
  local lvl = game.level()

  if lvl == "mansion_ground" then
    game.start(function()
      game.freeze_movement(true)
      game.play_sfx("audio/sfx/sfx_pipe_valve.wav", 0.9)
      game.notify("Шахта бельевого лифта ведет прямо в подвал.")

      if game.has_item("item_garage_key") then
        game.subtitle("Тамара: «Дверь гаража наверху заперта этим ключом. Пора проверить, что там!»", 5)
        game.freeze_movement(false)
        return
      end

      game.subtitle("Тамара стучит по тросу: «Лера! Ты слышишь меня?! Если ты там внизу — отправь мне ключ!»", 6)
      game.wait(6.2)

      game.fade(1, 1.5)
      game.wait(1.6)
      game.load_level("mansion_sub")
    end)

  elseif lvl == "mansion_sub" then
    game.start(function()
      game.freeze_movement(true)
      game.play_sfx("audio/sfx/sfx_pipe_valve.wav", 0.9)

      if game.has_item("item_garage_key") then
        game.subtitle("Лера кладет найденный латунный ключ от гаража в корзину лифта и тянет трос вверх...", 6)
        game.take_item("item_garage_key")
        game.wait(6.2)

        game.fade(1, 1.5)
        game.wait(1.6)
        game.give_item("item_garage_key", true)
        game.load_level("mansion_ground")
      else
        game.subtitle("Лера: «Лифт работает! Мне нужно найти в морге или котельной ключ от мастерской для Тамары!»", 6)
        game.wait(6.2)
        game.freeze_movement(false)
      end
    end)
  end
end
