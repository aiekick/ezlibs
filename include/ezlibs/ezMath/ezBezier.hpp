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

// ezBezier is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// ez::math::bezier : the curve conversions a path or a glyph needs — the
// evaluation of a quadratic and a cubic, the elevation of a quadratic to
// a cubic, the split of a cubic into quadratics (the TrueType model, an
// adaptive subdivision under a tolerance) and the svg elliptical arc to
// cubics (the endpoint to center parameterization of svg 1.1, F.6).
// nothing is rendered here

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <vector>

#include "ezVec2.hpp"

namespace ez {
namespace math {
namespace bezier {

// one quadratic segment : its control and its end (the start is the
// previous point of the chain)
struct quad2 {
    dvec2 control;
    dvec2 end;
    quad2() {}
    quad2(const dvec2& aControl, const dvec2& aEnd) : control(aControl), end(aEnd) {}
};

// one cubic segment : its two controls and its end
struct cubic2 {
    dvec2 control0;
    dvec2 control1;
    dvec2 end;
    cubic2() {}
    cubic2(const dvec2& aControl0, const dvec2& aControl1, const dvec2& aEnd) : control0(aControl0), control1(aControl1), end(aEnd) {}
};

namespace detail {

inline dvec2 scaled(const dvec2& aVector, double aFactor) {
    return dvec2(aVector.x * aFactor, aVector.y * aFactor);
}

inline dvec2 middle(const dvec2& aFirst, const dvec2& aSecond) {
    return dvec2((aFirst.x + aSecond.x) * 0.5, (aFirst.y + aSecond.y) * 0.5);
}

inline double distance(const dvec2& aFirst, const dvec2& aSecond) {
    const double dx = aSecond.x - aFirst.x;
    const double dy = aSecond.y - aFirst.y;
    return std::sqrt(dx * dx + dy * dy);
}

// the signed angle from aFrom to aTo (radians), the svg F.6.5.4 helper
inline double angleBetween(const dvec2& aFrom, const dvec2& aTo) {
    const double dot = aFrom.x * aTo.x + aFrom.y * aTo.y;
    const double lengths = std::sqrt((aFrom.x * aFrom.x + aFrom.y * aFrom.y) * (aTo.x * aTo.x + aTo.y * aTo.y));
    if (lengths <= 0.0) {
        return 0.0;
    }
    double cosine = dot / lengths;
    if (cosine > 1.0) {
        cosine = 1.0;
    } else if (cosine < -1.0) {
        cosine = -1.0;
    }
    const double angle = std::acos(cosine);
    const double cross = aFrom.x * aTo.y - aFrom.y * aTo.x;
    return (cross < 0.0) ? -angle : angle;
}

// the recursive half of cubicToQuadratics : the midpoint approximation,
// split in two at t = 1/2 while its bound exceeds the tolerance
inline void cubicToQuadraticsSplit(const dvec2& aP0, const dvec2& aC0, const dvec2& aC1, const dvec2& aP1,  //
                                   double aTolerance, int32_t aDepth, int32_t aMaxDepth, std::vector<quad2>& aoQuads) {
    // the two quadratic controls a cubic "wants" (one from each end) :
    // their distance bounds the error of the midpoint quadratic by
    // sqrt(3) / 18 (the classic cubic-to-quadratic bound)
    const dvec2 wantedFromStart = scaled(aC0, 1.5) - scaled(aP0, 0.5);
    const dvec2 wantedFromEnd = scaled(aC1, 1.5) - scaled(aP1, 0.5);
    const double bound = distance(wantedFromStart, wantedFromEnd) * (std::sqrt(3.0) / 18.0);
    if ((bound <= aTolerance) || (aDepth >= aMaxDepth)) {
        aoQuads.push_back(quad2(middle(wantedFromStart, wantedFromEnd), aP1));
        return;
    }
    // de casteljau at one half
    const dvec2 m01 = middle(aP0, aC0);
    const dvec2 m12 = middle(aC0, aC1);
    const dvec2 m23 = middle(aC1, aP1);
    const dvec2 m012 = middle(m01, m12);
    const dvec2 m123 = middle(m12, m23);
    const dvec2 mid = middle(m012, m123);
    cubicToQuadraticsSplit(aP0, m01, m012, mid, aTolerance, aDepth + 1, aMaxDepth, aoQuads);
    cubicToQuadraticsSplit(mid, m123, m23, aP1, aTolerance, aDepth + 1, aMaxDepth, aoQuads);
}

}  // namespace detail

// the point of a quadratic at aT in [0, 1]
inline dvec2 quadraticAt(const dvec2& aP0, const dvec2& aControl, const dvec2& aP1, double aT) {
    const double u = 1.0 - aT;
    return detail::scaled(aP0, u * u) + detail::scaled(aControl, 2.0 * u * aT) + detail::scaled(aP1, aT * aT);
}

// the point of a cubic at aT in [0, 1]
inline dvec2 cubicAt(const dvec2& aP0, const dvec2& aC0, const dvec2& aC1, const dvec2& aP1, double aT) {
    const double u = 1.0 - aT;
    return detail::scaled(aP0, u * u * u) + detail::scaled(aC0, 3.0 * u * u * aT) + detail::scaled(aC1, 3.0 * u * aT * aT) + detail::scaled(aP1, aT * aT * aT);
}

// the elevation : the cubic that IS the quadratic (exact)
inline void quadraticToCubic(const dvec2& aP0, const dvec2& aControl, const dvec2& aP1, dvec2& aoControl0, dvec2& aoControl1) {
    aoControl0 = aP0 + detail::scaled(aControl - aP0, 2.0 / 3.0);
    aoControl1 = aP1 + detail::scaled(aControl - aP1, 2.0 / 3.0);
}

// splits a cubic into the quadratics that stay within aTolerance of it
// (same unit as the points), appended to aoQuads in order — the chain
// starts at aP0 and ends exactly at aP1. a cubic that already is a
// quadratic gives ONE quadratic. aMaxDepth bounds the subdivision
inline void cubicToQuadratics(const dvec2& aP0, const dvec2& aC0, const dvec2& aC1, const dvec2& aP1,  //
                              double aTolerance, std::vector<quad2>& aoQuads, int32_t aMaxDepth = 16) {
    detail::cubicToQuadraticsSplit(aP0, aC0, aC1, aP1, (aTolerance > 0.0) ? aTolerance : 0.0, 0, aMaxDepth, aoQuads);
}

// flattens a cubic into the points of a polyline within aTolerance of the
// curve (the control polygon test, split at one half while it fails),
// appended to aoPoints WITHOUT aP0 and WITH aP1 — the chain continues
// from the previous point. aMaxDepth bounds the subdivision
inline void flattenCubic(const dvec2& aP0, const dvec2& aC0, const dvec2& aC1, const dvec2& aP1, double aTolerance, std::vector<dvec2>& aoPoints, int32_t aMaxDepth = 16) {
    // the distance of each control from the chord bounds the curve's
    // distance from it (the convex hull property)
    const double chordX = aP1.x - aP0.x;
    const double chordY = aP1.y - aP0.y;
    const double chordLength = std::sqrt(chordX * chordX + chordY * chordY);
    double deviation = 0.0;
    if (chordLength > 1e-12) {
        const double d0 = std::fabs(chordX * (aC0.y - aP0.y) - chordY * (aC0.x - aP0.x)) / chordLength;
        const double d1 = std::fabs(chordX * (aC1.y - aP0.y) - chordY * (aC1.x - aP0.x)) / chordLength;
        deviation = (d0 > d1) ? d0 : d1;
    } else {
        deviation = (detail::distance(aP0, aC0) > detail::distance(aP0, aC1)) ? detail::distance(aP0, aC0) : detail::distance(aP0, aC1);
    }
    if ((deviation <= ((aTolerance > 0.0) ? aTolerance : 0.0)) || (aMaxDepth <= 0)) {
        aoPoints.push_back(aP1);
        return;
    }
    const dvec2 m01 = detail::middle(aP0, aC0);
    const dvec2 m12 = detail::middle(aC0, aC1);
    const dvec2 m23 = detail::middle(aC1, aP1);
    const dvec2 m012 = detail::middle(m01, m12);
    const dvec2 m123 = detail::middle(m12, m23);
    const dvec2 mid = detail::middle(m012, m123);
    flattenCubic(aP0, m01, m012, mid, aTolerance, aoPoints, aMaxDepth - 1);
    flattenCubic(mid, m123, m23, aP1, aTolerance, aoPoints, aMaxDepth - 1);
}

// the svg elliptical arc from aP0 to aP1 as cubics (at most 90 degrees
// each), appended to aoCubics — the endpoint to center parameterization
// of svg 1.1 F.6.5, the radii scaled up when they cannot reach. false
// when the arc degenerates (coincident points, a zero radius) : the
// caller draws a straight line instead, as the svg spec says
inline bool arcToCubics(const dvec2& aP0, double aRadiusX, double aRadiusY, double aXAxisRotationDegrees,  //
                        bool aLargeArc, bool aSweep, const dvec2& aP1, std::vector<cubic2>& aoCubics) {
    const double c_pi = 3.14159265358979323846;
    if ((aP0.x == aP1.x) && (aP0.y == aP1.y)) {
        return false;
    }
    double radiusX = std::fabs(aRadiusX);
    double radiusY = std::fabs(aRadiusY);
    if ((radiusX <= 0.0) || (radiusY <= 0.0)) {
        return false;
    }
    const double phi = aXAxisRotationDegrees * c_pi / 180.0;
    const double cosPhi = std::cos(phi);
    const double sinPhi = std::sin(phi);
    // F.6.5.1 : the midpoint frame
    const double dx2 = (aP0.x - aP1.x) * 0.5;
    const double dy2 = (aP0.y - aP1.y) * 0.5;
    const double x1p = cosPhi * dx2 + sinPhi * dy2;
    const double y1p = -sinPhi * dx2 + cosPhi * dy2;
    // F.6.6 : radii too small are scaled up to fit
    const double lambda = (x1p * x1p) / (radiusX * radiusX) + (y1p * y1p) / (radiusY * radiusY);
    if (lambda > 1.0) {
        const double root = std::sqrt(lambda);
        radiusX *= root;
        radiusY *= root;
    }
    // F.6.5.2 : the center in the midpoint frame
    const double rx2 = radiusX * radiusX;
    const double ry2 = radiusY * radiusY;
    const double numerator = rx2 * ry2 - rx2 * y1p * y1p - ry2 * x1p * x1p;
    const double denominator = rx2 * y1p * y1p + ry2 * x1p * x1p;
    double coefficient = (denominator > 0.0) ? std::sqrt((numerator > 0.0) ? (numerator / denominator) : 0.0) : 0.0;
    if (aLargeArc == aSweep) {
        coefficient = -coefficient;
    }
    const double cxp = coefficient * (radiusX * y1p / radiusY);
    const double cyp = coefficient * (-radiusY * x1p / radiusX);
    // F.6.5.3 : the center in the user frame
    const double centerX = cosPhi * cxp - sinPhi * cyp + (aP0.x + aP1.x) * 0.5;
    const double centerY = sinPhi * cxp + cosPhi * cyp + (aP0.y + aP1.y) * 0.5;
    // F.6.5.4 to F.6.5.6 : the start angle and the sweep
    const dvec2 unitStart((x1p - cxp) / radiusX, (y1p - cyp) / radiusY);
    const dvec2 unitEnd((-x1p - cxp) / radiusX, (-y1p - cyp) / radiusY);
    const double startAngle = detail::angleBetween(dvec2(1.0, 0.0), unitStart);
    double sweepAngle = detail::angleBetween(unitStart, unitEnd);
    if (!aSweep && (sweepAngle > 0.0)) {
        sweepAngle -= 2.0 * c_pi;
    } else if (aSweep && (sweepAngle < 0.0)) {
        sweepAngle += 2.0 * c_pi;
    }
    // at most a quarter turn per cubic
    const int32_t segmentCount = static_cast<int32_t>(std::ceil(std::fabs(sweepAngle) / (c_pi * 0.5) - 1e-9));
    const int32_t count = (segmentCount < 1) ? 1 : segmentCount;
    const double delta = sweepAngle / static_cast<double>(count);
    const double kappa = (4.0 / 3.0) * std::tan(delta * 0.25);
    for (int32_t segmentIdx = 0; segmentIdx < count; ++segmentIdx) {
        const double angleA = startAngle + delta * static_cast<double>(segmentIdx);
        const double angleB = angleA + delta;
        const double cosA = std::cos(angleA);
        const double sinA = std::sin(angleA);
        const double cosB = std::cos(angleB);
        const double sinB = std::sin(angleB);
        // on the unit circle, then mapped through the ellipse frame
        const dvec2 unitControl0(cosA - kappa * sinA, sinA + kappa * cosA);
        const dvec2 unitControl1(cosB + kappa * sinB, sinB - kappa * cosB);
        const dvec2 unitTarget(cosB, sinB);
        cubic2 cubic;
        cubic.control0 = dvec2(centerX + radiusX * unitControl0.x * cosPhi - radiusY * unitControl0.y * sinPhi,  //
                               centerY + radiusX * unitControl0.x * sinPhi + radiusY * unitControl0.y * cosPhi);
        cubic.control1 = dvec2(centerX + radiusX * unitControl1.x * cosPhi - radiusY * unitControl1.y * sinPhi,  //
                               centerY + radiusX * unitControl1.x * sinPhi + radiusY * unitControl1.y * cosPhi);
        cubic.end = dvec2(centerX + radiusX * unitTarget.x * cosPhi - radiusY * unitTarget.y * sinPhi,  //
                          centerY + radiusX * unitTarget.x * sinPhi + radiusY * unitTarget.y * cosPhi);
        if (segmentIdx == count - 1) {
            cubic.end = aP1;  // the chain lands exactly where the path says
        }
        aoCubics.push_back(cubic);
    }
    return true;
}

}  // namespace bezier
}  // namespace math
}  // namespace ez
