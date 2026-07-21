#include <TestEzSeparation1D.h>

#include <vector>
#include <cmath>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezDiagram/ezSeparation1D.hpp>

////////////////////////////////////////////////////////////////////////////

namespace {
// local approximate compare : ez::isEqual uses a strict <, so a positive tolerance is mandatory.
bool approxEqual(float aLeft, float aRight) {
    return std::fabs(aLeft - aRight) < 1e-4f;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////

// no variable -> no position
bool TestEzSeparation1D_Empty() {
    std::vector<float> desired;
    std::vector<float> weight;
    std::vector<float> gap;
    std::vector<float> positions;
    ez::diagram::solveSeparation1D(desired, weight, gap, positions);
    CTEST_ASSERT(positions.empty());
    return true;
}

// a single variable sits exactly on its desired position
bool TestEzSeparation1D_Single() {
    std::vector<float> desired;
    desired.push_back(5.0f);
    std::vector<float> weight;
    weight.push_back(1.0f);
    std::vector<float> gap;
    std::vector<float> positions;
    ez::diagram::solveSeparation1D(desired, weight, gap, positions);
    CTEST_ASSERT(positions.size() == 1U);
    CTEST_ASSERT(approxEqual(positions[0], 5.0f));
    return true;
}

// already separated by more than the gap : the constraint is inactive, positions stay on desired
bool TestEzSeparation1D_TwoAlreadySeparated() {
    std::vector<float> desired;
    desired.push_back(0.0f);
    desired.push_back(10.0f);
    std::vector<float> weight(2U, 1.0f);
    std::vector<float> gap;
    gap.push_back(5.0f);
    std::vector<float> positions;
    ez::diagram::solveSeparation1D(desired, weight, gap, positions);
    CTEST_ASSERT(positions.size() == 2U);
    CTEST_ASSERT(approxEqual(positions[0], 0.0f));
    CTEST_ASSERT(approxEqual(positions[1], 10.0f));
    return true;
}

// coincident desired, equal weights : pushed apart symmetrically around the mean
bool TestEzSeparation1D_TwoOverlapForcedApart() {
    std::vector<float> desired(2U, 0.0f);
    std::vector<float> weight(2U, 1.0f);
    std::vector<float> gap;
    gap.push_back(4.0f);
    std::vector<float> positions;
    ez::diagram::solveSeparation1D(desired, weight, gap, positions);
    CTEST_ASSERT(positions.size() == 2U);
    CTEST_ASSERT(approxEqual(positions[0], -2.0f));
    CTEST_ASSERT(approxEqual(positions[1], 2.0f));
    return true;
}

// the heavier variable barely moves, the lighter one absorbs the separation
bool TestEzSeparation1D_WeightedGoal() {
    std::vector<float> desired(2U, 0.0f);
    std::vector<float> weight;
    weight.push_back(3.0f);
    weight.push_back(1.0f);
    std::vector<float> gap;
    gap.push_back(4.0f);
    std::vector<float> positions;
    ez::diagram::solveSeparation1D(desired, weight, gap, positions);
    CTEST_ASSERT(positions.size() == 2U);
    CTEST_ASSERT(approxEqual(positions[0], -1.0f));
    CTEST_ASSERT(approxEqual(positions[1], 3.0f));
    return true;
}

// desired order is inverted vs the constraint order : the constraints win, order is preserved
bool TestEzSeparation1D_OrderPreservedFromInverted() {
    std::vector<float> desired;
    desired.push_back(10.0f);
    desired.push_back(0.0f);
    std::vector<float> weight(2U, 1.0f);
    std::vector<float> gap;
    gap.push_back(2.0f);
    std::vector<float> positions;
    ez::diagram::solveSeparation1D(desired, weight, gap, positions);
    CTEST_ASSERT(positions.size() == 2U);
    CTEST_ASSERT(approxEqual(positions[0], 4.0f));
    CTEST_ASSERT(approxEqual(positions[1], 6.0f));
    CTEST_ASSERT(positions[1] - positions[0] >= 2.0f - 1e-4f);  // gap respected
    return true;
}

// three coincident variables : evenly spread, centered on the mean
bool TestEzSeparation1D_ThreeCollapsedCentered() {
    std::vector<float> desired(3U, 0.0f);
    std::vector<float> weight(3U, 1.0f);
    std::vector<float> gap(2U, 2.0f);
    std::vector<float> positions;
    ez::diagram::solveSeparation1D(desired, weight, gap, positions);
    CTEST_ASSERT(positions.size() == 3U);
    CTEST_ASSERT(approxEqual(positions[0], -2.0f));
    CTEST_ASSERT(approxEqual(positions[1], 0.0f));
    CTEST_ASSERT(approxEqual(positions[2], 2.0f));
    return true;
}

// already monotone with room to spare : positions stay on desired
bool TestEzSeparation1D_ThreeMonotoneUntouched() {
    std::vector<float> desired;
    desired.push_back(0.0f);
    desired.push_back(5.0f);
    desired.push_back(10.0f);
    std::vector<float> weight(3U, 1.0f);
    std::vector<float> gap(2U, 1.0f);
    std::vector<float> positions;
    ez::diagram::solveSeparation1D(desired, weight, gap, positions);
    CTEST_ASSERT(positions.size() == 3U);
    CTEST_ASSERT(approxEqual(positions[0], 0.0f));
    CTEST_ASSERT(approxEqual(positions[1], 5.0f));
    CTEST_ASSERT(approxEqual(positions[2], 10.0f));
    return true;
}

////////////////////////////////////////////////////////////////////////////

bool TestEzSeparation1D(const std::string& vTest) {
    IfTestExist(TestEzSeparation1D_Empty);
    else IfTestExist(TestEzSeparation1D_Single);
    else IfTestExist(TestEzSeparation1D_TwoAlreadySeparated);
    else IfTestExist(TestEzSeparation1D_TwoOverlapForcedApart);
    else IfTestExist(TestEzSeparation1D_WeightedGoal);
    else IfTestExist(TestEzSeparation1D_OrderPreservedFromInverted);
    else IfTestExist(TestEzSeparation1D_ThreeCollapsedCentered);
    else IfTestExist(TestEzSeparation1D_ThreeMonotoneUntouched);
    return false;
}
