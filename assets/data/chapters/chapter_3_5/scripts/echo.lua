-- Эхо (сущность): стоит по ту сторону воды и не может пересечь ручей. Только шепчет.
local LINES = {
  "Один заказ.",
  "Последний.",
  "Ты опоздала на смену.",
  "Я записала твой заказ, Лера.",
  "Иди сюда. Здесь тепло.",
}

function on_start(self)
  self:freeze(true)
  self:set_mode("idle")
  self:set_prop("t", 3)
  self:set_prop("i", 0)
end

function on_update(self, dt)
  if game.level() ~= "razyezd" then return end
  local x, y, z = self:get_position()
  local px, py, pz = game.player_pos()
  self:set_rotation(math.deg(math.atan(px - x, pz - z)))
  if self:distance_to_player() > 26 then return end
  local t = self:get_prop("t", 0) - dt
  if t <= 0 then
    local i = self:get_prop("i", 0) % #LINES + 1
    self:set_prop("i", i)
    game.subtitle("Эхо: «" .. LINES[i] .. "»", 3.5)
    t = 18
  end
  self:set_prop("t", t)
end
