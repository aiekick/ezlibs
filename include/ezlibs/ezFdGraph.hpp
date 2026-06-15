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

// ezFdGraph is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

// Force-directed graph layout solver.
// No drawing here, just the simulation : the host application derives its own
// nodes/links from ez::FdGraph::Node / ez::FdGraph::Link (inheritance, no
// templates) and renders the resulting positions itself.
// The optional force functions (node/node repulsion, node/link repulsion, link
// attraction, centroid gravity, snap to grid) are each gated by a flag in the
// Config so the host can compose the behavior it wants.

#include <cmath>
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <type_traits>

#include "ezMath/ezMath.hpp"

namespace ez {

class FdGraph {
public:
    typedef void* UserDatas;

    // Physics datas owned by a node. The host extends this struct to carry its
    // own per-node payload and reads it back through Node::getDatas<T>().
    struct NodeDatas {
        ez::math::fvec2 pos{};
        ez::math::fvec2 velocity{};
        ez::math::fvec2 force{};
        ez::math::fvec2 size{};
        std::vector<float> slots_y{};  // vertical offsets of the slots inside the node, used by updateLinks
        float mass{1.0f};
        uint32_t connCount{0};
        bool locked{false};  // a locked node is never moved by the solver (e.g. dragged by the user)
        UserDatas userDatas{nullptr};
        NodeDatas() = default;
        NodeDatas(const ez::math::fvec2& aPos, const ez::math::fvec2& aForce, float aMass) : pos(aPos), force(aForce), mass(aMass) {}
    };

    class Node {
    private:
        std::shared_ptr<NodeDatas> mp_nodeDatas;

    public:
        template <typename T = NodeDatas>
        explicit Node(const T& aDatas) : mp_nodeDatas(std::make_shared<T>(aDatas)) {
            static_assert(std::is_base_of<NodeDatas, T>::value, "T must derive of ez::FdGraph::NodeDatas");
        }
        virtual ~Node() = default;

        template <typename T = NodeDatas>
        const T& getDatas() const {
            // remove the need to use a slow dynamic_cast
            static_assert(std::is_base_of<NodeDatas, T>::value, "T must derive of ez::FdGraph::NodeDatas");
            return static_cast<const T&>(*mp_nodeDatas);
        }

        template <typename T = NodeDatas>
        T& getDatasRef() {
            // remove the need to use a slow dynamic_cast
            static_assert(std::is_base_of<NodeDatas, T>::value, "T must derive of ez::FdGraph::NodeDatas");
            return static_cast<T&>(*mp_nodeDatas);
        }

        // Semi-implicit Euler integration : force -> velocity -> position, with
        // damping. Mass is honored (default 1.0 behaves like a massless point).
        // Overridable so a derived node can customize its own integration.
        virtual void update(float aDeltaTime, float aDamping) {
            auto& datas = getDatasRef();
            if (datas.mass > 0.0f) {
                datas.velocity += (datas.force / datas.mass) * aDeltaTime;
            } else {
                datas.velocity += datas.force * aDeltaTime;
            }
            datas.velocity *= aDamping;
            datas.pos += datas.velocity * aDeltaTime;
        }
    };

    typedef std::shared_ptr<Node> NodePtr;
    typedef std::weak_ptr<Node> NodeWeak;

    // Physics datas owned by a link. The host extends this struct for its own
    // per-link payload. The connected nodes themselves are held by the Link.
    struct LinkDatas {
        size_t srcSlot{0};  // index into the source node slots_y
        size_t dstSlot{0};  // index into the destination node slots_y
        uint32_t color{0};
        std::vector<ez::math::fvec2> corners{};  // routing points, recomputed by updateLinks
    };

    class Link {
    private:
        std::shared_ptr<LinkDatas> mp_linkDatas;
        NodeWeak m_from;
        NodeWeak m_to;

    public:
        template <typename T = LinkDatas>
        explicit Link(const NodeWeak& aFrom, const NodeWeak& aTo, const T& aDatas = {})
            : mp_linkDatas(std::make_shared<T>(aDatas)), m_from(aFrom), m_to(aTo) {
            static_assert(std::is_base_of<LinkDatas, T>::value, "T must derive of ez::FdGraph::LinkDatas");
        }
        virtual ~Link() = default;

        template <typename T = LinkDatas>
        const T& getDatas() const {
            static_assert(std::is_base_of<LinkDatas, T>::value, "T must derive of ez::FdGraph::LinkDatas");
            return static_cast<const T&>(*mp_linkDatas);
        }

        template <typename T = LinkDatas>
        T& getDatasRef() {
            static_assert(std::is_base_of<LinkDatas, T>::value, "T must derive of ez::FdGraph::LinkDatas");
            return static_cast<T&>(*mp_linkDatas);
        }

        const NodeWeak& getFromNode() const { return m_from; }
        const NodeWeak& getToNode() const { return m_to; }
    };

    typedef std::shared_ptr<Link> LinkPtr;
    typedef std::weak_ptr<Link> LinkWeak;

    // Force-field tunables. Each optional force has its own enable flag.
    struct Config {
        float damping{0.99f};              // velocity damping (0-1, lower = brakes more)
        float nodeGap{40.0f};              // minimal distance between node edges
        float maxForce{50.0f};             // max applicable force per axis (avoids explosion)
        float nodeRepulsion{5000.0f};      // node to node repulsion intensity
        float linkAttraction{0.005f};      // link attraction intensity (spring)
        float gravity{0.002f};             // force toward the centroid (avoids spreading)
        float slotGapMultiplier{1.0f};     // extra gap per slot (proportional to connection count)
        float nodeToLinkRepulsion{5.0f};   // node to link repulsion coef
        ez::math::fvec2 anchorPoint{};     // graph anchor point
        float anchorStrength{1.0f};        // pull strength toward the anchor point
        bool sideSlots{false};             // slots on node sides (else centered)
        float snapGridSpacing{30.0f};      // grid spacing
        float snapGridStrength{0.1f};      // attraction strength toward grid lines (low = soft)
        bool enableRepulseNodes{true};
        bool enableRepulseNodesFromLinks{true};
        bool enableAttractLinks{true};
        bool enableSnapToGrid{true};
        bool enableCentroidGravity{true};
    };

private:
    std::vector<NodePtr> m_nodes;
    std::vector<LinkPtr> m_links;
    Config m_config;
    ez::math::fvec2 m_centroid{};
    float m_energy{0.0f};

public:
    void clear() {
        m_links.clear();
        m_nodes.clear();
        m_centroid = {};
        m_energy = 0.0f;
    }

    template <typename T = Node, typename U = NodeDatas>
    std::weak_ptr<T> addNode(const U& aDatas) {
        static_assert(std::is_base_of<Node, T>::value, "T must derive of ez::FdGraph::Node");
        static_assert(std::is_base_of<NodeDatas, U>::value, "U must derive of ez::FdGraph::NodeDatas");
        auto ptr = std::make_shared<T>(aDatas);
        m_nodes.push_back(ptr);
        return ptr;
    }

    template <typename T = Link, typename U = LinkDatas>
    std::weak_ptr<T> addLink(const NodeWeak& aFrom, const NodeWeak& aTo, const U& aDatas = {}) {
        static_assert(std::is_base_of<Link, T>::value, "T must derive of ez::FdGraph::Link");
        static_assert(std::is_base_of<LinkDatas, U>::value, "U must derive of ez::FdGraph::LinkDatas");
        if (aFrom.expired() || aTo.expired()) {
            return std::weak_ptr<T>();
        }
        auto ptr = std::make_shared<T>(aFrom, aTo, aDatas);
        m_links.push_back(ptr);
        aFrom.lock()->getDatasRef().connCount += 1;
        aTo.lock()->getDatasRef().connCount += 1;
        return ptr;
    }

    const Config& getConfig() const { return m_config; }
    Config& getConfigRef() { return m_config; }

    const std::vector<NodePtr>& getNodes() const { return m_nodes; }
    std::vector<NodePtr>& getNodesRef() { return m_nodes; }

    const std::vector<LinkPtr>& getLinks() const { return m_links; }
    std::vector<LinkPtr>& getLinksRef() { return m_links; }

    const ez::math::fvec2& getCentroid() const { return m_centroid; }
    float getEnergy() const { return m_energy; }

    // Scatter the unlocked nodes on a circle sized after the total node footprint.
    void init() {
        if (m_nodes.size() < 2U) {
            return;
        }
        float totalDiag = 0.0f;
        for (const auto& nodePtr : m_nodes) {
            totalDiag += nodePtr->getDatas().size.length() + 40.0f;
        }
        const float radius = std::max(totalDiag / 6.2831853f, 150.0f);
        float idx = 0.0f;
        const float angleStep = 6.2831853f / static_cast<float>(m_nodes.size());
        for (auto& nodePtr : m_nodes) {
            auto& datas = nodePtr->getDatasRef();
            if (datas.locked) {
                continue;
            }
            const float angle = angleStep * idx++;
            const float cx = std::cos(angle) * radius;
            const float cy = std::sin(angle) * radius;
            datas.pos = ez::math::fvec2(cx - datas.size.x * 0.5f, cy - datas.size.y * 0.5f);
        }
    }

    // Recompute the per-link routing corners (world position of each connected slot).
    void updateLinks() {
        for (auto& linkPtr : m_links) {
            auto fromPtr = linkPtr->getFromNode().lock();
            auto toPtr = linkPtr->getToNode().lock();
            if ((fromPtr == nullptr) || (toPtr == nullptr)) {
                continue;
            }
            auto& linkDatas = linkPtr->getDatasRef();
            const auto& srcDatas = fromPtr->getDatas();
            const auto& dstDatas = toPtr->getDatas();
            if ((linkDatas.srcSlot >= srcDatas.slots_y.size()) || (linkDatas.dstSlot >= dstDatas.slots_y.size())) {
                continue;
            }
            const float srcCenterX = srcDatas.pos.x + srcDatas.size.x * 0.5f;
            const float dstCenterX = dstDatas.pos.x + dstDatas.size.x * 0.5f;
            ez::math::fvec2 srcSlot, dstSlot;
            srcSlot.x = srcCenterX;
            srcSlot.y = srcDatas.pos.y + srcDatas.slots_y[linkDatas.srcSlot];
            dstSlot.x = dstCenterX;
            dstSlot.y = dstDatas.pos.y + dstDatas.slots_y[linkDatas.dstSlot];
            if (m_config.sideSlots) {
                // source side : if the target is to the right -> exit right, else left
                srcSlot.x = (dstCenterX >= srcCenterX) ? (srcDatas.pos.x + srcDatas.size.x) : srcDatas.pos.x;
                // target side : if the source is to the right -> enter right, else left
                dstSlot.x = (srcCenterX >= dstCenterX) ? (dstDatas.pos.x + dstDatas.size.x) : dstDatas.pos.x;
            }
            linkDatas.corners = {srcSlot, dstSlot};
        }
    }

    // Run one simulation step and return the total energy of the system.
    // Links routing (updateLinks) is expected to be refreshed by the host each
    // frame ; the node/link repulsion self-skips links with no routing yet.
    float step(float aDeltaTime) {
        if (m_nodes.size() < 2U) {
            return 0.0f;
        }
        m_resetForces();
        m_repulseNodes();
        m_repulseNodesFromLinks();
        m_attractLinks();
        m_snapToGrid();
        m_applyCentroidGravity();
        m_clampForces();
        return m_integrate(aDeltaTime);
    }

private:
    void m_resetForces() {
        for (auto& nodePtr : m_nodes) {
            nodePtr->getDatasRef().force = {};
        }
    }

    void m_repulseNodes() {
        if (!m_config.enableRepulseNodes) {
            return;
        }
        for (size_t i = 0; i < m_nodes.size(); ++i) {
            auto& datas1 = m_nodes[i]->getDatasRef();
            const ez::math::fvec2 center1 = datas1.pos + datas1.size * 0.5f;
            for (size_t j = i + 1; j < m_nodes.size(); ++j) {
                auto& datas2 = m_nodes[j]->getDatasRef();
                const ez::math::fvec2 center2 = datas2.pos + datas2.size * 0.5f;
                const ez::math::fvec2 delta = center2 - center1;
                const float centerDist = delta.length();
                if (centerDist < 1.0f) {
                    continue;
                }
                // edge-to-edge real distance : project on axes then keep the positive part
                const float halfWidth = (datas1.size.x + datas2.size.x) * 0.5f;
                const float halfHeight = (datas1.size.y + datas2.size.y) * 0.5f;
                const float gapX = std::max(std::abs(delta.x) - halfWidth, 0.0f);
                const float gapY = std::max(std::abs(delta.y) - halfHeight, 0.0f);
                const float actualGap = std::sqrt(gapX * gapX + gapY * gapY);
                // adaptive gap depending on the slot count
                const float slotCount = static_cast<float>(std::max(datas1.slots_y.size(), datas2.slots_y.size()));
                const float effectiveGap = m_config.nodeGap * (1.0f + slotCount * m_config.slotGapMultiplier);
                const float gapDeficit = effectiveGap - actualGap;
                const ez::math::fvec2 direction = delta / centerDist;
                ez::math::fvec2 force;
                if (gapDeficit > 0.0f) {
                    // too close : strong corrective repulsion
                    force = direction * gapDeficit * 10.0f;
                } else {
                    // normal inverse-square repulsion
                    force = direction * (m_config.nodeRepulsion / (centerDist * centerDist));
                }
                datas1.force -= force;
                datas2.force += force;
            }
        }
    }

    void m_repulseNodesFromLinks() {
        if (!m_config.enableRepulseNodesFromLinks) {
            return;
        }
        for (auto& nodePtr : m_nodes) {
            auto& nodeDatas = nodePtr->getDatasRef();
            if (nodeDatas.locked) {
                continue;
            }
            const ez::math::fvec2 nodeCenter = nodeDatas.pos + nodeDatas.size * 0.5f;
            for (auto& linkPtr : m_links) {
                // do not repulse a node from a link it belongs to
                if ((linkPtr->getFromNode().lock().get() == nodePtr.get()) || (linkPtr->getToNode().lock().get() == nodePtr.get())) {
                    continue;
                }
                const auto& corners = linkPtr->getDatas().corners;
                if (corners.size() < 2U) {
                    continue;
                }
                const ez::math::fvec2 segmentStart = corners.front();
                const ez::math::fvec2 segmentEnd = corners.back();
                const ez::math::fvec2 segmentVec = segmentEnd - segmentStart;
                const float segmentLength = segmentVec.length();
                if (segmentLength < 1.0f) {
                    continue;
                }
                // project the node center on the segment, clamped to its extremities
                const ez::math::fvec2 toNode = nodeCenter - segmentStart;
                float ratio = ez::math::dot(toNode, segmentVec) / (segmentLength * segmentLength);
                ratio = ez::math::clamp(ratio, 0.0f, 1.0f);
                const ez::math::fvec2 closestPoint = segmentStart + segmentVec * ratio;
                const ez::math::fvec2 delta = nodeCenter - closestPoint;
                const float distance = delta.length();
                if (distance < 1.0f) {
                    continue;
                }
                const float nodeRadius = (nodeDatas.size.x + nodeDatas.size.y) * 0.25f;
                const float desiredDistance = nodeRadius + m_config.nodeGap * 0.5f;
                const float penetration = desiredDistance - distance;
                if (penetration > 0.0f) {
                    const ez::math::fvec2 direction = delta / distance;
                    nodeDatas.force += direction * penetration * m_config.nodeToLinkRepulsion;
                }
            }
        }
    }

    void m_attractLinks() {
        if (!m_config.enableAttractLinks) {
            return;
        }
        for (auto& linkPtr : m_links) {
            auto fromPtr = linkPtr->getFromNode().lock();
            auto toPtr = linkPtr->getToNode().lock();
            if ((fromPtr == nullptr) || (toPtr == nullptr)) {
                continue;
            }
            auto& datas1 = fromPtr->getDatasRef();
            auto& datas2 = toPtr->getDatasRef();
            const ez::math::fvec2 delta = datas2.pos - datas1.pos;
            const float distance = delta.length();
            if (distance < 1.0f) {
                continue;
            }
            const float attraction = distance * m_config.linkAttraction;
            const ez::math::fvec2 direction = delta / distance;
            datas1.force += direction * attraction;
            datas2.force -= direction * attraction;
        }
    }

    void m_applyCentroidGravity() {
        if (!m_config.enableCentroidGravity) {
            return;
        }
        if (m_nodes.empty()) {
            return;
        }
        m_centroid = {};
        for (auto& nodePtr : m_nodes) {
            const auto& datas = nodePtr->getDatas();
            m_centroid += (datas.pos + datas.size * 0.5f);
        }
        m_centroid /= static_cast<float>(m_nodes.size());
        // anchor the whole centroid toward the configured anchor point
        const ez::math::fvec2 toAnchor = m_config.anchorPoint - m_centroid;
        for (auto& nodePtr : m_nodes) {
            auto& datas = nodePtr->getDatasRef();
            if (!datas.locked) {
                // local gravity toward the centroid
                const ez::math::fvec2 nodeCenter = datas.pos + datas.size * 0.5f;
                datas.force += (m_centroid - nodeCenter) * m_config.gravity;
                // global anchor toward the anchor point
                datas.force += toAnchor * m_config.anchorStrength;
            }
        }
    }

    void m_snapToGrid() {
        if (!m_config.enableSnapToGrid) {
            return;
        }
        if (m_config.snapGridSpacing <= 0.0f) {
            return;
        }
        for (auto& nodePtr : m_nodes) {
            auto& datas = nodePtr->getDatasRef();
            if (datas.locked) {
                continue;
            }
            const ez::math::fvec2 center = datas.pos + datas.size * 0.5f;
            // nearest position on the grid
            const float nearestX = std::round(center.x / m_config.snapGridSpacing) * m_config.snapGridSpacing;
            const float nearestY = std::round(center.y / m_config.snapGridSpacing) * m_config.snapGridSpacing;
            const float dx = nearestX - center.x;
            const float dy = nearestY - center.y;
            // force proportional to distance, but cut off beyond half a cell so far nodes are not pulled
            const float halfGrid = m_config.snapGridSpacing * 0.5f;
            const float fx = (std::abs(dx) < halfGrid) ? dx * m_config.snapGridStrength : 0.0f;
            const float fy = (std::abs(dy) < halfGrid) ? dy * m_config.snapGridStrength : 0.0f;
            datas.force += ez::math::fvec2(fx, fy);
        }
    }

    void m_clampForces() {
        for (auto& nodePtr : m_nodes) {
            auto& datas = nodePtr->getDatasRef();
            if (!datas.locked) {
                datas.force.x = ez::math::clamp(datas.force.x, -m_config.maxForce, m_config.maxForce);
                datas.force.y = ez::math::clamp(datas.force.y, -m_config.maxForce, m_config.maxForce);
            }
        }
    }

    float m_integrate(float aDeltaTime) {
        m_energy = 0.0f;
        for (auto& nodePtr : m_nodes) {
            auto& datas = nodePtr->getDatasRef();
            if (datas.locked) {
                continue;
            }
            nodePtr->update(aDeltaTime, m_config.damping);
            m_energy += datas.force.sumAbs();
        }
        return m_energy;
    }
};

}  // namespace ez
