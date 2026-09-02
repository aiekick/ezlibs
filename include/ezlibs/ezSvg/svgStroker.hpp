#pragma once

/*
MIT License

Copyright (c) 2014-2026 Stephane Cuillerdier (aka aiekick)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// ezSvg is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// the STROKER : a stroked sub path into the FILLED outline of its band —
// what a glyph needs, since a font knows no stroke. the sub path is
// flattened, both sides are offset by half the width with the joins of
// the style (miter within its limit, round, bevel) and the caps (butt,
// round, square) ; an open sub path gives ONE closed contour, a closed
// one a ring : the outer contour and, opposite, the inner one. every
// contour comes out with the orientation the caller asks, so the band
// ADDS to the fills it crosses under the nonzero rule instead of
// cancelling them. the overlaps a band makes at its inner corners are
// legal for a nonzero rasterizer

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../ezMath/ezBezier.hpp"
#include "svgTypes.hpp"

namespace ez {
namespace svg {

class Stroker {
public:
    // the polyline of a sub path within aTolerance of its curves : the
    // consecutive duplicates dropped, the closing point NOT repeated
    static void flatten(const SubPath& aSubPath, double aTolerance, std::vector<Point>& aoPoints) {
        aoPoints.clear();
        aoPoints.push_back(aSubPath.start);
        for (std::size_t segmentIdx = 0; segmentIdx < aSubPath.segments.size(); ++segmentIdx) {
            const Segment& segment = aSubPath.segments[segmentIdx];
            if (segment.kind == SegmentKind::Cubic) {
                ez::math::bezier::flattenCubic(aoPoints.back(), segment.control0, segment.control1, segment.end, aTolerance, aoPoints);
            } else {
                aoPoints.push_back(segment.end);
            }
        }
        std::vector<Point> cleaned;
        for (std::size_t pointIdx = 0; pointIdx < aoPoints.size(); ++pointIdx) {
            if (cleaned.empty() || (m_distance(cleaned.back(), aoPoints[pointIdx]) > 1e-9)) {
                cleaned.push_back(aoPoints[pointIdx]);
            }
        }
        while ((cleaned.size() > 1u) && (m_distance(cleaned.front(), cleaned.back()) <= 1e-9)) {
            cleaned.pop_back();
        }
        aoPoints.swap(cleaned);
    }
    // the signed area of the flattened polygon (the shoelace ; its sign
    // is the orientation in the frame of the points)
    static double signedArea(const SubPath& aSubPath, double aTolerance) {
        std::vector<Point> points;
        flatten(aSubPath, aTolerance, points);
        return m_signedArea(points);
    }
    // the band of one stroked sub path as closed polygonal contours,
    // appended to aoContours, every contour oriented to aOrientation
    // (+1 : a positive signed area, -1 : negative) — the inner contour
    // of a ring opposite. aTolerance : the chord error of the curves,
    // the joins and the caps, in the units of the points
    static void stroke(const SubPath& aSubPath, const Stroke& aStroke, double aTolerance, int32_t aOrientation, std::vector<SubPath>& aoContours) {
        const double halfWidth = aStroke.width * 0.5;
        if (halfWidth <= 0.0) {
            return;
        }
        const double tolerance = (aTolerance > 0.0) ? aTolerance : 1e-3;
        const int32_t orientation = (aOrientation < 0) ? -1 : 1;
        std::vector<Point> points;
        flatten(aSubPath, tolerance, points);
        if (points.empty()) {
            return;
        }
        if (points.size() == 1u) {
            // a dot : the round cap draws a disc, the square cap a square,
            // the butt cap nothing (the svg rule for a zero length sub path)
            std::vector<Point> contour;
            if (aStroke.cap == LineCap::Round) {
                m_arc(points[0], halfWidth, 0.0, 2.0 * m_pi(), tolerance, contour);
            } else if (aStroke.cap == LineCap::Square) {
                contour.push_back(Point(points[0].x - halfWidth, points[0].y - halfWidth));
                contour.push_back(Point(points[0].x + halfWidth, points[0].y - halfWidth));
                contour.push_back(Point(points[0].x + halfWidth, points[0].y + halfWidth));
                contour.push_back(Point(points[0].x - halfWidth, points[0].y + halfWidth));
            }
            m_emit(contour, orientation, aoContours);
            return;
        }
        if (aSubPath.closed && (points.size() >= 3u)) {
            std::vector<Point> leftRing;
            std::vector<Point> rightRing;
            m_offsetRing(points, halfWidth, aStroke, tolerance, 1.0, leftRing);
            m_offsetRing(points, halfWidth, aStroke, tolerance, -1.0, rightRing);
            // the outer contour is the wider one, whatever the frame
            const bool leftIsOuter = std::fabs(m_signedArea(leftRing)) >= std::fabs(m_signedArea(rightRing));
            m_emit(leftIsOuter ? leftRing : rightRing, orientation, aoContours);
            m_emit(leftIsOuter ? rightRing : leftRing, -orientation, aoContours);
            return;
        }
        // an open polyline : the left side forward, the end cap, the right
        // side backward, the start cap — one closed band
        std::vector<Point> leftSide;
        std::vector<Point> rightSide;
        m_offsetSide(points, halfWidth, aStroke, tolerance, 1.0, leftSide);
        m_offsetSide(points, halfWidth, aStroke, tolerance, -1.0, rightSide);
        std::vector<Point> contour(leftSide);
        const Point endDirection = m_direction(points[points.size() - 2u], points.back());
        m_cap(points.back(), endDirection, halfWidth, aStroke.cap, tolerance, leftSide.back(), rightSide.back(), contour);
        for (std::size_t pointIdx = rightSide.size() - 1u; pointIdx > 0u; --pointIdx) {
            contour.push_back(rightSide[pointIdx - 1u]);  // the right side backwards, its end already placed by the cap
        }
        const Point startDirection = m_direction(points[1], points[0]);  // leaving the start backwards
        m_cap(points[0], startDirection, halfWidth, aStroke.cap, tolerance, rightSide.front(), leftSide.front(), contour);
        contour.pop_back();  // the start cap landed on the first point : the polygon closes by itself
        m_emit(contour, orientation, aoContours);
    }

private:
    static double m_pi() {
        return 3.14159265358979323846;
    }
    static double m_distance(const Point& aFirst, const Point& aSecond) {
        const double dx = aSecond.x - aFirst.x;
        const double dy = aSecond.y - aFirst.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    static Point m_direction(const Point& aFrom, const Point& aTo) {
        const double length = m_distance(aFrom, aTo);
        return (length > 1e-12) ? Point((aTo.x - aFrom.x) / length, (aTo.y - aFrom.y) / length) : Point(1.0, 0.0);
    }
    // the left normal of a direction (a quarter turn)
    static Point m_normal(const Point& aDirection) {
        return Point(-aDirection.y, aDirection.x);
    }
    static double m_signedArea(const std::vector<Point>& aPoints) {
        double area = 0.0;
        for (std::size_t pointIdx = 0; pointIdx < aPoints.size(); ++pointIdx) {
            const Point& current = aPoints[pointIdx];
            const Point& next = aPoints[(pointIdx + 1u) % aPoints.size()];
            area += current.x * next.y - next.x * current.y;
        }
        return area * 0.5;
    }
    // the angular step of an arc so the chord stays within the tolerance
    static double m_arcStep(double aRadius, double aTolerance) {
        if (aRadius <= aTolerance) {
            return m_pi() * 0.5;
        }
        double step = 2.0 * std::acos(1.0 - aTolerance / aRadius);
        const double minStep = m_pi() / 36.0;  // five degrees : never a fan of thousands
        const double maxStep = m_pi() * 0.5;
        return (step < minStep) ? minStep : ((step > maxStep) ? maxStep : step);
    }
    // the points of an arc around aCenter from aFromAngle sweeping aSweep
    // (signed), the end point included, the start point EXCLUDED
    static void m_arc(const Point& aCenter, double aRadius, double aFromAngle, double aSweep, double aTolerance, std::vector<Point>& aoPoints) {
        const double step = m_arcStep(aRadius, aTolerance);
        const int32_t count = static_cast<int32_t>(std::ceil(std::fabs(aSweep) / step - 1e-9));
        const int32_t segments = (count < 1) ? 1 : count;
        for (int32_t segmentIdx = 1; segmentIdx <= segments; ++segmentIdx) {
            const double angle = aFromAngle + aSweep * static_cast<double>(segmentIdx) / static_cast<double>(segments);
            aoPoints.push_back(Point(aCenter.x + aRadius * std::cos(angle), aCenter.y + aRadius * std::sin(angle)));
        }
    }
    // the join at aVertex between the incoming and the outgoing directions
    // on side aSide (+1 left, -1 right). the OUTER side gets the corner of
    // the style between the two offset points ; the INNER side gets the
    // single point where the two offset lines cross when the segments
    // reach it, and both offset points otherwise (a short segment : the
    // small loop they make is legal under nonzero)
    static void m_join(const Point& aVertex, const Point& aIn, double aInLength, const Point& aOut, double aOutLength, double aHalfWidth, const Stroke& aStroke,
                       double aTolerance, double aSide, std::vector<Point>& aoPoints) {
        const Point normalIn = m_normal(aIn);
        const Point normalOut = m_normal(aOut);
        const Point before(aVertex.x + aSide * normalIn.x * aHalfWidth, aVertex.y + aSide * normalIn.y * aHalfWidth);
        const Point after(aVertex.x + aSide * normalOut.x * aHalfWidth, aVertex.y + aSide * normalOut.y * aHalfWidth);
        const double cross = aIn.x * aOut.y - aIn.y * aOut.x;
        const double dot = aIn.x * aOut.x + aIn.y * aOut.y;
        if (std::fabs(cross) < 1e-9 && dot > 0.0) {
            aoPoints.push_back(after);  // straight on : one point
            return;
        }
        const bool inner = (cross * aSide) > 0.0;
        if (inner) {
            // the crossing of the offset lines sits on the bisector, at
            // hw * tan(turn / 2) from each offset point along its segment
            const double cosine = normalIn.x * normalOut.x + normalIn.y * normalOut.y;
            const double halfTurn = std::acos((dot < -1.0) ? -1.0 : ((dot > 1.0) ? 1.0 : dot)) * 0.5;
            const double reach = aHalfWidth * std::tan(halfTurn);
            if ((1.0 + cosine > 1e-9) && (reach <= aInLength) && (reach <= aOutLength)) {
                const double factor = aSide * aHalfWidth / (1.0 + cosine);
                aoPoints.push_back(Point(aVertex.x + (normalIn.x + normalOut.x) * factor, aVertex.y + (normalIn.y + normalOut.y) * factor));
                return;
            }
            aoPoints.push_back(before);
            aoPoints.push_back(after);
            return;
        }
        aoPoints.push_back(before);
        {
            if (aStroke.join == LineJoin::Round) {
                const double fromAngle = std::atan2(before.y - aVertex.y, before.x - aVertex.x);
                double sweep = std::atan2(after.y - aVertex.y, after.x - aVertex.x) - fromAngle;
                while (sweep > m_pi()) {
                    sweep -= 2.0 * m_pi();
                }
                while (sweep < -m_pi()) {
                    sweep += 2.0 * m_pi();
                }
                std::vector<Point> arc;
                m_arc(aVertex, aHalfWidth, fromAngle, sweep, aTolerance, arc);
                if (!arc.empty()) {
                    arc.pop_back();  // the after point closes it below
                }
                aoPoints.insert(aoPoints.end(), arc.begin(), arc.end());
            } else if (aStroke.join == LineJoin::Miter) {
                // the miter point sits on the bisector of the normals ; its
                // length over the half width is 1 / sin(theta / 2) : beyond
                // the limit the join falls back to the bevel
                const double cosine = normalIn.x * normalOut.x + normalIn.y * normalOut.y;
                const double halfTheta = std::acos((dot < -1.0) ? -1.0 : ((dot > 1.0) ? 1.0 : dot)) * 0.5;
                const double miterRatio = (std::cos(halfTheta) > 1e-9) ? (1.0 / std::cos(halfTheta)) : 1e9;
                if ((miterRatio <= aStroke.miterLimit) && (1.0 + cosine > 1e-9)) {
                    const double factor = aSide * aHalfWidth / (1.0 + cosine);
                    aoPoints.push_back(Point(aVertex.x + (normalIn.x + normalOut.x) * factor, aVertex.y + (normalIn.y + normalOut.y) * factor));
                }
            }
        }
        aoPoints.push_back(after);
    }
    // one side of an open polyline, forward : the start offset, the joins,
    // the end offset
    static void m_offsetSide(const std::vector<Point>& aPoints, double aHalfWidth, const Stroke& aStroke, double aTolerance, double aSide, std::vector<Point>& aoSide) {
        aoSide.clear();
        const Point firstDirection = m_direction(aPoints[0], aPoints[1]);
        const Point firstNormal = m_normal(firstDirection);
        aoSide.push_back(Point(aPoints[0].x + aSide * firstNormal.x * aHalfWidth, aPoints[0].y + aSide * firstNormal.y * aHalfWidth));
        for (std::size_t pointIdx = 1; pointIdx + 1u < aPoints.size(); ++pointIdx) {
            m_join(aPoints[pointIdx], m_direction(aPoints[pointIdx - 1u], aPoints[pointIdx]), m_distance(aPoints[pointIdx - 1u], aPoints[pointIdx]),  //
                   m_direction(aPoints[pointIdx], aPoints[pointIdx + 1u]), m_distance(aPoints[pointIdx], aPoints[pointIdx + 1u]),          //
                   aHalfWidth, aStroke, aTolerance, aSide, aoSide);
        }
        const Point lastDirection = m_direction(aPoints[aPoints.size() - 2u], aPoints.back());
        const Point lastNormal = m_normal(lastDirection);
        aoSide.push_back(Point(aPoints.back().x + aSide * lastNormal.x * aHalfWidth, aPoints.back().y + aSide * lastNormal.y * aHalfWidth));
    }
    // one side of a closed polyline : a join at every vertex, the first
    // one included
    static void m_offsetRing(const std::vector<Point>& aPoints, double aHalfWidth, const Stroke& aStroke, double aTolerance, double aSide, std::vector<Point>& aoRing) {
        aoRing.clear();
        const std::size_t count = aPoints.size();
        for (std::size_t pointIdx = 0; pointIdx < count; ++pointIdx) {
            const Point& previous = aPoints[(pointIdx + count - 1u) % count];
            const Point& next = aPoints[(pointIdx + 1u) % count];
            m_join(aPoints[pointIdx], m_direction(previous, aPoints[pointIdx]), m_distance(previous, aPoints[pointIdx]),  //
                   m_direction(aPoints[pointIdx], next), m_distance(aPoints[pointIdx], next), aHalfWidth, aStroke, aTolerance, aSide, aoRing);
        }
    }
    // the cap at aEnd, leaving along aDirection, from the side point
    // aFrom (already in the contour) to the side point aTo (appended)
    static void m_cap(const Point& aEnd, const Point& aDirection, double aHalfWidth, LineCap aCap, double aTolerance, const Point& aFrom, const Point& aTo, std::vector<Point>& aoPoints) {
        if (aCap == LineCap::Round) {
            const double fromAngle = std::atan2(aFrom.y - aEnd.y, aFrom.x - aEnd.x);
            double sweep = std::atan2(aTo.y - aEnd.y, aTo.x - aEnd.x) - fromAngle;
            while (sweep > m_pi()) {
                sweep -= 2.0 * m_pi();
            }
            while (sweep < -m_pi()) {
                sweep += 2.0 * m_pi();
            }
            // the half turn goes through the tip, never the short way back
            const double tipAngle = std::atan2(aDirection.y, aDirection.x);
            double tipOffset = tipAngle - fromAngle;
            while (tipOffset > m_pi()) {
                tipOffset -= 2.0 * m_pi();
            }
            while (tipOffset < -m_pi()) {
                tipOffset += 2.0 * m_pi();
            }
            if ((tipOffset > 0.0) != (sweep > 0.0)) {
                sweep = (sweep > 0.0) ? (sweep - 2.0 * m_pi()) : (sweep + 2.0 * m_pi());
            }
            m_arc(aEnd, aHalfWidth, fromAngle, sweep, aTolerance, aoPoints);
            return;
        }
        if (aCap == LineCap::Square) {
            aoPoints.push_back(Point(aFrom.x + aDirection.x * aHalfWidth, aFrom.y + aDirection.y * aHalfWidth));
            aoPoints.push_back(Point(aTo.x + aDirection.x * aHalfWidth, aTo.y + aDirection.y * aHalfWidth));
        }
        aoPoints.push_back(aTo);
    }
    // a polygon as a closed sub path of lines, oriented as asked
    static void m_emit(const std::vector<Point>& aPolygon, int32_t aOrientation, std::vector<SubPath>& aoContours) {
        if (aPolygon.size() < 3u) {
            return;
        }
        const double area = m_signedArea(aPolygon);
        const bool reverse = ((area > 0.0) ? 1 : -1) != aOrientation;
        SubPath contour;
        contour.closed = true;
        if (reverse) {
            contour.start = aPolygon.back();
            for (std::size_t pointIdx = aPolygon.size() - 1u; pointIdx > 0u; --pointIdx) {
                contour.segments.push_back(Segment::line(aPolygon[pointIdx - 1u]));
            }
        } else {
            contour.start = aPolygon.front();
            for (std::size_t pointIdx = 1; pointIdx < aPolygon.size(); ++pointIdx) {
                contour.segments.push_back(Segment::line(aPolygon[pointIdx]));
            }
        }
        aoContours.push_back(contour);
    }
};

}  // namespace svg
}  // namespace ez
