-- generator.lua — генератор: нужен предохранитель, включает питание, шумит на весь бункер.
exports = { required_item = "item_fuse" }

local function refresh_lamps()
  local on = game.get_flag("power_on")
  for _, lamp in ipairs(game.find_all("powered_lamp")) do
    lamp:set_light(on)
  end
end

function on_start(self)
  refresh_lamps()
end

function on_interact(self)
  if game.get_flag("power_on") then
    game.notify("Генератор уже работает.")
    return
  end
  if game.has_item(self.params.required_item) then
    game.take_item(self.params.required_item)
    game.set_flag("power_on", true)
    game.play_sound("audio/sfx/sfx_generator_start.wav", 1.0)
    game.shake(0.5, 1.2)
    game.make_noise(self.x, self.y, self.z, 18)  -- маньяк услышит
    refresh_lamps()
    game.notify("Питание включено! Люк разблокирован. Но шум привлёк кого-то...")
  else
    game.notify("Не хватает предохранителя.")
  end
end
