#include "ui/EditorUI.h"
#include "ui/UnityTheme.h"
#include "rlImGui.h"
#include "imgui.h"
#include "map/MapParser.h"
#include "map/MapSerializer.h"
#include "map/BrushBuilder.h"
#include "render/TextureManager.h"
#include "core/Log.h"
#include <algorithm>
#include <cstring>

namespace knopmops::ui {

EditorUI::EditorUI() = default;
EditorUI::~EditorUI() = default;

void EditorUI::Init(map::MapDocument* doc, scene::Scene* scene, render::ViewportRenderer* renderer) {
    m_doc = doc;
    m_scene = scene;
    m_renderer = renderer;
    m_codeEditor.Init();
    m_ch4Director.Init();

    ApplyUnityDarkTheme();
    core::Log::Info("KnopMops Engine UI Initialized with Unity-like tile layout.");
}

void EditorUI::Render() {
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoNavFocus |
                                 ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("KnopMopsDockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    RenderMainMenuBar();

    ImGuiID dockspaceId = ImGui::GetID("KnopMopsDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    RenderToolbar();
    RenderHierarchy();
    RenderSceneViewport();
    RenderGameViewport();
    m_codeEditor.Render();
    RenderInspector();

    ImGui::Begin("Project & Console");
    if (ImGui::BeginTabBar("ProjectConsoleTabs")) {
        if (ImGui::BeginTabItem("Textures & Assets")) {
            RenderAssetBrowser();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Map Builder Tools")) {
            RenderMapTools();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Chapter 4 Director")) {
            m_ch4Director.RenderUI(m_scene, m_doc);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Console")) {
            RenderConsole();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    ImGui::End();
}

void EditorUI::RenderMainMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene & Map", "Ctrl+N")) ActionNewMap();
            if (ImGui::MenuItem("Open Template Motel")) ActionOpenMap("assets/maps/template_motel.map");
            if (ImGui::MenuItem("Open Map...", "Ctrl+O")) ActionOpenMap("assets/maps/template_motel.map");
            ImGui::Separator();
            if (ImGui::MenuItem("Save Map", "Ctrl+S")) ActionSaveMap();
            if (ImGui::MenuItem("Save Map As...")) ActionSaveMapAs("assets/maps/ch4_level.map");
            if (ImGui::MenuItem("Save Scene (.kmscene)")) {
                m_scene->SaveToFile("assets/data/chapter4/scene_layout.json");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build & Export Chapter 4")) {
                m_ch4Director.ExportChapter4(*m_scene, *m_doc);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) m_shouldExit = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Deselect All", "Esc")) {
                m_doc->DeselectAll();
                m_scene->Deselect();
            }
            if (ImGui::MenuItem("Duplicate Selected", "Ctrl+D")) ActionDuplicateSelected();
            if (ImGui::MenuItem("Delete Selected", "Del")) ActionDeleteSelected();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("GameObject")) {
            if (ImGui::BeginMenu("3D Object")) {
                if (ImGui::MenuItem("Cube Brush")) {
                    map::MapBrush b = map::BrushBuilder::CreateCubeAt({0, 1.0f, 0}, {2.0f, 2.0f, 2.0f}, "motel_wall");
                    b.name = "Cube_" + std::to_string(m_doc->GetWorldBrushes().size());
                    m_doc->AddBrush(b);
                }
                if (ImGui::MenuItem("Floor Plane")) {
                    map::MapBrush b = map::BrushBuilder::CreateBox({-4.0f, -0.2f, -4.0f}, {4.0f, 0.0f, 4.0f}, "motel_carpet");
                    b.name = "Floor_" + std::to_string(m_doc->GetWorldBrushes().size());
                    m_doc->AddBrush(b);
                }
                if (ImGui::MenuItem("Cylinder Column")) {
                    map::MapBrush b = map::BrushBuilder::CreateCylinder({0, 1.5f, 0}, 0.5f, 3.0f, 8, "motel_concrete");
                    b.name = "Column_" + std::to_string(m_doc->GetWorldBrushes().size());
                    m_doc->AddBrush(b);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Point Light")) {
                    auto* obj = m_scene->CreateGameObject("Point_Light");
                    obj->transform.position = Vector3{0, 2.5f, 0};
                    obj->light.enabled = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Horror Props")) {
                if (ImGui::MenuItem("Search Slot (Loot Drawer/Crate)")) {
                    auto* obj = m_scene->CreateGameObject("Search_Slot");
                    obj->transform.position = Vector3{1.0f, 0.4f, 1.0f};
                    obj->searchSlot.enabled = true;
                    obj->searchSlot.slotId = "slot_ch4_" + std::to_string(m_scene->GetGameObjects().size());
                }
                if (ImGui::MenuItem("Hiding Spot (Wardrobe)")) {
                    auto* obj = m_scene->CreateGameObject("Hiding_Wardrobe");
                    obj->transform.position = Vector3{-2.0f, 0.0f, 2.0f};
                    obj->hidingSpot.enabled = true;
                }
                if (ImGui::MenuItem("Interactive Door")) {
                    auto* obj = m_scene->CreateGameObject("Interactive_Door");
                    obj->transform.position = Vector3{0.0f, 0.0f, -4.0f};
                    obj->door.enabled = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("AI")) {
                if (ImGui::MenuItem("Prowler Monster AI Spawn")) {
                    auto* obj = m_scene->CreateGameObject("Prowler_Enemy");
                    obj->transform.position = Vector3{-3.0f, 0.0f, -3.0f};
                    obj->aiSpawn.enabled = true;
                    obj->script.enabled = true;
                    obj->script.scriptFile = "ProwlerMonsterAI.cpp";
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            if (ImGui::BeginMenu("Layout")) {
                if (ImGui::MenuItem("Default Unity Layout")) {
                    core::Log::Info("Reset to Default Unity tile layout.");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("KnopMops Engine Architecture")) {
                core::Log::Info("KnopMops Engine v1.0.0 — Unity-like C++20 Survival Horror Engine.");
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void EditorUI::RenderToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);

    auto toolButton = [this](const char* label, render::TransformMode mode) {
        bool active = m_renderer->GetTransformMode() == mode;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.88f, 1.0f));
        if (ImGui::Button(label)) m_renderer->SetTransformMode(mode);
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    };

    toolButton("Select [Q]", render::TransformMode::Select);
    toolButton("Move [W]", render::TransformMode::Translate);
    toolButton("Rotate [E]", render::TransformMode::Rotate);
    toolButton("Scale [R]", render::TransformMode::Scale);

    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(130.0f);
    const char* snapItems[] = {"Snap: Off", "Snap: 0.1m", "Snap: 0.25m (8u)", "Snap: 0.5m (16u)", "Snap: 1.0m (32u)", "Snap: 2.0m (64u)"};
    static int currentSnap = 3;
    if (ImGui::Combo("##SnapCombo", &currentSnap, snapItems, IM_ARRAYSIZE(snapItems))) {
        float snaps[] = {0.0f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
        m_renderer->SetGridSnap(snaps[currentSnap]);
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    if (m_isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("⏹ Stop Test")) {
            m_isPlaying = false;
            core::Log::Info("Stopped Play-in-Editor simulation.");
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "PLAYING (%.1fs)", m_playTime += 0.016f);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.70f, 0.30f, 1.0f));
        if (ImGui::Button("▶ Play In Editor")) {
            m_isPlaying = true;
            m_playTime = 0.0f;
            core::Log::Success("Started Play-in-Editor test mode.");
        }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    float fps = static_cast<float>(GetFPS());
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 110.0f);
    ImGui::Text("%.1f FPS (%.2fms)", fps, 1000.0f / (fps > 0 ? fps : 60.0f));

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::RenderHierarchy() {
    ImGui::Begin("Hierarchy");

    ImGui::InputTextWithHint("##HierarchySearch", "Search GameObjects & Brushes...", m_hierarchyFilter, IM_ARRAYSIZE(m_hierarchyFilter));
    ImGui::Separator();

    std::string search = m_hierarchyFilter;
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    std::string sceneHeader = "Scene: " + m_scene->GetName() + " (" + std::to_string(m_scene->GetGameObjects().size()) + " Objects)";
    if (ImGui::TreeNodeEx(sceneHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& obj : m_scene->GetGameObjects()) {
            if (!search.empty()) {
                std::string oname = obj->name;
                std::transform(oname.begin(), oname.end(), oname.begin(), ::tolower);
                if (oname.find(search) == std::string::npos) continue;
            }

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (m_scene->GetSelectedId() == static_cast<int>(obj->id)) flags |= ImGuiTreeNodeFlags_Selected;

            std::string label = obj->name;
            if (obj->camera.enabled) label = "[Cam] " + label;
            else if (obj->light.enabled) label = "[Light] " + label;
            else if (obj->searchSlot.enabled) label = "[Slot] " + label;
            else if (obj->hidingSpot.enabled) label = "[Hide] " + label;
            else if (obj->door.enabled) label = "[Door] " + label;
            else if (obj->aiSpawn.enabled) label = "[AI] " + label;

            ImGui::TreeNodeEx((void*)(intptr_t)obj->id, flags, "%s", label.c_str());

            if (ImGui::IsItemClicked()) {
                m_scene->Select(obj->id);
                m_doc->DeselectAll();
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("World Brushes (.map)", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& brushes = m_doc->GetWorldBrushes();
        for (size_t i = 0; i < brushes.size(); ++i) {
            auto& b = brushes[i];
            if (!search.empty()) {
                std::string bname = b.name;
                std::transform(bname.begin(), bname.end(), bname.begin(), ::tolower);
                if (bname.find(search) == std::string::npos) continue;
            }

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (b.selected) flags |= ImGuiTreeNodeFlags_Selected;

            std::string label = "[Brush] " + b.name;
            ImGui::TreeNodeEx((void*)(intptr_t)(10000 + i), flags, "%s", label.c_str());

            if (ImGui::IsItemClicked()) {
                m_doc->SelectBrush(i);
                m_scene->Deselect();
            }
        }
        ImGui::TreePop();
    }

    ImGui::Separator();
    if (ImGui::Button("+ GameObject", ImVec2(95, 24))) {
        m_scene->CreateGameObject("GameObject");
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Brush", ImVec2(70, 24))) {
        map::MapBrush b = map::BrushBuilder::CreateCubeAt({0, 1, 0}, {2, 2, 2}, "motel_wall");
        b.name = "Brush_" + std::to_string(m_doc->GetWorldBrushes().size());
        m_doc->AddBrush(b);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete", ImVec2(70, 24))) {
        ActionDeleteSelected();
    }

    ImGui::End();
}

void EditorUI::RenderSceneViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Scene View (3D Level Editor)", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 viewSize = ImGui::GetContentRegionAvail();
    int w = static_cast<int>(viewSize.x);
    int h = static_cast<int>(viewSize.y);

    if (w > 100 && h > 100) {
        m_renderer->Resize(w, h);
    }

    bool isHovered = ImGui::IsWindowHovered();
    bool isFocused = ImGui::IsWindowFocused();
    float dt = GetFrameTime();

    m_renderer->Update(dt, isHovered, isFocused);
    m_renderer->Render(*m_doc);

    rlImGuiImageRenderTextureFit(&m_renderer->GetRenderTexture(), true);

    if (isHovered) {
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 windowPos = ImGui::GetWindowPos();
        Vector2 mouseInViewport{mousePos.x - windowPos.x, mousePos.y - windowPos.y};

        bool isMouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        bool isMousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        m_renderer->HandleMousePicking(mouseInViewport, *m_doc, isMouseDown, isMousePressed);

        if (IsKeyPressed(KEY_W)) m_renderer->SetTransformMode(render::TransformMode::Translate);
        if (IsKeyPressed(KEY_E)) m_renderer->SetTransformMode(render::TransformMode::Rotate);
        if (IsKeyPressed(KEY_R)) m_renderer->SetTransformMode(render::TransformMode::Scale);
        if (IsKeyPressed(KEY_Q)) m_renderer->SetTransformMode(render::TransformMode::Select);
        if (IsKeyPressed(KEY_DELETE)) ActionDeleteSelected();
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) ActionDuplicateSelected();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::RenderGameViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Game View (Player POV)", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    rlImGuiImageRenderTextureFit(&m_renderer->GetRenderTexture(), true);

    ImGui::SetCursorPos(ImVec2(15, 15));
    if (m_isPlaying) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "● GAME RUNNING (Press Esc to unlock cursor)");
        ImGui::SetCursorPos(ImVec2(15, 35));
        ImGui::Text("HP: 100 | Stamina: 95%% | Flashlight: ON [F]");
    } else {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "GAME PREVIEW (Press Play on Toolbar to simulate)");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::RenderInspector() {
    ImGui::Begin("Inspector");

    auto* selectedObj = m_scene->GetSelected();
    int selBrush = m_doc->GetSelectedBrushIndex();

    if (selectedObj) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "GAMEOBJECT: %s", selectedObj->name.c_str());
        ImGui::Separator();

        char nameBuf[64];
        std::strncpy(nameBuf, selectedObj->name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            selectedObj->name = nameBuf;
            m_scene->SetModified(true);
        }
        ImGui::Checkbox("Active", &selectedObj->active);

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            float pos[3] = {selectedObj->transform.position.x, selectedObj->transform.position.y, selectedObj->transform.position.z};
            if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                selectedObj->transform.position = {pos[0], pos[1], pos[2]};
                m_scene->SetModified(true);
            }

            float rot[3] = {selectedObj->transform.rotation.x, selectedObj->transform.rotation.y, selectedObj->transform.rotation.z};
            if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
                selectedObj->transform.rotation = {rot[0], rot[1], rot[2]};
                m_scene->SetModified(true);
            }

            float sc[3] = {selectedObj->transform.scale.x, selectedObj->transform.scale.y, selectedObj->transform.scale.z};
            if (ImGui::DragFloat3("Scale", sc, 0.05f)) {
                selectedObj->transform.scale = {sc[0], sc[1], sc[2]};
                m_scene->SetModified(true);
            }
        }

        if (selectedObj->camera.enabled && ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("FOV", &selectedObj->camera.fov, 40.0f, 110.0f);
            ImGui::Checkbox("Is Main Player Camera", &selectedObj->camera.isMainPlayerCamera);
        }

        if (selectedObj->light.enabled && ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Intensity", &selectedObj->light.intensity, 50.0f, 1000.0f);
            ImGui::SliderFloat("Radius", &selectedObj->light.radius, 1.0f, 30.0f);
        }

        if (selectedObj->searchSlot.enabled && ImGui::CollapsingHeader("Search Slot Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            char slotId[64];
            std::strncpy(slotId, selectedObj->searchSlot.slotId.c_str(), sizeof(slotId));
            if (ImGui::InputText("Slot ID", slotId, sizeof(slotId))) selectedObj->searchSlot.slotId = slotId;

            char dname[64];
            std::strncpy(dname, selectedObj->searchSlot.displayName.c_str(), sizeof(dname));
            if (ImGui::InputText("Display Name", dname, sizeof(dname))) selectedObj->searchSlot.displayName = dname;

            char item[64];
            std::strncpy(item, selectedObj->searchSlot.containedItem.c_str(), sizeof(item));
            if (ImGui::InputText("Contained Item", item, sizeof(item))) selectedObj->searchSlot.containedItem = item;

            ImGui::Checkbox("Is Locked", &selectedObj->searchSlot.isLocked);
            if (selectedObj->searchSlot.isLocked) {
                char reqTool[64];
                std::strncpy(reqTool, selectedObj->searchSlot.requiredTool.c_str(), sizeof(reqTool));
                if (ImGui::InputText("Required Tool", reqTool, sizeof(reqTool))) selectedObj->searchSlot.requiredTool = reqTool;
            }
        }

        if (selectedObj->hidingSpot.enabled && ImGui::CollapsingHeader("Hiding Spot Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Spot Type: %s", selectedObj->hidingSpot.spotType.c_str());
            ImGui::SliderFloat("Exit Yaw", &selectedObj->hidingSpot.exitYaw, 0.0f, 360.0f);
        }

        if (selectedObj->door.enabled && ImGui::CollapsingHeader("Door Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Is Locked", &selectedObj->door.isLocked);
            char reqKey[64];
            std::strncpy(reqKey, selectedObj->door.requiredKey.c_str(), sizeof(reqKey));
            if (ImGui::InputText("Required Key", reqKey, sizeof(reqKey))) selectedObj->door.requiredKey = reqKey;
        }

        if (selectedObj->aiSpawn.enabled && ImGui::CollapsingHeader("AI Monster Spawn Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Monster: %s", selectedObj->aiSpawn.monsterType.c_str());
            ImGui::SliderFloat("Hearing Radius", &selectedObj->aiSpawn.hearingRadius, 4.0f, 25.0f);
            ImGui::SliderFloat("Vision Range", &selectedObj->aiSpawn.visionDistance, 5.0f, 30.0f);
        }

        if (selectedObj->script.enabled && ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Attached Script: %s", selectedObj->script.scriptFile.c_str());
            if (ImGui::Button("Edit Script in Code Editor")) {
                m_codeEditor.OpenFile("assets/scripts/" + selectedObj->script.scriptFile);
            }
        }

        ImGui::Separator();
        if (ImGui::BeginCombo("##AddComponentCombo", "Add Component...")) {
            if (ImGui::Selectable("Camera")) selectedObj->camera.enabled = true;
            if (ImGui::Selectable("Light")) selectedObj->light.enabled = true;
            if (ImGui::Selectable("Search Slot")) selectedObj->searchSlot.enabled = true;
            if (ImGui::Selectable("Hiding Spot")) selectedObj->hidingSpot.enabled = true;
            if (ImGui::Selectable("Door")) selectedObj->door.enabled = true;
            if (ImGui::Selectable("Script")) selectedObj->script.enabled = true;
            ImGui::EndCombo();
        }
    } else if (selBrush >= 0 && selBrush < static_cast<int>(m_doc->GetWorldBrushes().size())) {
        auto& brush = m_doc->GetWorldBrushes()[selBrush];
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "BRUSH: %s", brush.name.c_str());
        ImGui::Separator();

        char nameBuf[64];
        std::strncpy(nameBuf, brush.name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) brush.name = nameBuf;

        Vector3 size{
            brush.bounds.max.x - brush.bounds.min.x,
            brush.bounds.max.y - brush.bounds.min.y,
            brush.bounds.max.z - brush.bounds.min.z
        };
        ImGui::Text("Dimensions: %.2fm x %.2fm x %.2fm", size.x, size.y, size.z);

        if (ImGui::Button("Hollow Out Room", ImVec2(-FLT_MIN, 26))) {
            auto hollowed = map::BrushBuilder::HollowBrush(brush, 0.25f, "motel_wallpaper");
            m_doc->RemoveBrush(selBrush);
            for (const auto& b : hollowed) m_doc->AddBrush(b);
        }
    } else {
        ImGui::TextDisabled("Select an object or brush to inspect properties.");
    }

    ImGui::End();
}

void EditorUI::RenderAssetBrowser() {
    auto& texMgr = render::TextureManager::Instance();
    const auto& items = texMgr.GetAllTextures();

    ImGui::InputTextWithHint("##TextureSearch", "Filter textures...", m_textureFilter, IM_ARRAYSIZE(m_textureFilter));
    ImGui::Separator();

    std::string search = m_textureFilter;
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    float thumbSize = 64.0f;
    float padding = 8.0f;
    float cellWidth = thumbSize + padding * 2.0f;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, static_cast<int>(panelWidth / cellWidth));

    if (ImGui::BeginTable("TextureGrid", columns)) {
        for (const auto& item : items) {
            if (!search.empty()) {
                std::string tname = item.name;
                std::transform(tname.begin(), tname.end(), tname.begin(), ::tolower);
                if (tname.find(search) == std::string::npos) continue;
            }

            ImGui::TableNextColumn();
            ImGui::PushID(item.name.c_str());

            if (rlImGuiImageButtonSize(item.name.c_str(), &item.texture, Vector2{thumbSize, thumbSize})) {
                int selB = m_doc->GetSelectedBrushIndex();
                if (selB >= 0 && selB < static_cast<int>(m_doc->GetWorldBrushes().size())) {
                    for (auto& face : m_doc->GetWorldBrushes()[selB].faces) {
                        face.texture = item.name;
                    }
                    m_doc->SetModified(true);
                    core::Log::Info("Applied texture '" + item.name + "' to selected brush.");
                }
            }

            ImGui::TextWrapped("%s", item.name.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void EditorUI::RenderMapTools() {
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "QUICK ROOM GENERATOR");
    ImGui::Separator();

    ImGui::DragFloat("Width (X)", &m_roomWidth, 0.25f, 2.0f, 64.0f, "%.2f m");
    ImGui::DragFloat("Length (Z)", &m_roomLength, 0.25f, 2.0f, 64.0f, "%.2f m");
    ImGui::DragFloat("Height (Y)", &m_roomHeight, 0.1f, 1.5f, 16.0f, "%.2f m");
    ImGui::DragFloat("Wall Thickness", &m_roomWallThick, 0.05f, 0.1f, 1.0f, "%.2f m");

    ImGui::InputText("Wall Texture", m_roomWallTex, sizeof(m_roomWallTex));
    ImGui::InputText("Floor Texture", m_roomFloorTex, sizeof(m_roomFloorTex));
    ImGui::InputText("Ceiling Texture", m_roomCeilingTex, sizeof(m_roomCeilingTex));

    if (ImGui::Button("Generate Room Brushes", ImVec2(-FLT_MIN, 30))) {
        auto brushes = map::BrushBuilder::CreateHollowRoom(
            Vector3{0.0f, m_roomHeight * 0.5f, 0.0f},
            Vector3{m_roomWidth, m_roomHeight, m_roomLength},
            m_roomWallThick,
            m_roomWallTex,
            m_roomFloorTex,
            m_roomCeilingTex
        );

        for (const auto& b : brushes) m_doc->AddBrush(b);
        core::Log::Success("Generated room with 6 brushes.");
    }
}

void EditorUI::RenderConsole() {
    const auto& entries = core::Log::Instance().GetEntries();

    if (ImGui::Button("Clear Log")) core::Log::Instance().Clear();
    ImGui::SameLine();
    ImGui::Text("Total Logs: %zu", entries.size());

    ImGui::Separator();
    if (ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        for (const auto& entry : entries) {
            ImVec4 col{0.8f, 0.8f, 0.8f, 1.0f};
            const char* prefix = "[INFO]";
            if (entry.level == core::LogLevel::Warning) { col = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); prefix = "[WARN]"; }
            else if (entry.level == core::LogLevel::Error) { col = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); prefix = "[ERROR]"; }
            else if (entry.level == core::LogLevel::Success) { col = ImVec4(0.3f, 1.0f, 0.4f, 1.0f); prefix = "[SUCCESS]"; }

            ImGui::TextColored(col, "%s %s %s", entry.timeString.c_str(), prefix, entry.message.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

void EditorUI::ActionNewMap() {
    m_doc->CreateDefaultScene();
    m_scene->SetupDefaultChapter4Scene();
    core::Log::Info("Created new Scene and Map.");
}

void EditorUI::ActionOpenMap(const std::string& path) {
    if (map::MapParser::LoadFromFile(path, *m_doc)) {
        m_renderer->ResetCamera();
    }
}

void EditorUI::ActionSaveMap() {
    std::string path = m_doc->GetFilePath();
    if (path.empty()) path = "assets/maps/ch4_level.map";
    map::MapSerializer::SaveToFile(path, *m_doc);
    m_doc->SetFilePath(path);
    m_doc->SetModified(false);
}

void EditorUI::ActionSaveMapAs(const std::string& path) {
    map::MapSerializer::SaveToFile(path, *m_doc);
    m_doc->SetFilePath(path);
    m_doc->SetModified(false);
}

void EditorUI::ActionDuplicateSelected() {
    auto* obj = m_scene->GetSelected();
    if (obj) {
        auto* dup = m_scene->CreateGameObject(obj->name + "_Copy");
        dup->transform = obj->transform;
        dup->transform.position = core::Vec3Add(dup->transform.position, {1.0f, 0.0f, 1.0f});
        dup->camera = obj->camera;
        dup->light = obj->light;
        dup->searchSlot = obj->searchSlot;
        dup->hidingSpot = obj->hidingSpot;
        dup->door = obj->door;
        dup->aiSpawn = obj->aiSpawn;
        dup->script = obj->script;
        m_scene->Select(dup->id);
        core::Log::Info("Duplicated GameObject: " + dup->name);
        return;
    }

    int selB = m_doc->GetSelectedBrushIndex();
    if (selB >= 0 && selB < static_cast<int>(m_doc->GetWorldBrushes().size())) {
        map::MapBrush dup = m_doc->GetWorldBrushes()[selB];
        dup.name += "_copy";
        Vector3 qOff = core::WorldToQuake(Vector3{1.0f, 0.0f, 1.0f});
        for (auto& f : dup.faces) {
            for (int k = 0; k < 3; ++k) f.planePoints[k] = core::Vec3Add(f.planePoints[k], qOff);
        }
        m_doc->AddBrush(dup);
        m_doc->SelectBrush(m_doc->GetWorldBrushes().size() - 1);
        core::Log::Info("Duplicated Brush.");
    }
}

void EditorUI::ActionDeleteSelected() {
    auto* obj = m_scene->GetSelected();
    if (obj) {
        m_scene->DestroyGameObject(obj->id);
        core::Log::Info("Deleted GameObject.");
        return;
    }

    int selB = m_doc->GetSelectedBrushIndex();
    if (selB >= 0) {
        m_doc->RemoveBrush(selB);
        core::Log::Info("Deleted Brush.");
    }
}

}
