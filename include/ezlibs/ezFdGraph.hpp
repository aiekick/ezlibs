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
        bool enabled{ true };   // a disabled link is not used by the solver
        float weight{1.0f};            // attraction scaling : a heavier link pulls its two nodes closer (weighted edge)
        ez::math::fvec2 fromOffset{};  // source endpoint offset, relative to the source node center (0 = center ; lets the host simulate a slot)
        ez::math::fvec2 toOffset{};    // destination endpoint offset, relative to the destination node center
        std::vector<ez::math::fvec2> corners{};  // routing points provided by the host ; consumed by node/link repulsion (empty -> link skipped)
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
    class AlignLinksDatas : public IComputeDatas {
    public:
        bool enabled{false};            // opt-in : a fairly strong layout constraint
        float strength{0.05f};          // how hard links snap toward the chosen axis
        bool alignHorizontal{true};     // pull links toward the horizontal axis
        bool alignVertical{true};       // pull links toward the vertical axis ; both on -> each link snaps to its nearest axis
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

    // Register the built-in forces, each with its own default datas block. The host can
    // skip this, call clearFunctors() to drop them, add its own after, or reach a force's
    // datas block back through getComputeDatas().
    void initDefaultComputeFunctors() {
        clearFunctors();
        registerFunctor<RepulseNodesDatas>(&FdGraph::computeRepulseNodes, {});
        registerFunctor<RepulseNodesFromLinksDatas>(&FdGraph::computeRepulseNodesFromLinks, {});
        registerFunctor<AttractLinksDatas>(&FdGraph::computeAttractLinks, {});
        registerFunctor<SnapToGridDatas>(&FdGraph::computeSnapToGrid, {});
        registerFunctor<CentroidGravityDatas>(&FdGraph::computeCentroidGravity, {});
    }

    // Run one simulation step and return the total energy of the system.
    // Link routing (the LinkDatas corners) is expected to be refreshed by the host each
    // frame ; the node/link repulsion self-skips links whose corners are not set yet.
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

    // Hard minimum-gap constraint, applied to POSITIONS (not forces) so it cannot be overpowered
    // by a strong link attraction the way the force-based floor can. Call it right after step() :
    // any pair of nodes whose rectangles are closer than aMinGap (edge to edge) gets pushed apart.
    void separateOverlaps(float aMinGap) {
        if (aMinGap <= 0.0f || m_nodes.ptrs.size() < 2U) {
            return;
        }
        for (int32_t pass = 0; pass < 4; ++pass) {  // a few relaxation passes so chains of overlaps converge
            for (size_t i = 0U; i < m_nodes.ptrs.size(); ++i) {
                auto& datas1 = m_nodes.ptrs[i]->getDatasRef();
                if (!datas1.enabled) {
                    continue;
                }
                const ez::math::fvec2 center1 = datas1.pos + datas1.size * 0.5f;
                for (size_t j = i + 1U; j < m_nodes.ptrs.size(); ++j) {
                    auto& datas2 = m_nodes.ptrs[j]->getDatasRef();
                    if (!datas2.enabled) {
                        continue;
                    }
                    const ez::math::fvec2 center2 = datas2.pos + datas2.size * 0.5f;
                    const ez::math::fvec2 delta = center2 - center1;
                    const float centerDist = delta.length();
                    if (centerDist < 0.0001f) {
                        continue;  // coincident centers : no sensible direction to push along
                    }
                    const float halfWidth = (datas1.size.x + datas2.size.x) * 0.5f;
                    const float halfHeight = (datas1.size.y + datas2.size.y) * 0.5f;
                    const float gapX = std::max(std::abs(delta.x) - halfWidth, 0.0f);
                    const float gapY = std::max(std::abs(delta.y) - halfHeight, 0.0f);
                    const float actualGap = std::sqrt(gapX * gapX + gapY * gapY);
                    const float deficit = aMinGap - actualGap;
                    if (deficit <= 0.0f) {
                        continue;
                    }
                    const ez::math::fvec2 dir = delta / centerDist;
                    // push apart (locked nodes stay put ; the free one then takes the whole shift)
                    if (!datas1.locked && !datas2.locked) {
                        datas1.pos -= dir * (deficit * 0.5f);
                        datas2.pos += dir * (deficit * 0.5f);
                    } else if (!datas1.locked) {
                        datas1.pos -= dir * deficit;
                    } else if (!datas2.locked) {
                        datas2.pos += dir * deficit;
                    }
                }
            }
        }
    }

    // Axis alignment applied to POSITIONS (like separateOverlaps), so it is neither capped by
    // maxForce nor cancelled out the way the force version is on multiply-connected nodes. Each link
    // pulls its two endpoints onto the same row (horizontal) or column (vertical) ; a node shared by
    // several links settles at the average over the passes -> "as horizontal/vertical as possible".
    // aStrength is a 0..1 relaxation factor (1 = snap onto the axis in one frame).
    void alignLinks(float aStrength, bool aHorizontal, bool aVertical) {
        if ((!aHorizontal && !aVertical) || aStrength <= 0.0f || m_links.ptrs.empty()) {
            return;
        }
        const float relax = ez::math::clamp(aStrength, 0.0f, 1.0f);
        for (int32_t pass = 0; pass < 4; ++pass) {
            for (auto& linkPtr : m_links.ptrs) {
                if (!linkPtr->getDatas().enabled) {
                    continue;
                }
                auto fromPtr = linkPtr->getFromNode().lock();
                auto toPtr = linkPtr->getToNode().lock();
                if (fromPtr == nullptr || toPtr == nullptr) {
                    continue;
                }
                auto& datas1 = fromPtr->getDatasRef();
                auto& datas2 = toPtr->getDatasRef();
                if (!datas1.enabled || !datas2.enabled) {
                    continue;
                }
                const auto& linkDatas = linkPtr->getDatas();
                const ez::math::fvec2 attachFrom = datas1.pos + datas1.size * 0.5f + linkDatas.fromOffset;
                const ez::math::fvec2 attachTo = datas2.pos + datas2.size * 0.5f + linkDatas.toOffset;
                const float dx = attachTo.x - attachFrom.x;
                const float dy = attachTo.y - attachFrom.y;
                const bool makeHorizontal = (aHorizontal && aVertical) ? (std::abs(dx) >= std::abs(dy)) : aHorizontal;
                if (makeHorizontal) {
                    const float corr = dy * 0.5f * relax;  // bring both endpoints onto the same row
                    if (!datas1.locked && !datas2.locked) {
                        datas1.pos.y += corr;
                        datas2.pos.y -= corr;
                    } else if (!datas1.locked) {
                        datas1.pos.y += dy * relax;
                    } else if (!datas2.locked) {
                        datas2.pos.y -= dy * relax;
                    }
                } else {
                    const float corr = dx * 0.5f * relax;  // bring both endpoints onto the same column
                    if (!datas1.locked && !datas2.locked) {
                        datas1.pos.x += corr;
                        datas2.pos.x -= corr;
                    } else if (!datas1.locked) {
                        datas1.pos.x += dx * relax;
                    } else if (!datas2.locked) {
                        datas2.pos.x -= dx * relax;
                    }
                }
            }
        }
    }

    bool execComputeDatas() {
        bool change = false;
        assert(m_computeDatasFunctors.size() == m_computeDatas.weaks.size());
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
        const auto& pDatas = std::dynamic_pointer_cast <RepulseNodesDatas>(aDatas.lock());
        if (pDatas == nullptr || !pDatas->enabled) {
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
                // real edge-to-edge gap between the two rectangles (rect-aware, size-independent)
                const float halfWidth = (datas1.size.x + datas2.size.x) * 0.5f;
                const float halfHeight = (datas1.size.y + datas2.size.y) * 0.5f;
                const float gapX = std::max(std::abs(delta.x) - halfWidth, 0.0f);
                const float gapY = std::max(std::abs(delta.y) - halfHeight, 0.0f);
                const float actualGap = std::sqrt(gapX * gapX + gapY * gapY);
                const ez::math::fvec2 direction = delta / centerDist;
                ez::math::fvec2 force{};
                const float gapDeficit = pDatas->nodeGap - actualGap;
                if (gapDeficit > 0.0f) {
                    // closer than the minimum nodeGap : strong linear push to restore it. The stiffness
                    // is FIXED (independent of nodeRepulsion), so the minimum gap always holds whatever
                    // the spread strength or the link attraction.
                    force = direction * (gapDeficit * 10.0f);
                } else {
                    // far enough : plain inverse-square repulsion -> this is what nodeRepulsion spreads.
                    force = direction * (pDatas->nodeRepulsion / (centerDist * centerDist));
                }
                datas1.force -= force;
                datas2.force += force;
            }
        }
    }

    static void computeRepulseNodesFromLinks(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        const auto& pDatas = std::dynamic_pointer_cast <RepulseNodesFromLinksDatas>(aDatas.lock());
        if (pDatas == nullptr || !pDatas->enabled) {
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
                    const float desiredDistance = nodeRadius + pDatas->nodeGap * 0.5f;
                    const float penetration = desiredDistance - distance;
                    if (penetration > 0.0f) {
                        const ez::math::fvec2 direction = delta / distance;
                        nodeDatas.force += direction * penetration * pDatas->nodeToLinkRepulsion;
                    }
                }
            }
        }
    }

    static void computeAttractLinks(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        (void)aNodes;
        const auto& pDatas = std::dynamic_pointer_cast <AttractLinksDatas>(aDatas.lock());
        if (pDatas == nullptr || !pDatas->enabled) {
            return;
        }
        for (const auto& linkWeak : aLinks) {
            auto linkPtr = linkWeak.lock();
            if (linkPtr == nullptr) {
                continue;
            }
            const auto& linkDatas = linkPtr->getDatas();
            if (linkDatas.enabled) {
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
                // spring between the two endpoints : node center + per-endpoint offset (offset 0 = center).
                // the offset lets the host anchor a link to a "slot" without the solver knowing about slots.
                const ez::math::fvec2 attachFrom = datas1.pos + datas1.size * 0.5f + linkDatas.fromOffset;
                const ez::math::fvec2 attachTo = datas2.pos + datas2.size * 0.5f + linkDatas.toOffset;
                const ez::math::fvec2 delta = attachTo - attachFrom;
                const float distance = delta.length();
                if (distance < 1.0f) {
                    continue;
                }
                const float attraction = distance * pDatas->linkAttraction * linkDatas.weight;
                const ez::math::fvec2 direction = delta / distance;
                datas1.force += direction * attraction;
                datas2.force -= direction * attraction;
            }
        }
    }

    static void computeAlignLinks(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        (void)aNodes;
        const auto& pDatas = std::dynamic_pointer_cast <AlignLinksDatas>(aDatas.lock());
        if (pDatas == nullptr || !pDatas->enabled) {
            return;
        }
        const bool alignH = pDatas->alignHorizontal;
        const bool alignV = pDatas->alignVertical;
        if (!alignH && !alignV) {
            return;
        }
        for (const auto& linkWeak : aLinks) {
            auto linkPtr = linkWeak.lock();
            if (linkPtr == nullptr) {
                continue;
            }
            const auto& linkDatas = linkPtr->getDatas();
            if (linkDatas.enabled) {
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
                // same endpoints as the attraction : node center + per-endpoint offset
                const ez::math::fvec2 attachFrom = datas1.pos + datas1.size * 0.5f + linkDatas.fromOffset;
                const ez::math::fvec2 attachTo = datas2.pos + datas2.size * 0.5f + linkDatas.toOffset;
                const ez::math::fvec2 delta = attachTo - attachFrom;
                // pick the target axis : a single enabled axis forces it, both enabled -> nearest axis
                const bool makeHorizontal = (alignH && alignV) ? (std::abs(delta.x) >= std::abs(delta.y)) : alignH;
                if (makeHorizontal) {
                    // close the vertical gap -> the link becomes horizontal
                    const float pull = delta.y * pDatas->strength;
                    datas1.force.y += pull;
                    datas2.force.y -= pull;
                } else {
                    // close the horizontal gap -> the link becomes vertical
                    const float pull = delta.x * pDatas->strength;
                    datas1.force.x += pull;
                    datas2.force.x -= pull;
                }
            }
        }
    }

    static void computeSnapToGrid(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        (void)aLinks;
        const auto& pDatas = std::dynamic_pointer_cast <SnapToGridDatas>(aDatas.lock());
        if (pDatas == nullptr || !pDatas->enabled || pDatas->snapGridSpacing <= 0.0f) {
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
            const float nearestX = std::round(center.x / pDatas->snapGridSpacing) * pDatas->snapGridSpacing;
            const float nearestY = std::round(center.y / pDatas->snapGridSpacing) * pDatas->snapGridSpacing;
            const float dx = nearestX - center.x;
            const float dy = nearestY - center.y;
            // force proportional to distance, but cut off beyond half a cell so far nodes are not pulled
            const float halfGrid = pDatas->snapGridSpacing * 0.5f;
            const float fx = (std::abs(dx) < halfGrid) ? dx * pDatas->snapGridStrength : 0.0f;
            const float fy = (std::abs(dy) < halfGrid) ? dy * pDatas->snapGridStrength : 0.0f;
            nodeDatas.force += ez::math::fvec2(fx, fy);
        }
    }

    static void computeCentroidGravity(const NodeContainer& aNodes, const LinkContainer& aLinks, const IComputeDatasWeak& aDatas) {
        (void)aLinks;
        const auto& pDatas = std::dynamic_pointer_cast <CentroidGravityDatas>(aDatas.lock());
        if (pDatas == nullptr || !pDatas->enabled) {
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
        pDatas->centroid = centroid;  // exposed to the host through the registered datas
        // anchor the whole centroid toward the configured anchor point
        const ez::math::fvec2 toAnchor = pDatas->anchorPoint - centroid;
        for (const auto& nodeWeak : aNodes) {
            auto nodePtr = nodeWeak.lock();
            if (nodePtr == nullptr) {
                continue;
            }
            auto& nodeDatas = nodePtr->getDatasRef();
            if (nodeDatas.enabled && !nodeDatas.locked) {
                // local gravity toward the centroid
                const ez::math::fvec2 nodeCenter = nodeDatas.pos + nodeDatas.size * 0.5f;
                nodeDatas.force += (centroid - nodeCenter) * pDatas->gravity;
                // global anchor toward the anchor point
                nodeDatas.force += toAnchor * pDatas->anchorStrength;
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
