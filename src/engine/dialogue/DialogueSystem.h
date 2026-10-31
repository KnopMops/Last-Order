#pragma once
#include "engine/core/StateStore.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::dialogue {

struct DialogueLine {
    std::string speaker;
    std::string text;
};

struct DialogueChoice {
    std::string text;
    std::string nextNode;
    std::string requireFlag;
    std::string forbidFlag;
    std::string requireHeldItem;
    std::vector<core::EngineAction> actions;
};

struct DialogueNode {
    std::string id;
    std::vector<DialogueLine> lines;
    std::vector<DialogueChoice> choices;
    std::vector<core::EngineAction> onEnd;
    float timeoutSec = 0.0f;
    std::string timeoutNextNode;
};

class DialogueSystem {
public:
    using ActionCallback = std::function<void(const core::EngineAction&)>;

    void Init(core::StateStore* stateStore, ActionCallback onAction);
    bool LoadFromJsonFile(const std::string& jsonFilePath);
    void RegisterNode(const DialogueNode& node);

    bool StartDialogue(const std::string& nodeId);
    void CloseDialogue();

    void Update(float dt);

    void OnAdvanceOrConfirm();
    void OnSelectDelta(int delta);
    void OnSelectIndex(int index);

    bool IsActive() const { return m_activeNode != nullptr; }
    bool IsWaitingForChoice() const;

    const DialogueNode* GetActiveNode() const { return m_activeNode; }
    const DialogueLine* GetCurrentLine() const;
    std::string GetVisibleTextUTF8() const;
    bool IsLineFullyRevealed() const;

    const std::vector<DialogueChoice>& GetAvailableChoices() const { return m_filteredChoices; }
    int GetSelectedChoiceIndex() const { return m_selectedChoice; }

    bool HasActiveTimeout() const;
    float GetTimeoutRemaining() const { return m_timeoutRemaining; }
    float GetTimeoutTotal() const { return m_activeNode ? m_activeNode->timeoutSec : 0.0f; }

private:
    void RebuildFilteredChoices();
    void ExecuteActions(const std::vector<core::EngineAction>& actions);
    size_t CountUTF8Codepoints(const std::string& s) const;
    std::string SubstringUTF8(const std::string& s, size_t maxCodepoints) const;

    core::StateStore* m_state = nullptr;
    ActionCallback m_onAction;
    std::unordered_map<std::string, DialogueNode> m_nodes;

    const DialogueNode* m_activeNode = nullptr;
    size_t m_lineIndex = 0;
    float m_revealedChars = 0.0f;
    float m_charsPerSec = 45.0f;

    std::vector<DialogueChoice> m_filteredChoices;
    int m_selectedChoice = 0;
    float m_timeoutRemaining = 0.0f;
};

}
