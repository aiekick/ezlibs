#include <TestEzBezier.h>

#include <cmath>
#include <string>
#include <vector>

#include <ezlibs/ezMath/ezBezier.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

typedef ez::math::dvec2 dvec2;

double local_distance(const dvec2& aFirst, const dvec2& aSecond) {
    const double dx = aSecond.x - aFirst.x;
    const double dy = aSecond.y - aFirst.y;
    return std::sqrt(dx * dx + dy * dy);
}

// the nearest distance from a point to a densely sampled cubic
double local_distanceToCubic(const dvec2& aPoint, const dvec2& aP0, const dvec2& aC0, const dvec2& aC1, const dvec2& aP1) {
    double best = 1e300;
    const int32_t sampleCount = 2000;
    for (int32_t sampleIdx = 0; sampleIdx <= sampleCount; ++sampleIdx) {
        const double t = static_cast<double>(sampleIdx) / static_cast<double>(sampleCount);
        const double distance = local_distance(aPoint, ez::math::bezier::cubicAt(aP0, aC0, aC1, aP1, t));
        if (distance < best) {
            best = distance;
        }
    }
    return best;
}

}  // namespace

// the law of the split : every quadratic of the chain stays within the
// tolerance of the cubic, the chain is continuous and lands on the end
bool TestEzBezier_CubicToQuadraticsStaysWithinTolerance() {
    const dvec2 p0(0.0, 0.0);
    const dvec2 c0(30.0, 80.0);
    const dvec2 c1(70.0, 80.0);
    const dvec2 p1(100.0, 0.0);
    const double tolerance = 0.5;
    std::vector<ez::math::bezier::quad2> quads;
    ez::math::bezier::cubicToQuadratics(p0, c0, c1, p1, tolerance, quads);
    CTEST_ASSERT(quads.size() >= 2u);
    CTEST_ASSERT(quads.size() <= 64u);
    dvec2 start = p0;
    for (std::size_t quadIdx = 0; quadIdx < quads.size(); ++quadIdx) {
        for (int32_t sampleIdx = 0; sampleIdx <= 20; ++sampleIdx) {
            const double t = static_cast<double>(sampleIdx) / 20.0;
            const dvec2 point = ez::math::bezier::quadraticAt(start, quads[quadIdx].control, quads[quadIdx].end, t);
            CTEST_ASSERT(local_distanceToCubic(point, p0, c0, c1, p1) <= tolerance + 0.1);
        }
        start = quads[quadIdx].end;
    }
    CTEST_ASSERT(local_distance(start, p1) < 1e-9);
    // a tighter tolerance never gives fewer quadratics
    std::vector<ez::math::bezier::quad2> finer;
    ez::math::bezier::cubicToQuadratics(p0, c0, c1, p1, 0.05, finer);
    CTEST_ASSERT(finer.size() >= quads.size());
    return true;
}

// the elevation is exact : a cubic that is a quadratic splits into ONE
// quadratic, with the original control back
bool TestEzBezier_AnElevatedQuadraticGivesOneQuadratic() {
    const dvec2 p0(0.0, 0.0);
    const dvec2 control(50.0, 100.0);
    const dvec2 p1(100.0, 0.0);
    dvec2 c0;
    dvec2 c1;
    ez::math::bezier::quadraticToCubic(p0, control, p1, c0, c1);
    for (int32_t sampleIdx = 0; sampleIdx <= 10; ++sampleIdx) {
        const double t = static_cast<double>(sampleIdx) / 10.0;
        CTEST_ASSERT(local_distance(ez::math::bezier::quadraticAt(p0, control, p1, t), ez::math::bezier::cubicAt(p0, c0, c1, p1, t)) < 1e-9);
    }
    std::vector<ez::math::bezier::quad2> quads;
    ez::math::bezier::cubicToQuadratics(p0, c0, c1, p1, 0.01, quads);
    CTEST_ASSERT(quads.size() == 1u);
    CTEST_ASSERT(local_distance(quads[0].control, control) < 1e-9);
    CTEST_ASSERT(local_distance(quads[0].end, p1) < 1e-9);
    return true;
}

// a quarter turn is one cubic whose middle lies on the circle (the
// classic approximation is within three parts in ten thousand)
bool TestEzBezier_ArcOfAQuarterTurnLandsOnTheCircle() {
    std::vector<ez::math::bezier::cubic2> cubics;
    const dvec2 p0(10.0, 0.0);
    const dvec2 p1(0.0, 10.0);
    CTEST_ASSERT(ez::math::bezier::arcToCubics(p0, 10.0, 10.0, 0.0, false, true, p1, cubics));
    CTEST_ASSERT(cubics.size() == 1u);
    CTEST_ASSERT(local_distance(cubics[0].end, p1) < 1e-12);
    const dvec2 middle = ez::math::bezier::cubicAt(p0, cubics[0].control0, cubics[0].control1, cubics[0].end, 0.5);
    CTEST_ASSERT(std::fabs(local_distance(middle, dvec2(0.0, 0.0)) - 10.0) < 0.005);
    CTEST_ASSERT((middle.x > 0.0) && (middle.y > 0.0));
    // the other sweep goes the long way round through the other three quadrants
    std::vector<ez::math::bezier::cubic2> longWay;
    CTEST_ASSERT(ez::math::bezier::arcToCubics(p0, 10.0, 10.0, 0.0, true, false, p1, longWay));
    CTEST_ASSERT(longWay.size() == 3u);
    CTEST_ASSERT(local_distance(longWay[2].end, p1) < 1e-12);
    return true;
}

// radii that cannot reach the end point are scaled up until they do :
// the arc is then a half circle whose middle is one radius away
bool TestEzBezier_ArcRadiiTooSmallAreScaledUp() {
    std::vector<ez::math::bezier::cubic2> cubics;
    const dvec2 p0(0.0, 0.0);
    const dvec2 p1(100.0, 0.0);
    CTEST_ASSERT(ez::math::bezier::arcToCubics(p0, 10.0, 10.0, 0.0, false, true, p1, cubics));
    CTEST_ASSERT(cubics.size() == 2u);
    const dvec2 middle = cubics[0].end;
    CTEST_ASSERT(std::fabs(middle.x - 50.0) < 1e-9);
    CTEST_ASSERT(std::fabs(std::fabs(middle.y) - 50.0) < 1e-9);
    return true;
}

// coincident points or a zero radius : no arc, the caller draws a line
bool TestEzBezier_DegenerateArcsAreLines() {
    std::vector<ez::math::bezier::cubic2> cubics;
    CTEST_ASSERT(!ez::math::bezier::arcToCubics(dvec2(1.0, 1.0), 5.0, 5.0, 0.0, false, true, dvec2(1.0, 1.0), cubics));
    CTEST_ASSERT(!ez::math::bezier::arcToCubics(dvec2(0.0, 0.0), 0.0, 5.0, 0.0, false, true, dvec2(1.0, 1.0), cubics));
    CTEST_ASSERT(!ez::math::bezier::arcToCubics(dvec2(0.0, 0.0), 5.0, 0.0, 0.0, false, true, dvec2(1.0, 1.0), cubics));
    CTEST_ASSERT(cubics.empty());
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzBezier(const std::string& vTest) {
    IfTestExist(TestEzBezier_CubicToQuadraticsStaysWithinTolerance);
    else IfTestExist(TestEzBezier_AnElevatedQuadraticGivesOneQuadratic);
    else IfTestExist(TestEzBezier_ArcOfAQuarterTurnLandsOnTheCircle);
    else IfTestExist(TestEzBezier_ArcRadiiTooSmallAreScaledUp);
    else IfTestExist(TestEzBezier_DegenerateArcsAreLines);
    return false;
}
