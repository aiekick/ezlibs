#include <TestEzBilayerCrossings.h>

#include <vector>
#include <utility>
#include <cstdint>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezDiagram/ezBilayerCrossings.hpp>

////////////////////////////////////////////////////////////////////////////

// no edge -> no crossing
bool TestEzBilayerCrossings_Empty() {
    std::vector<std::pair<int32_t, int32_t> > edges;
    CTEST_ASSERT(ez::diagram::countBilayerCrossings(edges, 4) == 0U);
    return true;
}

// a single edge cannot cross anything
bool TestEzBilayerCrossings_SingleEdge() {
    std::vector<std::pair<int32_t, int32_t> > edges;
    edges.push_back(std::make_pair(0, 0));
    CTEST_ASSERT(ez::diagram::countBilayerCrossings(edges, 1) == 0U);
    return true;
}

// two edges reaching the same south vertex from different north vertices never cross
bool TestEzBilayerCrossings_ParallelNoCross() {
    std::vector<std::pair<int32_t, int32_t> > edges;
    edges.push_back(std::make_pair(0, 0));
    edges.push_back(std::make_pair(1, 0));
    CTEST_ASSERT(ez::diagram::countBilayerCrossings(edges, 1) == 0U);
    return true;
}

// north {0,1} -> south {0,1}, parallel (nested) : no crossing
bool TestEzBilayerCrossings_K22Nested() {
    std::vector<std::pair<int32_t, int32_t> > edges;
    edges.push_back(std::make_pair(0, 0));
    edges.push_back(std::make_pair(1, 1));
    CTEST_ASSERT(ez::diagram::countBilayerCrossings(edges, 2) == 0U);
    return true;
}

// north {0,1} -> south {1,0}, swapped : exactly one crossing
bool TestEzBilayerCrossings_K22Crossed() {
    std::vector<std::pair<int32_t, int32_t> > edges;
    edges.push_back(std::make_pair(0, 1));
    edges.push_back(std::make_pair(1, 0));
    CTEST_ASSERT(ez::diagram::countBilayerCrossings(edges, 2) == 1U);
    return true;
}

// north i -> south (n-1-i) : fully reversed, n*(n-1)/2 crossings (n = 5 -> 10)
bool TestEzBilayerCrossings_FullyReversed() {
    const int32_t count = 5;
    std::vector<std::pair<int32_t, int32_t> > edges;
    for (int32_t index = 0; index < count; ++index) {
        edges.push_back(std::make_pair(index, count - 1 - index));
    }
    CTEST_ASSERT(ez::diagram::countBilayerCrossings(edges, count) == 10U);
    return true;
}

// a north vertex with two edges + ordering that forces a single crossing with a third edge.
// north0 -> south0 and south2 ; north1 -> south1. The (north0->south2, north1->south1) pair crosses.
bool TestEzBilayerCrossings_MultiEdgePerVertex() {
    std::vector<std::pair<int32_t, int32_t> > edges;
    edges.push_back(std::make_pair(0, 0));
    edges.push_back(std::make_pair(0, 2));
    edges.push_back(std::make_pair(1, 1));
    CTEST_ASSERT(ez::diagram::countBilayerCrossings(edges, 3) == 1U);
    return true;
}

////////////////////////////////////////////////////////////////////////////

bool TestEzBilayerCrossings(const std::string& vTest) {
    IfTestExist(TestEzBilayerCrossings_Empty);
    else IfTestExist(TestEzBilayerCrossings_SingleEdge);
    else IfTestExist(TestEzBilayerCrossings_ParallelNoCross);
    else IfTestExist(TestEzBilayerCrossings_K22Nested);
    else IfTestExist(TestEzBilayerCrossings_K22Crossed);
    else IfTestExist(TestEzBilayerCrossings_FullyReversed);
    else IfTestExist(TestEzBilayerCrossings_MultiEdgePerVertex);
    return false;
}
