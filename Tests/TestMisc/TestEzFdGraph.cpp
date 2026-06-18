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

// A host-defined force : pushes every node along +X by a tunable amount and counts
// its own calls, to exercise registerFunctor / datas retrieval / clearFunctors.
struct PushForceDatas : public ez::FdGraph::IComputeDatas {
    float push{1.0f};
    int32_t callCount{0};
};

static void pushForce(const ez::FdGraph::NodeContainer& aNodes, const ez::FdGraph::LinkContainer& aLinks, const ez::FdGraph::IComputeDatasWeak& aDatas) {
    (void)aLinks;
    auto pDatas = aDatas.lock();
    if (pDatas == nullptr) {
        return;
    }
    auto& datas = static_cast<PushForceDatas&>(*pDatas);
    ++datas.callCount;
    for (const auto& nodeWeak : aNodes) {
        auto nodePtr = nodeWeak.lock();
        if (nodePtr != nullptr) {
            nodePtr->getDatasRef().force += ez::math::fvec2(datas.push, 0.0f);
        }
    }
}

// Disable every built-in force except the ones the test wants to isolate.
static void disableAllForcesExcept(const ez::FdGraph::DefaultFunctors& aDef, bool aRepulseNodes, bool aRepulseNodesFromLinks, bool aAttractLinks, bool aSnapToGrid, bool aCentroidGravity, bool aFlowLayout) {
    aDef.repulseNodes->enabled = aRepulseNodes;
    aDef.repulseNodesFromLinks->enabled = aRepulseNodesFromLinks;
    aDef.attractLinks->enabled = aAttractLinks;
    aDef.snapToGrid->enabled = aSnapToGrid;
    aDef.centroidGravity->enabled = aCentroidGravity;
    aDef.flowLayout->enabled = aFlowLayout;
}

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

    graph.clearDatas();
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

    graph.initSimulation();

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
    const auto& def = graph.initDefaultComputeFunctors();
    // isolate the node/node repulsion
    disableAllForcesExcept(def, true, false, false, false, false, false);

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
    const auto& def = graph.initDefaultComputeFunctors();
    // isolate the link attraction (spring)
    disableAllForcesExcept(def, false, false, true, false, false, false);

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
    const auto& corners = graph.getLinks()[0].lock()->getDatas().corners;
    CTEST_ASSERT(corners.size() == 2U);
    // centered slots : x sits at the node center
    CTEST_ASSERT(nearF(corners[0].x, 50.0f, 1e-3f));
    CTEST_ASSERT(nearF(corners[0].y, 50.0f, 1e-3f));
    CTEST_ASSERT(nearF(corners[1].x, 350.0f, 1e-3f));
    CTEST_ASSERT(nearF(corners[1].y, 20.0f, 1e-3f));

    // sideSlots : target is to the right -> src exits its right edge, dst enters its left edge
    graph.getConfigRef().sideSlots = true;
    graph.updateLinks();
    const auto& sideCorners = graph.getLinks()[0].lock()->getDatas().corners;
    CTEST_ASSERT(nearF(sideCorners[0].x, 100.0f, 1e-3f));
    CTEST_ASSERT(nearF(sideCorners[1].x, 300.0f, 1e-3f));
    return true;
}

bool TestEzFdGraph_LockedNodeStaysPut() {
    ez::FdGraph graph;
    graph.initDefaultComputeFunctors();
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
    const auto& def = graph.initDefaultComputeFunctors();
    // every force off -> the solver must be inert
    disableAllForcesExcept(def, false, false, false, false, false, false);

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
    const auto& def = graph.initDefaultComputeFunctors();
    disableAllForcesExcept(def, true, false, false, false, false, false);
    def.repulseNodes->nodeRepulsion = 1.0e9f;  // make the repulsion explode
    const float maxForce = 7.5f;
    graph.getConfigRef().maxForce = maxForce;  // then clamp it hard

    ez::FdGraph::NodeDatas datas;
    datas.size = ez::math::fvec2(10.0f, 10.0f);
    datas.pos = ez::math::fvec2(0.0f, 0.0f);
    graph.addNode(datas);
    datas.pos = ez::math::fvec2(3.0f, 4.0f);  // close -> huge repulsion
    graph.addNode(datas);

    graph.step(0.1f);
    for (const auto& nodePtr : graph.getNodes()) {
        const auto& force = nodePtr.lock()->getDatas().force;
        CTEST_ASSERT(std::abs(force.x) <= maxForce + 1e-4f);
        CTEST_ASSERT(std::abs(force.y) <= maxForce + 1e-4f);
    }
    return true;
}

bool TestEzFdGraph_DerivedDatasAndOverride() {
    ez::FdGraph graph;
    graph.initDefaultComputeFunctors();
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
    CTEST_ASSERT(graph.getLinks()[0].lock()->getDatas<MyLinkDatas>().label == "edge");

    // virtual update() is dispatched through the base Node pointer held by the graph
    graph.step(0.1f);
    CTEST_ASSERT(n0.lock()->getUpdateCount() == 1);
    CTEST_ASSERT(n1.lock()->getUpdateCount() == 1);
    return true;
}

bool TestEzFdGraph_DisabledNodeIsInert() {
    ez::FdGraph graph;
    const auto& def = graph.initDefaultComputeFunctors();
    disableAllForcesExcept(def, true, false, false, false, false, false);  // isolate node/node repulsion

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

bool TestEzFdGraph_FlowLayoutOrdersByDirection() {
    ez::FdGraph graph;
    const auto& def = graph.initDefaultComputeFunctors();
    // isolate the flow layout : every other force off
    disableAllForcesExcept(def, false, false, false, false, false, true);

    ez::FdGraph::NodeDatas datas;
    datas.size = ez::math::fvec2(40.0f, 40.0f);
    datas.slots_y = {0.0f};
    datas.slots_output = {true};                // node 0 is wired through an OUTPUT slot
    datas.pos = ez::math::fvec2(100.0f, 0.0f);  // starts on the RIGHT (reversed)
    auto outputNode = graph.addNode(datas);
    datas.slots_output = {false};               // node 1 is wired through an INPUT slot
    datas.pos = ez::math::fvec2(0.0f, 0.0f);
    auto inputNode = graph.addNode(datas);

    ez::FdGraph::LinkDatas ld;
    ld.srcSlot = 0U;
    ld.dstSlot = 0U;
    graph.addLink(outputNode, inputNode, ld);

    for (int32_t i = 0; i < 300; ++i) {
        graph.step(0.1f);
    }
    // the OUTPUT-side node ends up left of the INPUT-side node
    CTEST_ASSERT(outputNode.lock()->getDatas().pos.x < inputNode.lock()->getDatas().pos.x);
    return true;
}

bool TestEzFdGraph_FlowLayoutFollowsSlotNotLinkOrder() {
    ez::FdGraph graph;
    const auto& def = graph.initDefaultComputeFunctors();
    disableAllForcesExcept(def, false, false, false, false, false, true);

    ez::FdGraph::NodeDatas datas;
    datas.size = ez::math::fvec2(40.0f, 40.0f);
    datas.slots_y = {0.0f};
    // the link is created INPUT-node -> OUTPUT-node (reversed link order) : the flow must
    // still put the OUTPUT node on the left, driven only by the slot direction.
    datas.slots_output = {false};               // node 0 : INPUT side, used as the link 'from'
    datas.pos = ez::math::fvec2(0.0f, 0.0f);    // starts on the LEFT
    auto inputNode = graph.addNode(datas);
    datas.slots_output = {true};                // node 1 : OUTPUT side, used as the link 'to'
    datas.pos = ez::math::fvec2(100.0f, 0.0f);  // starts on the RIGHT
    auto outputNode = graph.addNode(datas);

    ez::FdGraph::LinkDatas ld;
    ld.srcSlot = 0U;  // from-node (inputNode) slot 0 -> an input
    ld.dstSlot = 0U;
    graph.addLink(inputNode, outputNode, ld);

    for (int32_t i = 0; i < 300; ++i) {
        graph.step(0.1f);
    }
    // despite the link going inputNode -> outputNode, the OUTPUT node ends up on the left
    CTEST_ASSERT(outputNode.lock()->getDatas().pos.x < inputNode.lock()->getDatas().pos.x);
    return true;
}

bool TestEzFdGraph_CustomFunctorRegisterAndTune() {
    ez::FdGraph graph;
    ez::FdGraph::NodeDatas datas;
    datas.size = ez::math::fvec2(10.0f, 10.0f);
    datas.pos = ez::math::fvec2(0.0f, 0.0f);
    graph.addNode(datas);  // step() needs at least two nodes to run
    datas.pos = ez::math::fvec2(100.0f, 0.0f);
    graph.addNode(datas);

    // register a host force and keep the typed reference to its datas
    auto& myDatas = graph.registerFunctor<PushForceDatas>(&pushForce);
    CTEST_ASSERT(graph.getComputeDatas().size() == 1U);
    CTEST_ASSERT(myDatas.callCount == 0);

    // a step runs the functor and the datas it consumes are reachable host-side
    graph.step(0.1f);
    CTEST_ASSERT(myDatas.callCount == 1);

    // tuning the datas host-side is seen by the functor on the next step
    myDatas.push = 0.0f;
    myDatas.callCount = 0;
    graph.step(0.1f);
    CTEST_ASSERT(myDatas.callCount == 1);

    // clearFunctors drops every functor AND its datas (the myDatas reference must not
    // be used after this point : the block it referred to has been destroyed)
    graph.clearFunctors();
    CTEST_ASSERT(graph.getComputeDatas().empty());
    CTEST_ASSERT(graph.getDefaultFunctors().repulseNodes == nullptr);

    // initDefaultComputeFunctors registers the six built-ins and exposes their handles
    const auto& def = graph.initDefaultComputeFunctors();
    CTEST_ASSERT(graph.getComputeDatas().size() == 6U);
    CTEST_ASSERT(def.repulseNodes != nullptr);
    CTEST_ASSERT(def.centroidGravity != nullptr);
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
    else IfTestExist(TestEzFdGraph_FlowLayoutOrdersByDirection);
    else IfTestExist(TestEzFdGraph_FlowLayoutFollowsSlotNotLinkOrder);
    else IfTestExist(TestEzFdGraph_CustomFunctorRegisterAndTune);
    return false;
}
