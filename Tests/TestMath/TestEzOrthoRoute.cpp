#include <vector>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMath/ezMath.hpp>
#include <ezlibs/ezMath/ezSdf.hpp>
#include <ezlibs/ezMath/ezOrthoRoute.hpp>

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

// Nothing to avoid : the route is the straight two-point segment.
template <typename T>
bool TestEzOrthoRoute_NoBoxesIsDirect() {
    const std::vector<ez::math::AABB<T>> boxes;
    const ez::math::OrthoRouteConfig<T> cfg;
    const std::vector<ez::math::vec2<T>> path = ez::math::orthoRoute(boxes, ez::math::vec2<T>(0, 0), ez::math::vec2<T>(96, 0), cfg);
    CTEST_ASSERT(path.size() == 2U);
    CTEST_ASSERT(ez::math::isEqual(path.front().x, static_cast<T>(0), static_cast<T>(1e-3)));
    CTEST_ASSERT(ez::math::isEqual(path.front().y, static_cast<T>(0), static_cast<T>(1e-3)));
    CTEST_ASSERT(ez::math::isEqual(path.back().x, static_cast<T>(96), static_cast<T>(1e-3)));
    CTEST_ASSERT(ez::math::isEqual(path.back().y, static_cast<T>(0), static_cast<T>(1e-3)));
    return true;
}

// A box squarely across the straight path : the route must bend, reach the exact endpoints, and
// every interior vertex must keep the configured clearance from the boxes.
template <typename T>
bool TestEzOrthoRoute_AroundBoxBendsAndStaysClear() {
    std::vector<ez::math::AABB<T>> boxes;
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(40, -24), ez::math::vec2<T>(56, 24)));
    const ez::math::OrthoRouteConfig<T> cfg;  // gridStep 16, clearance 8
    const ez::math::vec2<T> start(0, 0);
    const ez::math::vec2<T> end(96, 0);
    const std::vector<ez::math::vec2<T>> path = ez::math::orthoRoute(boxes, start, end, cfg);
    CTEST_ASSERT(ez::math::isEqual(path.front().x, start.x, static_cast<T>(1e-3)));
    CTEST_ASSERT(ez::math::isEqual(path.front().y, start.y, static_cast<T>(1e-3)));
    CTEST_ASSERT(ez::math::isEqual(path.back().x, end.x, static_cast<T>(1e-3)));
    CTEST_ASSERT(ez::math::isEqual(path.back().y, end.y, static_cast<T>(1e-3)));
    CTEST_ASSERT(path.size() > 2U);  // it had to detour
    for (size_t pointIndex = 1U; pointIndex + 1U < path.size(); ++pointIndex) {
        CTEST_ASSERT(ez::math::sdfToBoxes(path[pointIndex], boxes) >= cfg.clearance - static_cast<T>(1e-3));
    }
    return true;
}

// Every segment of the route is axis-aligned (orthogonal).
template <typename T>
bool TestEzOrthoRoute_IsOrthogonal() {
    std::vector<ez::math::AABB<T>> boxes;
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(40, -24), ez::math::vec2<T>(56, 24)));
    const ez::math::OrthoRouteConfig<T> cfg;
    const std::vector<ez::math::vec2<T>> path = ez::math::orthoRoute(boxes, ez::math::vec2<T>(0, 0), ez::math::vec2<T>(96, 0), cfg);
    CTEST_ASSERT(path.size() >= 2U);
    for (size_t pointIndex = 1U; pointIndex < path.size(); ++pointIndex) {
        const bool sameX = ez::math::isEqual(path[pointIndex].x, path[pointIndex - 1U].x, static_cast<T>(1e-2));
        const bool sameY = ez::math::isEqual(path[pointIndex].y, path[pointIndex - 1U].y, static_cast<T>(1e-2));
        CTEST_ASSERT(sameX || sameY);
    }
    return true;
}

// Degenerate : start == end yields a single point.
template <typename T>
bool TestEzOrthoRoute_StartEqualsEndIsSinglePoint() {
    const std::vector<ez::math::AABB<T>> boxes;
    const ez::math::OrthoRouteConfig<T> cfg;
    const std::vector<ez::math::vec2<T>> path = ez::math::orthoRoute(boxes, ez::math::vec2<T>(10, 10), ez::math::vec2<T>(10, 10), cfg);
    CTEST_ASSERT(path.size() >= 1U);
    CTEST_ASSERT(ez::math::isEqual(path.front().x, static_cast<T>(10), static_cast<T>(1e-3)));
    CTEST_ASSERT(ez::math::isEqual(path.front().y, static_cast<T>(10), static_cast<T>(1e-3)));
    return true;
}

////////////////////////////////////////////////////////////////////////////

bool TestEzOrthoRoute(const std::string& vTest) {
    IfTestExist(TestEzOrthoRoute_NoBoxesIsDirect<float>);
    else IfTestExist(TestEzOrthoRoute_NoBoxesIsDirect<double>);
    else IfTestExist(TestEzOrthoRoute_AroundBoxBendsAndStaysClear<float>);
    else IfTestExist(TestEzOrthoRoute_AroundBoxBendsAndStaysClear<double>);
    else IfTestExist(TestEzOrthoRoute_IsOrthogonal<float>);
    else IfTestExist(TestEzOrthoRoute_IsOrthogonal<double>);
    else IfTestExist(TestEzOrthoRoute_StartEqualsEndIsSinglePoint<float>);
    else IfTestExist(TestEzOrthoRoute_StartEqualsEndIsSinglePoint<double>);
    return false;
}

////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
