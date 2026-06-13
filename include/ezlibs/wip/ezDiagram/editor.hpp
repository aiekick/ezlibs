#pragma once

/*
MIT License

Copyright (c) 2014-2024 Stephane Cuillerdier (aka aiekick)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// ezDiagram is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

#include <functional>

#include <imguipack.h>

#include "interfaces/icanvas.h"
#include "interfaces/isolver.h"
#include "node.hpp"
#include "link.hpp"

class Editor {
public:
    struct Gizmos {
        bool drawCentroid{true};
        bool drawNodes{true};
        bool drawLinks{true};
        bool drawRelations{false};
        bool useOrtho{true};
        bool useSpline{false};
        bool drawSnapGrid{false};
    };

    struct Datas {
        bool enableEnergyThreshold{false};
        float energyThreshold{0.35f};
        int32_t countStepPerFrame{50};
        float relationLineThickness{2.0f};
        float relationCornerRadius{8.0f};
    };

private:
    ICanvas& mr_canvas;
    ISolver& mr_solver;

    // node dragging
    int32_t m_draggingNode{-1};
    ImVec2 m_dragginfOffset;

    bool m_realTime{false};
    Gizmos m_gizmos;
    Datas m_datas;

    const float m_control_pane_width = 450.0f;

    // Default datas
    const Datas m_const_editor_datas;           // contain the default values
    const ISolver::Datas m_const_solver_datas;  // def value for solver datas

    std::vector<std::function<void()>> m_operations;
    size_t m_opCursor{0};

public:
    Editor(ISolver& arSolver, ICanvas& arCanvas) : mr_solver(arSolver), mr_canvas(arCanvas) {
    }

    // settings accessors (used for persistence)
    Datas& rDatas() {
        return m_datas;
    }
    const Datas& getDatas() const {
        return m_datas;
    }
    Gizmos& rGizmos() {
        return m_gizmos;
    }
    const Gizmos& getGizmos() const {
        return m_gizmos;
    }
    bool isRealTime() const {
        return m_realTime;
    }
    void setRealTime(bool aFlag) {
        m_realTime = aFlag;
    }
    void clear() {
        m_operations.clear();
    }
    int32_t addNode(const Node& aNode) {
        int32_t id = static_cast<int32_t>(mr_solver.getDatas().containers.nodes.size());
        mr_solver.rDatas().containers.nodes.tryAdd(aNode.getDatas().name, aNode);
        return id;
    }

    int32_t addLink(const Link& aLink) {
        int32_t id = static_cast<int32_t>(mr_solver.getDatas().containers.links.size());
        mr_solver.rDatas().containers.links.push_back(aLink);
        return id;
    }

    void recolorize() {
        // Colorize nodes
        int32_t idx = 0;
        const auto count_nodes = static_cast<int32_t>(mr_solver.getDatas().containers.nodes.size());
        for (auto& node : mr_solver.rDatas().containers.nodes) {
            auto col = m_getRainBowColor(idx++, count_nodes);
            col.x *= 0.5f;
            col.y *= 0.5f;
            col.z *= 0.5f;
            node.rDatas().color = ImGui::GetColorU32(col);
        }
        // Colorize links
        idx = 0;
        const auto count_links = static_cast<int32_t>(mr_solver.getDatas().containers.links.size());
        for (auto& link : mr_solver.rDatas().containers.links) {
            const auto col = m_getRainBowColor(idx++, static_cast<int32_t>(mr_solver.getDatas().containers.links.size()));
            link.rDatas().color = ImGui::GetColorU32(col);
        }
    }

    bool init() {
        recolorize();
        resetState();
        return true;
    }

    void simulate() {
        if (m_realTime) {
            for (size_t idx = 0; idx < m_datas.countStepPerFrame; ++idx) {
                mr_solver.step(ImGui::GetIO().DeltaTime);
                if (mr_solver.getDatas().system.energy < m_datas.energyThreshold) {
                    break;
                }
            }
            mr_solver.updateLinks();
        }
    }

    void resetState() {
        mr_solver.init();
        const auto& dispSize = ImGui::GetIO().DisplaySize;
        mr_canvas.init(ImVec2(dispSize.x - m_control_pane_width, dispSize.y));
        mr_solver.updateLinks();
    }

    void clearGraph() {
        mr_solver.rDatas().containers.nodes.clear();
        mr_solver.rDatas().containers.links.clear();
        m_opCursor = 0;
        m_draggingNode = -1;
        stopSimulation();
    }

    void addOperation(std::function<void()> aOp) {
        m_operations.push_back(std::move(aOp));

    }

    void startSimulation() {
        m_realTime = true;
    }

    void switchSimulation() {
        m_realTime = !m_realTime;
    }

    void stopSimulation() {
        m_realTime = false;
    }

    void drawGraph() {
        if (ImGui::BeginMenuBar()) {
            const float canvas_scale = mr_canvas.getDatas().scale;
            const auto count_nodes = static_cast<int32_t>(mr_solver.getDatas().containers.nodes.size());
            const auto count_links = static_cast<int32_t>(mr_solver.getDatas().containers.links.size());
            ImGui::TextColored({.8f, .8f, .8f, 1}, "Zoom: %.2f | Nodes: %d | Links: %d | Energy: %.2f", canvas_scale, count_nodes, count_links, mr_solver.getDatas().system.energy);
            ImGui::Spacing();
            if (m_realTime) {
                ImGui::TextColored({.4f, 1.0f, .4f, 1}, "[LIVE]");
                ImGui::Spacing();
            }
            ImGui::TextColored({.6f, .6f, .6f, 1}, "LMB: drag | RMB: pan | Wheel: zoom | DblClick: layout | Space: live");
            ImGui::EndMenuBar();
        }

        const auto mouse_world = mr_canvas.localToWorld(ImGui::GetIO().MousePos);
        if (mr_canvas.begin(
                "##cavnas",
                ImGuiWindowFlags_NoScrollbar              //
                    | ImGuiWindowFlags_NoScrollWithMouse  //
                    | ImGuiWindowFlags_NoTitleBar         //
                    | ImGuiWindowFlags_NoResize           //
                )) {
            // Draw centroid
            if (m_gizmos.drawCentroid) {
                auto* pDrawList = ImGui::GetWindowDrawList();
                const auto center = mr_canvas.worldToLocal(mr_solver.getDatas().computed.centroid);
                pDrawList->AddCircle(center, 5.0f * mr_canvas.getDatas().scale, IM_COL32(255, 255, 0, 200), 6);
                pDrawList->AddCircle(center, 10.0f * mr_canvas.getDatas().scale, IM_COL32(255, 255, 0, 200), 6);
            }

            // Draw links
            if (m_gizmos.drawLinks) {
                for (const auto& link : mr_solver.getDatas().containers.links) {
                    link.draw(mr_canvas);
                }
            }

            if (m_gizmos.drawRelations) {
                m_drawRelations();
            }

            if (m_gizmos.drawSnapGrid) {
                m_drawSnapGrid();
            }

            // Draw nodes
            if (m_gizmos.drawNodes) {
                int32_t node_idx{};
                for (const auto& node : mr_solver.getDatas().containers.nodes) {
                    const auto& datas = node.getDatas();
                    node.draw(mr_canvas);
                    // start node dragging
                    if (!datas.locked && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        // Check if mouse is inside the node bounds
                        if (mouse_world.x >= datas.pos.x && mouse_world.x <= datas.pos.x + datas.size.x && mouse_world.y >= datas.pos.y && mouse_world.y <= datas.pos.y + datas.size.y) {
                            m_draggingNode = node_idx;
                            m_dragginfOffset = ImVec2(mouse_world.x - datas.pos.x, mouse_world.y - datas.pos.y);
                        }
                    }
                    node_idx++;
                }
            }

            // node dragging
            if (m_draggingNode >= 0) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    auto& datas = mr_solver.rDatas().containers.nodes.at(m_draggingNode).rDatas();
                    datas.pos.x = mouse_world.x - m_dragginfOffset.x;
                    datas.pos.y = mouse_world.y - m_dragginfOffset.y;
                    datas.locked = true;  // lock
                    mr_solver.updateLinks();
                }
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    auto& datas = mr_solver.rDatas().containers.nodes.at(m_draggingNode).rDatas();
                    datas.locked = false;  // release
                    m_draggingNode = -1;
                }
            }

            // Double-click: init layout
            if (ImGui::IsWindowHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && (m_draggingNode < 0)) {
                resetState();
            }

            // toggle simulation
            if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
                switchSimulation();
            }
        }
        mr_canvas.end();
    }

    void drawControl() {
        bool change = false;
        if (ImGui::BeginChild(ImGui::GetID("##Control"), ImVec2(0, 0), 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
            const float wcol{160.0f};
            const float w{ImGui::GetContentRegionAvail().x - 60.0f};
            auto& solver_datas = mr_solver.rDatas();
            auto& system_datas = solver_datas.system;
            const auto& system_defs = m_const_solver_datas.system;
            if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
                m_displayAlignedWidget(w, "run", wcol, [this]() { ImGui::Checkbox("##run", &m_realTime); });
                m_displayAlignedWidget(w, "energy threshold", wcol, [this, &change]() { change |= ImGui::Checkbox("##enable energy threshold", &m_datas.enableEnergyThreshold); });
                if (m_datas.enableEnergyThreshold) {
                    const char* help1 = "only one steps is done if energy is below energy threshold";
                    change |= m_inputFloat(w, wcol, "energy threshold", &m_datas.energyThreshold, 0.01f, 0.1f, 0.0f, 1.0f, m_const_editor_datas.energyThreshold, help1);
                }
                change |= m_inputInt(w, wcol, "steps", &m_datas.countStepPerFrame, 1, 500, 1, 1000, m_const_editor_datas.countStepPerFrame);
                ImGui::Separator();
                m_displayAlignedWidget(w, "repulse nodes", wcol, [&system_datas, &change]() { change |= ImGui::Checkbox("##en-repulse", &system_datas.enableRepulseNodes); });
                m_displayAlignedWidget(w, "attract links", wcol, [&system_datas, &change]() { change |= ImGui::Checkbox("##en-attract", &system_datas.enableAttractLinks); });
                m_displayAlignedWidget(w, "repulse from links", wcol, [&system_datas, &change]() { change |= ImGui::Checkbox("##en-repulse-links", &system_datas.enableRepulseNodesFromLinks); });
                m_displayAlignedWidget(w, "snap to grid", wcol, [&system_datas, &change]() { change |= ImGui::Checkbox("##en-snap", &system_datas.enableSnapToGrid); });
                m_displayAlignedWidget(w, "centroid gravity", wcol, [&system_datas, &change]() { change |= ImGui::Checkbox("##en-gravity", &system_datas.enableCentroidGravity); });
            }
            if (ImGui::CollapsingHeader("Step Builder", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Step %zu / %zu", m_opCursor, m_operations.size());
                bool canNext = m_opCursor < m_operations.size();
                if (!canNext) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::ContrastedButton("Next", nullptr, nullptr, 80.0f)) {
                    m_operations[m_opCursor++]();
                    recolorize();
                    startSimulation();
                }
                if (!canNext) {
                    ImGui::EndDisabled();
                }
                ImGui::SameLine();
                if (!canNext) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::ContrastedButton("All", nullptr, nullptr, 80.0f)) {
                    while (m_opCursor < m_operations.size()) {
                        m_operations[m_opCursor++]();
                    }
                    recolorize();
                    startSimulation();
                }
                if (!canNext) {
                    ImGui::EndDisabled();
                }
                ImGui::SameLine();
                if (ImGui::ContrastedButton("Clear", nullptr, nullptr, 80.0f)) {
                    clearGraph();
                }
            }
            if (ImGui::CollapsingHeader("System", ImGuiTreeNodeFlags_DefaultOpen)) {
                m_displayAlignedWidget(w, "energy", wcol, [&system_datas]() { ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%.2f", system_datas.energy); });
                change |= m_inputFloat(w, wcol, "damping", &system_datas.damping, 0.005f, 0.01f, 0.0f, 1.0f, system_defs.damping);
                change |= m_inputFloat(w, wcol, "max force", &system_datas.maxForce, 1.0f, 10.0f, 0.0f, 1000.0f, system_defs.maxForce);
                change |= m_inputFloat(w, wcol, "node gap", &system_datas.nodeGap, 1.0f, 10.0f, 0.0f, 1000.0f, system_defs.nodeGap);
                change |= m_inputFloat(w, wcol, "node repulsion", &system_datas.nodeRepulsion, 10.0f, 100.0f, 0.0f, 10000.0f, system_defs.nodeRepulsion);
                change |= m_inputFloat(w, wcol, "link attraction", &system_datas.linkAttraction, 0.001f, 0.1f, 0.0f, 1.0f, system_defs.linkAttraction);
                change |= m_inputFloat(w, wcol, "centroid gravity", &system_datas.gravity, 0.0001f, 0.001f, 0.0f, 0.2f, system_defs.gravity);
                const char* help7 = "gap is bigger regarding node slot cout. (more slots => more gap)";
                change |= m_inputFloat(w, wcol, "gap (slot count)", &system_datas.slotGapMultiplier, 0.001f, 0.01f, -0.5f, 1.0f, system_defs.slotGapMultiplier, help7);
                change |= m_inputFloat(w, wcol, "node to link repulse", &system_datas.nodeToLinkRepulsion, 0.1f, 1.0f, 0.0f, 100.0f, system_defs.nodeToLinkRepulsion);
                change |= m_inputFloat(w, wcol, "anchor strength", &system_datas.anchorStrength, 0.05f, 0.1f, 0.0f, 1.0f, system_defs.anchorStrength);
                m_displayAlignedWidget(w, "slots on sides", wcol, [&system_datas, &change]() { change |= ImGui::Checkbox("##slots on sides", &system_datas.sideSlots); });
                const char* help10 = "Espacement de la grille magn�tique";
                change |= m_inputFloat(w, wcol, "grid spacing", &system_datas.snapGridSpacing, 5.0f, 50.0f, 10.0f, 1000.0f, system_defs.snapGridSpacing, help10);
                const char* help11 = "Force d'attraction vers la grille";
                change |= m_inputFloat(w, wcol, "grid strength", &system_datas.snapGridStrength, 0.01f, 0.1f, 0.0f, 5.0f, system_defs.snapGridStrength, help11);
            }
            if (ImGui::CollapsingHeader("Gizmos", ImGuiTreeNodeFlags_DefaultOpen)) {
                m_displayAlignedWidget(w, "draw centroid", wcol, [this]() { ImGui::Checkbox("##draw-centroid", &m_gizmos.drawCentroid); });
                m_displayAlignedWidget(w, "draw nodes", wcol, [this]() { ImGui::Checkbox("##draw-nodes", &m_gizmos.drawNodes); });
                m_displayAlignedWidget(w, "draw links", wcol, [this]() { ImGui::Checkbox("##draw-links", &m_gizmos.drawLinks); });
                m_displayAlignedWidget(w, "draw snap grid", wcol, [this]() { ImGui::Checkbox("##draw-snap-grid", &m_gizmos.drawSnapGrid); });
                m_displayAlignedWidget(w, "draw relations (virtual)", wcol, [this]() { ImGui::Checkbox("##draw-relations", &m_gizmos.drawRelations); });
                if (m_gizmos.drawRelations) {
                    m_displayAlignedWidget(w, "use spline", wcol, [this]() { ImGui::Checkbox("##use-spline", &m_gizmos.useSpline); });
                    m_displayAlignedWidget(w, "use ortho", wcol, [this]() { ImGui::Checkbox("##use-ortho", &m_gizmos.useOrtho); });
                    const char* help_thickness = "Line thickness for relation drawing";
                    change |= m_inputFloat(w, wcol, "relation thickness", &m_datas.relationLineThickness, 0.1f, 1.0f, 0.5f, 10.0f, m_const_editor_datas.relationLineThickness, help_thickness);
                    const char* help_corner = "Corner radius for orthogonal relations";
                    change |= m_inputFloat(w, wcol, "corner radius", &m_datas.relationCornerRadius, 0.5f, 2.0f, 0.0f, 20.0f, m_const_editor_datas.relationCornerRadius, help_corner);
                }
            }
            if (change) {
                // startSimulation();
            }
        }
        ImGui::EndChild();
    }


private:
    ImVec4 m_getRainBowColor(float ratio) {
        float r = ratio * 6.3f;
        return ImVec4(std::cos(r) * 0.5f + 0.5f, std::cos(23.0f + r) * 0.5f + 0.5f, std::cos(21.0f + r) * 0.5f + 0.5f, 1.0f);
    }

    ImVec4 m_getRainBowColor(int32_t idx, int32_t count) {
        return m_getRainBowColor((float)(idx + 1) / (float)count);
    }

    void m_displayAlignedWidget(const float& vWidth, const std::string& vLabel, const float& vOffsetFromStart, std::function<void()> vWidget) {
        float px = ImGui::GetCursorPosX();
        ImGui::Text("%s", vLabel.c_str());
        ImGui::SameLine(vOffsetFromStart);
        const float w = vWidth - (ImGui::GetCursorPosX() - px);
        ImGui::PushItemWidth(w);
        if (vWidget != nullptr) {
            vWidget();
        }
        ImGui::PopItemWidth();
    }

    bool m_inputFloat(float w, float first_col, const char* name, float* v, float step, float fast_step, float min, float max, float def, const char* aHelp = nullptr) {
        bool change{false};
        m_displayAlignedWidget(w, name, first_col, [=, &change]() {
            ImGui::PushID(v);
            change |= ImGui::InputFloat("##InputFloat", v, step, fast_step);
            *v = ImClamp(*v, min, max);
            ImGui::SameLine();
            if (ImGui::ContrastedSmallButton("R")) {
                *v = def;
                change = true;
            }
            if (aHelp != nullptr) {
                ImGui::SameLine();
                ImGui::ContrastedSmallButton("?");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(aHelp);
                }
            }
            ImGui::PopID();
        });
        return change;
    }

    bool m_inputFloat(float w, float first_col, const char* name, float* v, float step, float fast_step, float def, const char* aHelp = nullptr) {
        return m_inputFloat(w, first_col, name, v, step, fast_step, std::numeric_limits<float>::min(), std::numeric_limits<float>::max(), def, aHelp);
    }

    bool m_inputInt(float w, float first_col, const char* name, int32_t* v, int32_t step, int32_t fast_step, int32_t min, int32_t max, int32_t def, const char* aHelp = nullptr) {
        bool change{false};
        m_displayAlignedWidget(w, name, first_col, [=, &change]() {
            ImGui::PushID(v);
            change |= ImGui::InputInt("##InputInt", v, step, fast_step);
            *v = ImClamp(*v, min, max);
            ImGui::SameLine();
            if (ImGui::ContrastedSmallButton("R")) {
                *v = def;
                change = true;
            }
            if (aHelp != nullptr) {
                ImGui::SameLine();
                ImGui::ContrastedSmallButton("?");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(aHelp);
                }
            }
            ImGui::PopID();
        });
        return change;
    }

    bool m_inputInt(float w, float first_col, const char* name, int32_t* v, int32_t step, int32_t fast_step, int32_t def, const char* aHelp = nullptr) {
        return m_inputInt(w, first_col, name, v, step, fast_step, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), def, aHelp);
    }

    // on va dessiner des relations ER, donc plus des lignes
    // mais des relation :
    // - orthogonale pour commencer donc avec des points milieux
    // - spline dans un second temps en s'appuyant sur les liens orthognaux si les liens orthi rendent bien
    // dans cette meme fonction on va placer les debuts et fins des relations sur les bords des nodes (bords verticaux seulement)
    void m_drawRelations() {
        auto* pDrawList = ImGui::GetWindowDrawList();
        const float canvas_scale = mr_canvas.getDatas().scale;
        const float line_thickness = m_datas.relationLineThickness * canvas_scale;
        const float corner_radius = m_datas.relationCornerRadius * canvas_scale;

        const auto& nodes = mr_solver.getDatas().containers.nodes;
        for (const auto& link : mr_solver.getDatas().containers.links) {
            const auto& ldatas = link.getDatas();

            // Get source and destination nodes
            const auto& srcNode = nodes[ldatas.srcNodeID].getDatas();
            const auto& dstNode = nodes[ldatas.dstNodeID].getDatas();

            // Calculate slot positions
            const ImVec2 srcSlotPos = {srcNode.pos.x + srcNode.size.x * 0.5f, srcNode.pos.y + srcNode.slots_y[ldatas.srcNodeSlotID]};
            const ImVec2 dstSlotPos = {dstNode.pos.x + dstNode.size.x * 0.5f, dstNode.pos.y + dstNode.slots_y[ldatas.dstNodeSlotID]};

            // Determine which vertical edge to use for source node
            bool srcExitRight = dstSlotPos.x > srcSlotPos.x;
            ImVec2 srcPoint = {srcExitRight ? (srcNode.pos.x + srcNode.size.x) : srcNode.pos.x, srcSlotPos.y};

            // Determine which vertical edge to use for destination node
            bool dstEnterLeft = srcSlotPos.x < dstSlotPos.x;
            ImVec2 dstPoint = {dstEnterLeft ? dstNode.pos.x : (dstNode.pos.x + dstNode.size.x), dstSlotPos.y};

            // Calculate orthogonal routing with midpoints
            float horizontalGap = 20.0f;  // Gap from node edge
            ImVec2 p0 = srcPoint;
            ImVec2 p1 = {p0.x + (srcExitRight ? horizontalGap : -horizontalGap), p0.y};

            // Calculate middle X position
            float midX = (p1.x + dstPoint.x) * 0.5f;

            // Create orthogonal path with 4 segments
            ImVec2 p2 = {midX, p1.y};
            ImVec2 p3 = {midX, dstPoint.y};
            ImVec2 p4 = {dstPoint.x + (dstEnterLeft ? -horizontalGap : horizontalGap), dstPoint.y};
            ImVec2 p5 = dstPoint;

            if (m_gizmos.useSpline) {
                m_drawSplinePath(pDrawList, {p0, p1, p2, p3, p4, p5}, ldatas.color, line_thickness);
            }

            if (m_gizmos.useOrtho) {
                m_drawOrthogonalPath(pDrawList, {p0, p1, p2, p3, p4, p5}, ldatas.color, line_thickness, corner_radius);
            }

            // Draw arrow at destination
            if (m_gizmos.useSpline || m_gizmos.useOrtho) {
                m_drawArrow(pDrawList, p4, p5, ldatas.color, canvas_scale);
            }
        }
    }

    // Helper function to draw orthogonal path with rounded corners
    void m_drawOrthogonalPath(ImDrawList* pDrawList, const std::vector<ImVec2>& points, ImU32 color, float thickness, float cornerRadius) {
        if (points.size() < 2)
            return;

        for (size_t i = 0; i + 1 < points.size(); ++i) {
            ImVec2 p0 = mr_canvas.worldToLocal(points[i]);
            ImVec2 p1 = mr_canvas.worldToLocal(points[i + 1]);

            // For straight segments, just draw a line
            if (i == 0 || i + 2 >= points.size()) {
                pDrawList->AddLine(p0, p1, color, thickness);
            } else {
                // Draw line with potential corner handling
                ImVec2 prevDir = points[i] - points[i - 1];
                ImVec2 nextDir = points[i + 1] - points[i];

                // Normalize directions
                float prevLen = ImSqrt(ImLengthSqr(prevDir));
                float nextLen = ImSqrt(ImLengthSqr(nextDir));

                if (prevLen > 0.1f)
                    prevDir = prevDir * (1.0f / prevLen);
                if (nextLen > 0.1f)
                    nextDir = nextDir * (1.0f / nextLen);

                // Check if there's a corner (directions are different)
                float dot = prevDir.x * nextDir.x + prevDir.y * nextDir.y;
                if (ImAbs(dot) < 0.99f) {
                    // This is a corner, draw line
                    pDrawList->AddLine(p0, p1, color, thickness);
                } else {
                    pDrawList->AddLine(p0, p1, color, thickness);
                }
            }
        }
    }

    // Helper function to draw spline path using quadratic Bezier curves
    void m_drawSplinePath(ImDrawList* pDrawList, const std::vector<ImVec2>& points, ImU32 color, float thickness) {
        if (points.size() < 2)
            return;

        if (points.size() == 2) {
            // Just draw a line for 2 points
            pDrawList->AddLine(mr_canvas.worldToLocal(points[0]), mr_canvas.worldToLocal(points[1]), color, thickness);
            return;
        }

        // Convert orthogonal path to smooth spline using quadratic Bezier curves
        // For each pair of segments, we create a smooth curve
        // We'll use every other point as a control point for quadratic Bezier
        for (size_t i = 0; i + 2 < points.size(); i += 2) {
            ImVec2 p0 = mr_canvas.worldToLocal(points[i]);
            ImVec2 p1 = mr_canvas.worldToLocal(points[i + 1]);
            ImVec2 p2 = mr_canvas.worldToLocal(points[i + 2]);

            // Use the middle point as control point for quadratic Bezier
            pDrawList->AddBezierQuadratic(p0, p1, p2, color, thickness);
        }

        // Handle remaining segment if we have an odd number of segments
        size_t remaining = (points.size() - 1) % 2;
        if (remaining != 0) {
            size_t i = points.size() - 2;
            ImVec2 p0 = mr_canvas.worldToLocal(points[i]);
            ImVec2 p1 = mr_canvas.worldToLocal(points[i + 1]);
            pDrawList->AddLine(p0, p1, color, thickness);
        }
    }

    // Helper function to draw arrow
    void m_drawArrow(ImDrawList* pDrawList, const ImVec2& from, const ImVec2& to, ImU32 color, float scale) {
        ImVec2 localTo = mr_canvas.worldToLocal(to);
        ImVec2 d = to - from;
        float l = ImSqrt(ImLengthSqr(d));

        if (l > 0.1f) {
            d = d * (1.0f / l);
            float arrowSize = 8.0f * scale;
            ImVec2 perpendicular = {-d.y, d.x};

            pDrawList->AddTriangleFilled(
                localTo,
                {localTo.x - d.x * arrowSize + perpendicular.x * arrowSize * 0.4f, localTo.y - d.y * arrowSize + perpendicular.y * arrowSize * 0.4f},
                {localTo.x - d.x * arrowSize - perpendicular.x * arrowSize * 0.4f, localTo.y - d.y * arrowSize - perpendicular.y * arrowSize * 0.4f},
                color);
        }
    }

    void m_drawSnapGrid() {
        auto* pDrawList = ImGui::GetWindowDrawList();
        const float spacing = mr_solver.getDatas().system.snapGridSpacing;
        const float strength = mr_solver.getDatas().system.snapGridStrength;
        if (spacing < 1.0f)
            return;

        // D�terminer la zone visible en coordonn�es monde
        ImVec2 topLeft = mr_canvas.localToWorld(ImVec2());
        ImVec2 bottomRight = mr_canvas.localToWorld(ImGui::GetContentRegionAvail());

        // Aligner sur la grille
        float startX = std::floor(topLeft.x / spacing) * spacing;
        float startY = std::floor(topLeft.y / spacing) * spacing;
        float endX = std::ceil(bottomRight.x / spacing) * spacing;
        float endY = std::ceil(bottomRight.y / spacing) * spacing;

        // Intensit� visuelle proportionnelle � la force
        uint8_t alpha = static_cast<uint8_t>(ImClamp(strength * 200.0f, 100.0f, 255.0f));
        ImU32 color = IM_COL32(100, 150, 255, alpha);

        // Lignes verticales
        for (float x = startX; x <= endX; x += spacing) {
            ImVec2 p0 = mr_canvas.worldToLocal(ImVec2(x, topLeft.y));
            ImVec2 p1 = mr_canvas.worldToLocal(ImVec2(x, bottomRight.y));
            pDrawList->AddLine(p0, p1, color, 1.0f);
        }

        // Lignes horizontales
        for (float y = startY; y <= endY; y += spacing) {
            ImVec2 p0 = mr_canvas.worldToLocal(ImVec2(topLeft.x, y));
            ImVec2 p1 = mr_canvas.worldToLocal(ImVec2(bottomRight.x, y));
            pDrawList->AddLine(p0, p1, color, 1.0f);
        }
    }
};
