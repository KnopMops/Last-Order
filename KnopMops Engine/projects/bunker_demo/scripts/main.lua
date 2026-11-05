-- main.lua — глобальный скрипт главы (аналог Autoload в Godot).
-- Живёт всю игру, переживает смену уровней. Здесь удобно подписываться на события.
--
-- События: level_loaded(name), item_added(id), item_removed(id), item_used(id),
--          player_caught(monster), monster_mode(monster, mode), noise(x,y,z,radius),
--          trigger_enter(obj), slot_searched(obj, item), player_hidden(obj), player_unhidden(),
--          var_changed(key, value), chapter_won()

game.on("level_loaded", function(name)
  game.log("Загружен уровень: " .. name)
  if name == "level1" then
    -- вступительная сцена: корутина с game.wait()
    game.start(function()
      game.fade(1, 0)
      game.lock_player(true)
      game.wait(0.1)
      game.fade(0, 2.5)
      game.subtitle("Голова раскалывается... Где я? За стеной кто-то ходит.", 4.5)
      game.wait(4.5)
      game.lock_player(false)
      game.notify("WASD — ходьба, Shift — бег, Ctrl — присесть, E — действие, F — фонарь, Tab — инвентарь")
    end)
  end
end)

game.on("player_caught", function(monster)
  game.shake(1.0, 0.6)
end)

game.on("item_added", function(id)
  if id == "item_bunker_key" then
    game.subtitle("Ключ от шлюза. Теперь можно выбраться из этой комнаты.", 3.5)
  end
end)
