#include <vector>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMath/ezMath.hpp>
#include <ezlibs/ezMath/ezSdf.hpp>

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

// At the center of a box the SDF is the deepest negative value : -min(halfWidth, halfHeight).
// Box [0,0]-[4,2] -> half (2,1) -> center value = -1.
template <typename T>
bool TestEzSdf_BoxCenterIsNegativeHalfMinExtent() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(4, 2));
    const T dist = ez::math::sdfToBox(ez::math::vec2<T>(2, 1), box);
    CTEST_ASSERT(ez::math::isEqual(dist, static_cast<T>(-1), static_cast<T>(1e-4)));
    return true;
}

// On the border the SDF is exactly 0. Box [0,0]-[2,2], point on the right face (2,1).
template <typename T>
bool TestEzSdf_BoxOnBorderIsZero() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(2, 2));
    const T dist = ez::math::sdfToBox(ez::math::vec2<T>(2, 1), box);
    CTEST_ASSERT(ez::math::isEqual(dist, static_cast<T>(0), static_cast<T>(1e-4)));
    return true;
}

// Outside, aligned with a face : the SDF is the orthogonal distance to that face.
// Box [0,0]-[4,2], point (6,1) -> 2 units right of the x=4 face.
template <typename T>
bool TestEzSdf_BoxFaceDistance() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(4, 2));
    const T dist = ez::math::sdfToBox(ez::math::vec2<T>(6, 1), box);
    CTEST_ASSERT(ez::math::isEqual(dist, static_cast<T>(2), static_cast<T>(1e-4)));
    return true;
}

// Outside, past a corner : the SDF is the euclidean distance to that corner.
// Box [0,0]-[2,2], point (5,6) -> distance from corner (2,2) = sqrt(9+16) = 5.
template <typename T>
bool TestEzSdf_BoxCornerDistance() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(2, 2));
    const T dist = ez::math::sdfToBox(ez::math::vec2<T>(5, 6), box);
    CTEST_ASSERT(ez::math::isEqual(dist, static_cast<T>(5), static_cast<T>(1e-4)));
    return true;
}

// The union returns the distance to the NEAREST box. Two boxes far apart, point
// between them but closer to the first -> distance to the first.
template <typename T>
bool TestEzSdf_BoxesReturnNearest() {
    std::vector<ez::math::AABB<T>> boxes;
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(2, 2)));
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(10, 0), ez::math::vec2<T>(12, 2)));
    const T dist = ez::math::sdfToBoxes(ez::math::vec2<T>(5, 1), boxes);
    CTEST_ASSERT(ez::math::isEqual(dist, static_cast<T>(3), static_cast<T>(1e-4)));
    return true;
}

// Inside one of the boxes the union is negative (the most negative box wins via min).
template <typename T>
bool TestEzSdf_BoxesNegativeInside() {
    std::vector<ez::math::AABB<T>> boxes;
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(2, 2)));
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(10, 0), ez::math::vec2<T>(12, 2)));
    const T dist = ez::math::sdfToBoxes(ez::math::vec2<T>(11, 1), boxes);
    CTEST_ASSERT(ez::math::isEqual(dist, static_cast<T>(-1), static_cast<T>(1e-4)));
    return true;
}

// No box at all : the field is "infinitely far" from any obstacle (large positive sentinel).
template <typename T>
bool TestEzSdf_BoxesEmptyIsLargePositive() {
    const std::vector<ez::math::AABB<T>> boxes;
    const T dist = ez::math::sdfToBoxes(ez::math::vec2<T>(0, 0), boxes);
    CTEST_ASSERT(dist > static_cast<T>(1e6));
    return true;
}

////////////////////////////////////////////////////////////////////////////
// gradient (grad SDF) : unit escape direction, pointing away from the nearest box.

// Outside, aligned with a face : the gradient is the outward unit normal of that face.
// Box [0,0]-[4,2], point (6,1) right of the x=4 face -> (1,0).
template <typename T>
bool TestEzSdf_GradFaceOutsidePointsAway() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(4, 2));
    const ez::math::vec2<T> grad = ez::math::sdfGradToBox(ez::math::vec2<T>(6, 1), box);
    CTEST_ASSERT(ez::math::isEqual(grad.x, static_cast<T>(1), static_cast<T>(1e-4)));
    CTEST_ASSERT(ez::math::isEqual(grad.y, static_cast<T>(0), static_cast<T>(1e-4)));
    return true;
}

// Outside on the left : the gradient points left (away from the box). Point (-2,1) -> (-1,0).
template <typename T>
bool TestEzSdf_GradLeftOutsidePointsLeft() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(4, 2));
    const ez::math::vec2<T> grad = ez::math::sdfGradToBox(ez::math::vec2<T>(-2, 1), box);
    CTEST_ASSERT(ez::math::isEqual(grad.x, static_cast<T>(-1), static_cast<T>(1e-4)));
    CTEST_ASSERT(ez::math::isEqual(grad.y, static_cast<T>(0), static_cast<T>(1e-4)));
    return true;
}

// Outside past a corner : the gradient is the unit direction from the corner to the point.
// Box [0,0]-[2,2], point (5,6) -> (3,4)/5 = (0.6, 0.8).
template <typename T>
bool TestEzSdf_GradCornerOutsideIsDiagonal() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(2, 2));
    const ez::math::vec2<T> grad = ez::math::sdfGradToBox(ez::math::vec2<T>(5, 6), box);
    CTEST_ASSERT(ez::math::isEqual(grad.x, static_cast<T>(0.6), static_cast<T>(1e-4)));
    CTEST_ASSERT(ez::math::isEqual(grad.y, static_cast<T>(0.8), static_cast<T>(1e-4)));
    return true;
}

// Inside : the gradient pushes out along the nearest face. Box [0,0]-[4,4], point (3,2) is
// closest to the x=4 face -> (1,0).
template <typename T>
bool TestEzSdf_GradInsidePushesToNearestFace() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(4, 4));
    const ez::math::vec2<T> grad = ez::math::sdfGradToBox(ez::math::vec2<T>(3, 2), box);
    CTEST_ASSERT(ez::math::isEqual(grad.x, static_cast<T>(1), static_cast<T>(1e-4)));
    CTEST_ASSERT(ez::math::isEqual(grad.y, static_cast<T>(0), static_cast<T>(1e-4)));
    return true;
}

// At the exact center the gradient is undefined -> (0,0). Box [0,0]-[2,2], point (1,1).
template <typename T>
bool TestEzSdf_GradAtCenterIsZero() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(2, 2));
    const ez::math::vec2<T> grad = ez::math::sdfGradToBox(ez::math::vec2<T>(1, 1), box);
    CTEST_ASSERT(ez::math::isEqual(grad.x, static_cast<T>(0), static_cast<T>(1e-4)));
    CTEST_ASSERT(ez::math::isEqual(grad.y, static_cast<T>(0), static_cast<T>(1e-4)));
    return true;
}

// The union gradient follows the NEAREST box. Boxes A[0,0]-[2,2], B[10,0]-[12,2], point (7,1)
// is closer to B -> escape direction away from B is (-1,0).
template <typename T>
bool TestEzSdf_GradBoxesUsesNearest() {
    std::vector<ez::math::AABB<T>> boxes;
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(2, 2)));
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(10, 0), ez::math::vec2<T>(12, 2)));
    const ez::math::vec2<T> grad = ez::math::sdfGradToBoxes(ez::math::vec2<T>(7, 1), boxes);
    CTEST_ASSERT(ez::math::isEqual(grad.x, static_cast<T>(-1), static_cast<T>(1e-4)));
    CTEST_ASSERT(ez::math::isEqual(grad.y, static_cast<T>(0), static_cast<T>(1e-4)));
    return true;
}

// No box at all : no escape direction -> (0,0).
template <typename T>
bool TestEzSdf_GradBoxesEmptyIsZero() {
    const std::vector<ez::math::AABB<T>> boxes;
    const ez::math::vec2<T> grad = ez::math::sdfGradToBoxes(ez::math::vec2<T>(0, 0), boxes);
    CTEST_ASSERT(ez::math::isEqual(grad.x, static_cast<T>(0), static_cast<T>(1e-4)));
    CTEST_ASSERT(ez::math::isEqual(grad.y, static_cast<T>(0), static_cast<T>(1e-4)));
    return true;
}

////////////////////////////////////////////////////////////////////////////
// classification : does a straight link cross a box ?

// A horizontal segment passing through a box -> hit. Box [2,-1]-[4,1], segment (0,0)->(6,0).
template <typename T>
bool TestEzSdf_SegmentCrossesBoxIsTrue() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(2, -1), ez::math::vec2<T>(4, 1));
    CTEST_ASSERT(ez::math::segmentHitsBox(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(6, 0), box));
    return true;
}

// A segment passing beside the box (same x span, different y) -> no hit.
template <typename T>
bool TestEzSdf_SegmentBesideBoxIsFalse() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(2, 2), ez::math::vec2<T>(4, 4));
    CTEST_ASSERT(!ez::math::segmentHitsBox(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(6, 0), box));
    return true;
}

// A segment fully inside the box -> hit.
template <typename T>
bool TestEzSdf_SegmentInsideBoxIsTrue() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(4, 4));
    CTEST_ASSERT(ez::math::segmentHitsBox(ez::math::vec2<T>(1, 1), ez::math::vec2<T>(2, 2), box));
    return true;
}

// A segment whose start endpoint is inside the box -> hit.
template <typename T>
bool TestEzSdf_SegmentEndpointInsideIsTrue() {
    const ez::math::AABB<T> box(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(2, 2));
    CTEST_ASSERT(ez::math::segmentHitsBox(ez::math::vec2<T>(1, 1), ez::math::vec2<T>(5, 1), box));
    return true;
}

// Against a set : a hit on any box returns true.
template <typename T>
bool TestEzSdf_SegmentBoxesAnyHitIsTrue() {
    std::vector<ez::math::AABB<T>> boxes;
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(2, 2), ez::math::vec2<T>(4, 4)));
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(2, -1), ez::math::vec2<T>(4, 1)));
    CTEST_ASSERT(ez::math::segmentHitsBoxes(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(6, 0), boxes));
    return true;
}

// Against a set : a clear segment (no box on its path) returns false.
template <typename T>
bool TestEzSdf_SegmentBoxesNoneHitIsFalse() {
    std::vector<ez::math::AABB<T>> boxes;
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(2, 2), ez::math::vec2<T>(4, 4)));
    boxes.push_back(ez::math::AABB<T>(ez::math::vec2<T>(2, 5), ez::math::vec2<T>(4, 7)));
    CTEST_ASSERT(!ez::math::segmentHitsBoxes(ez::math::vec2<T>(0, 0), ez::math::vec2<T>(6, 0), boxes));
    return true;
}

////////////////////////////////////////////////////////////////////////////

bool TestEzSdf(const std::string& vTest) {
    IfTestExist(TestEzSdf_BoxCenterIsNegativeHalfMinExtent<float>);
    else IfTestExist(TestEzSdf_BoxCenterIsNegativeHalfMinExtent<double>);
    else IfTestExist(TestEzSdf_BoxOnBorderIsZero<float>);
    else IfTestExist(TestEzSdf_BoxOnBorderIsZero<double>);
    else IfTestExist(TestEzSdf_BoxFaceDistance<float>);
    else IfTestExist(TestEzSdf_BoxFaceDistance<double>);
    else IfTestExist(TestEzSdf_BoxCornerDistance<float>);
    else IfTestExist(TestEzSdf_BoxCornerDistance<double>);
    else IfTestExist(TestEzSdf_BoxesReturnNearest<float>);
    else IfTestExist(TestEzSdf_BoxesReturnNearest<double>);
    else IfTestExist(TestEzSdf_BoxesNegativeInside<float>);
    else IfTestExist(TestEzSdf_BoxesNegativeInside<double>);
    else IfTestExist(TestEzSdf_BoxesEmptyIsLargePositive<float>);
    else IfTestExist(TestEzSdf_BoxesEmptyIsLargePositive<double>);
    else IfTestExist(TestEzSdf_GradFaceOutsidePointsAway<float>);
    else IfTestExist(TestEzSdf_GradFaceOutsidePointsAway<double>);
    else IfTestExist(TestEzSdf_GradLeftOutsidePointsLeft<float>);
    else IfTestExist(TestEzSdf_GradLeftOutsidePointsLeft<double>);
    else IfTestExist(TestEzSdf_GradCornerOutsideIsDiagonal<float>);
    else IfTestExist(TestEzSdf_GradCornerOutsideIsDiagonal<double>);
    else IfTestExist(TestEzSdf_GradInsidePushesToNearestFace<float>);
    else IfTestExist(TestEzSdf_GradInsidePushesToNearestFace<double>);
    else IfTestExist(TestEzSdf_GradAtCenterIsZero<float>);
    else IfTestExist(TestEzSdf_GradAtCenterIsZero<double>);
    else IfTestExist(TestEzSdf_GradBoxesUsesNearest<float>);
    else IfTestExist(TestEzSdf_GradBoxesUsesNearest<double>);
    else IfTestExist(TestEzSdf_GradBoxesEmptyIsZero<float>);
    else IfTestExist(TestEzSdf_GradBoxesEmptyIsZero<double>);
    else IfTestExist(TestEzSdf_SegmentCrossesBoxIsTrue<float>);
    else IfTestExist(TestEzSdf_SegmentCrossesBoxIsTrue<double>);
    else IfTestExist(TestEzSdf_SegmentBesideBoxIsFalse<float>);
    else IfTestExist(TestEzSdf_SegmentBesideBoxIsFalse<double>);
    else IfTestExist(TestEzSdf_SegmentInsideBoxIsTrue<float>);
    else IfTestExist(TestEzSdf_SegmentInsideBoxIsTrue<double>);
    else IfTestExist(TestEzSdf_SegmentEndpointInsideIsTrue<float>);
    else IfTestExist(TestEzSdf_SegmentEndpointInsideIsTrue<double>);
    else IfTestExist(TestEzSdf_SegmentBoxesAnyHitIsTrue<float>);
    else IfTestExist(TestEzSdf_SegmentBoxesAnyHitIsTrue<double>);
    else IfTestExist(TestEzSdf_SegmentBoxesNoneHitIsFalse<float>);
    else IfTestExist(TestEzSdf_SegmentBoxesNoneHitIsFalse<double>);
    return false;
}

////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
