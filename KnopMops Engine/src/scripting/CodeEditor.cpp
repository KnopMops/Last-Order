#include "scripting/CodeEditor.h"
#include "imgui.h"
#include "core/Log.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace knopmops::scripting {

CodeEditor::CodeEditor() {
    SetupDefaultTemplates();
}

CodeEditor::~CodeEditor() = default;

void CodeEditor::Init() {
    if (!m_templates.empty()) {
        LoadTemplate(0);
    }
}

void CodeEditor::SetSourceText(const std::string& text) {
    m_sourceText = text;
    m_textBuffer.assign(text.begin(), text.end());
    m_textBuffer.resize(std::max(m_textBuffer.size() + 1024, size_t(65536)), '\0');
    m_isModified = false;
}

void CodeEditor::LoadTemplate(size_t index) {
    if (index < m_templates.size()) {
        m_currentFile = m_templates[index].filename;
        SetSourceText(m_templates[index].code);
        core::Log::Info("Loaded script template: " + m_templates[index].name);
    }
}

void CodeEditor::OpenFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        core::Log::Error("Could not open script file: " + path);
        return;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    m_currentFile = path;
    SetSourceText(ss.str());
    core::Log::Success("Opened script: " + path);
}

bool CodeEditor::SaveFile(const std::string& path) {
    std::string target = path.empty() ? m_currentFile : path;
    if (target.empty()) target = "assets/scripts/CustomScript.cpp";

    m_sourceText = m_textBuffer.data();
    std::ofstream file(target);
    if (!file.is_open()) {
        core::Log::Error("Failed to save script to: " + target);
        return false;
    }
    file << m_sourceText;
    m_currentFile = target;
    m_isModified = false;
    core::Log::Success("Saved script file: " + target);
    return true;
}

void CodeEditor::SetupDefaultTemplates() {
    m_templates.push_back({
        "Player Controller (FPS + Stamina + Flashlight)",
        "PlayerController.cpp",
        "#include \"game/PlayerController.h\"\n\n"
        "namespace game::chapter4 {\n\n"
        "void PlayerController::OnStart() {\n"
        "    m_stamina = 100.0f;\n"
        "    m_hasFlashlight = true;\n"
        "    m_flashlightBattery = 1.0f;\n"
        "}\n\n"
        "void PlayerController::OnUpdate(float dt) {\n"
        "    bool sprint = IsKeyDown(KEY_LEFT_SHIFT) && m_stamina > 0.0f;\n"
        "    float speed = sprint ? 5.2f : 2.8f;\n\n"
        "    if (sprint) m_stamina = std::max(0.0f, m_stamina - dt * 15.0f);\n"
        "    else m_stamina = std::min(100.0f, m_stamina + dt * 8.0f);\n\n"
        "    if (IsKeyPressed(KEY_F)) {\n"
        "        m_flashlightOn = !m_flashlightOn;\n"
        "        PlaySfx(\"audio/sfx/sfx_flashlight_click.wav\");\n"
        "    }\n"
        "}\n\n"
        "}\n"
    });

    m_templates.push_back({
        "Prowler Monster AI (Patrol + Investigate + Hunt)",
        "ProwlerMonsterAI.cpp",
        "#include \"game/ProwlerMonsterAI.h\"\n\n"
        "namespace game::chapter4 {\n\n"
        "void ProwlerMonsterAI::OnStart() {\n"
        "    m_state = AIState::Patrol;\n"
        "    m_moveSpeed = 2.4f;\n"
        "    m_hearingRadius = 9.0f;\n"
        "}\n\n"
        "void ProwlerMonsterAI::OnUpdate(float dt) {\n"
        "    switch (m_state) {\n"
        "    case AIState::Patrol:\n"
        "        MoveAlongWaypoints(dt);\n"
        "        if (CanSeePlayer()) EnterHuntMode();\n"
        "        break;\n"
        "    case AIState::InvestigateSound:\n"
        "        WalkToNoiseSource(dt);\n"
        "        if (ReachedNoiseSource()) {\n"
        "            SearchRoomForHidingPlayer(8.0f);\n"
        "            SetNotification(\"Видимо, показалось...\");\n"
        "            m_state = AIState::Patrol;\n"
        "        }\n"
        "        break;\n"
        "    case AIState::Hunt:\n"
        "        ChasePlayer(dt * 1.5f);\n"
        "        if (PlayerInDistance(1.2f)) KillPlayer();\n"
        "        break;\n"
        "    }\n"
        "}\n\n"
        "void ProwlerMonsterAI::OnHearNoise(Vector3 soundPos, float volume) {\n"
        "    if (m_state != AIState::Hunt) {\n"
        "        m_noiseTarget = soundPos;\n"
        "        m_state = AIState::InvestigateSound;\n"
        "        SetNotification(\"МАНЬЯК УСЛЫШАЛ ШУМ И ИДЕТ ПРОВЕРЯТЬ!\");\n"
        "    }\n"
        "}\n\n"
        "}\n"
    });

    m_templates.push_back({
        "Keypad Lock Puzzle (4-Digit PIN Code)",
        "KeypadLock.cpp",
        "#include \"game/KeypadLock.h\"\n\n"
        "namespace game::chapter4 {\n\n"
        "void KeypadLock::OnStart() {\n"
        "    m_correctPin = \"4815\";\n"
        "    m_currentInput.clear();\n"
        "    m_isUnlocked = false;\n"
        "}\n\n"
        "void KeypadLock::PressDigit(char digit) {\n"
        "    if (m_isUnlocked || m_currentInput.size() >= 4) return;\n"
        "    m_currentInput.push_back(digit);\n"
        "    PlaySfx(\"audio/sfx/sfx_ch3_keypad_beep.wav\");\n\n"
        "    if (m_currentInput.size() == 4) {\n"
        "        if (m_currentInput == m_correctPin) {\n"
        "            m_isUnlocked = true;\n"
        "            PlaySfx(\"audio/sfx/sfx_door_unlocked.wav\");\n"
        "            ShowNotification(\"Электронный замок открыт!\");\n"
        "        } else {\n"
        "            PlaySfx(\"audio/sfx/sfx_keypad_error.wav\");\n"
        "            m_currentInput.clear();\n"
        "        }\n"
        "    }\n"
        "}\n\n"
        "}\n"
    });

    m_templates.push_back({
        "Car Escape Ending (Repair & Drive)",
        "CarEscapeEnding.cpp",
        "#include \"game/CarEscapeEnding.h\"\n\n"
        "namespace game::chapter4 {\n\n"
        "bool CarEscapeEnding::TryStartCar(const std::vector<std::string>& inventory) {\n"
        "    bool hasBattery = HasItem(inventory, \"item_car_battery\");\n"
        "    bool hasSparkplug = HasItem(inventory, \"item_car_sparkplug\");\n"
        "    bool hasGas = HasItem(inventory, \"item_gas_canister\");\n"
        "    bool hasKeys = HasItem(inventory, \"item_car_key\");\n\n"
        "    if (hasBattery && hasSparkplug && hasGas && hasKeys) {\n"
        "        PlayCutscene(\"ending_car_escape\");\n"
        "        return true;\n"
        "    }\n"
        "    ShowNotification(\"Автомобиль не заводится: проверьте детали мотора.\");\n"
        "    return false;\n"
        "}\n\n"
        "}\n"
    });
}

void CodeEditor::Render() {
    ImGui::Begin("Code & Script Editor");

    if (ImGui::Button("Save Script [Ctrl+S]")) {
        SaveFile();
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::BeginCombo("##TemplatesCombo", ("Template: " + m_currentFile).c_str())) {
        for (size_t i = 0; i < m_templates.size(); ++i) {
            bool selected = (m_currentFile == m_templates[i].filename);
            if (ImGui::Selectable(m_templates[i].name.c_str(), selected)) {
                LoadTemplate(i);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();

    ImGui::TextDisabled("File: %s", m_currentFile.c_str());

    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (m_textBuffer.empty()) SetSourceText("// Write Chapter 4 C++ script code here\n");

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
    if (ImGui::InputTextMultiline("##ScriptSource", m_textBuffer.data(), m_textBuffer.size(),
                                  ImVec2(-FLT_MIN, -FLT_MIN), flags)) {
        m_isModified = true;
    }

    ImGui::End();
}

}

