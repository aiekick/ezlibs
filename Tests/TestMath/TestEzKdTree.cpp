#include <ezlibs/ezMath/ezKdTree.hpp>
#include <ezlibs/ezMath/ezMath.hpp>
#include <ezlibs/ezCTest.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4305)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

////////////////////////////////////////////////////////////////////////////
// Helpers
////////////////////////////////////////////////////////////////////////////

namespace {

// Brute-force reference: compute the k-NN by scanning every point.
// Used to validate the kdTree results on small sets where the exact
// answer is easy to compute.
struct bruteHit {
    size_t index;
    float squaredDistance;
};

float squaredDistanceL2(const std::vector<float>& a, const std::vector<float>& b) {
    float total = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        const float d = a[i] - b[i];
        total += d * d;
    }
    return total;
}

std::vector<bruteHit> bruteForceNearest(
    const std::vector<std::vector<float>>& aPoints,
    const std::vector<float>& aQuery,
    size_t aK) {
    std::vector<bruteHit> all;
    all.reserve(aPoints.size());
    for (size_t i = 0; i < aPoints.size(); ++i) {
        all.push_back({i, squaredDistanceL2(aPoints[i], aQuery)});
    }
    std::sort(all.begin(), all.end(),
              [](const bruteHit& a, const bruteHit& b) { return a.squaredDistance < b.squaredDistance; });
    if (all.size() > aK) {
        all.resize(aK);
    }
    return all;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzKdTree_DefaultEmpty() {
    ez::math::kdTree<int> tree(3);
    CTEST_ASSERT(tree.size() == 0);
    CTEST_ASSERT(tree.empty());
    CTEST_ASSERT(tree.dimension() == 3);
    return true;
}

bool TestEzKdTree_PutWrongDimensionIsRejected() {
    ez::math::kdTree<int> tree(3);
    tree.put(std::vector<float>{1.0f, 2.0f}, 42);  // 2-D point in a 3-D tree
    CTEST_ASSERT(tree.size() == 0);
    return true;
}

bool TestEzKdTree_SinglePointFoundExactly() {
    ez::math::kdTree<int> tree(3);
    tree.put(std::vector<float>{1.0f, 2.0f, 3.0f}, 42);

    std::vector<ez::math::kdTree<int>::hit> hits;
    tree.nearest(std::vector<float>{1.0f, 2.0f, 3.0f},
                 std::numeric_limits<float>::max(),
                 100, 1, hits);
    CTEST_ASSERT(hits.size() == 1);
    CTEST_ASSERT(hits[0].value == 42);
    CTEST_ASSERT(ez::math::isEqual(hits[0].squaredDistance, 0.0f, 1e-6f));
    return true;
}

bool TestEzKdTree_NearestRespectsMaxDistance() {
    ez::math::kdTree<int> tree(2);
    tree.put(std::vector<float>{0.0f, 0.0f}, 1);
    tree.put(std::vector<float>{10.0f, 10.0f}, 2);

    std::vector<ez::math::kdTree<int>::hit> hits;
    tree.nearest(std::vector<float>{0.0f, 0.0f}, 1.0f, 100, 5, hits);
    // Only the (0, 0) point fits within squared distance 1.0
    CTEST_ASSERT(hits.size() == 1);
    CTEST_ASSERT(hits[0].value == 1);
    return true;
}

bool TestEzKdTree_KNearestReturnsKBestSorted() {
    ez::math::kdTree<int> tree(2);
    tree.put(std::vector<float>{0.0f, 0.0f}, 0);
    tree.put(std::vector<float>{1.0f, 0.0f}, 1);
    tree.put(std::vector<float>{2.0f, 0.0f}, 2);
    tree.put(std::vector<float>{3.0f, 0.0f}, 3);

    std::vector<ez::math::kdTree<int>::hit> hits;
    tree.nearest(std::vector<float>{0.0f, 0.0f},
                 std::numeric_limits<float>::max(),
                 100, 2, hits);
    CTEST_ASSERT(hits.size() == 2);
    // Closest then second-closest (for the Lowe ratio test downstream)
    CTEST_ASSERT(hits[0].value == 0);
    CTEST_ASSERT(hits[1].value == 1);
    CTEST_ASSERT(hits[0].squaredDistance < hits[1].squaredDistance);
    return true;
}

bool TestEzKdTree_AgreesWithBruteForceOnSmallSet() {
    // Build a small 3-D point cloud and check that the tree returns the
    // same top-3 as a brute-force scan, for several queries.
    std::vector<std::vector<float>> points = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
        {-2.0f, 3.0f, 0.5f},
        {5.0f, 5.0f, 5.0f},
        {0.5f, 0.5f, 0.5f},
        {3.0f, -1.0f, 2.0f},
        {-1.0f, -1.0f, -1.0f},
        {2.5f, 2.5f, 2.5f},
        {0.1f, 0.1f, 0.1f},
        {4.0f, 4.0f, 4.0f},
    };
    ez::math::kdTree<size_t> tree(3);
    for (size_t i = 0; i < points.size(); ++i) {
        tree.put(points[i], i);
    }

    const std::vector<std::vector<float>> queries = {
        {0.0f, 0.0f, 0.0f},
        {2.5f, 2.5f, 2.5f},
        {-3.0f, 4.0f, 0.0f},
    };
    for (const auto& query : queries) {
        std::vector<ez::math::kdTree<size_t>::hit> treeHits;
        tree.nearest(query, std::numeric_limits<float>::max(), 100, 3, treeHits);
        const auto bruteHits = bruteForceNearest(points, query, 3);
        CTEST_ASSERT(treeHits.size() == bruteHits.size());
        for (size_t i = 0; i < treeHits.size(); ++i) {
            CTEST_ASSERT(treeHits[i].value == bruteHits[i].index);
            CTEST_ASSERT(ez::math::isEqual(treeHits[i].squaredDistance, bruteHits[i].squaredDistance, 1e-5f));
        }
    }
    return true;
}

bool TestEzKdTree_DepthGrowsAfterSplit() {
    // Insert more points than k_leafCapacity to force at least one split.
    ez::math::kdTree<int> tree(2);
    for (size_t i = 0; i < 100; ++i) {
        const float x = static_cast<float>(i);
        tree.put(std::vector<float>{x, x * 0.5f}, static_cast<int>(i));
    }
    CTEST_ASSERT(tree.size() == 100);
    CTEST_ASSERT(tree.depth() >= 2);
    return true;
}

bool TestEzKdTree_NearestOnEmptyReturnsNothing() {
    ez::math::kdTree<int> tree(3);
    std::vector<ez::math::kdTree<int>::hit> hits;
    tree.nearest(std::vector<float>{0.0f, 0.0f, 0.0f},
                 std::numeric_limits<float>::max(),
                 100, 3, hits);
    CTEST_ASSERT(hits.empty());
    return true;
}

bool TestEzKdTree_NearestRespectsMaxElementsZero() {
    ez::math::kdTree<int> tree(2);
    tree.put(std::vector<float>{0.0f, 0.0f}, 1);
    std::vector<ez::math::kdTree<int>::hit> hits;
    tree.nearest(std::vector<float>{0.0f, 0.0f},
                 std::numeric_limits<float>::max(),
                 100, 0, hits);
    CTEST_ASSERT(hits.empty());
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzKdTree(const std::string& vTest) {
    IfTestExist(TestEzKdTree_DefaultEmpty);
    else IfTestExist(TestEzKdTree_PutWrongDimensionIsRejected);
    else IfTestExist(TestEzKdTree_SinglePointFoundExactly);
    else IfTestExist(TestEzKdTree_NearestRespectsMaxDistance);
    else IfTestExist(TestEzKdTree_KNearestReturnsKBestSorted);
    else IfTestExist(TestEzKdTree_AgreesWithBruteForceOnSmallSet);
    else IfTestExist(TestEzKdTree_DepthGrowsAfterSplit);
    else IfTestExist(TestEzKdTree_NearestOnEmptyReturnsNothing);
    else IfTestExist(TestEzKdTree_NearestRespectsMaxElementsZero);
    return false;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
