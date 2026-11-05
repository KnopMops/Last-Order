-- Спокойная поездка на дрезине: плавный разгон и торможение, тихие события.
-- Узел «Маршрут» запускает сценарий при старте уровня.
local TOP_SPEED = 1.9   -- м/с
local START_X, END_X = 2, 67

local function lamps(on)
  for _, l in ipairs(game.find_all("tunnel_lamp")) do l:set_light(on) end
end

-- плавная кривая скорости: разгон на первых метрах, торможение на последних
local function speed_at(x)
  local ease_in = math.min(1, (x - START_X) / 6 + 0.12)
  local ease_out = math.min(1, (END_X - x) / 6 + 0.08)
  return TOP_SPEED * math.max(0.12, math.min(ease_in, ease_out))
end

local function place(x, dres, echo, back)
  game.teleport_player(x, 0.05, 0, game.player_yaw())
  dres:set_position(x, 0.2, 0)
  echo:set_position(x - 9 + (back or 0), 0, 0)
end

-- движение каждый кадр с реальным dt (без рывков)
local function ride(x, to, dres, echo, per_frame)
  local last = game.time()
  while x < to do
    game.wait(0)
    local now = game.time()
    local dt = math.min(now - last, 0.05)
    last = now
    x = x + speed_at(x) * dt
    place(x, dres, echo)
    if per_frame then
      local r = per_frame(x, dt)
      if r then return x, r end
    end
  end
  return x
end

local function run(self)
  local dres = game.find("Дрезина")
  local echo = game.find("Эхо_позади")
  game.fade(1, 0)
  game.wait(0.1)
  game.fade(0, 2.5)
  game.subtitle("Дрезина мягко трогается. Колёса тихо считают стыки рельс.", 5)
  game.wait(2)

  local x = ride(START_X, 24, dres, echo)

  -- одна мягкая «вспышка» ламп вместо резкого мерцания
  game.subtitle("Лампы дрогнули и сразу выровнялись. За стеной что-то прошуршало.", 5)
  lamps(false); game.wait(0.25)
  lamps(true); game.wait(0.3)
  game.shake(0.12, 0.8)

  -- стрелка (с запасом времени)
  while true do
    x = ride(x, 36, dres, echo)
    self:set_prop("e", false)
    game.notify("Впереди стрелка. Нажми [E], чтобы перевести рычаг.", 5)
    local t, ok = 0, false
    local last = game.time()
    while t < 6 do
      game.wait(0)
      local now = game.time()
      t = t + (now - last)
      last = now
      if self:get_prop("e", false) then ok = true break end
    end
    if ok then
      game.play_sound("audio/sfx/sfx_pipe_valve.wav", 1.0)
      game.notify("Стрелка переведена.")
      break
    end
    game.subtitle("Дрезина мягко встала. Попробуй ещё раз.", 3)
    x = 30
    place(x, dres, echo)
  end

  -- «не оглядывайся»: спокойное предупреждение, без резких телепортов
  x = ride(x, 42, dres, echo)
  game.subtitle("Не оглядывайся. Что бы ты ни слышала — просто смотри вперёд.", 5)
  local look = 0
  x = ride(x, 64, dres, echo, function(cx, dt)
    local diff = (game.player_yaw() - 90 + 540) % 360 - 180
    if math.abs(diff) > 110 then look = look + dt else look = math.max(0, look - dt * 0.5) end
    echo:set_position(cx - 9 + math.min(look * 2, 4), 0, 0)
    if look > 2.2 then return "back" end
  end)
  -- если всё же оглянулась: тень приближается, свет гаснет, дрезина плавно возвращается на 6 метров
  if look > 2.2 then
    lamps(false)
    game.subtitle("Она была совсем рядом. Лампы погасли.", 4)
    game.shake(0.3, 0.8)
    game.wait(1.0)
    lamps(true)
    x = ride(math.max(START_X, 58), 64, dres, echo)
  end

  x = ride(x, END_X, dres, echo)
  echo:set_active(false)
  game.freeze_movement(false)
  game.teleport_player(68.5, 0.05, 0, game.player_yaw())
  game.subtitle("Динамик над воротами: «Говорит дежурный Заслона, майор Игнатов. Лера, вы опоздали на смену на три года. Мы вас ждали».", 9)
  game.wait(9.5)
  game.subtitle("«Проходите. Но сначала положите кассеты на стол и ничего не слушайте, пока я не скажу».", 7)
  game.wait(7)
  game.notify("Стол — оставить кассеты. Рюкзак — забрать их с собой.", 8)
end

function on_start(self)
  game.freeze_movement(true)
  game.start(function() run(self) end)
end

-- нажатие E запоминаем каждый кадр
function on_update(self, dt)
  if game.key_pressed("E") then self:set_prop("e", true) end
end
