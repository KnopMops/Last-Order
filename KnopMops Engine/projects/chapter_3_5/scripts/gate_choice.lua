-- Выбор у ворот «Заслона» и финал главы 3.5.
-- Переменная ch35_took_tapes (true/false) попадёт в Главу 4.
exports = { take = false }

local function say(text, secs)
  game.subtitle(text, secs)
  game.wait(secs + 0.5)
end

function on_interact(self)
  if game.get_flag("choice_done") then return end
  game.set_flag("choice_done", true)
  local take = self.params.take
  game.set("ch35_took_tapes", take)
  game.lock_player(true)

  game.start(function()
    if take then
      say("Лера сунула кассеты в рюкзак. Динамик над воротами на секунду замолчал.", 4)
      say("Игнатов: «Вы взяли кассеты. Что ж... Тамара ждала именно этого. Идите».", 5)
    else
      say("Кассеты легли на стол. Что-то щёлкнуло, и стол медленно уехал в стену.", 4)
      say("Игнатов: «Спасибо, Лера. Так будет спокойнее для всех. Тамара ждёт вас внизу».", 5)
    end

    local gate = game.find("Ворота")
    gate:unlock()
    gate:open()
    game.shake(0.8, 1.5)
    game.play_sound("audio/sfx/sfx_door_smash.wav", 0.9)
    say("Ворота «Заслона» открываются. Из глубины тянет озоном и холодным кафелем.", 5)

    -- Лера входит в коридор с фотографиями смен
    game.teleport_player(88.5, 0.05, 0, 90)
    game.lock_player(false)
    game.freeze_movement(true)
    game.fade(0.55, 1.5)
    say("На стене — ряд фотографий: ночные смены кафе «Тёплый угол». Десять улыбающихся людей в фартуках.", 6)
    say("На последней — Лера. Тот же бейдж, та же усталая улыбка. Подпись: «Смена №11. Три года назад».", 6)
    say("За спиной глухо падает створка — ворота закрылись. Лампы гаснут одна за другой.", 5)
    game.shake(1.2, 1.2)
    say("Эхо — голосом из всех динамиков сразу: «Одиннадцатая принята. Добро пожаловать на смену, Лера».", 7)
    game.fade(1, 2)
    game.wait(2.4)
    game.win("ГЛАВА 3.5 ЗАВЕРШЕНА", "Впереди — Глава 4: «Заслон». Тамара жива. Но чтобы вывести её наверх, придётся пройти каждую смену, которую не пережили остальные десять.")
  end)
end
