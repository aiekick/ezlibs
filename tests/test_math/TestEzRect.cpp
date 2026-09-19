#include <TestEzRect.h>

#include <cstdint>
#include <string>

#include <ezlibs/ezMath/ezMath.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

// the fixture is built by the test itself : every number below comes from
// these two makers, never from a shipped default
template <typename TTYPE>
ez::math::rect<TTYPE> local_makeRect(int32_t aPosX, int32_t aPosY, int32_t aSizeX, int32_t aSizeY) {
    return ez::math::rect<TTYPE>(  //
        ez::math::vec2<TTYPE>(static_cast<TTYPE>(aPosX), static_cast<TTYPE>(aPosY)),
        ez::math::vec2<TTYPE>(static_cast<TTYPE>(aSizeX), static_cast<TTYPE>(aSizeY)));
}

template <typename TTYPE>
ez::math::vec2<TTYPE> local_makePoint(int32_t aPointX, int32_t aPointY) {
    return ez::math::vec2<TTYPE>(static_cast<TTYPE>(aPointX), static_cast<TTYPE>(aPointY));
}

// field by field, so the checks never lean on the operator== under test
template <typename TTYPE>
bool local_isSameRect(const ez::math::rect<TTYPE>& aRect, int32_t aPosX, int32_t aPosY, int32_t aSizeX, int32_t aSizeY) {
    return ez::math::isEqual(aRect.pos.x, static_cast<TTYPE>(aPosX)) &&  //
        ez::math::isEqual(aRect.pos.y, static_cast<TTYPE>(aPosY)) &&     //
        ez::math::isEqual(aRect.size.x, static_cast<TTYPE>(aSizeX)) &&   //
        ez::math::isEqual(aRect.size.y, static_cast<TTYPE>(aSizeY));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// a default rect is the neutral one : nothing at the origin, and empty
template <typename TTYPE>
bool TestEzRect_DefaultIsEmptyAtOrigin() {
    const ez::math::rect<TTYPE> defaultRect;
    CTEST_ASSERT(local_isSameRect<TTYPE>(defaultRect, 0, 0, 0, 0));
    CTEST_ASSERT(defaultRect.empty());
    return true;
}

// the constructor keeps what it is given, both fields, in order
template <typename TTYPE>
bool TestEzRect_ConstructorKeepsPosAndSize() {
    const ez::math::rect<TTYPE> builtRect = local_makeRect<TTYPE>(10, 20, 30, 40);
    CTEST_ASSERT(local_isSameRect<TTYPE>(builtRect, 10, 20, 30, 40));
    CTEST_ASSERT(!builtRect.empty());
    return true;
}

// the typed conversion casts both fields at once : an irect out of a frect
// in one call, and back again without a loss on whole values
bool TestEzRect_TypedConversionCastsBothFields() {
    const ez::math::frect sourceRect(ez::math::vec2<float>(1.9f, -2.9f), ez::math::vec2<float>(3.9f, 4.9f));
    const ez::math::irect convertedRect(sourceRect);
    CTEST_ASSERT(local_isSameRect<int32_t>(convertedRect, 1, -2, 3, 4));
    const ez::math::irect wholeRect = local_makeRect<int32_t>(10, 20, 30, 40);
    const ez::math::frect widenedRect(wholeRect);
    CTEST_ASSERT(local_isSameRect<float>(widenedRect, 10, 20, 30, 40));
    return true;
}

// a rect is empty as soon as one of its sides brings no surface
template <typename TTYPE>
bool TestEzRect_EmptyOnNullOrNegativeSize() {
    CTEST_ASSERT(!local_makeRect<TTYPE>(10, 20, 30, 40).empty());
    CTEST_ASSERT(local_makeRect<TTYPE>(10, 20, 0, 40).empty());
    CTEST_ASSERT(local_makeRect<TTYPE>(10, 20, 30, 0).empty());
    CTEST_ASSERT(local_makeRect<TTYPE>(10, 20, 0, 0).empty());
    CTEST_ASSERT(local_makeRect<TTYPE>(10, 20, -30, 40).empty());
    CTEST_ASSERT(local_makeRect<TTYPE>(10, 20, 30, -40).empty());
    return true;
}

// the point range is half open : the top left corner belongs to the rect,
// the bottom right one belongs to the next rect
template <typename TTYPE>
bool TestEzRect_ContainsPointIsHalfOpen() {
    const ez::math::rect<TTYPE> hostRect = local_makeRect<TTYPE>(10, 20, 30, 40);
    CTEST_ASSERT(hostRect.contains(local_makePoint<TTYPE>(10, 20)));
    CTEST_ASSERT(hostRect.contains(local_makePoint<TTYPE>(39, 59)));
    CTEST_ASSERT(hostRect.contains(local_makePoint<TTYPE>(25, 40)));
    CTEST_ASSERT(!hostRect.contains(local_makePoint<TTYPE>(40, 60)));
    CTEST_ASSERT(!hostRect.contains(local_makePoint<TTYPE>(40, 30)));
    CTEST_ASSERT(!hostRect.contains(local_makePoint<TTYPE>(25, 60)));
    CTEST_ASSERT(!hostRect.contains(local_makePoint<TTYPE>(9, 30)));
    CTEST_ASSERT(!hostRect.contains(local_makePoint<TTYPE>(25, 19)));
    return true;
}

// the rect range is inclusive : a rect contains itself, and a spill on any
// of the four sides breaks the containment
template <typename TTYPE>
bool TestEzRect_ContainsRectIsInclusive() {
    const ez::math::rect<TTYPE> hostRect = local_makeRect<TTYPE>(10, 20, 30, 40);
    CTEST_ASSERT(hostRect.contains(hostRect));
    CTEST_ASSERT(hostRect.contains(local_makeRect<TTYPE>(15, 25, 10, 10)));
    CTEST_ASSERT(hostRect.contains(local_makeRect<TTYPE>(30, 50, 10, 10)));
    CTEST_ASSERT(!hostRect.contains(local_makeRect<TTYPE>(5, 25, 10, 10)));
    CTEST_ASSERT(!hostRect.contains(local_makeRect<TTYPE>(15, 15, 10, 10)));
    CTEST_ASSERT(!hostRect.contains(local_makeRect<TTYPE>(35, 25, 10, 10)));
    CTEST_ASSERT(!hostRect.contains(local_makeRect<TTYPE>(15, 55, 10, 10)));
    return true;
}

// an intersection needs a surface : two rects sharing only an edge do not
// intersect, and the answer does not depend on the order
template <typename TTYPE>
bool TestEzRect_IntersectsNeedsRealOverlap() {
    const ez::math::rect<TTYPE> hostRect = local_makeRect<TTYPE>(10, 20, 30, 40);
    const ez::math::rect<TTYPE> overlappingRect = local_makeRect<TTYPE>(30, 50, 30, 40);
    CTEST_ASSERT(hostRect.intersects(hostRect));
    CTEST_ASSERT(hostRect.intersects(overlappingRect));
    CTEST_ASSERT(overlappingRect.intersects(hostRect));
    const ez::math::rect<TTYPE> rightNeighbourRect = local_makeRect<TTYPE>(40, 20, 10, 40);
    const ez::math::rect<TTYPE> bottomNeighbourRect = local_makeRect<TTYPE>(10, 60, 30, 10);
    CTEST_ASSERT(!hostRect.intersects(rightNeighbourRect));
    CTEST_ASSERT(!rightNeighbourRect.intersects(hostRect));
    CTEST_ASSERT(!hostRect.intersects(bottomNeighbourRect));
    CTEST_ASSERT(!bottomNeighbourRect.intersects(hostRect));
    const ez::math::rect<TTYPE> farRect = local_makeRect<TTYPE>(100, 200, 10, 10);
    CTEST_ASSERT(!hostRect.intersects(farRect));
    CTEST_ASSERT(!farRect.intersects(hostRect));
    return true;
}

// the intersected box is the common surface, it does not depend on the
// order, and disjoint rects give an empty one whose size never wraps
template <typename TTYPE>
bool TestEzRect_IntersectedIsTheOverlap() {
    const ez::math::rect<TTYPE> hostRect = local_makeRect<TTYPE>(10, 20, 30, 40);
    const ez::math::rect<TTYPE> overlappingRect = local_makeRect<TTYPE>(30, 50, 30, 40);
    CTEST_ASSERT(local_isSameRect<TTYPE>(hostRect.intersected(overlappingRect), 30, 50, 10, 10));
    CTEST_ASSERT(local_isSameRect<TTYPE>(overlappingRect.intersected(hostRect), 30, 50, 10, 10));
    // a contained rect is its own intersection with its host
    const ez::math::rect<TTYPE> innerRect = local_makeRect<TTYPE>(15, 25, 10, 10);
    CTEST_ASSERT(local_isSameRect<TTYPE>(hostRect.intersected(innerRect), 15, 25, 10, 10));
    // the intersection with itself changes nothing
    CTEST_ASSERT(local_isSameRect<TTYPE>(hostRect.intersected(hostRect), 10, 20, 30, 40));
    // no overlap at all : an empty result, both sides clamped to zero
    const ez::math::rect<TTYPE> farRect = local_makeRect<TTYPE>(100, 200, 10, 10);
    const ez::math::rect<TTYPE> noOverlapRect = hostRect.intersected(farRect);
    CTEST_ASSERT(noOverlapRect.empty());
    CTEST_ASSERT(ez::math::isEqual(noOverlapRect.size.x, static_cast<TTYPE>(0)));
    CTEST_ASSERT(ez::math::isEqual(noOverlapRect.size.y, static_cast<TTYPE>(0)));
    // rects sharing only an edge overlap on nothing
    const ez::math::rect<TTYPE> rightNeighbourRect = local_makeRect<TTYPE>(40, 20, 10, 40);
    CTEST_ASSERT(hostRect.intersected(rightNeighbourRect).empty());
    return true;
}

// the merged box is the union, it does not depend on the order, and an
// empty side is ignored so a default rect is a neutral seed
template <typename TTYPE>
bool TestEzRect_MergedIsTheUnion() {
    const ez::math::rect<TTYPE> hostRect = local_makeRect<TTYPE>(10, 20, 30, 40);
    const ez::math::rect<TTYPE> overlappingRect = local_makeRect<TTYPE>(30, 50, 30, 40);
    CTEST_ASSERT(local_isSameRect<TTYPE>(hostRect.merged(overlappingRect), 10, 20, 50, 70));
    CTEST_ASSERT(local_isSameRect<TTYPE>(overlappingRect.merged(hostRect), 10, 20, 50, 70));
    // a disjoint rect widens the box up to itself
    const ez::math::rect<TTYPE> farRect = local_makeRect<TTYPE>(100, 200, 10, 10);
    CTEST_ASSERT(local_isSameRect<TTYPE>(hostRect.merged(farRect), 10, 20, 100, 190));
    // a contained rect brings nothing
    CTEST_ASSERT(local_isSameRect<TTYPE>(hostRect.merged(local_makeRect<TTYPE>(15, 25, 10, 10)), 10, 20, 30, 40));
    CTEST_ASSERT(local_isSameRect<TTYPE>(hostRect.merged(hostRect), 10, 20, 30, 40));
    // the empty seed is neutral, on either side
    const ez::math::rect<TTYPE> emptyRect;
    CTEST_ASSERT(local_isSameRect<TTYPE>(emptyRect.merged(hostRect), 10, 20, 30, 40));
    CTEST_ASSERT(local_isSameRect<TTYPE>(hostRect.merged(emptyRect), 10, 20, 30, 40));
    return true;
}

// the equality weighs the four fields, and the difference is its negation
template <typename TTYPE>
bool TestEzRect_EqualityComparesAllFourFields() {
    const ez::math::rect<TTYPE> hostRect = local_makeRect<TTYPE>(10, 20, 30, 40);
    CTEST_ASSERT(hostRect == local_makeRect<TTYPE>(10, 20, 30, 40));
    CTEST_ASSERT(!(hostRect != local_makeRect<TTYPE>(10, 20, 30, 40)));
    CTEST_ASSERT(hostRect != local_makeRect<TTYPE>(11, 20, 30, 40));
    CTEST_ASSERT(hostRect != local_makeRect<TTYPE>(10, 21, 30, 40));
    CTEST_ASSERT(hostRect != local_makeRect<TTYPE>(10, 20, 31, 40));
    CTEST_ASSERT(hostRect != local_makeRect<TTYPE>(10, 20, 30, 41));
    CTEST_ASSERT(!(hostRect == local_makeRect<TTYPE>(11, 20, 30, 40)));
    return true;
}

// the reframe moves the two corners apart : the top left offset shifts the
// origin and eats the size, the bottom right one only eats the size
template <typename TTYPE>
bool TestEzRect_ReframeMovesEdges() {
    const ez::math::rect<TTYPE> hostRect = local_makeRect<TTYPE>(10, 20, 30, 40);
    const ez::math::rect<TTYPE> shrunkRect = hostRect.reframe(local_makePoint<TTYPE>(2, 3), local_makePoint<TTYPE>(4, 5));
    CTEST_ASSERT(local_isSameRect<TTYPE>(shrunkRect, 12, 23, 24, 32));
    // the two corners stay put : the reframe never moves an edge it was not asked to
    const ez::math::rect<TTYPE> topLeftOnlyRect = hostRect.reframe(local_makePoint<TTYPE>(2, 3), local_makePoint<TTYPE>(0, 0));
    CTEST_ASSERT(local_isSameRect<TTYPE>(topLeftOnlyRect, 12, 23, 28, 37));
    const ez::math::rect<TTYPE> bottomRightOnlyRect = hostRect.reframe(local_makePoint<TTYPE>(0, 0), local_makePoint<TTYPE>(4, 5));
    CTEST_ASSERT(local_isSameRect<TTYPE>(bottomRightOnlyRect, 10, 20, 26, 35));
    // no offset at all : the rect comes back untouched
    CTEST_ASSERT(local_isSameRect<TTYPE>(hostRect.reframe(local_makePoint<TTYPE>(0, 0), local_makePoint<TTYPE>(0, 0)), 10, 20, 30, 40));
    // negative offsets grow the rect instead of shrinking it
    const ez::math::rect<TTYPE> grownRect = hostRect.reframe(local_makePoint<TTYPE>(-1, -2), local_makePoint<TTYPE>(-3, -4));
    CTEST_ASSERT(local_isSameRect<TTYPE>(grownRect, 9, 18, 34, 46));
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzRect(const std::string& vTest) {
    IfTestExist(TestEzRect_DefaultIsEmptyAtOrigin<float>);
    else IfTestExist(TestEzRect_DefaultIsEmptyAtOrigin<int32_t>);
    else IfTestExist(TestEzRect_DefaultIsEmptyAtOrigin<uint32_t>);
    else IfTestExist(TestEzRect_ConstructorKeepsPosAndSize<float>);
    else IfTestExist(TestEzRect_ConstructorKeepsPosAndSize<int32_t>);
    else IfTestExist(TestEzRect_ConstructorKeepsPosAndSize<uint32_t>);
    else IfTestExist(TestEzRect_TypedConversionCastsBothFields);
    else IfTestExist(TestEzRect_EmptyOnNullOrNegativeSize<float>);
    else IfTestExist(TestEzRect_EmptyOnNullOrNegativeSize<int32_t>);
    else IfTestExist(TestEzRect_ContainsPointIsHalfOpen<float>);
    else IfTestExist(TestEzRect_ContainsPointIsHalfOpen<int32_t>);
    else IfTestExist(TestEzRect_ContainsPointIsHalfOpen<uint32_t>);
    else IfTestExist(TestEzRect_ContainsRectIsInclusive<float>);
    else IfTestExist(TestEzRect_ContainsRectIsInclusive<int32_t>);
    else IfTestExist(TestEzRect_ContainsRectIsInclusive<uint32_t>);
    else IfTestExist(TestEzRect_IntersectsNeedsRealOverlap<float>);
    else IfTestExist(TestEzRect_IntersectsNeedsRealOverlap<int32_t>);
    else IfTestExist(TestEzRect_IntersectsNeedsRealOverlap<uint32_t>);
    else IfTestExist(TestEzRect_IntersectedIsTheOverlap<float>);
    else IfTestExist(TestEzRect_IntersectedIsTheOverlap<int32_t>);
    else IfTestExist(TestEzRect_IntersectedIsTheOverlap<uint32_t>);
    else IfTestExist(TestEzRect_MergedIsTheUnion<float>);
    else IfTestExist(TestEzRect_MergedIsTheUnion<int32_t>);
    else IfTestExist(TestEzRect_MergedIsTheUnion<uint32_t>);
    else IfTestExist(TestEzRect_EqualityComparesAllFourFields<float>);
    else IfTestExist(TestEzRect_EqualityComparesAllFourFields<int32_t>);
    else IfTestExist(TestEzRect_EqualityComparesAllFourFields<uint32_t>);
    else IfTestExist(TestEzRect_ReframeMovesEdges<float>);
    else IfTestExist(TestEzRect_ReframeMovesEdges<int32_t>);
    return false;
}
