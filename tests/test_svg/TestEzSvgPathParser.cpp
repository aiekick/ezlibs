#include <TestEzSvgPathParser.h>

#include <cmath>
#include <string>
#include <vector>

#include <ezlibs/ezSvg/ezSvg.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

bool local_near(const ez::svg::Point& aPoint, double aX, double aY, double aTolerance = 1e-6) {
    return (std::fabs(aPoint.x - aX) < aTolerance) && (std::fabs(aPoint.y - aY) < aTolerance);
}

bool local_parse(const char* aData, std::vector<ez::svg::SubPath>& aoSubPaths) {
    std::string error;
    return ez::svg::PathParser::parse(aData, aoSubPaths, error);
}

}  // namespace

// M then h v H : three lines, and the z closes the sub path without a
// fourth segment (the closing edge is implicit)
bool TestEzSvgPathParser_ASquareIsThreeLinesAndAClose() {
    std::vector<ez::svg::SubPath> subPaths;
    CTEST_ASSERT(local_parse("M0 0h24v24H0z", subPaths));
    CTEST_ASSERT(subPaths.size() == 1u);
    const ez::svg::SubPath& square = subPaths[0];
    CTEST_ASSERT(local_near(square.start, 0.0, 0.0));
    CTEST_ASSERT(square.segments.size() == 3u);
    CTEST_ASSERT(square.segments[0].kind == ez::svg::SegmentKind::Line);
    CTEST_ASSERT(local_near(square.segments[0].end, 24.0, 0.0));
    CTEST_ASSERT(local_near(square.segments[1].end, 24.0, 24.0));
    CTEST_ASSERT(local_near(square.segments[2].end, 0.0, 24.0));
    CTEST_ASSERT(square.closed);
    return true;
}

// lower case commands are relative to the current point, and svg glues
// its numbers : "5-5-5-5" is four of them, "1.5.5" two
bool TestEzSvgPathParser_RelativeAndGluedNumbers() {
    std::vector<ez::svg::SubPath> subPaths;
    CTEST_ASSERT(local_parse("m10,10l5-5-5-5", subPaths));
    CTEST_ASSERT(subPaths.size() == 1u);
    CTEST_ASSERT(local_near(subPaths[0].start, 10.0, 10.0));
    CTEST_ASSERT(subPaths[0].segments.size() == 2u);
    CTEST_ASSERT(local_near(subPaths[0].segments[0].end, 15.0, 5.0));
    CTEST_ASSERT(local_near(subPaths[0].segments[1].end, 10.0, 0.0));
    CTEST_ASSERT(!subPaths[0].closed);
    std::vector<ez::svg::SubPath> glued;
    CTEST_ASSERT(local_parse("M1.5.5L2e1-3E-1", glued));
    CTEST_ASSERT(glued.size() == 1u);
    CTEST_ASSERT(local_near(glued[0].start, 1.5, 0.5));
    CTEST_ASSERT(local_near(glued[0].segments[0].end, 20.0, -0.3));
    return true;
}

// the pairs after a moveto are linetos, with the relativity of the moveto
bool TestEzSvgPathParser_ImplicitLinetoAfterMoveto() {
    std::vector<ez::svg::SubPath> subPaths;
    CTEST_ASSERT(local_parse("M0 0 10 0 10 10", subPaths));
    CTEST_ASSERT(subPaths.size() == 1u);
    CTEST_ASSERT(subPaths[0].segments.size() == 2u);
    CTEST_ASSERT(local_near(subPaths[0].segments[1].end, 10.0, 10.0));
    std::vector<ez::svg::SubPath> relative;
    CTEST_ASSERT(local_parse("m5 5 10 0 0 10", relative));
    CTEST_ASSERT(relative[0].segments.size() == 2u);
    CTEST_ASSERT(local_near(relative[0].segments[0].end, 15.0, 5.0));
    CTEST_ASSERT(local_near(relative[0].segments[1].end, 15.0, 15.0));
    return true;
}

// S reflects the last cubic control about the current point ; after a
// non-curve command the first control is the current point itself
bool TestEzSvgPathParser_SmoothCurvesReflectTheControl() {
    std::vector<ez::svg::SubPath> subPaths;
    CTEST_ASSERT(local_parse("M0 0 C 10 10, 20 10, 30 0 S 50 -10, 60 0", subPaths));
    CTEST_ASSERT(subPaths[0].segments.size() == 2u);
    const ez::svg::Segment& second = subPaths[0].segments[1];
    CTEST_ASSERT(second.kind == ez::svg::SegmentKind::Cubic);
    CTEST_ASSERT(local_near(second.control0, 40.0, -10.0));
    CTEST_ASSERT(local_near(second.control1, 50.0, -10.0));
    CTEST_ASSERT(local_near(second.end, 60.0, 0.0));
    std::vector<ez::svg::SubPath> afterLine;
    CTEST_ASSERT(local_parse("M0 0 L 10 0 S 20 10, 30 0", afterLine));
    CTEST_ASSERT(local_near(afterLine[0].segments[1].control0, 10.0, 0.0));
    return true;
}

// a quadratic is elevated to the cubic that IS the quadratic, and T
// reflects the last quadratic control
bool TestEzSvgPathParser_QuadraticsAreElevated() {
    std::vector<ez::svg::SubPath> subPaths;
    CTEST_ASSERT(local_parse("M0 0 Q 10 10 20 0 T 40 0", subPaths));
    CTEST_ASSERT(subPaths[0].segments.size() == 2u);
    const ez::svg::Segment& first = subPaths[0].segments[0];
    CTEST_ASSERT(first.kind == ez::svg::SegmentKind::Cubic);
    CTEST_ASSERT(local_near(first.control0, 20.0 / 3.0, 20.0 / 3.0));
    CTEST_ASSERT(local_near(first.control1, 20.0 - 20.0 / 3.0, 20.0 / 3.0));
    // T : the control is the reflection of (10, 10) about (20, 0) = (30, -10)
    const ez::svg::Segment& second = subPaths[0].segments[1];
    CTEST_ASSERT(local_near(second.control0, 20.0 + (30.0 - 20.0) * 2.0 / 3.0, -10.0 * 2.0 / 3.0));
    CTEST_ASSERT(local_near(second.end, 40.0, 0.0));
    return true;
}

// a quarter circle arc gives one cubic that stays on its circle ; a
// zero radius arc is a line, as the spec says
bool TestEzSvgPathParser_ArcsBecomeCubics() {
    std::vector<ez::svg::SubPath> subPaths;
    CTEST_ASSERT(local_parse("M10 0 A10 10 0 0 1 0 10", subPaths));
    CTEST_ASSERT(subPaths[0].segments.size() == 1u);
    const ez::svg::Segment& arc = subPaths[0].segments[0];
    CTEST_ASSERT(arc.kind == ez::svg::SegmentKind::Cubic);
    CTEST_ASSERT(local_near(arc.end, 0.0, 10.0));
    const ez::svg::Point middle = ez::math::bezier::cubicAt(subPaths[0].start, arc.control0, arc.control1, arc.end, 0.5);
    const double radius = std::sqrt(middle.x * middle.x + middle.y * middle.y);
    CTEST_ASSERT(std::fabs(radius - 10.0) < 0.05);
    CTEST_ASSERT((middle.x > 0.0) && (middle.y > 0.0));  // the small arc through the first quadrant
    std::vector<ez::svg::SubPath> flat;
    CTEST_ASSERT(local_parse("M0 0 A0 0 0 0 1 10 10", flat));
    CTEST_ASSERT(flat[0].segments.size() == 1u);
    CTEST_ASSERT(flat[0].segments[0].kind == ez::svg::SegmentKind::Line);
    CTEST_ASSERT(local_near(flat[0].segments[0].end, 10.0, 10.0));
    return true;
}

// the arc flags are single characters and may be glued to what follows
bool TestEzSvgPathParser_GluedArcFlags() {
    std::vector<ez::svg::SubPath> subPaths;
    CTEST_ASSERT(local_parse("M0 0a5 5 0 0110 0", subPaths));
    CTEST_ASSERT(!subPaths[0].segments.empty());
    CTEST_ASSERT(local_near(subPaths[0].segments.back().end, 10.0, 0.0));
    return true;
}

// z brings the current point back to the start of the sub path ; a
// segment after it opens a new sub path there
bool TestEzSvgPathParser_ClosingReturnsToTheStart() {
    std::vector<ez::svg::SubPath> subPaths;
    CTEST_ASSERT(local_parse("M10 10 L20 10 L20 20 Z l0 10", subPaths));
    CTEST_ASSERT(subPaths.size() == 2u);
    CTEST_ASSERT(subPaths[0].closed);
    CTEST_ASSERT(!subPaths[1].closed);
    CTEST_ASSERT(local_near(subPaths[1].start, 10.0, 10.0));
    CTEST_ASSERT(local_near(subPaths[1].segments[0].end, 10.0, 20.0));
    return true;
}

// a path without a moveto, an unknown command, a missing number : false
// with a readable error, and what parsed before stays
bool TestEzSvgPathParser_RefusesABrokenPath() {
    std::vector<ez::svg::SubPath> subPaths;
    std::string error;
    CTEST_ASSERT(!ez::svg::PathParser::parse("L10 10", subPaths, error));
    CTEST_ASSERT(!error.empty());
    CTEST_ASSERT(!ez::svg::PathParser::parse("M0 0 X 1", subPaths, error));
    CTEST_ASSERT(!ez::svg::PathParser::parse("M0 0 L 5", subPaths, error));
    CTEST_ASSERT(!ez::svg::PathParser::parse("M0 0 Z 5 5", subPaths, error));
    std::vector<ez::svg::SubPath> partial;
    CTEST_ASSERT(!ez::svg::PathParser::parse("M0 0 L 5 5 Z M1 1 L", partial, error));
    CTEST_ASSERT(partial.size() == 1u);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzSvgPathParser(const std::string& vTest) {
    IfTestExist(TestEzSvgPathParser_ASquareIsThreeLinesAndAClose);
    else IfTestExist(TestEzSvgPathParser_RelativeAndGluedNumbers);
    else IfTestExist(TestEzSvgPathParser_ImplicitLinetoAfterMoveto);
    else IfTestExist(TestEzSvgPathParser_SmoothCurvesReflectTheControl);
    else IfTestExist(TestEzSvgPathParser_QuadraticsAreElevated);
    else IfTestExist(TestEzSvgPathParser_ArcsBecomeCubics);
    else IfTestExist(TestEzSvgPathParser_GluedArcFlags);
    else IfTestExist(TestEzSvgPathParser_ClosingReturnsToTheStart);
    else IfTestExist(TestEzSvgPathParser_RefusesABrokenPath);
    return false;
}
