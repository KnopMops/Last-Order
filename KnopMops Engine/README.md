# KnopMops Engine 1.0 — редактор глав для «Последний заказ»

Unity/Godot-подобная среда для **создания глав игры**: уровни (браши + объекты), свет, двери, тайники,
укрытия, маньяк с ИИ, предметы и **программирование на Lua**. Play-In-Editor запускает главу
прямо в окне редактора тем же рантаймом, что и сама игра.

## Быстрый старт
```
build.bat                       (в папке KnopMops Engine; собирает bin\KnopMops Engine.exe)
bin\KnopMops Engine.exe         (при первом запуске создаётся пример projects\bunker_demo)
```
1. **Scene** — расставляйте объекты (меню GameObject), гизмо: `W` двигать, `E` поворот, `R` масштаб, `F` фокус.
2. **Inspector** — компоненты: Mesh, Light, Collider, Search Slot, Hiding Spot, Door, AI Monster, Item Pickup, Script (Lua).
3. **Script Editor** — Lua-скрипты главы; справка по API: галочка `API`.
4. **Play (Ctrl+P)** — пройти главу в редакторе (клик в окне Game — захват мыши, Esc — освободить).
5. **Chapter** — предметы, тексты, проверка на софтлоки, **Export to Game**.
6. В игре: `Last Order.exe --chapter <id>` или меню «Выбор главы → Пользовательские главы».
   Игру нужно пересобрать (`build.bat` в корне репозитория) — в ней появилась поддержка глав.

## Структура проекта главы (`projects/<id>/`)
```
chapter.json        паспорт: название, предметы, стартовый уровень, тексты
levels/*.kmscene    объекты уровня (JSON)      levels/*.map   геометрия (Quake .map)
scripts/*.lua       скрипты (main.lua — глобальный, как Autoload в Godot)
prefabs/*.kmprefab  префабы (ПКМ по объекту -> Save as Prefab; в Lua: game.spawn)
textures/ audio/    свои ресурсы (по желанию)
```
Экспорт копирует главу и использованные текстуры в `assets/data/chapters/<id>/`.

## Консольные режимы
* `KnopMops Engine.exe --selftest [проект]` — автопрохождение примера (Lua, предметы, двери, маньяк, уровни). Код 0 = всё ок.
* `KnopMops Engine.exe --export <проект>` — экспорт в игру из консоли.

## Ограничения
Коллайдеры и браши — по AABB; ИИ ходит по прямым между waypoints (ставьте точки в поворотах);
иерархия объектов наследует только позицию; подсветки синтаксиса Lua нет (есть проверка синтаксиса и список ошибок).

См. также: [`AGENTS.md`](AGENTS.md) (промпт и справочник для ИИ), [`ARCHITECTURE.md`](ARCHITECTURE.md).
