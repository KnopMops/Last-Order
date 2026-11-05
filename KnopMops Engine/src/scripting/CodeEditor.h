#pragma once

#include <string>
#include <vector>

namespace knopmops::scripting {

struct ScriptTemplate {
    std::string name;
    std::string filename;
    std::string code;
};

class CodeEditor {
public:
    CodeEditor();
    ~CodeEditor();

    void Init();
    void Render();

    void OpenFile(const std::string& path);
    bool SaveFile(const std::string& path = "");

    const std::string& GetCurrentFile() const { return m_currentFile; }
    const std::string& GetSourceText() const { return m_sourceText; }
    void SetSourceText(const std::string& text);

    const std::vector<ScriptTemplate>& GetTemplates() const { return m_templates; }
    void LoadTemplate(size_t index);

private:
    std::string m_currentFile = "PlayerController.cpp";
    std::string m_sourceText;
    std::vector<char> m_textBuffer;
    bool m_isModified = false;
    std::vector<ScriptTemplate> m_templates;

    void SetupDefaultTemplates();
};

}

