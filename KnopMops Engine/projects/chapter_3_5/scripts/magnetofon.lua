local TAPES = {
  { id = "item_tape_1", lines = {
    { "Тамара: «Смена как смена. Только зеркала в кафе у меня пустые, понимаешь? Я в них не вижу себя».", 6 },
    { "Тамара: «Записываю, чтобы не сойти с ума. Если кто-то слушает — не принимайте заказов после трёх ночи».", 6 },
  } },
  { id = "item_tape_2", lines = {
    { "Повар: «Посетители не едят. Ни разу. Я проверял подвал — там стоит ещё один стол, накрытый на одиннадцать персон».", 7 },
    { "Повар: «Я пойду посмотрю, кто его накрыл. Если я не вернусь — это не шутка».", 5 },
  } },
  { id = "item_tape_3", lines = {
    { "Незнакомый голос: «Заказ принят. Номер заказа — одиннадцать. Объект — восемьдесят шесть. Сменщица опоздает. Это нормально — они всегда опаздывают».", 8 },
    { "Незнакомый голос: «Мы оформили допуск. Код дрезины: один, один, восемь, шесть. Дежурный Игнатов, Заслон».", 7 },
  } },
}

local function all_heard()
  for i = 1, #TAPES do
    if not game.get_flag("heard_" .. i) then return false end
  end
  return true
end

function on_interact(self)
  if self:get_prop("busy", false) then return end

  for i, t in ipairs(TAPES) do
    if game.has_item(t.id) and not game.get_flag("heard_" .. i) then
      self:set_prop("busy", true)
      game.lock_player(true)
      game.play_sound("audio/sfx/sfx_radio_click.wav", 0.9)

      local total = 0
      for _, l in ipairs(t.lines) do total = total + l[2] + 0.4 end

      game.play_tv(i, total)
      game.cutscene_camera(1.80, 1.25, 22.10, 1.60, 1.08, 23.60)
      game.notify("Воспроизведение: Кассета №" .. i .. " на телевизоре")

      game.start(function()
        for _, l in ipairs(t.lines) do
          if not game.is_tv_playing() then break end
          game.subtitle(l[1], l[2])
          game.wait(l[2] + 0.4)
        end
        game.stop_tv()
        game.cutscene_camera_end()
        game.set_flag("heard_" .. i, true)
        if i == 3 then game.set("know_code", true) end
        game.lock_player(false)
        self:set_prop("busy", false)
        if i == 3 then game.notify("Код дрезины запомнен: 1186") end
      end)
      return
    end
  end

  local found = 0
  for _, t in ipairs(TAPES) do if game.has_item(t.id) then found = found + 1 end end
  if all_heard() then
    game.notify("Все кассеты прослушаны. Код дрезины: 1186. Включите телевизор для повтора видео.")
  else
    game.notify("В магнитофоне пусто. Найдено кассет: " .. found .. " из 3. Включите телевизор для повтора.")
  end
end
