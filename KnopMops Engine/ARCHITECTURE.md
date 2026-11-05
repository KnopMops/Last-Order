# Архитектура KnopMops Engine 1.0

```
kmruntime (static lib, общая для редактора и игры)       cmake/kmruntime.cmake
 ├ scene/Scene              GameObject + компоненты, иерархия, префабы, JSON (.kmscene)
 ├ map/*                    браши Quake .map (парсер, сериализатор, BrushBuilder)
 ├ render/TextureManager, DrawUtil
 ├ runtime/ChapterProject   chapter.json, уровни/скрипты, Validate (софтлоки), ExportToGame
 ├ runtime/ScriptHost       Lua 5.4: game.*, методы объектов, хуки, события, корутины, таймеры
 ├ runtime/PlaySession      симуляция: игрок, коллизии, взаимодействие, двери, укрытия, ИИ, свет, HUD
 └ runtime/ProjectTemplate  шаблоны проекта/уровня и Lua-шаблоны

Редактор: src/ui, src/render/ViewportRenderer, src/scripting     Игра: src/game/custom/CustomChapter
 (Dear ImGui Docking + rlImGui)                                   (PlaySession в экране GameScreen::CustomChapter)
```
* Play-In-Editor: `PlaySession::StartFromMemory` (копия несохранённой сцены) -> Render3D/RenderHUD в RenderTexture.
* Игра: `PlaySession::Start` читает `assets/data/chapters/<id>/`.
* Координаты: мир — метры, Y вверх; .map — Quake-единицы (32/м), Z вверх (`core/Math3D.h`). Yaw 0 = взгляд в +Z.
* Рендер мира: треугольники брашей дробятся до <=2 м, освещение считается на CPU по вершинам (лампы + фонарь + туман).
* raylib: `rlSetTexture` вызывать ПОСЛЕ `rlBegin`, иначе смена режима сбрасывает текстуру.
* ImGui: докинг включается флагом `ConfigFlags_DockingEnable` (в 1.0 он не был включён).
