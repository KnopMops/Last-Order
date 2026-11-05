-- Пульт дрезины: нужны предохранитель, ключ стрелки и код.
exports = { code = "1186", next_level = "tunnel" }

function on_interact(self)
  local need = {}
  if not game.has_item("item_fuse") then need[#need + 1] = "предохранитель" end
  if not game.has_item("item_switch_key") then need[#need + 1] = "ключ стрелки" end
  if #need > 0 then
    game.notify("Не хватает: " .. table.concat(need, ", "))
    return
  end
  game.ask_code("Код пульта дрезины", self.params.code, function(ok)
    if not ok then
      game.notify("Неверный код. Кассеты в будке подскажут.")
      return
    end
    game.take_item("item_fuse")
    game.play_sound("audio/sfx/sfx_generator_start.wav", 1.0)
    game.start(function()
      game.lock_player(true)
      game.subtitle("Дрезина дёргается и ползёт на восток — в туннель.", 4)
      game.fade(1, 2)
      game.wait(2.4)
      game.load_level(self.params.next_level)
    end)
  end)
end
