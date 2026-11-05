#pragma once

#include "runtime/ChapterProject.h"
#include <string>
#include <vector>

namespace knopmops::scripting {

// Встроенный редактор Lua-скриптов (scripts/*.lua и любые .lua через файловый проводник).
class CodeEditor {
public:
    void SetProject(runtime::ChapterProject* project);
    void Render();

    // Открывает скрипт по имени внутри scripts/ (например, "generator.lua").
    void OpenFile(const std::string& scriptFile);
    // Открывает любой текстовый файл по полному пути.
    void OpenPath(const std::string& fullPath);
    bool SaveFile();
    // Создаёт пустой скрипт в scripts/ и открывает его.
    bool CreateScript(const std::string& fileName);

    const std::string& CurrentFile() const { return m_file; }
    bool IsModified() const { return m_modified; }

private:
    runtime::ChapterProject* m_project = nullptr;
    std::string m_file;   // имя для показа
    std::string m_path;   // полный путь
    std::string m_text;
    bool m_modified = false;
    bool m_requestFocus = false;
    std::vector<std::string> m_errors;
    std::string m_status;
    int m_cursorLine = 1;
    int m_cursorCol = 1;
    int m_jumpLine = -1;
    char m_newName[64] = "novyi_skript.lua";
    bool m_showApi = false;

    void RunCheck();
};

}
