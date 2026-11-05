-- «Стоячий»: двигается только когда игрок на него НЕ смотрит (и не видит через стену).
-- Умная навигация: идёт к игроку по кратчайшему пути через проёмы и двери, а не толпится у стены.
-- Касание не убивает, но возвращает к входу в депо.
local FOV_HALF = 50
local SPEED = 1.7

function on_start(self)
  self:freeze(true)
  self:set_mode("idle")
  local x, y, z = self:get_position()
  self:set_prop("hx", x)
  self:set_prop("hz", z)
end

local function reset_all()
  for _, m in ipairs(game.find_all("mimic")) do
    local _, y = m:get_position()
    m:set_position(m:get_prop("hx", 0), y, m:get_prop("hz", 0))
  end
end

function on_update(self, dt)
  if game.get_flag("mimic_busy") or game.player_hidden() then return end
  local px, py, pz = game.player_pos()
  local x, y, z = self:get_position()
  local dx, dz = px - x, pz - z
  local dist = math.sqrt(dx * dx + dz * dz)
  if dist > 40 then return end

  -- игрок смотрит на «стоячего»: в конус зрения И без стены между ними
  local ang = math.deg(math.atan(x - px, z - pz))
  local diff = (ang - game.player_yaw() + 540) % 360 - 180
  local looking = math.abs(diff) < FOV_HALF and self:visible_to_player()
  if looking then return end

  if dist < 1.1 then
    game.set_flag("mimic_busy", true)
    game.start(function()
      game.shake(1.0, 0.6)
      game.fade(1, 0.2)
      game.notify("Холодные пальцы коснулись плеча...")
      game.wait(0.5)
      game.teleport_player(14.2, 0, 21.2, game.player_yaw())
      reset_all()
      game.fade(0, 0.8)
      game.wait(1.0)
      game.set_flag("mimic_busy", false)
    end)
    return
  end

  -- путь с обходом стен; рядом с игроком шаг короче, чтобы не «слипаться»
  self:walk_to(px, pz, SPEED, dt)

  if dist < 9 then
    local nb = self:get_prop("nb", 0) - dt
    if nb <= 0 then
      game.play_sound("audio/sfx/sfx_stalker_breathe.wav", 0.8)
      nb = 5
    end
    self:set_prop("nb", nb)
  end
end
