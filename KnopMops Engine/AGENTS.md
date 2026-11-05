# Руководство для ИИ-агентов: KnopMops Engine 1.0

Ты помогаешь делать главы для игры **Last Order** в редакторе KnopMops Engine. Главу составляют:
уровни (геометрия `.map` + объекты `.kmscene`), Lua-скрипты и `chapter.json`. Движок — C++/raylib;
**игровую логику главы пиши на Lua**, а не на C++. C++ правь только при расширении самого движка.

## Что где лежит
* Проект главы: `KnopMops Engine/projects/<id>/` (см. README). Пример: `projects/bunker_demo`.
* Рантайм (общий для редактора и игры): `KnopMops Engine/src/{scene,map,runtime}`; редактор: `src/ui`, `src/render`, `src/scripting`.
* Предметы — `chapter.json -> items`, id вида `item_crowbar`. Звуки игры: `assets/audio/sfx/*.wav` (путь `audio/sfx/имя.wav`).
* Текстуры: `KnopMops Engine/assets/textures/*.png` (имя без расширения, напр. `motel_concrete`).

## Модель сцены
Объект (`GameObject`): имя, тег, `active`, Transform (position / rotation.y / scale), `props` и компоненты:
Mesh Renderer (shape Auto/None/Cube/Sphere/Cylinder), Player, Camera, Light, Box Collider (size в метрах, `isTrigger`),
Search Slot, Hiding Spot, Door, AI Monster, Item Pickup, Script. Position = центр объекта; у Player/Monster — точка на полу.
Yaw 0 смотрит в +Z, 90 — в +X. Дверь: ширина = scale.x, шарнир на -X краю; дверь в стене вдоль Z: rotation.y = 90.
Коллайдеры осевые (AABB). На уровне обязателен объект с компонентом Player и хотя бы одна лампа (или Ambient > 0).

## Lua API
Скрипт — файл `scripts/*.lua`, назначается объекту (Script -> файл). Параметры в Инспекторе:
`exports = { code = "1234", speed = 2.5 }` -> `self.params.code`.

Хуки: `on_start(self)`, `on_update(self,dt)`, `on_interact(self)` (нужен Script -> Interactable), `on_locked`, `on_unlocked`,
`on_search(self,item)`, `on_item_picked`, `on_door_opened/closed`, `on_hide/on_unhide`, `on_trigger_enter/exit(self,player)` (Collider -> Is Trigger).

Объект (`self`, `game.find(имя)`, `game.find_all(тег)`): поля `name tag active x y z yaw`; методы
`get_position() set_position(x,y,z) set_rotation(deg) set_active(b) destroy() distance_to_player() distance_to(o)
get_prop(k[,def]) set_prop(k,v) lock() unlock() is_locked() open() close() toggle() is_open() set_item(id) get_item()
set_light(b) set_color(r,g,b) set_visible(b) set_texture(name) set_emissive(b) set_interactable(b[,текст])`;
маньяк: `set_mode('patrol'|'chase'|'search'|'investigate'|'idle') get_mode() freeze(b) set_speed(walk,chase) investigate(x,y,z)`.

`game.*`: `notify(t[,сек]) subtitle(t[,сек]) log(...) has_item give_item(id[,silent]) take_item items() set(k,v) get(k[,def])
set_flag get_flag play_sound(путь[,гр]) play_ambient stop_ambient find find_all spawn(префаб,x,y,z) player() player_pos()
teleport_player(x,y,z[,yaw]) lock_player(b) enable_flashlight(b) player_hidden() make_noise(x,y,z,радиус) alert_monsters(x,y,z)
load_level(имя[,spawn]) level() win(заголовок,текст) lose(текст) fade(alpha,сек) shake(сила,сек)
cutscene_camera(px,py,pz,lx,ly,lz) cutscene_camera_end() play_tv(кассета[,сек]) stop_tv() is_tv_playing() ask_code(подсказка,код,function(ok,введено) end)
after(сек,fn) every(сек,fn) cancel(id) start(fn) wait(сек) time() distance(...) key_pressed('E') key_down('SHIFT') on(событие,fn)`.

События для `game.on`: `level_loaded item_added item_removed item_used player_caught monster_mode noise trigger_enter
slot_searched player_hidden player_unhidden var_changed chapter_won`.
`game.wait` работает только внутри `game.start(function() ... end)` (корутина — катсцены и последовательности).
Ошибка скрипта пишется в Console (файл:строка) и отключает этот скрипт до перезапуска Play.

## Как создать главу
1. File -> New Chapter Project (или копия `bunker_demo`). Заполни Chapter -> Settings / Items.
2. Геометрия: Map Tools -> Generate Room, либо `GameObject -> 3D Object -> Wall/Floor Brush` + Size в Inspector (проёмы — отдельными стенами). Сетка 0.5 м.
3. Объекты: Player, лампы, ящики (Search Slot), двери (Door + ключ), шкафы (Hiding Spot), предметы, триггеры, маньяк (waypoints в Inspector).
4. Логика: Script Editor -> New Script (шаблоны: интерактив, триггер, маньяк, катсцена, кодовый замок); общие события — `scripts/main.lua`.
5. Проверка: Play (Ctrl+P); Chapter -> «Проверить главу» ищет ошибки Lua, неописанные предметы и **софтлоки**.
6. Экспорт: Chapter -> Export to Game; запуск `Last Order.exe --chapter <id>`.
Автотест примера: `"KnopMops Engine.exe" --selftest` -> «ВСЕ ТЕСТЫ ПРОЙДЕНЫ». Добавляя фичи рантайма — расширяй `EditorUI::RunSelfTest`.

## Правила правок движка
* Новая функция скрипта: структура `LuaApi` в `runtime/ScriptHost.cpp` + `RegisterApi` + `kApiHelp` (`scripting/CodeEditor.cpp`) + этот файл.
* Новое поле компонента: `scene/Scene.h` + `ObjectToJson/ObjectFromJson` + Инспектор (`EditorUI::RenderObjectInspector`) + `PlaySession`.
* Рантайм не должен зависеть от ImGui — он линкуется в игру. В игре правки только через `src/game/custom/CustomChapter.*`.
* Браши: нормаль (p2-p1)x(p3-p1) каждой грани обязана смотреть НАРУЖУ, иначе у браша нет полигонов.
* Не запускай `KnopMops Engine.exe` во время сборки (файл занят).

## Версия 1.0: добавлено
* **Узел** — любой GameObject (точка со свойствами `props`). **Коллайдер**: shape 0 Box / 1 Sphere / 2 Capsule (`size`, `radius`, `height`, `offset`, `trigger`).
* **Rigidbody** (`rigidbody`: mass, gravity, kinematic, drag, bounce, friction) — симуляция в `PlaySession::UpdatePhysics`. Lua: `add_force set_velocity get_velocity is_grounded`, `game.raycast`.
* **PlayerController** (компонент `player`): speeds, radius, height, jump, gravity, sens.
* Редактор: окна «Файлы» и «Менеджер сцен», режим «Освещение (как в игре)», перетаскивание объектов мышью, «Вся карта» (Home). Интерфейс на русском.
* Шаблонов нет: новый проект/скрипт/сцена создаются пустыми. Проект для игры: `projects/last_order`. Подробности для пользователя: `РУКОВОДСТВО.md`.
* Формат .kmscene (объект): id, parent, name, tag, active, props, transform{pos,rot,scale}, mesh{shape,texture,tint,emissive}, и по необходимости
  camera, player, light, collider, rigidbody, searchSlot, hidingSpot, door, aiSpawn, itemPickup, script. Пример: любой файл в `projects/bunker_demo/levels/`.
