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

#include <cmath>
#include <memory>
#include <algorithm>

#include "interfaces/isolver.h"

/*
The solver will solve the graph to be in a stable state
No drawing here, just simulation
*/

class Solver : public ISolver {
    ISolver::Datas m_datas;

public:
    void clear() {
        m_datas = {};
    }
    int32_t addNode(const std::shared_ptr<INode>& aNode) override {
        int32_t id = static_cast<int32_t>(m_datas.containers.nodes.size());
        m_datas.containers.nodes.tryAdd(aNode->getDatas().name, aNode);
        return id;
    }

    int32_t addLink(const std::shared_ptr<ILink>& aLink) override {
        int32_t id = static_cast<int32_t>(m_datas.containers.links.size());
        m_datas.containers.links.push_back(aLink);
        return id;
    }
    Datas& rDatas() override {
        return m_datas;
    }
    const Datas& getDatas() const override {
        return m_datas;
    }
    void init() override {
        if (m_datas.containers.nodes.size() < 2U) {
            return;
        }

        // Scatter on circle
        float total_diag = 0;
        for (const auto& node : m_datas.containers.nodes) {
            const auto& datas = node->getDatas();
            const auto& node_size = datas.size;
            total_diag += ImSqrt(ImLengthSqr(node_size)) + 40.0f;
        }
        const float radius = std::max(total_diag / 6.2831853f, 150.0f);

        float idx_f = 0.0f;
        const float count_nodes_inv_f = 6.2831853f / static_cast<float>(m_datas.containers.nodes.size());
        for (auto& rNode : m_datas.containers.nodes) {
            if (rNode->getDatas().locked) {
                continue;
            }
            const float angle = count_nodes_inv_f * idx_f++;
            const float cx = std::cos(angle) * radius;
            const float cy = std::sin(angle) * radius;
            auto& rDatas{rNode->rDatas()};
            const auto& node_size = rDatas.size;
            rDatas.pos = ImVec2(cx - node_size.x * 0.5f, cy - node_size.y * 0.5f);
        }
    }

    void updateLinks() override {
        auto& nodes = m_datas.containers.nodes;
        for (auto& link : m_datas.containers.links) {
            const auto& ldatas = link->getDatas();
            const auto& datas1 = nodes[ldatas.srcNodeID]->getDatas();
            const auto& datas2 = nodes[ldatas.dstNodeID]->getDatas();

            ImVec2 srcSlot, dstSLot;
            float sx = datas1.pos.x + datas1.size.x * 0.5f;
            srcSlot.y = datas1.pos.y + datas1.slots_y[ldatas.srcNodeSlotID];
            float dx = datas2.pos.x + datas2.size.x * 0.5f;
            dstSLot.y = datas2.pos.y + datas2.slots_y[ldatas.dstNodeSlotID];

            srcSlot.x = sx;
            dstSLot.x = dx;

            if (m_datas.system.sideSlots) {
                // Source side: if target is to the right -> exit right, else left
                srcSlot.x = ((dx >= sx) ? datas1.pos.x + datas1.size.x : datas1.pos.x);
                // Target side: if source is to the right -> enter right, else left
                dstSLot.x = ((sx >= dx) ? datas2.pos.x + datas2.size.x : datas2.pos.x);
            }

            link->rDatas().corners = {srcSlot, dstSLot};
        }
    }

    float step(float aDt) override {
        if (m_datas.containers.nodes.size() < 2U) {
            return 0.0f;
        }
        m_resetForces();
        m_repulseNodes();
        m_repulseNodesFromLinks();
        m_attractLinks();
        m_snapToGrid();
        m_applyCentroidGravity();
        m_clampForces();
        return m_updatePositions(aDt);
    }

private:
    void m_resetForces() {
        for (auto& node : m_datas.containers.nodes) {
            node->rDatas().force = {};
        }
    }

    void m_repulseNodes() {
        if (!m_datas.system.enableRepulseNodes) {
            return;
        }
        for (size_t i = 0; i < m_datas.containers.nodes.size(); i++) {
            auto& datas1 = m_datas.containers.nodes[i]->rDatas();
            ImVec2 center1 = datas1.pos + datas1.size * 0.5f;

            for (size_t j = i + 1; j < m_datas.containers.nodes.size(); j++) {
                auto& datas2 = m_datas.containers.nodes[j]->rDatas();
                ImVec2 center2 = datas2.pos + datas2.size * 0.5f;

                ImVec2 delta = center2 - center1;
                float center_dist = ImSqrt(ImLengthSqr(delta));

                if (center_dist < 1.0f) {
                    continue;
                }

                // bord-a-bord real distance
                float half_w = (datas1.size.x + datas2.size.x) * 0.5f;
                float half_h = (datas1.size.y + datas2.size.y) * 0.5f;

                // project on axes to get the real distance between edges
                ImVec2 abs_delta = ImVec2(ImAbs(delta.x), ImAbs(delta.y));
                float actual_gap = ImSqrt(
                    ImMax(abs_delta.x - half_w, 0.0f) * ImMax(abs_delta.x - half_w, 0.0f)    //
                    + ImMax(abs_delta.y - half_h, 0.0f) * ImMax(abs_delta.y - half_h, 0.0f)  //
                );

                // adaptive gap depending on slot count
                float slot_count = std::max(datas1.slots_y.size(), datas2.slots_y.size());
                float effective_gap = m_datas.system.nodeGap * (1.0f + slot_count * m_datas.system.slotGapMultiplier);

                // missing distance to reach the minimal gap
                float gap_deficit = effective_gap - actual_gap;

                ImVec2 direction = delta / center_dist;
                ImVec2 force = {};

                if (gap_deficit > 0.0f) {
                    // too close : repulse
                    force = direction * gap_deficit * 10.0f;
                } else {
                    // normal repulsion
                    force = direction * (m_datas.system.nodeRepulsion / (center_dist * center_dist));
                }

                datas1.force -= force;
                datas2.force += force;
            }
        }
    }

    void m_repulseNodesFromLinks() {
        if (!m_datas.system.enableRepulseNodesFromLinks) {
            return;
        }
        auto& nodes = m_datas.containers.nodes;
        for (auto& node : nodes) {
            auto& nodeDatas = node->rDatas();
            if (nodeDatas.locked) {
                continue;
            }

            ImVec2 nodeCenter = nodeDatas.pos + nodeDatas.size * 0.5f;

            for (auto& link : m_datas.containers.links) {
                const auto& linkDatas = link->getDatas();

                // do not repulse a node from the links it belongs to
                if ((linkDatas.srcNodeID == &node - &nodes[0])  //
                    || (linkDatas.dstNodeID == &node - &nodes[0])) {
                    continue;
                }

                // get the segment extremities
                const auto& corners = linkDatas.corners;
                if (corners.size() < 2) {
                    continue;
                }

                ImVec2 segmentStart = corners[0];
                ImVec2 segmentEnd = corners[corners.size() - 1];

                // distance from point to segment
                ImVec2 segmentVec = segmentEnd - segmentStart;
                float segmentLength = ImSqrt(ImLengthSqr(segmentVec));

                if (segmentLength < 1.0f) {
                    continue;
                }

                // project the node center on the segment
                ImVec2 toNode = nodeCenter - segmentStart;
                float t = ImDot(toNode, segmentVec) / (segmentLength * segmentLength);
                t = ImClamp(t, 0.0f, 1.0f);  // clamp on the segment

                ImVec2 closestPoint = segmentStart + segmentVec * t;
                ImVec2 delta = nodeCenter - closestPoint;
                float distance = ImSqrt(ImLengthSqr(delta));

                if (distance < 1.0f) {
                    continue;
                }

                // effective node radius (approximation)
                float nodeRadius = (nodeDatas.size.x + nodeDatas.size.y) * 0.25f;
                float desired_distance = nodeRadius + m_datas.system.nodeGap * 0.5f;

                float penetration = desired_distance - distance;

                if (penetration > 0.0f) {
                    // repulse the node away from the segment
                    ImVec2 direction = delta / distance;
                    ImVec2 force = direction * penetration * 5.0f;  // repulsion coef
                    nodeDatas.force += force;
                }
            }
        }
    }

    void m_attractLinks() {
        if (!m_datas.system.enableAttractLinks) {
            return;
        }
        for (auto& link : m_datas.containers.links) {
            auto& datas = link->rDatas();
            auto& datas1 = m_datas.containers.nodes.at(datas.srcNodeID)->rDatas();
            auto& datas2 = m_datas.containers.nodes.at(datas.dstNodeID)->rDatas();

            ImVec2 delta = datas2.pos - datas1.pos;
            float distance = ImSqrt(ImLengthSqr(delta));

            if (distance < 1.0f) {
                continue;
            }
            float attraction = distance * m_datas.system.linkAttraction;
            ImVec2 direction = delta / distance;

            datas1.force += direction * attraction;
            datas2.force -= direction * attraction;
        }
    }

    void m_applyCentroidGravity() {
        if (!m_datas.system.enableCentroidGravity) {
            return;
        }
        if (m_datas.containers.nodes.empty()) {
            return;
        }

        m_datas.computed.centroid = {};
        size_t count = 0;

        for (auto& node : m_datas.containers.nodes) {
            auto& datas = node->rDatas();
            m_datas.computed.centroid += (datas.pos + datas.size * 0.5f);
            count++;
        }

        if (count == 0) {
            return;
        }

        m_datas.computed.centroid /= static_cast<float>(count);

        // anchor the centroid toward the canvas center
        ImVec2 toCenter = m_datas.system.anchorPoint - m_datas.computed.centroid;

        for (auto& node : m_datas.containers.nodes) {
            auto& datas = node->rDatas();
            if (!datas.locked) {
                // local gravity toward the centroid
                ImVec2 nodeCenter = datas.pos + datas.size * 0.5f;
                datas.force += (m_datas.computed.centroid - nodeCenter) * m_datas.system.gravity;

                // global anchor toward the canvas center
                datas.force += toCenter * m_datas.system.anchorStrength;
            }
        }
    }

    void m_snapToGrid() {
        if (!m_datas.system.enableSnapToGrid) {
            return;
        }
        for (auto& node : m_datas.containers.nodes) {
            auto& datas = node->rDatas();
            if (datas.locked) {
                continue;
            }

            const ImVec2 center = datas.pos + datas.size * 0.5f;

            // nearest position on the grid
            const float nearestX = std::round(center.x / m_datas.system.snapGridSpacing) * m_datas.system.snapGridSpacing;
            const float nearestY = std::round(center.y / m_datas.system.snapGridSpacing) * m_datas.system.snapGridSpacing;

            // distance to the nearest grid line
            const float dx = nearestX - center.x;
            const float dy = nearestY - center.y;

            // force proportional to distance, but decaying beyond a threshold
            // so as not to pull far nodes
            const float halfGrid = m_datas.system.snapGridSpacing * 0.5f;
            const float fx = (std::abs(dx) < halfGrid) ? dx * m_datas.system.snapGridStrength : 0.0f;
            const float fy = (std::abs(dy) < halfGrid) ? dy * m_datas.system.snapGridStrength : 0.0f;

            datas.force += ImVec2(fx, fy);
        }
    }

    void m_clampForces() {
        for (auto& node : m_datas.containers.nodes) {
            auto& datas = node->rDatas();
            if (!datas.locked) {
                datas.force.x = ImClamp(datas.force.x, -m_datas.system.maxForce, m_datas.system.maxForce);
                datas.force.y = ImClamp(datas.force.y, -m_datas.system.maxForce, m_datas.system.maxForce);
            }
        }
    }

    float m_updatePositions(float aDt) {
        m_datas.system.energy = 0.0f;

        for (auto& node : m_datas.containers.nodes) {
            auto& datas = node->rDatas();
            if (datas.locked) {
                continue;
            }

            datas.velocity += datas.force * aDt;
            datas.velocity *= m_datas.system.damping;
            datas.pos += datas.velocity * aDt;

            m_datas.system.energy += ImAbs(datas.force.x) + ImAbs(datas.force.y);
        }

        return m_datas.system.energy;
    }
};
