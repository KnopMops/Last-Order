#include "engine/dialogue/DialogueSystem.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace engine::dialogue {

void DialogueSystem::Init(core::StateStore* stateStore, ActionCallback onAction) {
    m_state = stateStore;
    m_onAction = std::move(onAction);
}

static std::vector<core::EngineAction> ParseActions(const nlohmann::json& arr) {
    std::vector<core::EngineAction> out;
    if (!arr.is_array()) return out;
    for (const auto& item : arr) {
        core::EngineAction act;
        act.type = item.value("type", "");
        act.target = item.value("target", "");
        act.intValue = item.value("value", 0);
        if (!act.type.empty()) out.push_back(act);
    }
    return out;
}

bool DialogueSystem::LoadFromJsonFile(const std::string& jsonFilePath) {
    std::ifstream in(jsonFilePath);
    if (!in.is_open()) return false;

    nlohmann::json root;
    try {
        in >> root;
    } catch (...) {
        return false;
    }

    if (!root.contains("nodes") || !root["nodes"].is_array()) return false;

    for (const auto& jn : root["nodes"]) {
        DialogueNode node;
        node.id = jn.value("id", "");
        node.timeoutSec = jn.value("timeoutSec", 0.0f);
        node.timeoutNextNode = jn.value("timeoutNextNode", "");

        if (jn.contains("lines") && jn["lines"].is_array()) {
            for (const auto& jl : jn["lines"]) {
                node.lines.push_back(DialogueLine{
                    jl.value("speaker", ""),
                    jl.value("text", "")
                });
            }
        }

        if (jn.contains("choices") && jn["choices"].is_array()) {
            for (const auto& jc : jn["choices"]) {
                DialogueChoice ch;
                ch.text = jc.value("text", "");
                ch.nextNode = jc.value("nextNode", "");
                ch.requireFlag = jc.value("requireFlag", "");
                ch.forbidFlag = jc.value("forbidFlag", "");
                ch.requireHeldItem = jc.value("requireHeldItem", "");
                if (jc.contains("actions")) {
                    ch.actions = ParseActions(jc["actions"]);
                }
                node.choices.push_back(ch);
            }
        }

        if (jn.contains("onEnd")) {
            node.onEnd = ParseActions(jn["onEnd"]);
        }

        if (!node.id.empty()) {
            m_nodes[node.id] = std::move(node);
        }
    }
    return true;
}

void DialogueSystem::RegisterNode(const DialogueNode& node) {
    m_nodes[node.id] = node;
}

bool DialogueSystem::StartDialogue(const std::string& nodeId) {
    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end() || it->second.lines.empty()) {
        CloseDialogue();
        return false;
    }

    m_activeNode = &it->second;
    m_lineIndex = 0;
    m_revealedChars = 0.0f;
    m_selectedChoice = 0;
    m_timeoutRemaining = m_activeNode->timeoutSec;
    RebuildFilteredChoices();
    return true;
}

void DialogueSystem::CloseDialogue() {
    m_activeNode = nullptr;
    m_lineIndex = 0;
    m_revealedChars = 0.0f;
    m_filteredChoices.clear();
    m_selectedChoice = 0;
    m_timeoutRemaining = 0.0f;
}

void DialogueSystem::RebuildFilteredChoices() {
    m_filteredChoices.clear();
    if (!m_activeNode) return;

    for (const auto& ch : m_activeNode->choices) {
        if (!ch.requireFlag.empty() && m_state && !m_state->HasFlag(ch.requireFlag)) continue;
        if (!ch.forbidFlag.empty() && m_state && m_state->HasFlag(ch.forbidFlag)) continue;
        if (!ch.requireHeldItem.empty() && m_state && m_state->GetHeldItem() != ch.requireHeldItem) continue;
        m_filteredChoices.push_back(ch);
    }
    if (m_selectedChoice >= static_cast<int>(m_filteredChoices.size())) {
        m_selectedChoice = 0;
    }
}

void DialogueSystem::Update(float dt) {
    if (!m_activeNode) return;

    const DialogueLine* line = GetCurrentLine();
    if (line) {
        size_t totalCp = CountUTF8Codepoints(line->text);
        if (m_revealedChars < static_cast<float>(totalCp)) {
            m_revealedChars = std::min(static_cast<float>(totalCp), m_revealedChars + dt * m_charsPerSec);
        }
    }

    if (IsWaitingForChoice() && HasActiveTimeout()) {
        m_timeoutRemaining -= dt;
        if (m_timeoutRemaining <= 0.0f) {
            std::string fallback = m_activeNode->timeoutNextNode;
            ExecuteActions(m_activeNode->onEnd);
            if (!fallback.empty()) {
                StartDialogue(fallback);
            } else {
                CloseDialogue();
            }
        }
    }
}

void DialogueSystem::OnAdvanceOrConfirm() {
    if (!m_activeNode) return;

    const DialogueLine* line = GetCurrentLine();
    if (!line) return;

    if (!IsLineFullyRevealed()) {
        m_revealedChars = static_cast<float>(CountUTF8Codepoints(line->text));
        return;
    }

    if (m_lineIndex + 1 < m_activeNode->lines.size()) {
        m_lineIndex++;
        m_revealedChars = 0.0f;
        return;
    }

    if (!m_filteredChoices.empty()) {
        int idx = std::clamp(m_selectedChoice, 0, static_cast<int>(m_filteredChoices.size()) - 1);
        DialogueChoice chosen = m_filteredChoices[idx];
        auto endActions = m_activeNode->onEnd;
        ExecuteActions(endActions);
        ExecuteActions(chosen.actions);
        if (!chosen.nextNode.empty()) {
            StartDialogue(chosen.nextNode);
        } else {
            CloseDialogue();
        }
        return;
    }

    auto endActions = m_activeNode->onEnd;
    CloseDialogue();
    ExecuteActions(endActions);
}

void DialogueSystem::OnSelectDelta(int delta) {
    if (!IsWaitingForChoice() || m_filteredChoices.empty()) return;
    int count = static_cast<int>(m_filteredChoices.size());
    m_selectedChoice = (m_selectedChoice + delta + count) % count;
}

void DialogueSystem::OnSelectIndex(int index) {
    if (!IsWaitingForChoice()) return;
    if (index >= 0 && index < static_cast<int>(m_filteredChoices.size())) {
        m_selectedChoice = index;
        OnAdvanceOrConfirm();
    }
}

bool DialogueSystem::IsWaitingForChoice() const {
    return m_activeNode && (m_lineIndex + 1 == m_activeNode->lines.size()) &&
           IsLineFullyRevealed() && !m_filteredChoices.empty();
}

const DialogueLine* DialogueSystem::GetCurrentLine() const {
    if (!m_activeNode || m_lineIndex >= m_activeNode->lines.size()) return nullptr;
    return &m_activeNode->lines[m_lineIndex];
}

bool DialogueSystem::IsLineFullyRevealed() const {
    const DialogueLine* line = GetCurrentLine();
    if (!line) return true;
    return static_cast<size_t>(m_revealedChars) >= CountUTF8Codepoints(line->text);
}

std::string DialogueSystem::GetVisibleTextUTF8() const {
    const DialogueLine* line = GetCurrentLine();
    if (!line) return "";
    return SubstringUTF8(line->text, static_cast<size_t>(m_revealedChars));
}

bool DialogueSystem::HasActiveTimeout() const {
    return m_activeNode && m_activeNode->timeoutSec > 0.0f;
}

void DialogueSystem::ExecuteActions(const std::vector<core::EngineAction>& actions) {
    for (const auto& act : actions) {
        if (m_state) {
            if (act.type == "set_flag") m_state->SetFlag(act.target);
            else if (act.type == "clear_flag") m_state->ClearFlag(act.target);
            else if (act.type == "add_sanity") m_state->AddSanity(act.intValue);
            else if (act.type == "clear_item") m_state->ClearHeldItem();
        }
        if (m_onAction) {
            m_onAction(act);
        }
    }
}

size_t DialogueSystem::CountUTF8Codepoints(const std::string& s) const {
    size_t count = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) count++;
    }
    return count;
}

std::string DialogueSystem::SubstringUTF8(const std::string& s, size_t maxCodepoints) const {
    size_t cpCount = 0;
    size_t bytePos = 0;
    while (bytePos < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[bytePos]);
        if ((c & 0xC0) != 0x80) {
            if (cpCount == maxCodepoints) break;
            cpCount++;
        }
        bytePos++;
    }
    return s.substr(0, bytePos);
}

}
