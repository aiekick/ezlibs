#include "TestEzFdGraph.h"
#include <ezlibs/ezFdGraph.hpp>
#include <ezlibs/ezCTest.hpp>

#include <cmath>
#include <string>
#include <vector>
#include <cstdint>

////////////////////////////////////////////////////////////////////////////
//// HELPERS ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

static bool nearF(float aValue, float aRef, float aEpsilon) {
    return std::abs(aValue - aRef) < aEpsilon;
}

// Host-side payloads and a node subclass, mirroring how the cdpViewer will
// derive its own CdpNode / CdpLink from ez::FdGraph::Node / Link.
struct MyNodeDatas : public ez::FdGraph::NodeDatas {
    int32_t tag{0};
    MyNodeDatas() = default;
};

struct MyLinkDatas : public ez::FdGraph::LinkDatas {
    std::string label;
    MyLinkDatas() = default;
};

class CountingNode : public ez::FdGraph::Node {
private:
    int32_t m_updateCount{0};

public:
    template <typename T>
    explicit CountingNode(const T& aDatas) : Node(aDatas) {}
    void update(float aDeltaTime, float aDamping) override {
        ++m_updateCount;
        Node::update(aDeltaTime, aDamping);
    }
    int32_t getUpdateCount() const { return m_updateCount; }
};

////////////////////////////////////////////////////////////////////////////
//// TESTS //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzFdGraph_AddNodesAndLinks() {
    ez::FdGraph graph;
    CTEST_ASSERT(graph.getNodes().empty());
    CTEST_ASSERT(graph.getLinks().empty());

    auto n0 = graph.addNode(ez::FdGraph::NodeDatas());
    auto n1 = graph.addNode(ez::FdGraph::NodeDatas());
    CTEST_ASSERT(graph.getNodes().size() == 2U);
    CTEST_ASSERT(!n0.expired());
    CTEST_ASSERT(!n1.expired());
    CTEST_ASSERT(n0.lock()->getDatas().connCount == 0U);

    auto l0 = graph.addLink(n0, n1);
    CTEST_ASSERT(graph.getLinks().size() == 1U);
    CTEST_ASSERT(!l0.expired());
    // addLink increments connCount on both endpoints
    CTEST_ASSERT(n0.lock()->getDatas().connCount == 1U);
    CTEST_ASSERT(n1.lock()->getDatas().connCount == 1U);

    // an expired endpoint yields no link
    ez::FdGraph::NodeWeak dead;
    auto bad = graph.addLink(dead, n1);
    CTEST_ASSERT(bad.expired());
    CTEST_ASSERT(graph.getLinks().size() == 1U);

    graph.clear();
    CTEST_ASSERT(graph.getNodes().empty());
    CTEST_ASSERT(graph.getLinks().empty());
    return true;
}

bool TestEzFdGraph_InitScatter() {
    ez::FdGraph graph;
    const size_t count = 6U;
    std::vector<ez::FdGraph::NodeWeak> nodes;
    for (size_t i = 0; i < count; ++i) {
        ez::FdGraph::NodeDatas datas;
        datas.size = ez::math::fvec2(80.0f, 40.0f);
        nodes.push_back(graph.addNode(datas));
    }

    graph.init();

    // every node got a non-origin position
    for (size_t i = 0; i < count; ++i) {
        const auto& pos = nodes[i].lock()->getDatas().pos;
        CTEST_ASSERT(!(pos.x == 0.0f && pos.y == 0.0f));
    }
    // two different angular slots -> different positions
    const auto& p0 = nodes[0].lock()->getDatas().pos;
    const auto& p1 = nodes[1].lock()->getDatas().pos;
    CTEST_ASSERT((p0.x != p1.x) || (p0.y != p1.y));
    return true;
}

bool TestEzFdGraph_EnergyDecreases() {
    ez::FdGraph graph;
    auto& cfg = graph.getConfigRef();
    // isolate the node/node repulsion
    cfg.enableRepulseNodesFromLinks = false;
    cfg.enableAttractLinks = false;
    cfg.enableSnapToGrid = false;
    cfg.enableCentroidGravity = false;

    ez::FdGraph::NodeDatas datas;
    datas.size = ez::math::fvec2(40.0f, 40.0f);
    datas.pos = ez::math::fvec2(-5.0f, 0.0f);
    graph.addNode(datas);
    datas.pos = ez::math::fvec2(5.0f, 0.0f);
    graph.addNode(datas);

    const float deltaTime = 0.05f;
    const float firstEnergy = graph.step(deltaTime);
    CTEST_ASSERT(firstEnergy > 0.0f);

    float lastEnergy = firstEnergy;
    bool decreasedOverall = false;
    for (int32_t i = 0; i < 500; ++i) {
        lastEnergy = graph.step(deltaTime);
        if (lastEnergy < firstEnergy) {
            decreasedOverall = true;
        }
    }
    // as the nodes drift apart the inverse-square repulsion weakens -> energy drops
    CTEST_ASSERT(decreasedOverall);
    CTEST_ASSERT(lastEnergy >= 0.0f);
    CTEST_ASSERT(graph.getEnergy() == lastEnergy);
    return true;
}

bool TestEzFdGraph_LinkAttractionPullsTogether() {
    ez::FdGraph graph;
    auto& cfg = graph.getConfigRef();
    // isolate the link attraction (spring)
    cfg.enableRepulseNodes = false;
    cfg.enableRepulseNodesFromLinks = false;
    cfg.enableSnapToGrid = false;
    cfg.enableCentroidGravity = false;

    ez::FdGraph::NodeDatas datas;
    datas.size = ez::math::fvec2(20.0f, 20.0f);
    datas.pos = ez::math::fvec2(-200.0f, 0.0f);
    auto n0 = graph.addNode(datas);
    datas.pos = ez::math::fvec2(200.0f, 0.0f);
    auto n1 = graph.addNode(datas);
    graph.addLink(n0, n1);

    const float startDist = (n1.lock()->getDatas().pos - n0.lock()->getDatas().pos).length();
    for (int32_t i = 0; i < 300; ++i) {
        graph.step(0.1f);
    }
    const float endDist = (n1.lock()->getDatas().pos - n0.lock()->getDatas().pos).length();
    CTEST_ASSERT(endDist < startDist);
    return true;
}

bool TestEzFdGraph_UpdateLinksCorners() {
    ez::FdGraph graph;
    ez::FdGraph::NodeDatas d0;
    d0.pos = ez::math::fvec2(0.0f, 0.0f);
    d0.size = ez::math::fvec2(100.0f, 60.0f);
    d0.slots_y = {10.0f, 30.0f, 50.0f};
    auto n0 = graph.addNode(d0);

    ez::FdGraph::NodeDatas d1;
    d1.pos = ez::math::fvec2(300.0f, 0.0f);
    d1.size = ez::math::fvec2(100.0f, 60.0f);
    d1.slots_y = {20.0f, 40.0f};
    auto n1 = graph.addNode(d1);

    ez::FdGraph::LinkDatas ld;
    ld.srcSlot = 2U;  // y = 0 + 50
    ld.dstSlot = 0U;  // y = 0 + 20
    graph.addLink(n0, n1, ld);

    graph.updateLinks();
    const auto& corners = graph.getLinks()[0]->getDatas().corners;
    CTEST_ASSERT(corners.size() == 2U);
    // centered slots : x sits at the node center
    CTEST_ASSERT(nearF(corners[0].x, 50.0f, 1e-3f));
    CTEST_ASSERT(nearF(corners[0].y, 50.0f, 1e-3f));
    CTEST_ASSERT(nearF(corners[1].x, 350.0f, 1e-3f));
    CTEST_ASSERT(nearF(corners[1].y, 20.0f, 1e-3f));

    // sideSlots : target is to the right -> src exits its right edge, dst enters its left edge
    graph.getConfigRef().sideSlots = true;
    graph.updateLinks();
    const auto& sideCorners = graph.getLinks()[0]->getDatas().corners;
    CTEST_ASSERT(nearF(sideCorners[0].x, 100.0f, 1e-3f));
    CTEST_ASSERT(nearF(sideCorners[1].x, 300.0f, 1e-3f));
    return true;
}

bool TestEzFdGraph_LockedNodeStaysPut() {
    ez::FdGraph graph;
    ez::FdGraph::NodeDatas d0;
    d0.pos = ez::math::fvec2(123.0f, -45.0f);
    d0.size = ez::math::fvec2(40.0f, 40.0f);
    d0.locked = true;
    auto n0 = graph.addNode(d0);

    ez::FdGraph::NodeDatas d1;
    d1.pos = ez::math::fvec2(0.0f, 0.0f);
    d1.size = ez::math::fvec2(40.0f, 40.0f);
    auto n1 = graph.addNode(d1);
    graph.addLink(n0, n1);

    for (int32_t i = 0; i < 100; ++i) {
        graph.step(0.1f);
    }
    // the locked node never moved
    const auto& lockedPos = n0.lock()->getDatas().pos;
    CTEST_ASSERT(nearF(lockedPos.x, 123.0f, 1e-4f));
    CTEST_ASSERT(nearF(lockedPos.y, -45.0f, 1e-4f));
    // the free node did move
    const auto& freePos = n1.lock()->getDatas().pos;
    CTEST_ASSERT(!(freePos.x == 0.0f && freePos.y == 0.0f));
    return true;
}

bool TestEzFdGraph_DisabledForcesNoMotion() {
    ez::FdGraph graph;
    auto& cfg = graph.getConfigRef();
    cfg.enableRepulseNodes = false;
    cfg.enableRepulseNodesFromLinks = false;
    cfg.enableAttractLinks = false;
    cfg.enableSnapToGrid = false;
    cfg.enableCentroidGravity = false;

    ez::FdGraph::NodeDatas d0;
    d0.pos = ez::math::fvec2(10.0f, 20.0f);
    d0.size = ez::math::fvec2(40.0f, 40.0f);
    auto n0 = graph.addNode(d0);

    ez::FdGraph::NodeDatas d1;
    d1.pos = ez::math::fvec2(60.0f, 80.0f);
    d1.size = ez::math::fvec2(40.0f, 40.0f);
    auto n1 = graph.addNode(d1);
    graph.addLink(n0, n1);

    float energy = 0.0f;
    for (int32_t i = 0; i < 50; ++i) {
        energy = graph.step(0.1f);
    }
    // no force enabled -> no energy and no motion
    CTEST_ASSERT(nearF(energy, 0.0f, 1e-6f));
    CTEST_ASSERT(nearF(n0.lock()->getDatas().pos.x, 10.0f, 1e-4f));
    CTEST_ASSERT(nearF(n0.lock()->getDatas().pos.y, 20.0f, 1e-4f));
    CTEST_ASSERT(nearF(n1.lock()->getDatas().pos.x, 60.0f, 1e-4f));
    CTEST_ASSERT(nearF(n1.lock()->getDatas().pos.y, 80.0f, 1e-4f));
    return true;
}

bool TestEzFdGraph_ClampForces() {
    ez::FdGraph graph;
    auto& cfg = graph.getConfigRef();
    cfg.enableRepulseNodesFromLinks = false;
    cfg.enableAttractLinks = false;
    cfg.enableSnapToGrid = false;
    cfg.enableCentroidGravity = false;
    cfg.nodeRepulsion = 1.0e9f;  // make the repulsion explode
    cfg.maxForce = 7.5f;         // then clamp it hard

    ez::FdGraph::NodeDatas datas;
    datas.size = ez::math::fvec2(10.0f, 10.0f);
    datas.pos = ez::math::fvec2(0.0f, 0.0f);
    graph.addNode(datas);
    datas.pos = ez::math::fvec2(3.0f, 4.0f);  // close -> huge repulsion
    graph.addNode(datas);

    graph.step(0.1f);
    for (const auto& nodePtr : graph.getNodes()) {
        const auto& force = nodePtr->getDatas().force;
        CTEST_ASSERT(std::abs(force.x) <= cfg.maxForce + 1e-4f);
        CTEST_ASSERT(std::abs(force.y) <= cfg.maxForce + 1e-4f);
    }
    return true;
}

bool TestEzFdGraph_DerivedDatasAndOverride() {
    ez::FdGraph graph;
    MyNodeDatas d0;
    d0.tag = 42;
    d0.size = ez::math::fvec2(30.0f, 30.0f);
    d0.pos = ez::math::fvec2(-10.0f, 0.0f);
    MyNodeDatas d1;
    d1.tag = 7;
    d1.size = ez::math::fvec2(30.0f, 30.0f);
    d1.pos = ez::math::fvec2(10.0f, 0.0f);

    auto n0 = graph.addNode<CountingNode>(d0);
    auto n1 = graph.addNode<CountingNode>(d1);
    CTEST_ASSERT(!n0.expired());
    CTEST_ASSERT(!n1.expired());
    // derived datas preserved and reachable, base view still works
    CTEST_ASSERT(n0.lock()->getDatas<MyNodeDatas>().tag == 42);
    CTEST_ASSERT(n1.lock()->getDatas<MyNodeDatas>().tag == 7);
    CTEST_ASSERT(n0.lock()->getDatas().size.x == 30.0f);

    // derived link datas
    MyLinkDatas ld;
    ld.label = "edge";
    auto l0 = graph.addLink<ez::FdGraph::Link>(n0, n1, ld);
    CTEST_ASSERT(!l0.expired());
    CTEST_ASSERT(graph.getLinks()[0]->getDatas<MyLinkDatas>().label == "edge");

    // virtual update() is dispatched through the base Node pointer held by the graph
    graph.step(0.1f);
    CTEST_ASSERT(n0.lock()->getUpdateCount() == 1);
    CTEST_ASSERT(n1.lock()->getUpdateCount() == 1);
    return true;
}

bool TestEzFdGraph_DisabledNodeIsInert() {
    ez::FdGraph graph;
    auto& cfg = graph.getConfigRef();
    cfg.enableRepulseNodesFromLinks = false;
    cfg.enableAttractLinks = false;
    cfg.enableSnapToGrid = false;
    cfg.enableCentroidGravity = false;  // isolate node/node repulsion

    ez::FdGraph::NodeDatas datas;
    datas.size = ez::math::fvec2(40.0f, 40.0f);
    datas.pos = ez::math::fvec2(0.0f, 0.0f);
    auto n0 = graph.addNode(datas);  // enabled
    datas.pos = ez::math::fvec2(5.0f, 0.0f);
    datas.enabled = false;           // disabled, very close : would repel n0 if active
    auto n1 = graph.addNode(datas);

    for (int32_t i = 0; i < 50; ++i) {
        graph.step(0.1f);
    }
    // the disabled node never moves, and being the only neighbour it must exert
    // no force on the enabled node (which therefore does not move either)
    CTEST_ASSERT(nearF(n1.lock()->getDatas().pos.x, 5.0f, 1e-4f));
    CTEST_ASSERT(nearF(n1.lock()->getDatas().pos.y, 0.0f, 1e-4f));
    CTEST_ASSERT(nearF(n0.lock()->getDatas().pos.x, 0.0f, 1e-4f));
    CTEST_ASSERT(nearF(n0.lock()->getDatas().pos.y, 0.0f, 1e-4f));

    // re-enable the second node : the pair now repels and they move apart
    n1.lock()->getDatasRef().enabled = true;
    const float beforeDist = (n1.lock()->getDatas().pos - n0.lock()->getDatas().pos).length();
    for (int32_t i = 0; i < 50; ++i) {
        graph.step(0.1f);
    }
    const float afterDist = (n1.lock()->getDatas().pos - n0.lock()->getDatas().pos).length();
    CTEST_ASSERT(afterDist > beforeDist);
    return true;
}

////////////////////////////////////////////////////////////////////////////
//// DISPATCH ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzFdGraph(const std::string& vTest) {
    IfTestExist(TestEzFdGraph_AddNodesAndLinks);
    else IfTestExist(TestEzFdGraph_InitScatter);
    else IfTestExist(TestEzFdGraph_EnergyDecreases);
    else IfTestExist(TestEzFdGraph_LinkAttractionPullsTogether);
    else IfTestExist(TestEzFdGraph_UpdateLinksCorners);
    else IfTestExist(TestEzFdGraph_LockedNodeStaysPut);
    else IfTestExist(TestEzFdGraph_DisabledForcesNoMotion);
    else IfTestExist(TestEzFdGraph_ClampForces);
    else IfTestExist(TestEzFdGraph_DerivedDatasAndOverride);
    else IfTestExist(TestEzFdGraph_DisabledNodeIsInert);
    return false;
}
