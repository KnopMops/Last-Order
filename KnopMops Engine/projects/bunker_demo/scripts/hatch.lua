-- hatch.lua — выходной люк: нужно питание + код (с записки в первой комнате).
exports = { code = "4815", next_level = "level2" }

function on_interact(self)
  if not game.get_flag("power_on") then
    game.notify("Люк обесточен. Нужно запустить генератор.")
    return
  end
  game.ask_code("Введите код люка", self.params.code, function(ok)
    if not ok then
      game.notify("Неверный код.")
      return
    end
    game.notify("Замок открыт.")
    game.start(function()
      game.fade(1, 1.2)
      game.wait(1.3)
      game.load_level(self.params.next_level)
    end)
  end)
end
