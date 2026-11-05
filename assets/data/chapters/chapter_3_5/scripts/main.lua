-- main.lua — Глава 3.5 «Разъезд 86»: старт зависит от концовки Главы 3 (переменная ch3_ending задаётся игрой).
-- Значения: front_door | car | sewer | radio | police

local INTRO = {
  front_door = "Гравий давно кончился. Сосны расступились, и впереди — ручей. По ту сторону воды кто-то стоит и ждёт.",
  car        = "«Копейка» чихнула и умерла у самого ручья. Под капотом Лера успела выдернуть предохранитель — вдруг пригодится.",
  sewer      = "Вода из коллектора была по пояс. Фонарь мигает и сдаётся. Лера выползла на берег и долго лежала лицом в мокрой траве.",
  radio      = "В кармане шипит рация. «Держитесь рельсов, Лера. Я слышу вас». Голос знает её имя. Откуда?",
  police     = "Патрульная машина уехала так быстро, что Лера не успела спросить, почему водитель ни разу не посмотрел в зеркало. На обочине — столб с цифрами: 86.",
}

game.on("level_loaded", function(name)
  if name ~= "razyezd" then return end

  local e = game.get("ch3_ending", "front_door")
  if not INTRO[e] then e = "front_door" end
  local sp = game.find("Spawn_" .. e) or game.find("Spawn_front_door")
  if sp then
    local x, y, z = sp:get_position()
    game.teleport_player(x, y, z, sp.yaw)
  end
  game.log("Глава 3.5 началась с концовки: " .. e)

  game.start(function()
    game.fade(1, 0)
    game.freeze_movement(true)
    game.wait(0.2)
    game.fade(0, 3)
    game.subtitle(INTRO[e], 7)

    if e == "car" then
      game.give_item("item_fuse", true)
      game.wait(7.5)
      game.notify("Получено: Предохранитель (из «Копейки»)")
    elseif e == "sewer" then
      game.set_battery(0.06)
      game.wait(7.5)
      game.notify("Фонарь почти сел. Берегите батарею.")
    elseif e == "radio" then
      game.set("know_code", true)
      game.wait(7.5)
      game.subtitle("Рация: «Это Игнатов, дежурный. Код дрезины — один-один-восемь-шесть. Не забывайте: вода вас защищает».", 7)
      game.wait(7.5)
    elseif e == "police" then
      game.set("know_code", true)
      game.wait(7.5)
      game.notify("В кармане куртки — служебная записка: «Дрезина. Код 1186».")
      game.wait(2)
    else
      game.wait(7.5)
    end

    game.freeze_movement(false)
    game.notify("Лера хромает: бег почти недоступен. [F] — фонарь, [Tab] — инвентарь.")
  end)
end)

game.on("player_caught", function(m)
  game.shake(1.0, 0.6)
end)
