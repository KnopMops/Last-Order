-- Триггер-подсказка: один раз показывает текст из параметра.
exports = { text = "" }

function on_trigger_enter(self, player)
  if self:get_prop("done", false) then return end
  self:set_prop("done", true)
  game.subtitle(self.params.text, 5)
end
