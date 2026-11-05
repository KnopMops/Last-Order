-- prowler_alarm.lua — триггер-зона (Collider -> Is Trigger). Один раз привлекает маньяка.
function on_trigger_enter(self, player)
  if self:get_prop("fired") then return end
  self:set_prop("fired", true)
  game.notify("Что-то скрипнуло в темноте...")
  game.alert_monsters(self.x, self.y, self.z)
end
