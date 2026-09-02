#include <TestEzTtfOutline.h>

#include <cmath>
#include <string>
#include <vector>

#include <ezlibs/ezTTF/ezTTF.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

bool local_is(const ez::ttf::GlyphPoint& aPoint, int16_t aX, int16_t aY, bool aOnCurve) {
    return (aPoint.x == aX) && (aPoint.y == aY) && (aPoint.onCurve == aOnCurve);
}

}  // namespace

bool TestEzTtfOutline_ASquareIsOneClosedContour() {
    ez::ttf::OutlinePen pen;
    pen.moveTo(0.0, 0.0);
    pen.lineTo(100.0, 0.0);
    pen.lineTo(100.0, 100.0);
    pen.lineTo(0.0, 100.0);
    pen.close();
    ez::ttf::Glyph glyph;
    CTEST_ASSERT(pen.toGlyph(glyph) == 1u);
    CTEST_ASSERT(!glyph.isComposite);
    CTEST_ASSERT(glyph.contours[0].size() == 4u);
    CTEST_ASSERT(local_is(glyph.contours[0][0], 0, 0, true));
    CTEST_ASSERT(local_is(glyph.contours[0][2], 100, 100, true));
    CTEST_ASSERT((glyph.xMin == 0) && (glyph.yMin == 0) && (glyph.xMax == 100) && (glyph.yMax == 100));
    // two contours : the ring of a square in a square
    pen.moveTo(25.0, 25.0);
    pen.lineTo(25.0, 75.0);
    pen.lineTo(75.0, 75.0);
    pen.lineTo(75.0, 25.0);
    pen.close();
    CTEST_ASSERT(pen.toGlyph(glyph) == 2u);
    return true;
}

// a circle of four cubics (the kappa constant) : the on-curve points sit
// on the circle, the off-curve controls just outside it
bool TestEzTtfOutline_ACircleOfCubicsBecomesQuadratics() {
    const double radius = 500.0;
    const double kappa = 0.5522847498 * radius;
    ez::ttf::OutlinePen pen;
    pen.setTolerance(0.5);
    pen.moveTo(radius, 0.0);
    pen.cubicTo(radius, kappa, kappa, radius, 0.0, radius);
    pen.cubicTo(-kappa, radius, -radius, kappa, -radius, 0.0);
    pen.cubicTo(-radius, -kappa, -kappa, -radius, 0.0, -radius);
    pen.cubicTo(kappa, -radius, radius, -kappa, radius, 0.0);
    pen.close();
    ez::ttf::Glyph glyph;
    CTEST_ASSERT(pen.toGlyph(glyph) == 1u);
    const std::vector<ez::ttf::GlyphPoint>& contour = glyph.contours[0];
    CTEST_ASSERT(contour.size() > 8u);
    std::size_t onCount = 0;
    std::size_t offCount = 0;
    for (std::size_t pointIdx = 0; pointIdx < contour.size(); ++pointIdx) {
        const double distance = std::sqrt(static_cast<double>(contour[pointIdx].x) * contour[pointIdx].x + static_cast<double>(contour[pointIdx].y) * contour[pointIdx].y);
        if (contour[pointIdx].onCurve) {
            CTEST_ASSERT(std::fabs(distance - radius) < 2.0);
            ++onCount;
        } else {
            CTEST_ASSERT((distance > radius - 1.0) && (distance < radius * 1.2));
            ++offCount;
        }
    }
    CTEST_ASSERT(onCount == offCount);  // one control between two on-curve points, all around
    CTEST_ASSERT(std::abs(static_cast<int32_t>(glyph.xMax) - 500) <= 1);
    CTEST_ASSERT(std::abs(static_cast<int32_t>(glyph.yMin) + 500) <= 1);
    return true;
}

// the svg frame (y down) into the font frame (y up) with a scale
bool TestEzTtfOutline_TheTransformFlipsAndScales() {
    ez::ttf::OutlinePen pen;
    pen.setTransform(2.0, 0.0, 0.0, -2.0, 0.0, 200.0);
    pen.moveTo(0.0, 0.0);
    pen.lineTo(10.0, 0.0);
    pen.lineTo(10.0, 10.0);
    pen.close();
    ez::ttf::Glyph glyph;
    CTEST_ASSERT(pen.toGlyph(glyph) == 1u);
    CTEST_ASSERT(local_is(glyph.contours[0][0], 0, 200, true));
    CTEST_ASSERT(local_is(glyph.contours[0][1], 20, 200, true));
    CTEST_ASSERT(local_is(glyph.contours[0][2], 20, 180, true));
    CTEST_ASSERT((glyph.yMin == 180) && (glyph.yMax == 200));
    return true;
}

// an explicit line back to the start is not a point : TrueType closes by
// itself. a contour under two points is not a contour
bool TestEzTtfOutline_DuplicateClosingPointIsDropped() {
    ez::ttf::OutlinePen pen;
    pen.moveTo(0.0, 0.0);
    pen.lineTo(50.0, 0.0);
    pen.lineTo(50.0, 50.0);
    pen.lineTo(0.0, 0.0);
    pen.close();
    pen.moveTo(7.0, 7.0);  // a lone point
    pen.close();
    pen.moveTo(1.0, 1.0);
    pen.lineTo(1.0, 1.0);  // the same point twice
    pen.close();
    ez::ttf::Glyph glyph;
    CTEST_ASSERT(pen.toGlyph(glyph) == 1u);
    CTEST_ASSERT(glyph.contours[0].size() == 3u);
    return true;
}

bool TestEzTtfOutline_QuantizationRoundsAndClamps() {
    ez::ttf::OutlinePen pen;
    pen.moveTo(0.4, 0.6);
    pen.lineTo(40000.0, -40000.0);
    pen.lineTo(30000.0, -30000.0);
    pen.lineTo(5.49, 5.5);
    pen.close();
    ez::ttf::Glyph glyph;
    CTEST_ASSERT(pen.toGlyph(glyph) == 1u);
    CTEST_ASSERT(local_is(glyph.contours[0][0], 0, 1, true));
    CTEST_ASSERT(local_is(glyph.contours[0][1], 32767, -32768, true));
    CTEST_ASSERT(local_is(glyph.contours[0][2], 30000, -30000, true));
    CTEST_ASSERT(local_is(glyph.contours[0][3], 5, 6, true));
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzTtfOutline(const std::string& vTest) {
    IfTestExist(TestEzTtfOutline_ASquareIsOneClosedContour);
    else IfTestExist(TestEzTtfOutline_ACircleOfCubicsBecomesQuadratics);
    else IfTestExist(TestEzTtfOutline_TheTransformFlipsAndScales);
    else IfTestExist(TestEzTtfOutline_DuplicateClosingPointIsDropped);
    else IfTestExist(TestEzTtfOutline_QuantizationRoundsAndClamps);
    return false;
}
