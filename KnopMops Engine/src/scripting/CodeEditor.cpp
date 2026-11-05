#include "scripting/CodeEditor.h"
#include "core/Log.h"
#include "imgui.h"
#include "runtime/ScriptHost.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace knopmops::scripting {

namespace fs = std::filesystem;

namespace {

struct EditCtx {
    std::string* text;
    int* line;
    int* col;
    int* jumpLine;
};

int EditCallback(ImGuiInputTextCallbackData* data) {
    EditCtx* ctx = static_cast<EditCtx*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        ctx->text->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = ctx->text->data();
    } else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
        if (*ctx->jumpLine > 0) {
            int ln = 1, pos = 0;
            while (pos < data->BufTextLen && ln < *ctx->jumpLine) {
                if (data->Buf[pos] == '\n') ++ln;
                ++pos;
            }
            data->CursorPos = pos;
            data->SelectionStart = pos;
            int end = pos;
            while (end < data->BufTextLen && data->Buf[end] != '\n') ++end;
            data->SelectionEnd = end;
            *ctx->jumpLine = -1;
        }
        int ln = 1, col = 1;
        for (int i = 0; i < data->CursorPos && i < data->BufTextLen; ++i) {
            if (data->Buf[i] == '\n') { ++ln; col = 1; } else ++col;
        }
        *ctx->line = ln;
        *ctx->col = col;
    }
    return 0;
}

const char* kApiHelp =
    "ОБЪЕКТ / УЗЕЛ (self и game.find(...)):\n"
    "  self.name / self.tag / self.active / self.x .y .z / self.yaw      поля\n"
    "  self:get_position() -> x,y,z      self:set_position(x,y,z)\n"
    "  self:set_active(b)  self:destroy()  self:distance_to_player()  self:distance_to(obj)\n"
    "  self:get_prop(k[,def]) / self:set_prop(k,v)     свойства узла (любые, задаются в Инспекторе)\n"
    "  self:lock() unlock() is_locked() open() close() toggle() is_open()   двери и ящики\n"
    "  self:set_item(id) / get_item()   содержимое ящика\n"
    "  self:set_light(b) set_color(r,g,b) set_visible(b) set_interactable(b[,текст])\n"
    "  self:set_texture(name) set_emissive(b)\n"
    "  ФИЗИКА (Rigidbody): self:add_force(x,y,z) set_velocity(x,y,z) get_velocity() is_grounded()\n"
    "  МАНЬЯК: self:set_mode('patrol'|'chase'|'search'|'investigate'|'idle') get_mode()\n"
    "          self:freeze(b) set_speed(walk,chase) investigate(x,y,z)\n"
    "  self.params.<имя>   значения из exports (настраиваются в Инспекторе)\n\n"
    "ХУКИ (объявляйте функции в скрипте):\n"
    "  on_start(self)  on_update(self,dt)  on_interact(self)  on_locked(self)  on_unlocked(self)\n"
    "  on_search(self,item)  on_item_picked(self,item)  on_door_opened(self)  on_door_closed(self)\n"
    "  on_hide(self)  on_unhide(self)  on_trigger_enter(self,player)  on_trigger_exit(self,player)\n\n"
    "game.*:\n"
    "  notify(текст[,сек])  subtitle(текст[,сек])  log(...)\n"
    "  has_item(id) give_item(id[,silent]) take_item(id) items()\n"
    "  set(k,v) get(k[,def]) set_flag(k,b) get_flag(k)       переменные главы\n"
    "  find(имя) find_all(тег) spawn(префаб,x,y,z) player() player_pos()\n"
    "  raycast(ox,oy,oz, dx,dy,dz[,дальность]) -> попал,дист,x,y,z,имя_объекта\n"
    "  teleport_player(x,y,z[,yaw]) lock_player(b) enable_flashlight(b) player_hidden()\n"
    "  make_noise(x,y,z,радиус) alert_monsters(x,y,z)\n"
    "  load_level(имя[,spawn]) level() win(заголовок,текст) lose(текст)\n"
    "  fade(alpha,сек) shake(сила,сек) cutscene_camera(px,py,pz, lx,ly,lz) cutscene_camera_end()\n"
    "  play_tv(кассета[,сек]) stop_tv() is_tv_playing()\n"
    "  ask_code(подсказка, правильный_код, function(ok,введено) ... end)\n"
    "  play_sound(путь[,громк]) play_ambient(путь[,громк]) stop_ambient()\n"
    "  after(сек,fn) every(сек,fn) cancel(id) start(fn) wait(сек) time()\n"
    "  key_pressed('E') key_down('SHIFT') distance(x1,y1,z1,x2,y2,z2)\n"
    "  on('событие', fn)  события: level_loaded item_added item_removed item_used player_caught\n"
    "                     monster_mode noise trigger_enter slot_searched player_hidden var_changed chapter_won\n";

}

void CodeEditor::SetProject(runtime::ChapterProject* project) {
    if (m_project != project) {
        m_project = project;
        m_file.clear();
        m_path.clear();
        m_text.clear();
        m_modified = false;
        m_errors.clear();
        m_status.clear();
    }
}

void CodeEditor::OpenFile(const std::string& scriptFile) {
    if (!m_project || scriptFile.empty()) return;
    OpenPath(m_project->ScriptPath(scriptFile));
}

void CodeEditor::OpenPath(const std::string& fullPath) {
    if (fullPath.empty()) return;
    if (m_modified && !m_path.empty()) SaveFile();
    std::ifstream f(fullPath, std::ios::binary);
    if (!f.is_open()) {
        core::Log::Error("Файл не найден: " + fullPath);
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    m_text = ss.str();
    m_path = fullPath;
    m_file = fs::path(fullPath).filename().string();
    m_modified = false;
    m_requestFocus = true;
    RunCheck();
}

bool CodeEditor::SaveFile() {
    if (m_path.empty()) return false;
    std::ofstream f(m_path, std::ios::binary);
    if (!f.is_open()) {
        core::Log::Error("Не удалось сохранить: " + m_path);
        return false;
    }
    f << m_text;
    m_modified = false;
    RunCheck();
    return true;
}

bool CodeEditor::CreateScript(const std::string& fileName) {
    if (!m_project) return false;
    std::string name = fileName;
    if (name.size() < 4 || name.substr(name.size() - 4) != ".lua") name += ".lua";
    if (fs::exists(m_project->ScriptPath(name))) {
        core::Log::Warn("Скрипт уже существует: " + name);
        OpenFile(name);
        return false;
    }
    fs::create_directories(m_project->ScriptsDir());
    std::ofstream f(m_project->ScriptPath(name), std::ios::binary);
    f << "-- " << name << "\n";
    f.close();
    core::Log::Success("Создан скрипт: " + name);
    OpenFile(name);
    return true;
}

void CodeEditor::RunCheck() {
    m_errors.clear();
    m_status.clear();
    if (m_path.empty()) return;
    if (fs::path(m_path).extension() != ".lua") return;
    std::string err;
    if (runtime::ScriptHost::CheckSyntaxText(m_text, "scripts/" + m_file, err)) {
        m_status = "Синтаксис в порядке";
    } else {
        std::stringstream ss(err);
        std::string line;
        while (std::getline(ss, line)) m_errors.push_back(line);
        m_status = "Ошибок: " + std::to_string(m_errors.size());
    }
}

void CodeEditor::Render() {
    ImGui::Begin("Редактор скриптов");

    if (!m_project || !m_project->IsLoaded()) {
        ImGui::TextDisabled("Откройте проект (Файл -> Открыть проект).");
        ImGui::End();
        return;
    }

    if (m_requestFocus) {
        ImGui::SetWindowFocus();
        m_requestFocus = false;
    }

    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) SaveFile();

    if (ImGui::Button("Сохранить (Ctrl+S)")) SaveFile();
    ImGui::SameLine();
    if (ImGui::Button("Проверить")) RunCheck();
    ImGui::SameLine();

    auto scripts = m_project->ListScripts();
    ImGui::SetNextItemWidth(220.0f);
    std::string preview = m_file.empty() ? "(выберите скрипт)" : m_file + (m_modified ? " *" : "");
    if (ImGui::BeginCombo("##ScriptCombo", preview.c_str())) {
        for (const auto& s : scripts) {
            if (ImGui::Selectable(s.c_str(), s == m_file)) OpenFile(s);
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Новый скрипт...")) ImGui::OpenPopup("NewScriptPopup");
    ImGui::SameLine();
    ImGui::Checkbox("Справка API", &m_showApi);

    if (ImGui::BeginPopup("NewScriptPopup")) {
        ImGui::InputText("Имя файла", m_newName, sizeof(m_newName));
        if (ImGui::Button("Создать пустой скрипт")) {
            CreateScript(m_newName);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    float errH = m_errors.empty() ? 0.0f : std::min(110.0f, 22.0f + m_errors.size() * 18.0f);
    float footerH = ImGui::GetTextLineHeightWithSpacing() + errH + 6.0f;
    float apiW = m_showApi ? ImGui::GetContentRegionAvail().x * 0.42f : 0.0f;

    ImVec2 editSize(ImGui::GetContentRegionAvail().x - apiW - (m_showApi ? 8.0f : 0.0f), ImGui::GetContentRegionAvail().y - footerH);
    if (m_path.empty()) {
        ImGui::BeginChild("NoFile", editSize, true);
        ImGui::TextDisabled("Выберите скрипт в списке, откройте .lua в окне «Файлы» или создайте новый.\nСкрипты назначаются объектам в Инспекторе -> Скрипт.");
        ImGui::EndChild();
    } else {
        EditCtx ctx{&m_text, &m_cursorLine, &m_cursorCol, &m_jumpLine};
        m_text.reserve(m_text.size() + 1);
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways;
        if (ImGui::InputTextMultiline("##code", m_text.data(), m_text.capacity() + 1, editSize, flags, EditCallback, &ctx)) {
            m_modified = true;
        }
    }

    if (m_showApi) {
        ImGui::SameLine();
        ImGui::BeginChild("ApiHelp", ImVec2(apiW, ImGui::GetContentRegionAvail().y - footerH), true);
        ImGui::TextUnformatted(kApiHelp);
        ImGui::EndChild();
    }

    if (!m_errors.empty()) {
        ImGui::BeginChild("Errors", ImVec2(0, errH), true);
        for (const auto& e : m_errors) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
            if (ImGui::Selectable(e.c_str())) {
                size_t p1 = e.find(':');
                if (p1 != std::string::npos) {
                    int ln = std::atoi(e.c_str() + p1 + 1);
                    if (ln > 0) m_jumpLine = ln;
                }
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }

    ImGui::TextDisabled("Стр %d, кол %d   |   %s%s", m_cursorLine, m_cursorCol, m_status.c_str(), m_modified ? "   |   не сохранено" : "");

    ImGui::End();
}

}
