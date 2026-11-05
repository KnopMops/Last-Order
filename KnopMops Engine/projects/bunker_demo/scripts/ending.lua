-- ending.lua — триггер финала главы.
function on_trigger_enter(self, player)
  game.win("ГЛАВА ПРОЙДЕНА", "Ты выбрался из бункера. Но это ещё не конец...")
end
