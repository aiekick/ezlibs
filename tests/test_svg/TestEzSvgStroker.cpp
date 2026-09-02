#include <TestEzSvgStroker.h>

#include <cmath>
#include <string>
#include <vector>

#include <ezlibs/ezSvg/ezSvg.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

typedef ez::svg::Point Point;

double local_distance(const Point& aFirst, const Point& aSecond) {
    const double dx = aSecond.x - aFirst.x;
    const double dy = aSecond.y - aFirst.y;
    return std::sqrt(dx * dx + dy * dy);
}

// the distance from a point to a segment
double local_distanceToSegment(const Point& aPoint, const Point& aFrom, const Point& aTo) {
    const double dx = aTo.x - aFrom.x;
    const double dy = aTo.y - aFrom.y;
    const double lengthSquared = dx * dx + dy * dy;
    double t = (lengthSquared > 0.0) ? (((aPoint.x - aFrom.x) * dx + (aPoint.y - aFrom.y) * dy) / lengthSquared) : 0.0;
    t = (t < 0.0) ? 0.0 : ((t > 1.0) ? 1.0 : t);
    return local_distance(aPoint, Point(aFrom.x + dx * t, aFrom.y + dy * t));
}

double local_distanceToPolyline(const Point& aPoint, const std::vector<Point>& aPolyline) {
    double best = 1e300;
    for (std::size_t pointIdx = 0; pointIdx + 1u < aPolyline.size(); ++pointIdx) {
        const double distance = local_distanceToSegment(aPoint, aPolyline[pointIdx], aPolyline[pointIdx + 1u]);
        best = (distance < best) ? distance : best;
    }
    return best;
}

// the points of a contour, its start first
std::vector<Point> local_points(const ez::svg::SubPath& aContour) {
    std::vector<Point> points;
    points.push_back(aContour.start);
    for (std::size_t segmentIdx = 0; segmentIdx < aContour.segments.size(); ++segmentIdx) {
        points.push_back(aContour.segments[segmentIdx].end);
    }
    return points;
}

double local_signedArea(const std::vector<Point>& aPoints) {
    double area = 0.0;
    for (std::size_t pointIdx = 0; pointIdx < aPoints.size(); ++pointIdx) {
        const Point& current = aPoints[pointIdx];
        const Point& next = aPoints[(pointIdx + 1u) % aPoints.size()];
        area += current.x * next.y - next.x * current.y;
    }
    return area * 0.5;
}

void local_bounds(const std::vector<Point>& aPoints, double& aoMinX, double& aoMinY, double& aoMaxX, double& aoMaxY) {
    aoMinX = aoMinY = 1e300;
    aoMaxX = aoMaxY = -1e300;
    for (std::size_t pointIdx = 0; pointIdx < aPoints.size(); ++pointIdx) {
        aoMinX = (aPoints[pointIdx].x < aoMinX) ? aPoints[pointIdx].x : aoMinX;
        aoMinY = (aPoints[pointIdx].y < aoMinY) ? aPoints[pointIdx].y : aoMinY;
        aoMaxX = (aPoints[pointIdx].x > aoMaxX) ? aPoints[pointIdx].x : aoMaxX;
        aoMaxY = (aPoints[pointIdx].y > aoMaxY) ? aPoints[pointIdx].y : aoMaxY;
    }
}

bool local_hasPointNear(const std::vector<Point>& aPoints, double aX, double aY, double aTolerance) {
    for (std::size_t pointIdx = 0; pointIdx < aPoints.size(); ++pointIdx) {
        if (local_distance(aPoints[pointIdx], Point(aX, aY)) <= aTolerance) {
            return true;
        }
    }
    return false;
}

ez::svg::SubPath local_polyline(const std::vector<Point>& aPoints, bool aClosed) {
    ez::svg::SubPath subPath;
    subPath.start = aPoints[0];
    for (std::size_t pointIdx = 1; pointIdx < aPoints.size(); ++pointIdx) {
        subPath.segments.push_back(ez::svg::Segment::line(aPoints[pointIdx]));
    }
    subPath.closed = aClosed;
    return subPath;
}

ez::svg::Stroke local_stroke(double aWidth, ez::svg::LineCap aCap, ez::svg::LineJoin aJoin, double aMiterLimit = 4.0) {
    ez::svg::Stroke stroke;
    stroke.paint = ez::svg::Paint::color(0, 0, 0);
    stroke.width = aWidth;
    stroke.cap = aCap;
    stroke.join = aJoin;
    stroke.miterLimit = aMiterLimit;
    return stroke;
}

}  // namespace

// a stroked line with butt caps is its rectangle, oriented as asked
bool TestEzSvgStroker_AHorizontalLineIsARectangle() {
    std::vector<Point> line;
    line.push_back(Point(0.0, 0.0));
    line.push_back(Point(10.0, 0.0));
    std::vector<ez::svg::SubPath> contours;
    ez::svg::Stroker::stroke(local_polyline(line, false), local_stroke(2.0, ez::svg::LineCap::Butt, ez::svg::LineJoin::Miter), 0.01, 1, contours);
    CTEST_ASSERT(contours.size() == 1u);
    CTEST_ASSERT(contours[0].closed);
    const std::vector<Point> points = local_points(contours[0]);
    CTEST_ASSERT(points.size() == 4u);
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    local_bounds(points, minX, minY, maxX, maxY);
    CTEST_ASSERT(std::fabs(minX - 0.0) < 1e-9 && std::fabs(maxX - 10.0) < 1e-9);
    CTEST_ASSERT(std::fabs(minY + 1.0) < 1e-9 && std::fabs(maxY - 1.0) < 1e-9);
    CTEST_ASSERT(local_signedArea(points) > 0.0);
    std::vector<ez::svg::SubPath> reversed;
    ez::svg::Stroker::stroke(local_polyline(line, false), local_stroke(2.0, ez::svg::LineCap::Butt, ez::svg::LineJoin::Miter), 0.01, -1, reversed);
    CTEST_ASSERT(reversed.size() == 1u);
    CTEST_ASSERT(local_signedArea(local_points(reversed[0])) < 0.0);
    // the square cap extends both ends by the half width
    std::vector<ez::svg::SubPath> square;
    ez::svg::Stroker::stroke(local_polyline(line, false), local_stroke(2.0, ez::svg::LineCap::Square, ez::svg::LineJoin::Miter), 0.01, 1, square);
    CTEST_ASSERT(square.size() == 1u);
    local_bounds(local_points(square[0]), minX, minY, maxX, maxY);
    CTEST_ASSERT(std::fabs(minX + 1.0) < 1e-9 && std::fabs(maxX - 11.0) < 1e-9);
    return true;
}

// round caps and joins stay on their circles : every point of the band
// is within the half width of the polyline, the tips reach it
bool TestEzSvgStroker_RoundCapsAndJoinsStayOnTheirCircles() {
    std::vector<Point> line;
    line.push_back(Point(0.0, 0.0));
    line.push_back(Point(10.0, 0.0));
    std::vector<ez::svg::SubPath> contours;
    ez::svg::Stroker::stroke(local_polyline(line, false), local_stroke(2.0, ez::svg::LineCap::Round, ez::svg::LineJoin::Round), 0.001, 1, contours);
    CTEST_ASSERT(contours.size() == 1u);
    std::vector<Point> points = local_points(contours[0]);
    CTEST_ASSERT(points.size() > 8u);
    for (std::size_t pointIdx = 0; pointIdx < points.size(); ++pointIdx) {
        CTEST_ASSERT(local_distanceToPolyline(points[pointIdx], line) <= 1.0 + 1e-6);
    }
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    local_bounds(points, minX, minY, maxX, maxY);
    CTEST_ASSERT(minX < -0.95 && maxX > 10.95);
    CTEST_ASSERT(minY < -0.95 && maxY > 0.95);
    // a right angle with a round join : the outer corner is rounded around the vertex
    std::vector<Point> corner;
    corner.push_back(Point(0.0, 0.0));
    corner.push_back(Point(10.0, 0.0));
    corner.push_back(Point(10.0, 10.0));
    std::vector<ez::svg::SubPath> rounded;
    ez::svg::Stroker::stroke(local_polyline(corner, false), local_stroke(2.0, ez::svg::LineCap::Butt, ez::svg::LineJoin::Round), 0.001, 1, rounded);
    CTEST_ASSERT(rounded.size() == 1u);
    points = local_points(rounded[0]);
    for (std::size_t pointIdx = 0; pointIdx < points.size(); ++pointIdx) {
        CTEST_ASSERT(local_distanceToPolyline(points[pointIdx], corner) <= 1.0 + 1e-6);
    }
    CTEST_ASSERT(local_hasPointNear(points, 10.0 + std::cos(0.25 * 3.14159265358979323846), -std::sin(0.25 * 3.14159265358979323846), 0.05));
    return true;
}

// a miter join makes the sharp corner, within its limit ; beyond, it
// falls back to the bevel and nothing leaves the band
bool TestEzSvgStroker_AMiterJoinMakesTheCornerWithinItsLimit() {
    std::vector<Point> corner;
    corner.push_back(Point(0.0, 0.0));
    corner.push_back(Point(10.0, 0.0));
    corner.push_back(Point(10.0, 10.0));
    std::vector<ez::svg::SubPath> mitered;
    ez::svg::Stroker::stroke(local_polyline(corner, false), local_stroke(2.0, ez::svg::LineCap::Butt, ez::svg::LineJoin::Miter, 4.0), 0.01, 1, mitered);
    CTEST_ASSERT(mitered.size() == 1u);
    CTEST_ASSERT(local_hasPointNear(local_points(mitered[0]), 11.0, -1.0, 1e-6));
    std::vector<ez::svg::SubPath> beveled;
    ez::svg::Stroker::stroke(local_polyline(corner, false), local_stroke(2.0, ez::svg::LineCap::Butt, ez::svg::LineJoin::Miter, 1.0), 0.01, 1, beveled);
    CTEST_ASSERT(beveled.size() == 1u);
    const std::vector<Point> points = local_points(beveled[0]);
    for (std::size_t pointIdx = 0; pointIdx < points.size(); ++pointIdx) {
        CTEST_ASSERT(local_distanceToPolyline(points[pointIdx], corner) <= 1.0 + 1e-6);
    }
    return true;
}

// a closed path gives a ring : the outer contour in the asked
// orientation, the inner one opposite — a band with a hole
bool TestEzSvgStroker_AClosedPathIsARingOfTwoOppositeContours() {
    std::vector<Point> square;
    square.push_back(Point(0.0, 0.0));
    square.push_back(Point(10.0, 0.0));
    square.push_back(Point(10.0, 10.0));
    square.push_back(Point(0.0, 10.0));
    std::vector<ez::svg::SubPath> contours;
    ez::svg::Stroker::stroke(local_polyline(square, true), local_stroke(2.0, ez::svg::LineCap::Butt, ez::svg::LineJoin::Miter), 0.01, -1, contours);
    CTEST_ASSERT(contours.size() == 2u);
    const std::vector<Point> outer = local_points(contours[0]);
    const std::vector<Point> inner = local_points(contours[1]);
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    local_bounds(outer, minX, minY, maxX, maxY);
    CTEST_ASSERT(std::fabs(minX + 1.0) < 1e-9 && std::fabs(maxX - 11.0) < 1e-9 && std::fabs(minY + 1.0) < 1e-9 && std::fabs(maxY - 11.0) < 1e-9);
    local_bounds(inner, minX, minY, maxX, maxY);
    CTEST_ASSERT(std::fabs(minX - 1.0) < 1e-9 && std::fabs(maxX - 9.0) < 1e-9 && std::fabs(minY - 1.0) < 1e-9 && std::fabs(maxY - 9.0) < 1e-9);
    CTEST_ASSERT(local_signedArea(outer) < 0.0);
    CTEST_ASSERT(local_signedArea(inner) > 0.0);
    // the same square walked the other way round gives the same ring
    std::vector<Point> backwards(square.rbegin(), square.rend());
    std::vector<ez::svg::SubPath> again;
    ez::svg::Stroker::stroke(local_polyline(backwards, true), local_stroke(2.0, ez::svg::LineCap::Butt, ez::svg::LineJoin::Miter), 0.01, -1, again);
    CTEST_ASSERT(again.size() == 2u);
    CTEST_ASSERT(local_signedArea(local_points(again[0])) < 0.0);
    CTEST_ASSERT(local_signedArea(local_points(again[1])) > 0.0);
    return true;
}

// a zero length sub path : a disc with the round cap, a square with the
// square cap, nothing with the butt cap (the svg rule)
bool TestEzSvgStroker_ADotFollowsItsCap() {
    std::vector<Point> dot;
    dot.push_back(Point(5.0, 5.0));
    dot.push_back(Point(5.0, 5.0));
    std::vector<ez::svg::SubPath> disc;
    ez::svg::Stroker::stroke(local_polyline(dot, false), local_stroke(4.0, ez::svg::LineCap::Round, ez::svg::LineJoin::Miter), 0.01, 1, disc);
    CTEST_ASSERT(disc.size() == 1u);
    const std::vector<Point> points = local_points(disc[0]);
    CTEST_ASSERT(points.size() >= 8u);
    for (std::size_t pointIdx = 0; pointIdx < points.size(); ++pointIdx) {
        CTEST_ASSERT(std::fabs(local_distance(points[pointIdx], Point(5.0, 5.0)) - 2.0) < 1e-6);
    }
    std::vector<ez::svg::SubPath> square;
    ez::svg::Stroker::stroke(local_polyline(dot, false), local_stroke(4.0, ez::svg::LineCap::Square, ez::svg::LineJoin::Miter), 0.01, 1, square);
    CTEST_ASSERT(square.size() == 1u);
    CTEST_ASSERT(local_points(square[0]).size() == 4u);
    std::vector<ez::svg::SubPath> nothing;
    ez::svg::Stroker::stroke(local_polyline(dot, false), local_stroke(4.0, ez::svg::LineCap::Butt, ez::svg::LineJoin::Miter), 0.01, 1, nothing);
    CTEST_ASSERT(nothing.empty());
    return true;
}

// a curve is flattened within the tolerance before the offset : the
// band of a quarter circle stays between its two radii
bool TestEzSvgStroker_CurvesAreFlattenedWithinTolerance() {
    const double kappa = 0.5522847498 * 10.0;
    ez::svg::SubPath arc;
    arc.start = Point(10.0, 0.0);
    arc.segments.push_back(ez::svg::Segment::cubic(Point(10.0, kappa), Point(kappa, 10.0), Point(0.0, 10.0)));
    std::vector<Point> flat;
    ez::svg::Stroker::flatten(arc, 0.05, flat);
    CTEST_ASSERT(flat.size() > 4u);
    for (std::size_t pointIdx = 0; pointIdx < flat.size(); ++pointIdx) {
        CTEST_ASSERT(std::fabs(local_distance(flat[pointIdx], Point(0.0, 0.0)) - 10.0) < 0.05);  // the kappa circle is within 0.03 percent
    }
    std::vector<ez::svg::SubPath> contours;
    ez::svg::Stroker::stroke(arc, local_stroke(2.0, ez::svg::LineCap::Butt, ez::svg::LineJoin::Round), 0.05, 1, contours);
    CTEST_ASSERT(contours.size() == 1u);
    const std::vector<Point> points = local_points(contours[0]);
    for (std::size_t pointIdx = 0; pointIdx < points.size(); ++pointIdx) {
        const double radius = local_distance(points[pointIdx], Point(0.0, 0.0));
        CTEST_ASSERT((radius > 9.0 - 0.1) && (radius < 11.0 + 0.1));
    }
    CTEST_ASSERT(std::fabs(ez::svg::Stroker::signedArea(local_polyline(std::vector<Point>(1, Point(0.0, 0.0)), true), 0.1)) < 1e-9);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzSvgStroker(const std::string& vTest) {
    IfTestExist(TestEzSvgStroker_AHorizontalLineIsARectangle);
    else IfTestExist(TestEzSvgStroker_RoundCapsAndJoinsStayOnTheirCircles);
    else IfTestExist(TestEzSvgStroker_AMiterJoinMakesTheCornerWithinItsLimit);
    else IfTestExist(TestEzSvgStroker_AClosedPathIsARingOfTwoOppositeContours);
    else IfTestExist(TestEzSvgStroker_ADotFollowsItsCap);
    else IfTestExist(TestEzSvgStroker_CurvesAreFlattenedWithinTolerance);
    return false;
}
