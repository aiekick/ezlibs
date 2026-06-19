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
#include <cassert>
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <functional>
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
        float mass{1.0f};
        uint32_t connCount{0};
        bool locked{false};   // a locked node is never moved by the solver (e.g. dragged by the user)
        bool enabled{true};   // a disabled node is fully ignored by the solver (no force, no link, not drawn)
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

    using NodePtr = std::shared_ptr<Node>;
    using NodeWeak = std::weak_ptr<Node>;

    // Physics datas owned by a link. The host extends this struct for its own
    // per-link payload. The connected nodes themselves are held by the Link.
    struct LinkDatas {
        size_t srcSlot{0};  // index into the source node slots_y
        size_t dstSlot{0};  // index into the destination node slots_y
        uint32_t color{0};
        bool enabled{ true };   // a disabled link is not used by the solver
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

    using LinkPtr = std::shared_ptr<Link>;
    using LinkWeak = std::weak_ptr<Link>;

    // Core solver tunables that are NOT forces : applied every step whatever the
    // registered functors are. Per-force knobs live in each force's datas block.
    struct Config {
        float damping{0.99f};   // velocity damping (0-1, lower = brakes more) ; used by the integration
        float maxForce{50.0f};  // max applicable force per axis (avoids explosion) ; used by the clamp
    };

    class IComputeDatas {
    public:
        virtual ~IComputeDatas() = default; // for have polymorphic class
    };
    using IComputeDatasPtr = std::shared_ptr<IComputeDatas>;
    using IComputeDatasWeak = std::weak_ptr<IComputeDatas>;
    using NodeContainer = std::vector<NodeWeak>;
    using LinkContainer = std::vector<LinkWeak>;
    using IComputeDatasContainer = std::vector<IComputeDatasWeak>;
    using ComputeFunctor = std::function<void(const NodeContainer&, const LinkContainer&, const IComputeDatasWeak&)>;
    using ComputeDatasFunctor = std::function<bool(const IComputeDatasWeak&)>;

    // Per-force tunable datas. Each default compute functor downcasts the erased
    // IComputeDatas back to its own block (same idiom as Node::getDatas<T>()), so
    // every parameter a force needs lives here. The host tunes a force by keeping
    // the reference returned by registerFunctor (or reads back e.g. the centroid).
    class RepulseNodesDatas : public IComputeDatas {
    public:
        bool enabled{true};
        float nodeGap{40.0f};             // minimal distance between node edges
        float nodeRepulsion{5000.0f};     // node to node repulsion intensity
    };
    class RepulseNodesFromLinksDatas : public IComputeDatas {
    public:
        bool enabled{true};
        float nodeGap{40.0f};             // minimal distance between node edges
        float nodeToLinkRepulsion{5.0f};  // node to link repulsion coef
    };
    class AttractLinksDatas : public IComputeDatas {
    public:
        bool enabled{true};
        float linkAttraction{0.005f};     // link attraction intensity (spring)
    };
    class SnapToGridDatas : public IComputeDatas {
    public:
        bool enabled{true};
        float snapGridSpacing{30.0f};     // grid spacing
        float snapGridStrength{0.1f};     // attraction strength toward grid lines (low = soft)
    };
    class CentroidGravityDatas : public IComputeDatas {
    public:
        bool enabled{true};
        float gravity{0.002f};            // force toward the centroid (avoids spreading)
        ez::math::fvec2 anchorPoint{};    // graph anchor point
        float anchorStrength{1.0f};       // pull strength toward the anchor point
        ez::math::fvec2 centroid{};       // recomputed each step ; read back by the host to display
    };

private:
    template <typename TTYPE>
    struct SmartContainer { // type interne
        std::vector<std::shared_ptr<TTYPE>> ptrs;
        std::vector<std::weak_ptr<TTYPE>> weaks;
        void clear() {
            ptrs.clear();
            weaks.clear();
        }
        void push_back(std::shared_ptr<TTYPE> aPtr) {
            ptrs.push_back(aPtr);
            weaks.push_back(aPtr);
        }
    };
    SmartContainer<Node> m_nodes;
    SmartContainer<Link> m_links;
    SmartContainer<IComputeDatas> m_computeDatas;
    std::vector<ComputeFunctor> m_computeFunctors;
    std::vector<ComputeDatasFunctor> m_computeDatasFunctors;
    Config m_config;
    ez::math::fvec2 m_centroid{};
    float m_energy{0.0f};

public:
    void clearDatas() {
        m_links.clear();
        m_nodes.clear();
        m_centroid = {};
        m_energy = 0.0f;
    }

    void clearFunctors() {
        m_computeDatas.clear();
        m_computeFunctors.clear();
        m_computeDatasFunctors.clear();
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

    // Register a compute functor with a fresh datas block of type TTYPE. The block
    // is owned by the graph (kept parallel to the functor) and a typed reference is
    // returned so the host can tune it at runtime, or read values it computes back.
    template <typename TTYPE>
    TTYPE& registerFunctor(const ComputeFunctor& aFunctor, const ComputeDatasFunctor& aComputeDatasFunctor) {
        static_assert(std::is_base_of<IComputeDatas, TTYPE>::value, "TTYPE must derive of IComputeDatas");
        auto pDatas = std::make_shared<TTYPE>();
        m_computeDatas.push_back(pDatas);
        m_computeFunctors.push_back(aFunctor);
        m_computeDatasFunctors.push_back(aComputeDatasFunctor);
        return *pDatas;
    }

    const Config& getConfig() const { return m_config; }
    Config& getConfigRef() { return m_config; }

    const NodeContainer& getNodes() const { return m_nodes.weaks; }
    NodeContainer& getNodesRef() { return m_nodes.weaks; }

    const LinkContainer& getLinks() const { return m_links.weaks; }
    LinkContainer& getLinksRef() { return m_links.weaks; }

    const IComputeDatasContainer& getComputeDatas() const { return m_computeDatas.weaks; }
    IComputeDatasContainer& getComputeDatas() { return m_computeDatas.weaks; }

    float getEnergy() const { return m_energy; }

    // Scatter the unlocked nodes on a circle sized after the total node footprint.
    void initSimulation() {
        if (m_nodes.ptrs.size() < 2U) {
            return;
        }
        float totalDiag = 0.0f;
        size_t enabledCount = 0;
        for (const auto& nodePtr : m_nodes.ptrs) {
            if (!nodePtr->getDatas().enabled) {
                continue;
            }
            totalDiag += nodePtr->getDatas().size.length() + 40.0f;
            ++enabledCount;
        }
        if (enabledCount < 2U) {
            return;
        }
        const float radius = std::max(totalDiag / 6.2831853f, 150.0f);
        float idx = 0.0f;
        const float angleStep = 6.2831853f / static_cast<float>(enabledCount);
        for (auto& nodePtr : m_nodes.ptrs) {
            auto& datas = nodePtr->getDatasRef();
            if (!datas.enabled || datas.locked) {
                continue;
            }
            const float angle = angleStep * idx++;
            const float cx = std::cos(angle) * radius;
            const float cy = std::sin(angle) * radius;
            datas.pos = ez::math::fvec2(cx - datas.size.x * 0.5f, cy - datas.size.y * 0.5f);
        }
    }

    // Register the built-in forces, each with its own default datas block, and keep
    // typed handles to them (returned and reachable via getDefaultFunctors()). The
    // host can skip this, call clearFunctors() to drop them, or add its own after.
    void initDefaultComputeFunctors() {
        clearFunctors();
        registerFunctor<RepulseNodesDatas>(&FdGraph::computeRepulseNodes, {});
        registerFunctor<RepulseNodesFromLinksDatas>(&FdGraph::computeRepulseNodesFromLinks, {});
        registerFunctor<AttractLinksDatas>(&FdGraph::computeAttractLinks, {});
        registerFunctor<SnapToGridDatas>(&FdGraph::computeSnapToGrid, {});
        registerFunctor<CentroidGravityDatas>(&FdGraph::computeCentroidGravity, {});
    }

    // Run one simulation step and return the total energy of the system.
    // Links routing (updateLinks) is expected to be refreshed by the host each
    // frame ; the node/link repulsion self-skips links with no routing yet.
    float step(float aDeltaTime) {
        if (m_nodes.ptrs.size() < 2U) { return 0.0f; }
        m_resetForces();
        assert(m_computeFunctors.size() == m_computeDatas.weaks.size());
        for (size_t idx = 0U; idx < m_computeFunctors.size(); ++idx) {
            const auto& datas = m_computeDatas.weaks.at(idx);
            const auto& pFunctor = m_computeFunctors.at(idx);
            if (pFunctor != nullptr) {  //
                pFunctor(m_nodes.weaks, m_links.weaks, datas);
            }
        }
        m_clampForces();
        return m_integrate(aDeltaTime);
    }

    bool execComputeDatas() {
        bool change = false;
        assert(computeDatasFunctor.size() == m_computeDatas.weaks.size());
        for (size_t idx = 0U; idx < m_computeDatasFunctors.size(); ++idx) {
            const auto& datas = m_computeDatas.weaks.at(idx);
            const auto& pFunctor = m_computeDatasFunctors.at(idx);
            if (pFunctor != nullptr) {  //
                change |= pFunctor(datas);
            }
        }
        return change;
    }

    // === Built-in compute functors (static, self-contained) =============
    // Each one locks the type-erased datas, downcasts it to its own block (same
    // idiom as Node::getDatas<T>()) and reads its tunables from there. They take no
    // Config / graph : every parameter a force needs lives in its datas block, so a
    // force can be copied, replaced or moved to another graph without dependencies.
    static void computeRepulseNodes(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        (void)aLinks;
        auto pDatas = aDatas.lock();
        if (pDatas == nullptr) {
            return;
        }
        const auto& datas = static_cast<const RepulseNodesDatas&>(*pDatas);
        if (!datas.enabled) {
            return;
        }
        // lock once : the graph keeps the nodes alive for the whole step, so the raw
        // NodeDatas pointers stay valid and we skip an O(N^2) flood of weak lock()
        std::vector<NodeDatas*> nodeDatas;
        nodeDatas.reserve(aNodes.size());
        for (const auto& nodeWeak : aNodes) {
            auto nodePtr = nodeWeak.lock();
            if ((nodePtr != nullptr) && nodePtr->getDatas().enabled) {
                nodeDatas.push_back(&nodePtr->getDatasRef());
            }
        }
        for (size_t i = 0; i < nodeDatas.size(); ++i) {
            auto& datas1 = *nodeDatas[i];
            const ez::math::fvec2 center1 = datas1.pos + datas1.size * 0.5f;
            for (size_t j = i + 1; j < nodeDatas.size(); ++j) {
                auto& datas2 = *nodeDatas[j];
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
                const ez::math::fvec2 direction = delta / centerDist;
                ez::math::fvec2 force;
                // normal inverse-square repulsion
                force = direction * (datas.nodeRepulsion / (centerDist * centerDist));
                datas1.force -= force;
                datas2.force += force;
            }
        }
    }

    static void computeRepulseNodesFromLinks(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        auto pDatas = aDatas.lock();
        if (pDatas == nullptr) {
            return;
        }
        const auto& datas = static_cast<const RepulseNodesFromLinksDatas&>(*pDatas);
        if (!datas.enabled) {
            return;
        }
        for (const auto& nodeWeak : aNodes) {
            auto nodePtr = nodeWeak.lock();
            if (nodePtr == nullptr) {
                continue;
            }
            auto& nodeDatas = nodePtr->getDatasRef();
            if (nodeDatas.locked || !nodeDatas.enabled) {
                continue;
            }
            const ez::math::fvec2 nodeCenter = nodeDatas.pos + nodeDatas.size * 0.5f;
            for (const auto& linkWeak : aLinks) {
                auto linkPtr = linkWeak.lock();
                if (linkPtr == nullptr) {
                    continue;
                }
                if (linkPtr->getDatas().enabled) {
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
                    const float desiredDistance = nodeRadius + datas.nodeGap * 0.5f;
                    const float penetration = desiredDistance - distance;
                    if (penetration > 0.0f) {
                        const ez::math::fvec2 direction = delta / distance;
                        nodeDatas.force += direction * penetration * datas.nodeToLinkRepulsion;
                    }
                }
            }
        }
    }

    static void computeAttractLinks(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        (void)aNodes;
        auto pDatas = aDatas.lock();
        if (pDatas == nullptr) {
            return;
        }
        const auto& datas = static_cast<const AttractLinksDatas&>(*pDatas);
        if (!datas.enabled) {
            return;
        }
        for (const auto& linkWeak : aLinks) {
            auto linkPtr = linkWeak.lock();
            if (linkPtr == nullptr) {
                continue;
            }
            if (linkPtr->getDatas().enabled) {
                auto fromPtr = linkPtr->getFromNode().lock();
                auto toPtr = linkPtr->getToNode().lock();
                if ((fromPtr == nullptr) || (toPtr == nullptr)) {
                    continue;
                }
                auto& datas1 = fromPtr->getDatasRef();
                auto& datas2 = toPtr->getDatasRef();
                if (!datas1.enabled || !datas2.enabled) {
                    continue;
                }
                const ez::math::fvec2 delta = datas2.pos - datas1.pos;
                const float distance = delta.length();
                if (distance < 1.0f) {
                    continue;
                }
                const float attraction = distance * datas.linkAttraction;
                const ez::math::fvec2 direction = delta / distance;
                datas1.force += direction * attraction;
                datas2.force -= direction * attraction;
            }
        }
    }

    static void computeSnapToGrid(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        (void)aLinks;
        auto pDatas = aDatas.lock();
        if (pDatas == nullptr) {
            return;
        }
        const auto& datas = static_cast<const SnapToGridDatas&>(*pDatas);
        if (!datas.enabled) {
            return;
        }
        if (datas.snapGridSpacing <= 0.0f) {
            return;
        }
        for (const auto& nodeWeak : aNodes) {
            auto nodePtr = nodeWeak.lock();
            if (nodePtr == nullptr) {
                continue;
            }
            auto& nodeDatas = nodePtr->getDatasRef();
            if (nodeDatas.locked || !nodeDatas.enabled) {
                continue;
            }
            const ez::math::fvec2 center = nodeDatas.pos + nodeDatas.size * 0.5f;
            // nearest position on the grid
            const float nearestX = std::round(center.x / datas.snapGridSpacing) * datas.snapGridSpacing;
            const float nearestY = std::round(center.y / datas.snapGridSpacing) * datas.snapGridSpacing;
            const float dx = nearestX - center.x;
            const float dy = nearestY - center.y;
            // force proportional to distance, but cut off beyond half a cell so far nodes are not pulled
            const float halfGrid = datas.snapGridSpacing * 0.5f;
            const float fx = (std::abs(dx) < halfGrid) ? dx * datas.snapGridStrength : 0.0f;
            const float fy = (std::abs(dy) < halfGrid) ? dy * datas.snapGridStrength : 0.0f;
            nodeDatas.force += ez::math::fvec2(fx, fy);
        }
    }

    static void computeCentroidGravity(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        (void)aLinks;
        auto pDatas = aDatas.lock();
        if (pDatas == nullptr) {
            return;
        }
        // non-const : this force stores the computed centroid back into its datas
        auto& datas = static_cast<CentroidGravityDatas&>(*pDatas);
        if (!datas.enabled) {
            return;
        }
        ez::math::fvec2 centroid{};
        size_t enabledCount = 0;
        for (const auto& nodeWeak : aNodes) {
            auto nodePtr = nodeWeak.lock();
            if (nodePtr == nullptr) {
                continue;
            }
            const auto& nodeDatas = nodePtr->getDatas();
            if (!nodeDatas.enabled) {
                continue;
            }
            centroid += (nodeDatas.pos + nodeDatas.size * 0.5f);
            ++enabledCount;
        }
        if (enabledCount == 0) {
            return;
        }
        centroid /= static_cast<float>(enabledCount);
        datas.centroid = centroid;  // exposed to the host through the registered datas
        // anchor the whole centroid toward the configured anchor point
        const ez::math::fvec2 toAnchor = datas.anchorPoint - centroid;
        for (const auto& nodeWeak : aNodes) {
            auto nodePtr = nodeWeak.lock();
            if (nodePtr == nullptr) {
                continue;
            }
            auto& nodeDatas = nodePtr->getDatasRef();
            if (nodeDatas.enabled && !nodeDatas.locked) {
                // local gravity toward the centroid
                const ez::math::fvec2 nodeCenter = nodeDatas.pos + nodeDatas.size * 0.5f;
                nodeDatas.force += (centroid - nodeCenter) * datas.gravity;
                // global anchor toward the anchor point
                nodeDatas.force += toAnchor * datas.anchorStrength;
            }
        }
    }

private:
    void m_resetForces() {
        for (auto& nodePtr : m_nodes.ptrs) {
            nodePtr->getDatasRef().force = {};
        }
    }

    void m_clampForces() {
        for (auto& nodePtr : m_nodes.ptrs) {
            auto& datas = nodePtr->getDatasRef();
            if (datas.enabled && !datas.locked) {
                datas.force.x = ez::math::clamp(datas.force.x, -m_config.maxForce, m_config.maxForce);
                datas.force.y = ez::math::clamp(datas.force.y, -m_config.maxForce, m_config.maxForce);
            }
        }
    }

    float m_integrate(float aDeltaTime) {
        m_energy = 0.0f;
        for (auto& nodePtr : m_nodes.ptrs) {
            auto& datas = nodePtr->getDatasRef();
            if (!datas.enabled || datas.locked) {
                continue;
            }
            nodePtr->update(aDeltaTime, m_config.damping);
            m_energy += datas.force.sumAbs();
        }
        return m_energy;
    }
};

}  // namespace ez
