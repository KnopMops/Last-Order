#include "ui/EditorUI.h"
#include "ui/UnityTheme.h"
#include "core/Log.h"
#include "core/Math3D.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "map/BrushBuilder.h"
#include "map/MapParser.h"
#include "map/MapSerializer.h"
#include "render/TextureManager.h"
#include "rlImGui.h"
#include "runtime/ProjectTemplate.h"
#include "tools/Chapter35Builder.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>

namespace knopmops::ui {

namespace fs = std::filesystem;

namespace {

constexpr const char* kDockSpaceName = "KnopMopsDockSpaceV2";

bool InputStr(const char* label, std::string& s, ImGuiInputTextFlags flags = 0) {
    char buf[512];
    std::strncpy(buf, s.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText(label, buf, sizeof(buf), flags)) {
        s = buf;
        return true;
    }
    return false;
}

bool DragV3(const char* label, Vector3& v, float speed = 0.1f) {
    float a[3] = {v.x, v.y, v.z};
    if (ImGui::DragFloat3(label, a, speed)) {
        v = Vector3{a[0], a[1], a[2]};
        return true;
    }
    return false;
}

bool ColorEdit(const char* label, Color& c) {
    float col[3] = {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f};
    if (ImGui::ColorEdit3(label, col)) {
        c.r = (unsigned char)std::clamp(col[0] * 255.0f, 0.0f, 255.0f);
        c.g = (unsigned char)std::clamp(col[1] * 255.0f, 0.0f, 255.0f);
        c.b = (unsigned char)std::clamp(col[2] * 255.0f, 0.0f, 255.0f);
        return true;
    }
    return false;
}

bool TextureField(const char* label, std::string& name) {
    bool changed = false;
    ImGui::PushID(label);
    if (ImGui::BeginCombo(label, name.empty() ? "(нет)" : name.c_str())) {
        if (ImGui::Selectable("(нет)", name.empty())) { name.clear(); changed = true; }
        for (const auto& t : render::TextureManager::Instance().GetAllTextures()) {
            if (ImGui::Selectable(t.name.c_str(), t.name == name)) { name = t.name; changed = true; }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool ReadFileTime(const std::string& path, fs::file_time_type& out) {
    std::error_code ec;
    out = fs::last_write_time(path, ec);
    return !ec;
}

}

EditorUI::EditorUI() = default;
EditorUI::~EditorUI() = default;

// ---------------------------------------------------------------------------------------------
// Инициализация / пути / проекты
// ---------------------------------------------------------------------------------------------
void EditorUI::FindPaths() {
    std::string app = GetApplicationDirectory();
    std::vector<std::string> cands = {"projects", "../projects", app + "projects", app + "../projects"};
    for (const auto& c : cands) {
        std::error_code ec;
        if (fs::exists(c, ec) && fs::is_directory(c, ec)) { m_projectsDir = fs::weakly_canonical(c, ec).generic_string(); break; }
    }
    if (m_projectsDir.empty()) {
        std::error_code ec;
        std::string dir = fs::weakly_canonical(app + "../projects", ec).generic_string();
        fs::create_directories(dir, ec);
        m_projectsDir = dir;
    }
    m_gameRoot = runtime::ChapterProject::DetectGameRoot(".");
    if (m_gameRoot.empty()) m_gameRoot = runtime::ChapterProject::DetectGameRoot(app);
    if (!m_gameRoot.empty()) std::replace(m_gameRoot.begin(), m_gameRoot.end(), '\\', '/');
    core::Log::Info("Проекты: " + m_projectsDir);
    core::Log::Info(m_gameRoot.empty() ? "Папка игры Last Order не найдена (экспорт недоступен)." : "Игра Last Order: " + m_gameRoot);
}

void EditorUI::SaveLastProject() {
    std::ofstream f(m_projectsDir + "/.last");
    f << m_project.Manifest().id << "\n" << m_levelName << "\n";
}

void EditorUI::Init(map::MapDocument* doc, scene::Scene* scene, render::ViewportRenderer* renderer) {
    m_doc = doc;
    m_scene = scene;
    m_renderer = renderer;
    ApplyUnityDarkTheme();

    // шрифт для 3D-подписей и HUD (кириллица)
    std::vector<int> cps;
    for (int c = 0x20; c <= 0x7E; ++c) cps.push_back(c);
    for (int c = 0xA0; c <= 0xFF; ++c) cps.push_back(c);
    for (int c = 0x400; c <= 0x4FF; ++c) cps.push_back(c);
    for (int c = 0x2010; c <= 0x203A; ++c) cps.push_back(c);
    std::string fontPath = std::string(GetApplicationDirectory()) + "assets/fonts/font_main.ttf";
    if (!FileExists(fontPath.c_str())) fontPath = "assets/fonts/font_main.ttf";
    if (FileExists(fontPath.c_str())) {
        m_hudFont = LoadFontEx(fontPath.c_str(), 30, cps.data(), (int)cps.size());
        SetTextureFilter(m_hudFont.texture, TEXTURE_FILTER_BILINEAR);
        m_hasHudFont = m_hudFont.texture.id != 0;
        if (m_hasHudFont) m_renderer->SetFont(m_hudFont);
    }

    FindPaths();

    // последний проект или пример
    std::string last;
    {
        std::ifstream f(m_projectsDir + "/.last");
        std::getline(f, last);
        std::string lvl;
        std::getline(f, lvl);
        if (!last.empty() && fs::exists(m_projectsDir + "/" + last + "/chapter.json")) {
            OpenProject(last);
            if (!lvl.empty() && fs::exists(m_project.LevelScenePath(lvl))) OpenLevel(lvl);
            return;
        }
    }
    auto projects = runtime::ChapterProject::ListProjects(m_projectsDir);
    if (std::find(projects.begin(), projects.end(), "last_order") == projects.end()) CreateProject("last_order", "Last Order");
    else OpenProject("last_order");
}

void EditorUI::Shutdown() {
    StopPlay();
    if (m_gameRT.id != 0) UnloadRenderTexture(m_gameRT);
    m_gameRT = RenderTexture2D{};
    if (m_hasHudFont) UnloadFont(m_hudFont);
    m_hasHudFont = false;
}

bool EditorUI::OpenProject(const std::string& name) {
    StopPlay();
    runtime::ChapterProject p;
    if (!p.Load(m_projectsDir + "/" + name)) {
        core::Log::Error("Не удалось открыть проект: " + name);
        return false;
    }
    m_project = p;
    m_codeEditor.SetProject(&m_project);
    m_issues.clear();
    m_exports.clear();
    if (fs::exists(m_project.TexturesDir())) render::TextureManager::Instance().AddDirectory(m_project.TexturesDir());

    std::string level = m_project.Manifest().startLevel;
    auto levels = m_project.ListLevels();
    if (std::find(levels.begin(), levels.end(), level) == levels.end()) {
        if (levels.empty()) {
            CreateLevel("level1");
            return true;
        }
        level = levels.front();
    }
    core::Log::Success("Проект открыт: " + m_project.Manifest().title);
    return OpenLevel(level);
}

bool EditorUI::CreateProject(const std::string& id, const std::string& title) {
    std::string err;
    if (!runtime::ProjectTemplate::Create(m_projectsDir, id, title, err)) {
        core::Log::Error("Создание проекта: " + err);
        return false;
    }
    core::Log::Success("Создан проект: " + id);
    return OpenProject(id);
}

bool EditorUI::OpenLevel(const std::string& level) {
    if (!m_project.IsLoaded()) return false;
    StopPlay();
    if (!m_scene->LoadFromFile(m_project.LevelScenePath(level))) {
        core::Log::Error("Не удалось открыть уровень: " + level);
        return false;
    }
    m_doc->Clear();
    if (fs::exists(m_project.LevelMapPath(level))) {
        map::MapParser::LoadFromFile(m_project.LevelMapPath(level), *m_doc);
    }
    m_doc->SetFilePath(m_project.LevelMapPath(level));
    m_doc->SetModified(false);
    m_scene->SetModified(false);
    m_externalMap.clear();
    m_levelName = level;
    m_renderer->FrameAll(*m_doc, *m_scene);
    SaveLastProject();
    core::Log::Info("Уровень открыт: " + level);
    return true;
}

bool EditorUI::CreateLevel(const std::string& level) {
    if (!m_project.IsLoaded() || level.empty()) return false;
    if (fs::exists(m_project.LevelScenePath(level))) {
        core::Log::Warn("Уровень уже существует: " + level);
        return OpenLevel(level);
    }
    scene::Scene sc;
    map::MapDocument doc;
    runtime::ProjectTemplate::BuildEmptyLevel(sc, doc, level);
    sc.SaveToFile(m_project.LevelScenePath(level));
    map::MapSerializer::SaveToFile(m_project.LevelMapPath(level), doc);
    core::Log::Success("Создан уровень: " + level);
    return OpenLevel(level);
}

bool EditorUI::SaveAll() {
    if (!m_project.IsLoaded()) return false;
    bool ok = m_project.Save();
    if (!m_levelName.empty() && m_externalMap.empty()) {
        ok &= m_scene->SaveToFile(m_project.LevelScenePath(m_levelName));
        ok &= map::MapSerializer::SaveToFile(m_project.LevelMapPath(m_levelName), *m_doc);
        m_doc->SetModified(false);
    }
    m_codeEditor.SaveFile();
    if (ok) core::Log::Success("Сохранено: " + m_project.Manifest().title + " / " + m_levelName);
    else core::Log::Error("Ошибка сохранения!");
    return ok;
}

bool EditorUI::ExportToGame() {
    if (!m_project.IsLoaded()) return false;
    if (m_gameRoot.empty()) {
        core::Log::Error("Папка игры Last Order не найдена — экспорт невозможен.");
        return false;
    }
    SaveAll();
    m_issues = m_project.Validate();
    bool hasErrors = false;
    for (const auto& i : m_issues) if (i.severity == runtime::ValidationIssue::Severity::Error) hasErrors = true;
    if (hasErrors && !m_ignoreErrorsOnExport) {
        core::Log::Error("Экспорт отменён: в главе есть ошибки (вкладка Chapter). Исправьте или отметьте «игнорировать ошибки».");
        return false;
    }
    std::string log;
    bool ok = m_project.ExportToGame(m_gameRoot, &log);
    std::string line;
    for (char c : log) { if (c == '\n') { core::Log::Info(line); line.clear(); } else line.push_back(c); }
    if (ok) core::Log::Success("Глава «" + m_project.Manifest().title + "» экспортирована в игру. Запуск: Last Order.exe --chapter " + m_project.Manifest().id);
    return ok;
}

void EditorUI::RunInGame() {
    if (!ExportToGame()) return;
    std::vector<std::string> exes = {m_gameRoot + "/release/Last Order.exe", m_gameRoot + "/build/debug/Last Order.exe", m_gameRoot + "/build/debug/Last Order_debug.exe"};
    for (const auto& e : exes) {
        if (!fs::exists(e)) continue;
        std::string dir = fs::path(e).parent_path().string();
        std::replace(dir.begin(), dir.end(), '/', '\\');
        std::string exeWin = e;
        std::replace(exeWin.begin(), exeWin.end(), '/', '\\');
        std::string cmd = "start \"\" /D \"" + dir + "\" \"" + exeWin + "\" --chapter " + m_project.Manifest().id;
        std::system(cmd.c_str());
        core::Log::Success("Игра запущена: " + e);
        return;
    }
    core::Log::Warn("Last Order.exe не найден. Соберите игру (build.bat в корне репозитория), затем запустите: Last Order.exe --chapter " + m_project.Manifest().id);
}

// ---------------------------------------------------------------------------------------------
// Play-In-Editor
// ---------------------------------------------------------------------------------------------
void EditorUI::StartPlay() {
    if (m_play || !m_project.IsLoaded()) return;
    m_play = std::make_unique<runtime::PlaySession>();
    runtime::PlaySession::Hooks hooks;
    if (!m_gameRoot.empty()) hooks.gameAssetsDir = m_gameRoot + "/assets";
    m_play->SetHooks(hooks);
    if (m_hasHudFont) m_play->SetFont(m_hudFont);
    m_play->SetPauseEnabled(false);
    m_scene->Deselect();
    if (!m_play->StartFromMemory(m_project, m_levelName, *m_scene, *m_doc)) {
        core::Log::Error("Play: не удалось запустить уровень.");
        m_play.reset();
        return;
    }
    m_captured = false;
    m_focusGameTab = true;
    m_playTime = 0.0f;
    core::Log::Success("▶ Play-In-Editor: кликните в окне Game, чтобы захватить мышь (Esc — освободить).");
}

void EditorUI::StopPlay() {
    if (!m_play) return;
    m_play.reset();
    if (m_captured) EnableCursor();
    m_captured = false;
    core::Log::Info("■ Play остановлен.");
}

// ---------------------------------------------------------------------------------------------
// Главный рендер UI
// ---------------------------------------------------------------------------------------------
void EditorUI::BuildDefaultLayout(unsigned int dockspaceId, float w, float h) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2(w, h));

    ImGuiID dock = dockspaceId;
    ImGuiID left = ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.17f, nullptr, &dock);
    ImGuiID right = ImGui::DockBuilderSplitNode(dock, ImGuiDir_Right, 0.24f, nullptr, &dock);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(dock, ImGuiDir_Down, 0.30f, nullptr, &dock);

    ImGui::DockBuilderDockWindow("Менеджер сцен", left);
    ImGui::DockBuilderDockWindow("Иерархия", left);
    ImGui::DockBuilderDockWindow("Инспектор", right);
    ImGui::DockBuilderDockWindow("Сцена", dock);
    ImGui::DockBuilderDockWindow("Игра", dock);
    ImGui::DockBuilderDockWindow("Редактор скриптов", dock);
    ImGui::DockBuilderDockWindow("Файлы", bottom);
    ImGui::DockBuilderDockWindow("Проект", bottom);
    ImGui::DockBuilderDockWindow("Глава", bottom);
    ImGui::DockBuilderDockWindow("Инструменты карты", bottom);
    ImGui::DockBuilderDockWindow("Консоль", bottom);
    ImGui::DockBuilderFinish(dockspaceId);
}

void EditorUI::Render() {
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                 ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("KnopMopsDockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    RenderMainMenuBar();
    RenderToolbar();

    ImGuiID dockspaceId = ImGui::GetID(kDockSpaceName);
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (m_resetLayout || (!m_layoutBuilt && ImGui::DockBuilderGetNode(dockspaceId) == nullptr)) {
        BuildDefaultLayout(dockspaceId, avail.x, avail.y);
        m_resetLayout = false;
        m_focusSceneTab = true;
    }
    m_layoutBuilt = true;
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();

    // Горячие клавиши
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) SaveAll();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false)) { if (m_play) StopPlay(); else StartPlay(); }
    }

    RenderHierarchy();
    RenderSceneViewport();
    RenderGameViewport();
    m_codeEditor.Render();
    RenderInspector();
    RenderProjectPanel();
    RenderChapterPanel();
    RenderMapTools();
    RenderFileExplorer();
    RenderSceneManager();
    RenderConsole();

    // заголовок окна
    static std::string lastTitle;
    std::string title = "KnopMops Engine 1.0 — " + (m_project.IsLoaded() ? m_project.Manifest().title + " / " + m_levelName : std::string("нет проекта"));
    if (m_scene->IsModified() || m_doc->IsModified()) title += " *";
    if (title != lastTitle) {
        SetWindowTitle(title.c_str());
        lastTitle = title;
    }
}

void EditorUI::RenderMainMenuBar() {
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("Файл")) {
        if (ImGui::MenuItem("Новый проект главы...")) ImGui::OpenPopup("NewProjectPopup");
        if (ImGui::BeginMenu("Открыть проект")) {
            for (const auto& p : runtime::ChapterProject::ListProjects(m_projectsDir)) {
                if (ImGui::MenuItem(p.c_str(), nullptr, m_project.IsLoaded() && m_project.Manifest().id == p)) OpenProject(p);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Новая сцена...")) ImGui::OpenPopup("NewLevelPopup");
        if (ImGui::BeginMenu("Открыть сцену", m_project.IsLoaded())) {
            for (const auto& l : m_project.ListLevels()) {
                if (ImGui::MenuItem(l.c_str(), nullptr, l == m_levelName)) OpenLevel(l);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Сохранить всё", "Ctrl+S")) SaveAll();
        ImGui::Separator();
        ImGui::Separator();
        if (ImGui::MenuItem("Выход", "Alt+F4")) m_shouldExit = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Правка")) {
        if (ImGui::MenuItem("Снять выделение", "Esc")) { m_doc->DeselectAll(); m_scene->Deselect(); }
        if (ImGui::MenuItem("Дублировать", "Ctrl+D")) ActionDuplicateSelected();
        if (ImGui::MenuItem("Удалить", "Del")) ActionDeleteSelected();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Объект")) {
        CreateObjectMenu();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Окно")) {
        if (ImGui::MenuItem("Сбросить раскладку")) m_resetLayout = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Справка")) {
        ImGui::TextDisabled("KnopMops Engine 1.0 — универсальный движок глав");
        ImGui::TextDisabled("Lua-скрипты: окно «Редактор скриптов» -> Справка API");
        ImGui::TextDisabled("Руководство: KnopMops Engine/РУКОВОДСТВО.md");
        ImGui::EndMenu();
    }

    // попапы
    if (ImGui::BeginPopupModal("NewProjectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("ID (имя папки)", m_newProjectId, sizeof(m_newProjectId));
        ImGui::InputText("Название", m_newProjectTitle, sizeof(m_newProjectTitle));
        if (ImGui::Button("Создать", ImVec2(120, 0))) {
            if (CreateProject(m_newProjectId, m_newProjectTitle)) ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("NewLevelPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Имя сцены", m_newLevelName, sizeof(m_newLevelName));
        if (ImGui::Button("Создать", ImVec2(120, 0))) {
            if (CreateLevel(m_newLevelName)) ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::EndMenuBar();
}

void EditorUI::RenderToolbar() {
    ImGui::SetCursorPosX(8.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);

    auto toolButton = [this](const char* label, render::TransformMode mode) {
        bool active = m_renderer->GetTransformMode() == mode;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.88f, 1.0f));
        if (ImGui::Button(label)) m_renderer->SetTransformMode(mode);
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    toolButton("Выбор [Q]", render::TransformMode::Select);
    toolButton("Двигать [W]", render::TransformMode::Translate);
    toolButton("Вращать [E]", render::TransformMode::Rotate);
    toolButton("Масштаб [R]", render::TransformMode::Scale);

    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(130.0f);
    const char* snapItems[] = {"Сетка: выкл", "Сетка: 0.1 м", "Сетка: 0.25 м", "Сетка: 0.5 м", "Сетка: 1.0 м", "Сетка: 2.0 м"};
    static int currentSnap = 3;
    if (ImGui::Combo("##SnapCombo", &currentSnap, snapItems, IM_ARRAYSIZE(snapItems))) {
        float snaps[] = {0.0f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
        m_renderer->SetGridSnap(snaps[currentSnap]);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    if (m_play) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Стоп [Ctrl+P]")) StopPlay();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "PLAYING %.0f c", m_play ? (float)m_play->Time() : 0.0f);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.62f, 0.28f, 1.0f));
        if (ImGui::Button("Играть [Ctrl+P]")) StartPlay();
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ImGui::Button("Вся карта [Home]")) m_renderer->FrameAll(*m_doc, *m_scene);
    ImGui::SameLine();
    if (ImGui::Button("Сверху")) m_renderer->SetTopView();
    ImGui::SameLine();
    if (ImGui::Button("Сохранить всё")) SaveAll();

    float fps = (float)GetFPS();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 120.0f);
    ImGui::TextDisabled("%.0f FPS", fps);
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
}

// ---------------------------------------------------------------------------------------------
// Создание объектов
// ---------------------------------------------------------------------------------------------
Vector3 EditorUI::FocusPoint() const {
    Vector3 t = m_renderer->GetOrbitTarget();
    float s = m_renderer->GetGridSnap();
    Vector3 p{t.x, 0.0f, t.z};
    if (s > 0.001f) { p.x = core::SnapValue(p.x, s); p.z = core::SnapValue(p.z, s); }
    return p;
}

scene::GameObject* EditorUI::CreateObjectAtFocus(const std::string& base, float yOffset) {
    std::string name = base;
    int n = 1;
    while (m_scene->FindByName(name)) name = base + "_" + std::to_string(++n);
    auto* o = m_scene->CreateGameObject(name);
    Vector3 p = FocusPoint();
    o->transform.position = Vector3{p.x, yOffset, p.z};
    o->meshRenderer.meshName = "None";
    m_doc->DeselectAll();
    m_scene->Select(o->id);
    return o;
}

void EditorUI::CreateObjectMenu() {
    if (ImGui::MenuItem("Пустой объект")) CreateObjectAtFocus("Объект", 0.0f);
    if (ImGui::MenuItem("Узел (точка со свойствами)")) {
        auto* o = CreateObjectAtFocus("Узел", 0.0f);
        o->tag = "node";
    }
    if (ImGui::BeginMenu("Физика")) {
        if (ImGui::MenuItem("Динамический куб (Rigidbody + Box)")) {
            auto* o = CreateObjectAtFocus("Куб-физика", 2.0f);
            o->meshRenderer.meshName = "Cube";
            o->meshRenderer.textureName = "motel_cardboard";
            o->transform.scale = {0.8f, 0.8f, 0.8f};
            o->collider.enabled = true; o->collider.shape = 0; o->collider.size = o->transform.scale;
            o->rigidbody.enabled = true;
        }
        if (ImGui::MenuItem("Динамический шар (Rigidbody + Sphere)")) {
            auto* o = CreateObjectAtFocus("Шар-физика", 2.0f);
            o->meshRenderer.meshName = "Sphere";
            o->transform.scale = {0.6f, 0.6f, 0.6f};
            o->collider.enabled = true; o->collider.shape = 1; o->collider.radius = 0.3f;
            o->rigidbody.enabled = true; o->rigidbody.bounciness = 0.6f;
        }
        if (ImGui::MenuItem("Статический коллайдер (невидимый)")) {
            auto* o = CreateObjectAtFocus("Коллайдер", 1.0f);
            o->collider.enabled = true; o->collider.size = {2.0f, 2.0f, 2.0f};
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("3D-объект")) {
        if (ImGui::MenuItem("Куб (объект)")) {
            auto* o = CreateObjectAtFocus("Cube", 0.5f);
            o->meshRenderer.meshName = "Cube";
        }
        if (ImGui::MenuItem("Сфера")) {
            auto* o = CreateObjectAtFocus("Sphere", 0.5f);
            o->meshRenderer.meshName = "Sphere";
        }
        if (ImGui::MenuItem("Цилиндр")) {
            auto* o = CreateObjectAtFocus("Cylinder", 0.5f);
            o->meshRenderer.meshName = "Cylinder";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Куб-браш (статичная геометрия)")) {
            Vector3 p = FocusPoint();
            map::MapBrush b = map::BrushBuilder::CreateCubeAt({p.x, 1.0f, p.z}, {2.0f, 2.0f, 2.0f}, "motel_concrete");
            b.name = "Cube_" + std::to_string(m_doc->GetWorldBrushes().size());
            m_doc->AddBrush(b);
            m_scene->Deselect();
            m_doc->SelectBrush(m_doc->GetWorldBrushes().size() - 1);
        }
        if (ImGui::MenuItem("Стена-браш")) {
            Vector3 p = FocusPoint();
            map::MapBrush b = map::BrushBuilder::CreateCubeAt({p.x, 1.5f, p.z}, {4.0f, 3.0f, 0.25f}, "motel_wallpaper");
            b.name = "Wall_" + std::to_string(m_doc->GetWorldBrushes().size());
            m_doc->AddBrush(b);
            m_scene->Deselect();
            m_doc->SelectBrush(m_doc->GetWorldBrushes().size() - 1);
        }
        if (ImGui::MenuItem("Пол-браш")) {
            Vector3 p = FocusPoint();
            map::MapBrush b = map::BrushBuilder::CreateBox({p.x - 4.0f, -0.2f, p.z - 4.0f}, {p.x + 4.0f, 0.0f, p.z + 4.0f}, "motel_tile");
            b.name = "Floor_" + std::to_string(m_doc->GetWorldBrushes().size());
            m_doc->AddBrush(b);
            m_scene->Deselect();
            m_doc->SelectBrush(m_doc->GetWorldBrushes().size() - 1);
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Старт игрока")) {
        auto* o = CreateObjectAtFocus("Player", 0.0f);
        o->tag = "player";
        o->player.enabled = true;
        o->camera.enabled = true;
        o->camera.isMainPlayerCamera = true;
    }
    if (ImGui::MenuItem("Точечный свет")) {
        auto* o = CreateObjectAtFocus("Lamp", 2.6f);
        o->light.enabled = true;
        o->meshRenderer.meshName = "Cube";
        o->transform.scale = {0.3f, 0.12f, 0.3f};
        o->meshRenderer.tint = Color{255, 245, 210, 255};
        o->meshRenderer.emissive = true;
    }

    if (ImGui::BeginMenu("Хоррор-предметы")) {
        if (ImGui::MenuItem("Ящик / тайник (Search Slot)")) {
            auto* o = CreateObjectAtFocus("Crate", 0.35f);
            o->transform.scale = {0.8f, 0.7f, 0.8f};
            o->meshRenderer.meshName = "Cube";
            o->meshRenderer.textureName = "motel_cardboard";
            o->collider.enabled = true;
            o->collider.size = o->transform.scale;
            o->searchSlot.enabled = true;
            o->searchSlot.slotId = o->name;
        }
        if (ImGui::MenuItem("Укрытие (Hiding Spot)")) {
            auto* o = CreateObjectAtFocus("Wardrobe", 1.05f);
            o->transform.scale = {1.0f, 2.1f, 0.7f};
            o->meshRenderer.meshName = "Cube";
            o->meshRenderer.textureName = "motel_wall_wood";
            o->collider.enabled = true;
            o->collider.size = o->transform.scale;
            o->hidingSpot.enabled = true;
        }
        if (ImGui::MenuItem("Дверь (Door)")) {
            auto* o = CreateObjectAtFocus("Дверь", 1.1f);
            o->transform.scale = {1.4f, 2.2f, 0.12f};
            o->meshRenderer.meshName = "Auto";
            o->collider.enabled = true;
            o->collider.size = o->transform.scale;
            o->door.enabled = true;
        }
        if (ImGui::MenuItem("Предмет для подбора (Item Pickup)")) {
            auto* o = CreateObjectAtFocus("Item", 0.2f);
            o->transform.scale = {0.22f, 0.22f, 0.22f};
            o->itemPickup.enabled = true;
        }
        if (ImGui::MenuItem("Триггер-зона")) {
            auto* o = CreateObjectAtFocus("Trigger", 1.0f);
            o->collider.enabled = true;
            o->collider.isTrigger = true;
            o->collider.size = {3.0f, 2.0f, 3.0f};
        }
        if (ImGui::MenuItem("Интерактивный объект (скрипт)")) {
            auto* o = CreateObjectAtFocus("Interactable", 0.5f);
            o->meshRenderer.meshName = "Cube";
            o->transform.scale = {0.5f, 0.5f, 0.5f};
            o->script.enabled = true;
            o->script.interactable = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("ИИ")) {
        if (ImGui::MenuItem("Маньяк (Prowler)")) {
            auto* o = CreateObjectAtFocus("Prowler", 0.0f);
            o->tag = "monster";
            o->aiSpawn.enabled = true;
            Vector3 p = o->transform.position;
            o->aiSpawn.patrolWaypoints = {{p.x + 3, 0, p.z}, {p.x + 3, 0, p.z + 3}, {p.x, 0, p.z + 3}, {p.x, 0, p.z}};
        }
        ImGui::EndMenu();
    }

    if (m_project.IsLoaded()) {
        auto prefabs = m_project.ListPrefabs();
        if (!prefabs.empty() && ImGui::BeginMenu("Префаб")) {
            for (const auto& pf : prefabs) {
                if (ImGui::MenuItem(pf.c_str())) {
                    Vector3 p = FocusPoint();
                    if (auto* o = m_scene->InstantiatePrefab(m_project.PrefabsDir() + "/" + pf, p)) {
                        m_doc->DeselectAll();
                        m_scene->Select(o->id);
                    }
                }
            }
            ImGui::EndMenu();
        }
    }
}

void EditorUI::ActionDuplicateSelected() {
    if (auto* obj = m_scene->GetSelected()) {
        if (auto* d = m_scene->Duplicate(obj->id)) {
            m_scene->Select(d->id);
            core::Log::Info("Дублирован объект: " + d->name);
        }
        return;
    }
    int selB = m_doc->GetSelectedBrushIndex();
    if (selB >= 0 && selB < (int)m_doc->GetWorldBrushes().size()) {
        map::MapBrush dup = m_doc->GetWorldBrushes()[selB];
        dup.name += "_copy";
        Vector3 qOff = core::WorldToQuake(Vector3{1.0f, 0.0f, 1.0f});
        for (auto& f : dup.faces) {
            for (int k = 0; k < 3; ++k) f.planePoints[k] = core::Vec3Add(f.planePoints[k], qOff);
        }
        dup.BuildPolygons();
        m_doc->AddBrush(dup);
        m_doc->SelectBrush(m_doc->GetWorldBrushes().size() - 1);
    }
}

void EditorUI::ActionDeleteSelected() {
    if (auto* obj = m_scene->GetSelected()) {
        m_scene->DestroyGameObject(obj->id);
        return;
    }
    int selB = m_doc->GetSelectedBrushIndex();
    if (selB >= 0) m_doc->RemoveBrush(selB);
}

// ---------------------------------------------------------------------------------------------
// Hierarchy
// ---------------------------------------------------------------------------------------------
void EditorUI::RenderHierarchyNode(scene::GameObject* obj, const std::string& filter) {
    auto children = m_scene->GetChildren(obj->id);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (m_scene->GetSelectedId() == (int)obj->id) flags |= ImGuiTreeNodeFlags_Selected;

    std::string tag;
    if (obj->player.enabled) tag = "[Игрок] ";
    else if (obj->aiSpawn.enabled) tag = "[ИИ] ";
    else if (obj->light.enabled) tag = "[Свет] ";
    else if (obj->door.enabled) tag = "[Дверь] ";
    else if (obj->searchSlot.enabled) tag = "[Ящик] ";
    else if (obj->hidingSpot.enabled) tag = "[Укрытие] ";
    else if (obj->itemPickup.enabled) tag = "[Предмет] ";
    else if (obj->collider.enabled && obj->collider.isTrigger) tag = "[Триггер] ";
    else if (obj->script.enabled) tag = "[Скрипт] ";
    std::string label = tag + obj->name;

    if (!obj->active) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    bool open = ImGui::TreeNodeEx((void*)(intptr_t)obj->id, flags, "%s", label.c_str());
    if (!obj->active) ImGui::PopStyleColor();

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        m_scene->Select(obj->id);
        m_doc->DeselectAll();
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        m_renderer->FocusOn(m_scene->WorldPosition(*obj), 6.0f);
    }

    if (ImGui::BeginDragDropSource()) {
        uint32_t id = obj->id;
        ImGui::SetDragDropPayload("GAMEOBJECT", &id, sizeof(id));
        ImGui::Text("%s", obj->name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("GAMEOBJECT")) {
            uint32_t id = *(const uint32_t*)p->Data;
            m_scene->SetParent(id, obj->id);
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Создать дочерний")) {
            auto* c = m_scene->CreateGameObject("Child");
            c->meshRenderer.meshName = "None";
            m_scene->SetParent(c->id, obj->id);
            m_scene->Select(c->id);
        }
        if (ImGui::MenuItem("Дублировать")) { m_scene->Select(obj->id); ActionDuplicateSelected(); }
        if (ImGui::MenuItem("Убрать из родителя", nullptr, false, obj->parent != 0)) m_scene->SetParent(obj->id, 0);
        if (ImGui::MenuItem("Сохранить как префаб")) {
            std::string fname = obj->name;
            std::replace(fname.begin(), fname.end(), ' ', '_');
            if (m_scene->SavePrefab(obj->id, m_project.PrefabsDir() + "/" + fname + ".kmprefab")) core::Log::Success("Префаб сохранён: " + fname + ".kmprefab");
        }
        if (ImGui::MenuItem("Фокус")) m_renderer->FocusOn(m_scene->WorldPosition(*obj), 6.0f);
        ImGui::Separator();
        if (ImGui::MenuItem("Удалить")) {
            uint32_t id = obj->id;
            ImGui::EndPopup();
            if (open) ImGui::TreePop();
            m_scene->DestroyGameObject(id);
            return;
        }
        ImGui::EndPopup();
    }

    if (open) {
        for (auto* c : children) {
            RenderHierarchyNode(c, filter);
        }
        ImGui::TreePop();
    }
}

void EditorUI::RenderHierarchy() {
    ImGui::Begin("Иерархия");

    ImGui::InputTextWithHint("##HierarchySearch", "Поиск...", m_hierarchyFilter, IM_ARRAYSIZE(m_hierarchyFilter));
    ImGui::Separator();
    std::string search = ToLower(m_hierarchyFilter);

    ImGui::BeginChild("HierarchyScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4.0f));
    std::string header = "Сцена: " + m_scene->GetName() + " (" + std::to_string(m_scene->GetGameObjects().size()) + ")";
    if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!search.empty()) {
            for (const auto& up : m_scene->GetGameObjects()) {
                if (ToLower(up->name).find(search) == std::string::npos) continue;
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (m_scene->GetSelectedId() == (int)up->id) flags |= ImGuiTreeNodeFlags_Selected;
                ImGui::TreeNodeEx((void*)(intptr_t)up->id, flags, "%s", up->name.c_str());
                if (ImGui::IsItemClicked()) { m_scene->Select(up->id); m_doc->DeselectAll(); }
            }
        } else {
            std::vector<uint32_t> roots;
            for (const auto& up : m_scene->GetGameObjects()) {
                if (up->parent == 0 || !m_scene->FindGameObject(up->parent)) roots.push_back(up->id);
            }
            for (uint32_t id : roots) {
                if (auto* o = m_scene->FindGameObject(id)) RenderHierarchyNode(o, search);
            }
        }
        ImGui::TreePop();
    }

    // зона «сделать корневым»
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 12.0f));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("GAMEOBJECT")) m_scene->SetParent(*(const uint32_t*)p->Data, 0);
        ImGui::EndDragDropTarget();
    }

    if (ImGui::TreeNodeEx("Браши карты (.map)", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& brushes = m_doc->GetWorldBrushes();
        for (size_t i = 0; i < brushes.size(); ++i) {
            auto& b = brushes[i];
            if (!search.empty() && ToLower(b.name).find(search) == std::string::npos) continue;
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (b.selected) flags |= ImGuiTreeNodeFlags_Selected;
            ImGui::TreeNodeEx((void*)(intptr_t)(1000000 + i), flags, "[Браш] %s", b.name.c_str());
            if (ImGui::IsItemClicked()) { m_doc->SelectBrush(i); m_scene->Deselect(); }
        }
        ImGui::TreePop();
    }
    ImGui::EndChild();

    if (ImGui::Button("+ Объект")) ImGui::OpenPopup("CreateObjPopup");
    ImGui::SameLine();
    if (ImGui::Button("Удалить")) ActionDeleteSelected();
    if (ImGui::BeginPopup("CreateObjPopup")) {
        CreateObjectMenu();
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------------------------
// Scene / Game views
// ---------------------------------------------------------------------------------------------
void EditorUI::RenderSceneViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Сцена", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (m_focusSceneTab) {
        ImGui::SetWindowFocus();
        m_focusSceneTab = false;
    }

    ImGui::SetCursorPosX(6.0f);
    ImGui::SetNextItemWidth(190.0f);
    const char* shading[] = {"Освещение (как в игре)", "Текстуры", "Каркас", "Плоский"};
    int sh = (int)m_renderer->GetShadingMode();
    if (ImGui::Combo("##Shading", &sh, shading, 4)) m_renderer->SetShadingMode((render::ViewportShading)sh);
    ImGui::SameLine();
    bool grid = m_renderer->IsShowGrid();
    if (ImGui::Checkbox("Сетка", &grid)) m_renderer->SetShowGrid(grid);
    ImGui::SameLine();
    bool col = m_renderer->IsShowColliders();
    if (ImGui::Checkbox("Коллайдеры", &col)) m_renderer->SetShowColliders(col);
    ImGui::SameLine();
    bool hc = m_renderer->IsHideCeilings();
    if (ImGui::Checkbox("Скрыть потолки", &hc)) m_renderer->SetHideCeilings(hc);

    ImVec2 viewSize = ImGui::GetContentRegionAvail();
    int w = (int)viewSize.x, h = (int)viewSize.y;
    if (w > 64 && h > 64) m_renderer->Resize(w, h);

    bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    m_renderer->Update(GetFrameTime(), windowHovered && !(m_play && m_captured), ImGui::IsWindowFocused());
    m_renderer->Render(*m_doc, *m_scene);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    rlImGuiImageRenderTextureFit(&m_renderer->GetRenderTexture(), true);
    bool imageHovered = ImGui::IsItemHovered();

    // drop префаба из Project
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("PREFAB")) {
            std::string file((const char*)p->Data);
            ImVec2 mp = ImGui::GetMousePos();
            Ray ray = GetScreenToWorldRay(Vector2{mp.x - origin.x, mp.y - origin.y}, m_renderer->GetCamera());
            Vector3 at = FocusPoint();
            if (std::fabs(ray.direction.y) > 1e-4f) {
                float t = -ray.position.y / ray.direction.y;
                if (t > 0) at = Vector3{ray.position.x + ray.direction.x * t, 0, ray.position.z + ray.direction.z * t};
            }
            float s = m_renderer->GetGridSnap();
            if (s > 0.001f) { at.x = core::SnapValue(at.x, s); at.z = core::SnapValue(at.z, s); }
            if (auto* o = m_scene->InstantiatePrefab(m_project.PrefabsDir() + "/" + file, at)) {
                m_doc->DeselectAll();
                m_scene->Select(o->id);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGuiIO& io = ImGui::GetIO();
    Vector2 mouseInVp{io.MousePos.x - origin.x, io.MousePos.y - origin.y};
    bool interact = !(m_play && m_captured);
    if (interact && (imageHovered || m_renderer->IsDragging())) {
        bool rmb = io.MouseDown[ImGuiMouseButton_Right];
        bool altOrbit = io.KeyAlt;
        if (!altOrbit && !rmb) {
            m_renderer->HandleMousePicking(mouseInVp, *m_doc, *m_scene, ImGui::IsMouseDown(ImGuiMouseButton_Left), ImGui::IsMouseClicked(ImGuiMouseButton_Left));
        }
        if (!rmb && !io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_renderer->SetTransformMode(render::TransformMode::Translate);
            if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_renderer->SetTransformMode(render::TransformMode::Rotate);
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_renderer->SetTransformMode(render::TransformMode::Scale);
            if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) m_renderer->SetTransformMode(render::TransformMode::Select);
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) ActionDeleteSelected();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) ActionDuplicateSelected();
            if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
                Vector3 p;
                if (m_renderer->GetSelectionPosition(*m_doc, *m_scene, &p)) m_renderer->FocusOn(p, 6.0f);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) m_renderer->FrameAll(*m_doc, *m_scene);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { m_doc->DeselectAll(); m_scene->Deselect(); }
        }
    }

    ImGui::End();
}

void EditorUI::RenderGameViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Игра", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    if (m_focusGameTab) {
        ImGui::SetWindowFocus();
        m_focusGameTab = false;
    }

    if (!m_play) {
        ImGui::SetCursorPos(ImVec2(16, 16));
        ImGui::TextDisabled("Нажмите Play (Ctrl+P), чтобы пройти главу прямо в редакторе.");
        ImGui::SetCursorPosX(16);
        ImGui::TextDisabled("Скрипты, предметы, двери, маньяк и свет работают так же, как в игре.");
        ImGui::End();
        return;
    }

    ImVec2 viewSize = ImGui::GetContentRegionAvail();
    int w = std::max(64, (int)viewSize.x), h = std::max(64, (int)viewSize.y);
    if (w != m_gameW || h != m_gameH || m_gameRT.id == 0) {
        if (m_gameRT.id != 0) UnloadRenderTexture(m_gameRT);
        m_gameRT = LoadRenderTexture(w, h);
        SetTextureFilter(m_gameRT.texture, TEXTURE_FILTER_BILINEAR);
        m_gameW = w;
        m_gameH = h;
    }

    bool hovered = ImGui::IsWindowHovered();
    if (!m_captured && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_captured = true;
        DisableCursor();
    }
    if (m_captured && IsKeyPressed(KEY_ESCAPE)) {
        m_captured = false;
        EnableCursor();
    }
    if (m_captured && !ImGui::IsWindowFocused()) {
        m_captured = false;
        EnableCursor();
    }

    m_play->Update(GetFrameTime(), m_captured);

    BeginTextureMode(m_gameRT);
    m_play->Render3D((float)w / (float)h);
    m_play->RenderHUD(w, h);
    EndTextureMode();

    ImVec2 origin = ImGui::GetCursorScreenPos();
    rlImGuiImageRenderTextureFit(&m_gameRT, true);

    if (!m_captured) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const char* msg = "Кликните, чтобы захватить мышь   (Esc — освободить)";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImVec2 p(origin.x + (w - ts.x) * 0.5f, origin.y + 12.0f);
        dl->AddRectFilled(ImVec2(p.x - 8, p.y - 4), ImVec2(p.x + ts.x + 8, p.y + ts.y + 4), IM_COL32(0, 0, 0, 170), 4.0f);
        dl->AddText(p, IM_COL32(255, 230, 160, 255), msg);
    }

    bool quit = m_play->WantsQuit();
    ImGui::End();
    if (quit) StopPlay();
}

// ---------------------------------------------------------------------------------------------
// Inspector
// ---------------------------------------------------------------------------------------------
const std::vector<runtime::ExportVar>& EditorUI::ExportsFor(const std::string& scriptFile) {
    static const std::vector<runtime::ExportVar> empty;
    if (scriptFile.empty() || !m_project.IsLoaded()) return empty;
    std::string path = m_project.ScriptPath(scriptFile);
    fs::file_time_type mt;
    if (!ReadFileTime(path, mt)) return empty;
    auto& c = m_exports[scriptFile];
    if (c.mtime != mt) {
        c.mtime = mt;
        c.vars = runtime::ScriptHost::ReadExports(path);
    }
    return c.vars;
}

void EditorUI::RenderInspector() {
    ImGui::Begin("Инспектор");
    if (auto* obj = m_scene->GetSelected()) {
        RenderObjectInspector(*obj);
    } else if (m_doc->GetSelectedBrushIndex() >= 0 && m_doc->GetSelectedBrushIndex() < (int)m_doc->GetWorldBrushes().size()) {
        RenderBrushInspector(m_doc->GetSelectedBrushIndex());
    } else {
        RenderSceneSettings();
    }
    ImGui::End();
}

void EditorUI::RenderSceneSettings() {
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "СЦЕНА: %s", m_scene->GetName().c_str());
    ImGui::TextDisabled("Выберите объект или браш в Иерархии / Сцене.");
    ImGui::Separator();
    auto& env = m_scene->Env();
    if (ImGui::SliderFloat("Ambient", &env.ambient, 0.0f, 0.6f)) m_scene->SetModified();
    if (ImGui::SliderFloat("Туман (дальность)", &env.fogEnd, 5.0f, 80.0f)) m_scene->SetModified();
    if (ColorEdit("Фон", env.background)) m_scene->SetModified();
    ImGui::Separator();
    ImGui::TextDisabled("Совет: освещение считается по лампам (Light) и фонарю игрока [F].");
    ImGui::TextDisabled("Без ламп на уровне будет почти темно — поднимите Ambient или добавьте Point Light.");
}

void EditorUI::RenderBrushInspector(int index) {
    auto& brush = m_doc->GetWorldBrushes()[index];
    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "БРАШ: %s", brush.name.c_str());
    ImGui::Separator();
    InputStr("Имя", brush.name);

    bool isBox = brush.faces.size() == 6;
    if (isBox) {
        Vector3 center = brush.center;
        Vector3 size{brush.bounds.max.x - brush.bounds.min.x, brush.bounds.max.y - brush.bounds.min.y, brush.bounds.max.z - brush.bounds.min.z};
        bool changed = false;
        Vector3 newCenter = center, newSize = size;
        changed |= DragV3("Позиция", newCenter, 0.1f);
        changed |= DragV3("Размер", newSize, 0.1f);
        if (changed) {
            newSize.x = std::max(0.05f, newSize.x);
            newSize.y = std::max(0.05f, newSize.y);
            newSize.z = std::max(0.05f, newSize.z);
            std::string tex = brush.faces[0].texture;
            map::MapBrush nb = map::BrushBuilder::CreateCubeAt(newCenter, newSize, tex);
            nb.name = brush.name;
            nb.selected = true;
            for (size_t i = 0; i < 6 && i < nb.faces.size(); ++i) nb.faces[i].texture = brush.faces[i].texture;
            brush = nb;
            m_doc->SetModified();
        }
    } else {
        ImGui::Text("Размер: %.2f x %.2f x %.2f", brush.bounds.max.x - brush.bounds.min.x, brush.bounds.max.y - brush.bounds.min.y, brush.bounds.max.z - brush.bounds.min.z);
        ImGui::TextDisabled("Не-коробочный браш: перемещение — гизмо.");
    }

    std::string tex = brush.faces.empty() ? "" : brush.faces[0].texture;
    if (TextureField("Текстура (все грани)", tex)) {
        for (auto& f : brush.faces) f.texture = tex;
        m_doc->SetModified();
    }
    ImGui::TextDisabled("Клик по текстуре во вкладке Project -> Textures применяет её к выбранному.");

    ImGui::Separator();
    if (ImGui::Button("Дублировать", ImVec2(-FLT_MIN, 0))) ActionDuplicateSelected();
    if (ImGui::Button("Выдолбить в комнату", ImVec2(-FLT_MIN, 0))) {
        auto hollowed = map::BrushBuilder::HollowBrush(brush, 0.25f, "motel_wallpaper");
        m_doc->RemoveBrush(index);
        for (const auto& b : hollowed) m_doc->AddBrush(b);
    }
    if (ImGui::Button("Удалить браш", ImVec2(-FLT_MIN, 0))) ActionDeleteSelected();
}

void EditorUI::RenderObjectInspector(scene::GameObject& o) {
    bool dirty = false;
    auto D = [&](bool c) { dirty |= c; return c; };

    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "ОБЪЕКТ  #%u", o.id);
    ImGui::Checkbox("##active", &o.active);
    ImGui::SameLine();
    D(InputStr("##name", o.name));
    D(InputStr("Тег", o.tag));
    if (o.parent != 0) {
        if (auto* p = m_scene->FindGameObject(o.parent)) ImGui::TextDisabled("Parent: %s (позиция локальная)", p->name.c_str());
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Трансформ", ImGuiTreeNodeFlags_DefaultOpen)) {
        D(DragV3("Позиция", o.transform.position, 0.1f));
        D(DragV3("Поворот", o.transform.rotation, 1.0f));
        D(DragV3("Масштаб", o.transform.scale, 0.05f));
        ImGui::TextDisabled(o.player.enabled || o.aiSpawn.enabled ? "Position = точка на полу (ноги)." : "Position = центр объекта. Поворот: Yaw (Y).");
    }

    if (ImGui::CollapsingHeader("Меш (Mesh Renderer)", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& m = o.meshRenderer;
        D(ImGui::Checkbox("Включён", &m.enabled));
        const char* shapes[] = {"Auto", "None", "Cube", "Sphere", "Cylinder"};
        int cur = 0;
        for (int i = 0; i < 5; ++i) if (m.meshName == shapes[i]) cur = i;
        if (D(ImGui::Combo("Форма", &cur, shapes, 5))) m.meshName = shapes[cur];
        D(TextureField("Текстура", m.textureName));
        D(ColorEdit("Цвет", m.tint));
        D(ImGui::Checkbox("Свечение (не затемнять)", &m.emissive));
        ImGui::TextDisabled("Auto: вид выбирается по компонентам (дверь, шкаф, ящик, предмет).");
    }

    auto removable = [&](const char* title, bool& enabled, auto&& body, bool openDefault = true) {
        if (!enabled) return;
        bool visible = true;
        if (ImGui::CollapsingHeader(title, &visible, openDefault ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            ImGui::PushID(title);
            body();
            ImGui::PopID();
        }
        if (!visible) { enabled = false; dirty = true; }
    };

    removable("PlayerController (игрок)", o.player.enabled, [&] {
        D(ImGui::DragFloat("Скорость ходьбы", &o.player.walkSpeed, 0.05f, 0.5f, 8.0f));
        D(ImGui::DragFloat("Скорость бега", &o.player.sprintSpeed, 0.05f, 0.5f, 12.0f));
        D(ImGui::DragFloat("Скорость в приседе", &o.player.crouchSpeed, 0.05f, 0.2f, 4.0f));
        D(ImGui::DragFloat("Высота глаз", &o.player.eyeHeight, 0.01f, 1.0f, 2.2f));
        D(ImGui::DragFloat("Радиус капсулы", &o.player.radius, 0.01f, 0.1f, 1.0f));
        D(ImGui::DragFloat("Высота капсулы", &o.player.height, 0.01f, 0.8f, 3.0f));
        D(ImGui::DragFloat("Скорость прыжка (0 = нет)", &o.player.jumpSpeed, 0.05f, 0.0f, 12.0f));
        D(ImGui::DragFloat("Гравитация", &o.player.gravity, 0.1f, 1.0f, 60.0f));
        D(ImGui::DragFloat("Чувствительность мыши", &o.player.mouseSens, 0.005f, 0.02f, 0.5f));
        D(ImGui::Checkbox("Фонарь в начале", &o.player.startWithFlashlight));
    });
    removable("Камера", o.camera.enabled, [&] {
        D(ImGui::SliderFloat("Угол обзора", &o.camera.fov, 40.0f, 110.0f));
        D(ImGui::Checkbox("Главная камера игрока", &o.camera.isMainPlayerCamera));
    });
    removable("Свет", o.light.enabled, [&] {
        D(ColorEdit("Цвет", o.light.color));
        D(ImGui::SliderFloat("Яркость", &o.light.intensity, 0.05f, 3.0f));
        D(ImGui::SliderFloat("Радиус", &o.light.radius, 1.0f, 30.0f));
        D(ImGui::Checkbox("Мерцание", &o.light.flicker));
    });
    removable("Коллайдер", o.collider.enabled, [&] {
        auto& c = o.collider;
        const char* shapes[] = {"Коробка (Box)", "Сфера / круг (Sphere)", "Капсула (Capsule)"};
        D(ImGui::Combo("Форма", &c.shape, shapes, 3));
        if (c.shape == 0) {
            D(DragV3("Размер (м)", c.size, 0.05f));
        } else {
            D(ImGui::DragFloat("Радиус", &c.radius, 0.02f, 0.05f, 20.0f));
            if (c.shape == 2) D(ImGui::DragFloat("Высота", &c.height, 0.02f, 0.1f, 20.0f));
        }
        D(DragV3("Смещение", c.offset, 0.05f));
        D(ImGui::Checkbox("Триггер (без столкновения)", &c.isTrigger));
        if (ImGui::Button("Подогнать под модель")) {
            const auto& sc = o.transform.scale;
            c.size = sc;
            c.radius = std::max({sc.x, sc.y, sc.z}) * 0.5f;
            if (c.shape == 2) { c.radius = std::max(sc.x, sc.z) * 0.5f; c.height = sc.y; }
            dirty = true;
        }
        ImGui::TextDisabled("Размеры в метрах и не зависят от Scale. В физике формы\nприближаются осевой коробкой по размеру коллайдера.");
    });
    removable("Rigidbody (физика)", o.rigidbody.enabled, [&] {
        auto& r = o.rigidbody;
        D(ImGui::DragFloat("Масса", &r.mass, 0.05f, 0.05f, 1000.0f));
        D(ImGui::Checkbox("Гравитация", &r.useGravity));
        D(ImGui::Checkbox("Кинематический (не падает)", &r.isKinematic));
        D(ImGui::SliderFloat("Сопротивление", &r.drag, 0.0f, 5.0f));
        D(ImGui::SliderFloat("Упругость (отскок)", &r.bounciness, 0.0f, 1.0f));
        D(ImGui::SliderFloat("Трение", &r.friction, 0.0f, 1.0f));
        if (!o.collider.enabled) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "Добавьте Коллайдер — без него тело проваливается.");
        ImGui::TextDisabled("Lua: obj:add_force / set_velocity / get_velocity.\nИгрок толкает тела; тела блокируют игрока.");
    });
    removable("Ящик (Search Slot)", o.searchSlot.enabled, [&] {
        auto& s = o.searchSlot;
        D(InputStr("ID слота", s.slotId));
        D(InputStr("Название", s.displayName));
        D(InputStr("Предмет внутри", s.containedItem));
        D(ImGui::Checkbox("Заперт", &s.isLocked));
        if (s.isLocked) {
            D(InputStr("Нужный инструмент", s.requiredTool));
            D(ImGui::Checkbox("Расходовать инструмент", &s.consumeTool));
        }
        D(ImGui::DragFloat("Время обыска (с)", &s.searchDuration, 0.05f, 0.1f, 10.0f));
        D(ImGui::DragFloat("Радиус шума (м)", &s.noiseRadius, 0.1f, 0.0f, 30.0f));
    });
    removable("Укрытие (Hiding Spot)", o.hidingSpot.enabled, [&] {
        auto& h = o.hidingSpot;
        const char* types[] = {"Wardrobe", "UnderBed", "Locker"};
        int cur = 0;
        for (int i = 0; i < 3; ++i) if (h.spotType == types[i]) cur = i;
        if (D(ImGui::Combo("Тип", &cur, types, 3))) h.spotType = types[cur];
        D(DragV3("Смещение глаз", h.eyeOffset, 0.05f));
        D(ImGui::SliderFloat("Угол выхода", &h.exitYaw, -180.0f, 360.0f));
        ImGui::TextDisabled("Exit Yaw: куда выходит игрок (0 = +Z, 90 = +X).");
    });
    removable("Дверь (Door)", o.door.enabled, [&] {
        auto& d = o.door;
        D(ImGui::Checkbox("Заперт", &d.isLocked));
        D(InputStr("Нужный ключ", d.requiredKey));
        D(ImGui::Checkbox("Расходовать ключ", &d.consumeKey));
        D(ImGui::Checkbox("Открыта в начале", &d.startOpen));
        D(ImGui::SliderFloat("Угол открытия", &d.openAngle, -170.0f, 170.0f));
        D(ImGui::SliderFloat("Скорость", &d.speed, 0.5f, 10.0f));
        D(InputStr("Звук открытия", d.openSound));
        ImGui::TextDisabled("Шарнир — на левом (-X) краю двери. Ширина двери = Scale.X.");
    });
    removable("Маньяк (ИИ)", o.aiSpawn.enabled, [&] {
        auto& a = o.aiSpawn;
        D(InputStr("Тип", a.monsterType));
        D(ImGui::SliderFloat("Радиус слуха", &a.hearingRadius, 2.0f, 30.0f));
        D(ImGui::SliderFloat("Угол зрения", &a.visionAngle, 30.0f, 180.0f));
        D(ImGui::SliderFloat("Дальность зрения", &a.visionDistance, 3.0f, 40.0f));
        D(ImGui::SliderFloat("Скорость ходьбы", &a.walkSpeed, 0.4f, 4.0f));
        D(ImGui::SliderFloat("Скорость погони", &a.chaseSpeed, 1.0f, 7.0f));
        D(ImGui::SliderFloat("Ожидание в точке", &a.waitAtWaypoint, 0.0f, 10.0f));
        ImGui::Text("Точки патруля (%zu)", a.patrolWaypoints.size());
        int removeIdx = -1;
        for (size_t i = 0; i < a.patrolWaypoints.size(); ++i) {
            ImGui::PushID((int)i);
            ImGui::SetNextItemWidth(-60.0f);
            D(DragV3("##wp", a.patrolWaypoints[i], 0.1f));
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) removeIdx = (int)i;
            ImGui::PopID();
        }
        if (removeIdx >= 0) { a.patrolWaypoints.erase(a.patrolWaypoints.begin() + removeIdx); dirty = true; }
        if (ImGui::Button("+ Точка (в центре вида)")) {
            Vector3 p = FocusPoint();
            a.patrolWaypoints.push_back(Vector3{p.x, o.transform.position.y, p.z});
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ у маньяка")) {
            a.patrolWaypoints.push_back(m_scene->WorldPosition(o));
            dirty = true;
        }
        ImGui::TextDisabled("Маньяк ходит по точкам, слышит шум, видит игрока в конусе.");
    });
    removable("Предмет (Item Pickup)", o.itemPickup.enabled, [&] {
        D(InputStr("ID предмета", o.itemPickup.itemId));
        if (ImGui::BeginCombo("##itempick", "Выбрать из главы...")) {
            for (const auto& it : m_project.Manifest().items) {
                if (ImGui::Selectable((it.name + "  (" + it.id + ")").c_str())) { o.itemPickup.itemId = it.id; dirty = true; }
            }
            ImGui::EndCombo();
        }
    });

    // --- Script ---
    removable("Скрипт (Lua)", o.script.enabled, [&] {
        auto& s = o.script;
        auto scripts = m_project.ListScripts();
        ImGui::SetNextItemWidth(-90.0f);
        if (ImGui::BeginCombo("##scriptfile", s.scriptFile.empty() ? "(выберите скрипт)" : s.scriptFile.c_str())) {
            if (ImGui::Selectable("(нет)", s.scriptFile.empty())) { s.scriptFile.clear(); dirty = true; }
            for (const auto& f : scripts) {
                if (ImGui::Selectable(f.c_str(), f == s.scriptFile)) { s.scriptFile = f; dirty = true; }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Открыть") && !s.scriptFile.empty()) m_codeEditor.OpenFile(s.scriptFile);
        if (ImGui::Button("Новый скрипт для объекта")) {
            std::string base = o.name;
            std::replace(base.begin(), base.end(), ' ', '_');
            std::string f = ToLower(base) + ".lua";
            if (m_codeEditor.CreateScript(f)) {
                s.scriptFile = f;
                dirty = true;
            }
        }
        D(ImGui::Checkbox("Интерактивный (подсказка [E])", &s.interactable));
        if (s.interactable) D(InputStr("Подсказка", s.interactPrompt));

        const auto& ex = ExportsFor(s.scriptFile);
        if (!ex.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Параметры скрипта (exports):");
            for (const auto& e : ex) {
                std::string cur = s.parameters.count(e.name) ? s.parameters[e.name] : e.defaultValue;
                ImGui::PushID(e.name.c_str());
                if (e.type == "bool") {
                    bool v = (cur == "true" || cur == "1");
                    if (ImGui::Checkbox(e.name.c_str(), &v)) { s.parameters[e.name] = v ? "true" : "false"; dirty = true; }
                } else if (e.type == "number") {
                    float v = (float)std::atof(cur.c_str());
                    if (ImGui::DragFloat(e.name.c_str(), &v, 0.05f)) {
                        char b[64];
                        std::snprintf(b, sizeof(b), "%g", v);
                        s.parameters[e.name] = b;
                        dirty = true;
                    }
                } else {
                    if (InputStr(e.name.c_str(), cur)) { s.parameters[e.name] = cur; dirty = true; }
                }
                ImGui::PopID();
            }
        } else if (!s.scriptFile.empty()) {
            ImGui::TextDisabled("В скрипте нет exports = { ... } — параметров нет.");
        }
    });

    // --- Custom properties ---
    if (ImGui::CollapsingHeader("Свойства узла (self:get_prop / set_prop)", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::string eraseKey;
        for (auto& kv : o.props) {
            ImGui::PushID(kv.first.c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
            ImGui::TextUnformatted(kv.first.c_str());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-40.0f);
            D(InputStr("##v", kv.second));
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) eraseKey = kv.first;
            ImGui::PopID();
        }
        if (!eraseKey.empty()) { o.props.erase(eraseKey); dirty = true; }
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
        ImGui::InputTextWithHint("##nk", "key", m_newPropKey, sizeof(m_newPropKey));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-60.0f);
        ImGui::InputTextWithHint("##nv", "value", m_newPropVal, sizeof(m_newPropVal));
        ImGui::SameLine();
        if (ImGui::Button("Добавить") && m_newPropKey[0]) {
            o.props[m_newPropKey] = m_newPropVal;
            m_newPropKey[0] = 0;
            m_newPropVal[0] = 0;
            dirty = true;
        }
    }

    // --- Add Component ---
    ImGui::Separator();
    if (ImGui::Button("Добавить компонент...", ImVec2(-FLT_MIN, 28))) ImGui::OpenPopup("AddComponentPopup");
    if (ImGui::BeginPopup("AddComponentPopup")) {
        auto add = [&](const char* label, bool& flag, auto&& init) {
            if (flag) return;
            if (ImGui::MenuItem(label)) { flag = true; init(); dirty = true; }
        };
        add("PlayerController (игрок)", o.player.enabled, [&] { if (!o.camera.enabled) { o.camera.enabled = true; o.camera.isMainPlayerCamera = true; } });
        add("Камера", o.camera.enabled, [] {});
        add("Свет", o.light.enabled, [] {});
        add("Коллайдер", o.collider.enabled, [&] { o.collider.size = o.transform.scale; o.collider.radius = std::max({o.transform.scale.x, o.transform.scale.y, o.transform.scale.z}) * 0.5f; });
        add("Rigidbody (физика)", o.rigidbody.enabled, [&] { if (!o.collider.enabled) { o.collider.enabled = true; o.collider.size = o.transform.scale; } });
        add("Ящик (Search Slot)", o.searchSlot.enabled, [&] { o.searchSlot.slotId = o.name; });
        add("Укрытие (Hiding Spot)", o.hidingSpot.enabled, [] {});
        add("Дверь (Door)", o.door.enabled, [] {});
        add("Маньяк (ИИ)", o.aiSpawn.enabled, [] {});
        add("Предмет (Item Pickup)", o.itemPickup.enabled, [] {});
        add("Скрипт (Lua)", o.script.enabled, [] {});
        ImGui::EndPopup();
    }

    ImGui::Separator();
    if (ImGui::Button("Дублировать")) ActionDuplicateSelected();
    ImGui::SameLine();
    if (ImGui::Button("Удалить")) { ActionDeleteSelected(); return; }
    ImGui::SameLine();
    if (ImGui::Button("Сохранить как префаб")) {
        std::string fname = o.name;
        std::replace(fname.begin(), fname.end(), ' ', '_');
        if (m_scene->SavePrefab(o.id, m_project.PrefabsDir() + "/" + fname + ".kmprefab")) core::Log::Success("Префаб сохранён: " + fname + ".kmprefab");
    }

    if (dirty) m_scene->SetModified();
}

// ---------------------------------------------------------------------------------------------
// Project panel
// ---------------------------------------------------------------------------------------------
void EditorUI::RenderTexturesTab() {
    auto& texMgr = render::TextureManager::Instance();
    const auto& items = texMgr.GetAllTextures();
    ImGui::InputTextWithHint("##TextureSearch", "Фильтр текстур...", m_textureFilter, IM_ARRAYSIZE(m_textureFilter));
    ImGui::SameLine();
    ImGui::TextDisabled("Клик: применить к выбранному брашу / объекту");
    std::string search = ToLower(m_textureFilter);

    float thumb = 64.0f;
    float cell = thumb + 16.0f;
    int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cell));
    if (ImGui::BeginTable("TextureGrid", columns)) {
        for (const auto& item : items) {
            if (!search.empty() && ToLower(item.name).find(search) == std::string::npos) continue;
            ImGui::TableNextColumn();
            ImGui::PushID(item.name.c_str());
            if (rlImGuiImageButtonSize(item.name.c_str(), &item.texture, Vector2{thumb, thumb})) {
                int selB = m_doc->GetSelectedBrushIndex();
                if (auto* o = m_scene->GetSelected()) {
                    o->meshRenderer.textureName = item.name;
                    if (o->meshRenderer.meshName == "None") o->meshRenderer.meshName = "Cube";
                    m_scene->SetModified();
                } else if (selB >= 0 && selB < (int)m_doc->GetWorldBrushes().size()) {
                    for (auto& face : m_doc->GetWorldBrushes()[selB].faces) face.texture = item.name;
                    m_doc->SetModified();
                }
            }
            ImGui::TextWrapped("%s", item.name.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void EditorUI::RenderProjectPanel() {
    ImGui::Begin("Проект");
    if (!m_project.IsLoaded()) {
        ImGui::TextDisabled("Нет открытого проекта. File -> New Chapter Project...");
        ImGui::End();
        return;
    }
    if (ImGui::BeginTabBar("ProjectTabs")) {
        if (ImGui::BeginTabItem("Скрипты")) {
            for (const auto& s : m_project.ListScripts()) {
                if (ImGui::Selectable(s.c_str(), s == m_codeEditor.CurrentFile())) m_codeEditor.OpenFile(s);
            }
            ImGui::Separator();
            if (ImGui::Button("+ Новый скрипт")) m_codeEditor.CreateScript("novyi_skript");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Префабы")) {
            auto prefabs = m_project.ListPrefabs();
            if (prefabs.empty()) ImGui::TextDisabled("Нет префабов. ПКМ по объекту в Hierarchy -> Save as Prefab.");
            for (const auto& p : prefabs) {
                ImGui::Selectable(p.c_str());
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("PREFAB", p.c_str(), p.size() + 1);
                    ImGui::Text("Префаб: %s", p.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (auto* o = m_scene->InstantiatePrefab(m_project.PrefabsDir() + "/" + p, FocusPoint())) {
                        m_doc->DeselectAll();
                        m_scene->Select(o->id);
                    }
                }
            }
            ImGui::TextDisabled("Перетащите в Scene или двойной клик. В Lua: game.spawn('имя', x, y, z).");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Текстуры")) {
            RenderTexturesTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------------------------
// Файловый проводник
// ---------------------------------------------------------------------------------------------
bool EditorUI::OpenExternalMap(const std::string& path) {
    StopPlay();
    m_doc->Clear();
    if (!map::MapParser::LoadFromFile(path, *m_doc)) {
        core::Log::Error("Не удалось открыть карту: " + path);
        return false;
    }
    std::string stem = fs::path(path).stem().string();
    m_scene->Clear();
    m_scene->SetName(stem);
    m_scene->Env().ambient = 0.35f;
    m_externalMap = path;
    m_levelName = "[карта] " + stem;
    m_doc->SetModified(false);
    m_renderer->FrameAll(*m_doc, *m_scene);
    core::Log::Success("Карта открыта для просмотра: " + path + "  (в проект не сохраняется; «Импорт как уровень» — в Менеджере сцен)");
    return true;
}

bool EditorUI::ImportMapAsLevel(const std::string& path) {
    if (!m_project.IsLoaded()) return false;
    std::string stem = fs::path(path).stem().string();
    std::string name = stem;
    int n = 1;
    while (fs::exists(m_project.LevelScenePath(name))) name = stem + "_" + std::to_string(++n);
    scene::Scene sc;
    map::MapDocument tmp;
    runtime::ProjectTemplate::BuildEmptyLevel(sc, tmp, name);
    // убрать пол и лампу шаблона пустого уровня: оставляем только игрока
    for (const char* nm : {"Лампа"}) if (auto* o = sc.FindByName(nm)) sc.DestroyGameObject(o->id);
    sc.Env().ambient = 0.35f;
    sc.SaveToFile(m_project.LevelScenePath(name));
    std::error_code ec;
    fs::create_directories(m_project.Root() + "/levels", ec);
    fs::copy_file(path, m_project.LevelMapPath(name), fs::copy_options::overwrite_existing, ec);
    if (ec) { core::Log::Error("Копирование карты: " + ec.message()); return false; }
    core::Log::Success("Карта импортирована как уровень: " + name);
    return OpenLevel(name);
}

void EditorUI::RenameLevel(const std::string& from, const std::string& to) {
    if (to.empty() || from == to || fs::exists(m_project.LevelScenePath(to))) { core::Log::Warn("Переименование невозможно."); return; }
    std::error_code ec;
    fs::rename(m_project.LevelScenePath(from), m_project.LevelScenePath(to), ec);
    if (fs::exists(m_project.LevelMapPath(from))) fs::rename(m_project.LevelMapPath(from), m_project.LevelMapPath(to), ec);
    if (m_project.Manifest().startLevel == from) m_project.Manifest().startLevel = to;
    m_project.Save();
    if (m_levelName == from) m_levelName = to;
    core::Log::Success("Сцена переименована: " + from + " -> " + to);
}

void EditorUI::DuplicateLevel(const std::string& level) {
    std::string name = level + "_copy";
    int n = 1;
    while (fs::exists(m_project.LevelScenePath(name))) name = level + "_copy" + std::to_string(++n);
    std::error_code ec;
    fs::copy_file(m_project.LevelScenePath(level), m_project.LevelScenePath(name), ec);
    if (fs::exists(m_project.LevelMapPath(level))) fs::copy_file(m_project.LevelMapPath(level), m_project.LevelMapPath(name), ec);
    core::Log::Success("Сцена продублирована: " + name);
}

void EditorUI::DeleteLevel(const std::string& level) {
    auto levels = m_project.ListLevels();
    if (levels.size() <= 1) { core::Log::Warn("Нельзя удалить единственную сцену."); return; }
    std::error_code ec;
    fs::remove(m_project.LevelScenePath(level), ec);
    fs::remove(m_project.LevelMapPath(level), ec);
    levels = m_project.ListLevels();
    if (m_project.Manifest().startLevel == level && !levels.empty()) { m_project.Manifest().startLevel = levels.front(); m_project.Save(); }
    if (m_levelName == level && !levels.empty()) OpenLevel(levels.front());
    core::Log::Success("Сцена удалена: " + level);
}

void EditorUI::RenderSceneManager() {
    ImGui::Begin("Менеджер сцен");
    if (!m_project.IsLoaded()) {
        ImGui::TextDisabled("Нет открытого проекта.");
        ImGui::End();
        return;
    }
    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1.0f), "Проект: %s", m_project.Manifest().title.c_str());
    ImGui::TextDisabled("Сцена = файл уровня (.kmscene + .map). Двойной клик — открыть.");
    ImGui::Separator();

    auto levels = m_project.ListLevels();
    int idx = 0;
    for (const auto& l : levels) {
        bool cur = (l == m_levelName) && m_externalMap.empty();
        bool start = (l == m_project.Manifest().startLevel);
        std::string label = std::to_string(idx++) + "  " + l + (start ? "   [старт]" : "") + (cur ? "   <- открыта" : "");
        if (ImGui::Selectable(label.c_str(), cur, ImGuiSelectableFlags_AllowDoubleClick)) {
            if (ImGui::IsMouseDoubleClicked(0) && !cur) OpenLevel(l);
        }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Открыть")) OpenLevel(l);
            if (ImGui::MenuItem("Сделать стартовой")) { m_project.Manifest().startLevel = l; m_project.Save(); }
            if (ImGui::MenuItem("Дублировать")) DuplicateLevel(l);
            if (ImGui::MenuItem("Переименовать...")) {
                m_renameLevel = l;
                std::snprintf(m_renameBuf, sizeof(m_renameBuf), "%s", l.c_str());
                ImGui::CloseCurrentPopup();
                m_pendingDeleteLevel.clear();
                ImGui::OpenPopup("RenameLevelPopup");
            }
            if (ImGui::MenuItem("Удалить...")) m_pendingDeleteLevel = l;
            ImGui::EndPopup();
        }
    }
    if (!m_externalMap.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Просмотр карты: %s", fs::path(m_externalMap).filename().string().c_str());
        if (ImGui::Button("Импортировать как уровень проекта")) ImportMapAsLevel(m_externalMap);
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputText("##nl", m_newLevelName, sizeof(m_newLevelName));
    ImGui::SameLine();
    if (ImGui::Button("+ Новая сцена")) CreateLevel(m_newLevelName);

    if (m_renameLevel.size() && !ImGui::IsPopupOpen("RenameLevelPopup")) ImGui::OpenPopup("RenameLevelPopup");
    if (ImGui::BeginPopupModal("RenameLevelPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Новое имя", m_renameBuf, sizeof(m_renameBuf));
        if (ImGui::Button("ОК", ImVec2(100, 0))) { RenameLevel(m_renameLevel, m_renameBuf); m_renameLevel.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(100, 0))) { m_renameLevel.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (!m_pendingDeleteLevel.empty()) ImGui::OpenPopup("DeleteLevelPopup");
    if (ImGui::BeginPopupModal("DeleteLevelPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Удалить сцену «%s» (файлы .kmscene и .map)?", m_pendingDeleteLevel.c_str());
        if (ImGui::Button("Удалить", ImVec2(120, 0))) { DeleteLevel(m_pendingDeleteLevel); m_pendingDeleteLevel.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(120, 0))) { m_pendingDeleteLevel.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    ImGui::End();
}

void EditorUI::RenderFileExplorer() {
    ImGui::Begin("Файлы");

    std::vector<std::pair<std::string, std::string>> roots;
    if (m_project.IsLoaded()) roots.push_back({"Проект", m_project.Root()});
    if (!m_gameRoot.empty()) {
        roots.push_back({"Игра: assets", m_gameRoot + "/assets"});
        roots.push_back({"Игра: карты (data/maps)", m_gameRoot + "/assets/data/maps"});
    }
    roots.push_back({"Все проекты", m_projectsDir});
    if (m_explorerRoot >= (int)roots.size()) m_explorerRoot = 0;

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("##root", roots[m_explorerRoot].first.c_str())) {
        for (int i = 0; i < (int)roots.size(); ++i) {
            if (ImGui::Selectable(roots[i].first.c_str(), i == m_explorerRoot)) { m_explorerRoot = i; m_explorerDir = roots[i].second; }
        }
        ImGui::EndCombo();
    }
    const std::string& rootDir = roots[m_explorerRoot].second;
    if (m_explorerDir.empty() || m_explorerDir.rfind(rootDir, 0) != 0) m_explorerDir = rootDir;

    ImGui::SameLine();
    if (ImGui::Button("^ Вверх") && m_explorerDir != rootDir) {
        m_explorerDir = fs::path(m_explorerDir).parent_path().generic_string();
        if (m_explorerDir.rfind(rootDir, 0) != 0) m_explorerDir = rootDir;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_explorerDir.c_str());
    ImGui::Separator();

    std::vector<fs::directory_entry> dirs, files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(m_explorerDir, ec)) {
        if (e.path().filename().string().rfind('.', 0) == 0) continue;
        (e.is_directory(ec) ? dirs : files).push_back(e);
    }
    auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b) { return a.path().filename() < b.path().filename(); };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);

    ImGui::BeginChild("fileList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 6.0f));
    int shown = 0;
    auto drawEntry = [&](const fs::directory_entry& e, bool isDir) {
        if (++shown > 600) return;
        std::string name = e.path().filename().string();
        std::string ext = ToLower(e.path().extension().string());
        std::string label = (isDir ? "[папка]  " : "") + name;
        ImGui::PushID(name.c_str());
        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(0)) {
            std::string full = e.path().generic_string();
            std::string parent = e.path().parent_path().generic_string();
            if (isDir) {
                m_explorerDir = full;
            } else if (ext == ".lua" || ext == ".json" || ext == ".txt" || ext == ".md" || ext == ".kmprefab") {
                if (ext == ".kmprefab" && m_project.IsLoaded() && parent == fs::path(m_project.PrefabsDir()).generic_string()) {
                    if (auto* o = m_scene->InstantiatePrefab(full, FocusPoint())) { m_doc->DeselectAll(); m_scene->Select(o->id); }
                } else {
                    m_codeEditor.OpenPath(full);
                }
            } else if (ext == ".kmscene" || ext == ".map") {
                std::string levelsDir = m_project.IsLoaded() ? fs::path(m_project.Root() + "/levels").generic_string() : "";
                if (!levelsDir.empty() && parent == levelsDir) OpenLevel(e.path().stem().string());
                else if (ext == ".map") OpenExternalMap(full);
                else core::Log::Warn("Сцену (.kmscene) вне проекта открыть нельзя.");
            } else if (ext == ".png") {
                core::Log::Info("Текстура: " + e.path().stem().string() + " (применяйте во вкладке «Текстуры»)");
            }
        }
        if (!isDir && ext == ".kmprefab" && m_project.IsLoaded() && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("PREFAB", name.c_str(), name.size() + 1);
            ImGui::Text("Префаб: %s", name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginPopupContextItem()) {
            if (!isDir && ext == ".map" && ImGui::MenuItem("Открыть для просмотра")) OpenExternalMap(e.path().generic_string());
            if (!isDir && ext == ".map" && m_project.IsLoaded() && ImGui::MenuItem("Импортировать как уровень проекта")) ImportMapAsLevel(e.path().generic_string());
            if (!isDir && (ext == ".lua" || ext == ".json" || ext == ".txt") && ImGui::MenuItem("Открыть в редакторе")) m_codeEditor.OpenPath(e.path().generic_string());
            if (ImGui::MenuItem("Удалить...")) m_pendingDelete = e.path().generic_string();
            ImGui::EndPopup();
        }
        ImGui::PopID();
    };
    for (const auto& d : dirs) drawEntry(d, true);
    for (const auto& f : files) drawEntry(f, false);
    ImGui::EndChild();

    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##nf", m_newFileName, sizeof(m_newFileName));
    ImGui::SameLine();
    if (ImGui::Button("+ Файл")) {
        std::ofstream(m_explorerDir + "/" + m_newFileName) << "";
        std::string nm = m_newFileName;
        if (nm.size() > 4 && nm.substr(nm.size() - 4) == ".lua") m_codeEditor.OpenPath(m_explorerDir + "/" + nm);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##nd", m_newFolderName, sizeof(m_newFolderName));
    ImGui::SameLine();
    if (ImGui::Button("+ Папка")) { std::error_code e2; fs::create_directories(m_explorerDir + "/" + m_newFolderName, e2); }

    if (!m_pendingDelete.empty()) ImGui::OpenPopup("DeleteFilePopup");
    if (ImGui::BeginPopupModal("DeleteFilePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Удалить «%s»?", m_pendingDelete.c_str());
        if (ImGui::Button("Удалить", ImVec2(120, 0))) {
            std::error_code e3;
            fs::remove_all(m_pendingDelete, e3);
            core::Log::Info("Удалено: " + m_pendingDelete);
            m_pendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(120, 0))) { m_pendingDelete.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------------------------
// Chapter panel
// ---------------------------------------------------------------------------------------------
void EditorUI::RenderChapterPanel() {
    ImGui::Begin("Глава");
    if (!m_project.IsLoaded()) {
        ImGui::TextDisabled("Нет открытого проекта.");
        ImGui::End();
        return;
    }
    auto& m = m_project.Manifest();

    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1.0f), "ГЛАВА: %s  [%s]", m.title.c_str(), m.id.c_str());
    ImGui::Separator();

    if (ImGui::BeginTabBar("ChapterTabs")) {
        if (ImGui::BeginTabItem("Настройки")) {
            InputStr("Название", m.title);
            InputStr("Описание", m.description);
            InputStr("Вступительный субтитр", m.intro);
            InputStr("Текст проигрыша", m.loseText);
            InputStr("Заголовок победы", m.winTitle);

            auto levels = m_project.ListLevels();
            if (ImGui::BeginCombo("Стартовый уровень", m.startLevel.c_str())) {
                for (const auto& l : levels) if (ImGui::Selectable(l.c_str(), l == m.startLevel)) m.startLevel = l;
                ImGui::EndCombo();
            }
            ImGui::SliderFloat("Расход батареи фонаря (1/с)", &m.flashlightDrain, 0.0f, 0.03f, "%.4f");

            ImGui::Separator();
            ImGui::Text("Стартовые предметы:");
            int rem = -1;
            for (size_t i = 0; i < m.startItems.size(); ++i) {
                ImGui::PushID((int)i);
                ImGui::SetNextItemWidth(220.0f);
                InputStr("##si", m.startItems[i]);
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) rem = (int)i;
                ImGui::PopID();
            }
            if (rem >= 0) m.startItems.erase(m.startItems.begin() + rem);
            if (ImGui::SmallButton("+ предмет")) m.startItems.push_back("");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Предметы")) {
            ImGui::TextDisabled("База предметов главы: id используется в ящиках, дверях, скриптах.");
            int rem = -1;
            if (ImGui::BeginTable("items", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, ImGui::GetContentRegionAvail().y - 30))) {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 1.2f);
                ImGui::TableSetupColumn("Название", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Описание", ImGuiTableColumnFlags_WidthStretch, 2.0f);
                ImGui::TableSetupColumn("##x", ImGuiTableColumnFlags_WidthFixed, 24.0f);
                ImGui::TableHeadersRow();
                for (size_t i = 0; i < m.items.size(); ++i) {
                    ImGui::PushID((int)i);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN); InputStr("##id", m.items[i].id);
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN); InputStr("##nm", m.items[i].name);
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN); InputStr("##ds", m.items[i].description);
                    ImGui::TableNextColumn(); if (ImGui::SmallButton("X")) rem = (int)i;
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (rem >= 0) m.items.erase(m.items.begin() + rem);
            if (ImGui::Button("+ Добавить предмет")) m.items.push_back(runtime::ItemDef{"item_new", "Новый предмет", ""});
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Проверка и экспорт")) {
            if (ImGui::Button("Проверить главу (скрипты, софтлоки)")) {
                SaveAll();
                m_issues = m_project.Validate();
            }

            ImGui::Separator();
            ImGui::BeginChild("issues");
            for (const auto& i : m_issues) {
                ImVec4 col(0.8f, 0.8f, 0.8f, 1.0f);
                const char* pre = "[i]";
                if (i.severity == runtime::ValidationIssue::Severity::Warning) { col = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); pre = "[!]"; }
                if (i.severity == runtime::ValidationIssue::Severity::Error) { col = ImVec4(1.0f, 0.35f, 0.3f, 1.0f); pre = "[X]"; }
                ImGui::TextColored(col, "%s %s", pre, i.text.c_str());
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void EditorUI::RenderMapTools() {
    ImGui::Begin("Инструменты карты");
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "ГЕНЕРАТОР КОМНАТЫ (браши)");
    ImGui::Separator();

    ImGui::DragFloat("Ширина (X)", &m_roomWidth, 0.25f, 2.0f, 64.0f, "%.2f m");
    ImGui::DragFloat("Длина (Z)", &m_roomLength, 0.25f, 2.0f, 64.0f, "%.2f m");
    ImGui::DragFloat("Высота (Y)", &m_roomHeight, 0.1f, 1.5f, 16.0f, "%.2f m");
    ImGui::DragFloat("Толщина стен", &m_roomWallThick, 0.05f, 0.1f, 1.0f, "%.2f m");

    std::string w = m_roomWallTex, f = m_roomFloorTex, c = m_roomCeilingTex;
    if (TextureField("Стена", w)) std::snprintf(m_roomWallTex, sizeof(m_roomWallTex), "%s", w.c_str());
    if (TextureField("Пол", f)) std::snprintf(m_roomFloorTex, sizeof(m_roomFloorTex), "%s", f.c_str());
    if (TextureField("Потолок", c)) std::snprintf(m_roomCeilingTex, sizeof(m_roomCeilingTex), "%s", c.c_str());

    if (ImGui::Button("Создать комнату в центре вида", ImVec2(-FLT_MIN, 30))) {
        Vector3 p = FocusPoint();
        auto brushes = map::BrushBuilder::CreateHollowRoom(Vector3{p.x, m_roomHeight * 0.5f, p.z}, Vector3{m_roomWidth, m_roomHeight, m_roomLength},
                                                           m_roomWallThick, m_roomWallTex, m_roomFloorTex, m_roomCeilingTex);
        for (const auto& b : brushes) m_doc->AddBrush(b);
        core::Log::Success("Комната создана: " + std::to_string(brushes.size()) + " брашей.");
    }
    ImGui::TextDisabled("Проёмы для дверей: создайте стены отдельными Wall Brush-ами и меняйте Size в Inspector.");
    ImGui::End();
}

void EditorUI::RenderConsole() {
    ImGui::Begin("Консоль");
    const auto& entries = core::Log::Instance().GetEntries();
    if (ImGui::Button("Очистить")) core::Log::Instance().Clear();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu сообщений", entries.size());
    ImGui::Separator();
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& entry : entries) {
        ImVec4 col{0.8f, 0.8f, 0.8f, 1.0f};
        const char* prefix = "[INFO]";
        if (entry.level == core::LogLevel::Warning) { col = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); prefix = "[WARN]"; }
        else if (entry.level == core::LogLevel::Error) { col = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); prefix = "[ERROR]"; }
        else if (entry.level == core::LogLevel::Success) { col = ImVec4(0.3f, 1.0f, 0.4f, 1.0f); prefix = "[OK]"; }
        ImGui::TextColored(col, "%s %s %s", entry.timeString.c_str(), prefix, entry.message.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

int EditorUI::RunBuildChapter35() {
    std::string err;
    if (!tools::BuildChapter35(m_projectsDir, err)) {
        std::printf("[FAIL] BuildChapter35: %s\n", err.c_str());
        return 1;
    }
    std::printf("Проект chapter_3_5 собран: %s/chapter_3_5\n", m_projectsDir.c_str());
    return 0;
}

int EditorUI::RunSelfTest35() {
    int fails = 0;
    auto check = [&](bool ok, const std::string& what) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
        std::fflush(stdout);
        if (!ok) ++fails;
    };
    if (!OpenProject("chapter_3_5")) { std::printf("[FAIL] OpenProject\n"); return 1; }
    int errs = 0;
    for (const auto& i : m_project.Validate()) {
        if (i.severity == runtime::ValidationIssue::Severity::Error) { ++errs; std::printf("   ERROR: %s\n", i.text.c_str()); }
    }
    check(errs == 0, "Validate: нет ошибок");

    auto startWith = [&](runtime::PlaySession& ps, const std::string& ending) {
        runtime::PlaySession::Hooks h;
        if (!m_gameRoot.empty()) h.gameAssetsDir = m_gameRoot + "/assets";
        runtime::Var v; v.type = runtime::Var::Type::String; v.str = ending;
        h.initialVars["ch3_ending"] = v;
        ps.SetHooks(h);
        ps.SetPauseEnabled(false);
        return ps.Start(m_project, "razyezd");
    };
    auto tickN = [&](runtime::PlaySession& ps, float sec) { for (int i = 0; i < (int)(sec * 30); ++i) ps.Update(1.0f / 30.0f, false); };

    // разные начала
    {
        runtime::PlaySession a; check(startWith(a, "car"), "Старт: car"); tickN(a, 9);
        check(a.HasItem("item_fuse"), "car: предохранитель выдан");
        runtime::PlaySession b; startWith(b, "sewer"); tickN(b, 9);
        check(b.Player().battery < 0.1f, "sewer: фонарь почти разряжен");
        runtime::PlaySession c; startWith(c, "radio"); tickN(c, 16);
        check(c.GetVar("know_code").boolean, "radio: код известен с начала");
        check(c.Player().pos.x < -20.0f, "radio: Лера появилась у ручья (x=" + std::to_string(c.Player().pos.x) + ")");
    }

    runtime::PlaySession ps;
    startWith(ps, "front_door");
    tickN(ps, 12);
    check(!ps.Player().locked && !ps.Player().moveLocked, "После вступления игрок свободен");

    // «стоячие»: касание возвращает ко входу
    ps.TeleportPlayer({18.0f, 0, 21.0f}, 270.0f);
    tickN(ps, 14);
    check(ps.Player().pos.x < 15.5f, "Стоячие дошли и вернули игрока к входу (x=" + std::to_string(ps.Player().pos.x) + ")");

    // умная навигация: игрок снаружи депо смотрит в другую сторону — «стоячие» должны выйти через проём, а не толпиться у стены
    {
        ps.TeleportPlayer({11.0f, 0, 21.2f}, 270.0f);
        float minX = 1e9f;
        for (int i = 0; i < 30 * 25; ++i) {
            ps.TeleportPlayer({11.0f, 0, 21.2f}, 270.0f);
            ps.Update(1.0f / 30.0f, false);
            for (const auto& m : ps.Monsters()) {
                auto* o = ps.GetScene().FindGameObject(m.objId);
                if (o && o->tag == "mimic") minX = std::min(minX, m.pos.x);
            }
        }
        check(minX < 15.0f, "ИИ: «стоячие» нашли проём и вышли из депо (мин. x=" + std::to_string(minX) + ")");
    }
    // предметы и кассеты
    ps.DebugUse("Ящик_будки"); ps.DebugUse("Скамейка"); ps.DebugUse("Верстак_депо");
    ps.DebugUse("Шкафчик_платформы"); ps.DebugUse("Ящик_депо");
    check(ps.HasItem("item_tape_1") && ps.HasItem("item_tape_2") && ps.HasItem("item_tape_3"), "Все три кассеты найдены");
    check(ps.HasItem("item_switch_key") && ps.HasItem("item_fuse"), "Ключ стрелки и предохранитель найдены");
    for (int i = 0; i < 3; ++i) { ps.DebugUse("Магнитофон"); tickN(ps, 20); }
    check(ps.GetVar("heard_1").boolean && ps.GetVar("heard_2").boolean && ps.GetVar("heard_3").boolean, "Магнитофон проиграл три кассеты");

    // дрезина
    ps.DebugUse("Пульт_дрезины");
    check(ps.CodeOverlayActive(), "Пульт запросил код");
    ps.DebugAnswerCode("1186");
    tickN(ps, 6);
    check(ps.CurrentLevel() == "tunnel", "Дрезина увезла в туннель");

    // поездка (E нажимаем «виртуально» каждый кадр)
    for (int i = 0; i < 30 * 70 && ps.Player().pos.x < 68.0f; ++i) {
        if (auto* r = ps.GetScene().FindByName("Маршрут")) r->props["e"] = "true";
        ps.Update(1.0f / 30.0f, false);
    }
    tickN(ps, 20);
    check(ps.Player().pos.x > 67.0f, "Поездка завершена (x=" + std::to_string(ps.Player().pos.x) + ")");
    check(!ps.Player().moveLocked, "После поездки движение разблокировано");

    ps.DebugUse("Рюкзак_Леры");
    tickN(ps, 60);
    check(ps.State() == runtime::EndState::Won, "Выбор у ворот -> победа");
    check(ps.GetVar("ch35_took_tapes").boolean, "Флаг ch35_took_tapes записан");

    std::printf("\nИТОГ 3.5: %s (%d провалов)\n", fails == 0 ? "ВСЕ ТЕСТЫ ПРОЙДЕНЫ" : "ЕСТЬ ОШИБКИ", fails);
    return fails == 0 ? 0 : 1;
}

int EditorUI::RunExport(const std::string& projectName) {
    if (!OpenProject(projectName)) {
        std::printf("[FAIL] OpenProject %s\n", projectName.c_str());
        return 1;
    }
    m_issues = m_project.Validate();
    for (const auto& i : m_issues) {
        const char* sev = i.severity == runtime::ValidationIssue::Severity::Error ? "ERROR" : (i.severity == runtime::ValidationIssue::Severity::Warning ? "WARN" : "INFO");
        std::printf("  [%s] %s\n", sev, i.text.c_str());
    }
    bool ok = ExportToGame();
    for (const auto& e : core::Log::Instance().GetEntries()) std::printf("  %s\n", e.message.c_str());
    std::printf("%s\n", ok ? "EXPORT OK" : "EXPORT FAILED");
    return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------
// Автотест: полное прохождение примера без ввода
// ---------------------------------------------------------------------------------------------
int EditorUI::RunSelfTest(const std::string& projectName) {
    int fails = 0;
    auto check = [&](bool ok, const std::string& what) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
        std::fflush(stdout);
        if (!ok) ++fails;
    };

    if (!OpenProject(projectName)) {
        std::printf("[FAIL] OpenProject %s\n", projectName.c_str());
        return 1;
    }
    auto issues = m_project.Validate();
    int errs = 0;
    for (const auto& i : issues) {
        if (i.severity == runtime::ValidationIssue::Severity::Error) { ++errs; std::printf("   ERROR: %s\n", i.text.c_str()); }
    }
    check(errs == 0, "Validate: нет ошибок/софтлоков");

    auto ex = runtime::ScriptHost::ReadExports(m_project.ScriptPath("generator.lua"));
    check(!ex.empty() && ex[0].name == "required_item", "ReadExports(generator.lua)");

    runtime::PlaySession ps;
    runtime::PlaySession::Hooks hooks;
    if (!m_gameRoot.empty()) hooks.gameAssetsDir = m_gameRoot + "/assets";
    ps.SetHooks(hooks);
    ps.SetPauseEnabled(false);
    check(ps.Start(m_project, m_project.Manifest().startLevel), "Start level1");
    check(ps.DebugTriCount() > 100 && ps.DebugBoxCount() > 10, "Геометрия карты загружена: tris=" + std::to_string(ps.DebugTriCount()) + " boxes=" + std::to_string(ps.DebugBoxCount()));

    auto tick = [&](float seconds) {
        int n = (int)(seconds * 30.0f);
        for (int i = 0; i < n; ++i) ps.Update(1.0f / 30.0f, false);
    };
    tick(6.0f);
    check(std::fabs(ps.Player().pos.y) < 0.05f, "Игрок стоит на полу (y≈0): " + std::to_string(ps.Player().pos.y));
    check(ps.State() == runtime::EndState::Playing, "Игра идёт");

    ps.DebugUse("Crate_Tools");
    check(ps.HasItem("item_crowbar"), "Ящик -> монтировка");
    ps.DebugUse("Crate_Locked");
    check(ps.HasItem("item_bunker_key"), "Запертый ящик (монтировка) -> ключ");
    ps.DebugUse("Door_East");
    tick(3.0f);
    check(ps.Rt(ps.GetScene().FindByName("Door_East")->id).doorOpen, "Дверь отперта ключом и открыта");
    check(ps.Rt(ps.GetScene().FindByName("Door_East")->id).doorT > 0.99f, "Анимация двери завершена");

    ps.TeleportPlayer({8.0f, 0.0f, 0.0f}, 90.0f);
    tick(1.0f);
    ps.DebugUse("Batteries");
    check(ps.HasItem("item_batteries"), "Подбор батареек");
    check(!ps.GetScene().FindByName("Batteries")->active, "Предмет исчез со сцены");

    ps.TeleportPlayer({13.5f, 0.0f, 3.0f}, 0.0f);
    ps.DebugUse("Crate_Fuse");
    check(ps.HasItem("item_fuse"), "Предохранитель найден");
    ps.DebugUse("Generator");
    check(ps.GetVar("power_on").boolean, "Lua: генератор включил питание (flag power_on)");
    check(!ps.HasItem("item_fuse"), "Lua: предохранитель потрачен");

    ps.DebugUse("Exit_Hatch");
    check(ps.CodeOverlayActive(), "Lua: люк запросил код (ask_code)");
    check(!ps.DebugAnswerCode("0000"), "Неверный код отклонён");
    ps.DebugUse("Exit_Hatch");
    ps.DebugAnswerCode("4815");
    tick(4.0f);
    check(ps.CurrentLevel() == "level2", "Lua: корутина fade/wait -> load_level('level2')");
    check(ps.HasItem("item_batteries"), "Инвентарь переносится между уровнями");

    // физика: Rigidbody падает на пол, отскакивает и останавливается; прыжок игрока
    {
        auto* ball = ps.GetScene().CreateGameObject("ТестШар");
        ball->transform.position = {6.0f, 2.0f, 0.5f};
        ball->transform.scale = {0.5f, 0.5f, 0.5f};
        ball->collider.enabled = true; ball->collider.shape = 1; ball->collider.radius = 0.25f;
        ball->rigidbody.enabled = true; ball->rigidbody.bounciness = 0.5f;
        ps.AdoptObject(ball->id);
        tick(4.0f);
        float by = ps.GetScene().FindByName("ТестШар")->transform.position.y;
        check(by > 0.2f && by < 0.35f, "Физика: шар упал на пол и лежит (y=" + std::to_string(by) + ")");
        ps.DestroyObject(ps.GetScene().FindByName("ТестШар")->id);
        tick(0.2f);
    }
    // маньяк ловит игрока
    ps.TeleportPlayer({10.5f, 0.0f, 0.0f}, 90.0f);
    tick(25.0f);
    check(ps.State() == runtime::EndState::Lost, "Маньяк заметил и поймал игрока (Lost)");

    ps.RestartLevel();
    tick(1.0f);
    check(ps.State() == runtime::EndState::Playing, "Рестарт уровня после смерти");

    // укрытие
    ps.TeleportPlayer({9.0f, 0.0f, 0.5f}, 0.0f);
    ps.DebugUse("Locker");
    check(ps.Player().hiddenIn != 0, "Игрок спрятался в шкафчике");
    ps.DebugUse("Locker");
    check(ps.Player().hiddenIn == 0, "Игрок вышел из укрытия");

    // префаб
    auto* spawned = ps.Spawn("crate", {2.0f, 0.35f, 0.0f});
    check(spawned != nullptr, "game.spawn: префаб crate");

    // победа
    ps.TeleportPlayer({25.0f, 0.0f, 0.0f}, 90.0f);
    tick(0.5f);
    check(ps.State() == runtime::EndState::Won, "Lua: ending.lua -> game.win()");

    std::printf("\nИТОГ: %s (%d провалов)\n", fails == 0 ? "ВСЕ ТЕСТЫ ПРОЙДЕНЫ" : "ЕСТЬ ОШИБКИ", fails);
    std::fflush(stdout);
    return fails == 0 ? 0 : 1;
}

}
