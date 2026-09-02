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

// ezTTF is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// the OUTLINE PEN : contours drawn in floating point (moveTo, lineTo,
// quadTo, cubicTo, close) through an affine transform, turned into the
// TrueType model of a Glyph — the cubics split into quadratics under a
// tolerance (ezBezier), the points quantized to font units. a glyph
// importer (svg, a plotter, a generator) draws, the pen delivers

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../ezMath/ezBezier.hpp"
#include "ttfTypes.hpp"
#include "ttfGlyph.hpp"

namespace ez {
namespace ttf {

class OutlinePen {
private:
    struct PenPoint {
        double x;
        double y;
        bool onCurve;
        PenPoint() : x(0.0), y(0.0), onCurve(true) {}
        PenPoint(double aX, double aY, bool aOnCurve) : x(aX), y(aY), onCurve(aOnCurve) {}
    };

private:
    // the affine map from the drawing frame to the font frame :
    // x' = scaleX * x + skewX * y + translateX, y' = skewY * x + scaleY * y + translateY
    double m_scaleX{1.0};
    double m_skewY{0.0};
    double m_skewX{0.0};
    double m_scaleY{1.0};
    double m_translateX{0.0};
    double m_translateY{0.0};
    double m_tolerance{0.5};  // font units : how far a quadratic may stray from its cubic
    std::vector<std::vector<PenPoint> > m_contours;
    std::vector<PenPoint> m_current;
    bool m_open{false};

public:
    void setTransform(double aScaleX, double aSkewY, double aSkewX, double aScaleY, double aTranslateX, double aTranslateY) {
        m_scaleX = aScaleX;
        m_skewY = aSkewY;
        m_skewX = aSkewX;
        m_scaleY = aScaleY;
        m_translateX = aTranslateX;
        m_translateY = aTranslateY;
    }
    void setTolerance(double aFontUnits) {
        m_tolerance = (aFontUnits > 0.0) ? aFontUnits : 0.0;
    }
    void clear() {
        m_contours.clear();
        m_current.clear();
        m_open = false;
    }
    void moveTo(double aX, double aY) {
        m_flush();
        m_current.push_back(m_map(aX, aY, true));
        m_open = true;
    }
    void lineTo(double aX, double aY) {
        m_ensureOpen();
        m_current.push_back(m_map(aX, aY, true));
    }
    void quadTo(double aControlX, double aControlY, double aX, double aY) {
        m_ensureOpen();
        m_current.push_back(m_map(aControlX, aControlY, false));
        m_current.push_back(m_map(aX, aY, true));
    }
    // the cubic is split in the FONT frame : the tolerance is in font units
    void cubicTo(double aControl0X, double aControl0Y, double aControl1X, double aControl1Y, double aX, double aY) {
        m_ensureOpen();
        const PenPoint start = m_current.back();
        const PenPoint control0 = m_map(aControl0X, aControl0Y, false);
        const PenPoint control1 = m_map(aControl1X, aControl1Y, false);
        const PenPoint end = m_map(aX, aY, true);
        std::vector<ez::math::bezier::quad2> quads;
        ez::math::bezier::cubicToQuadratics(ez::math::dvec2(start.x, start.y), ez::math::dvec2(control0.x, control0.y),  //
                                            ez::math::dvec2(control1.x, control1.y), ez::math::dvec2(end.x, end.y), m_tolerance, quads);
        for (std::size_t quadIdx = 0; quadIdx < quads.size(); ++quadIdx) {
            m_current.push_back(PenPoint(quads[quadIdx].control.x, quads[quadIdx].control.y, false));
            m_current.push_back(PenPoint(quads[quadIdx].end.x, quads[quadIdx].end.y, true));
        }
    }
    void close() {
        m_flush();
    }
    // the contours quantized to font units (rounded, clamped to int16), a
    // closing point equal to the start dropped (TrueType closes by
    // itself), the consecutive duplicates merged, the contours under two
    // points skipped ; the bounds computed. the contour count is returned
    std::size_t toGlyph(Glyph& aoGlyph) {
        m_flush();
        aoGlyph = Glyph();
        for (std::size_t contourIdx = 0; contourIdx < m_contours.size(); ++contourIdx) {
            const std::vector<PenPoint>& source = m_contours[contourIdx];
            std::vector<GlyphPoint> contour;
            for (std::size_t pointIdx = 0; pointIdx < source.size(); ++pointIdx) {
                GlyphPoint point;
                point.x = m_quantize(source[pointIdx].x);
                point.y = m_quantize(source[pointIdx].y);
                point.onCurve = source[pointIdx].onCurve;
                if (!contour.empty() && point.onCurve && contour.back().onCurve && (contour.back().x == point.x) && (contour.back().y == point.y)) {
                    continue;  // the same on-curve point twice
                }
                contour.push_back(point);
            }
            while ((contour.size() > 1u) && contour.back().onCurve && contour.front().onCurve &&  //
                   (contour.back().x == contour.front().x) && (contour.back().y == contour.front().y)) {
                contour.pop_back();  // the explicit closing point
            }
            if (contour.size() < 2u) {
                continue;
            }
            aoGlyph.contours.push_back(contour);
        }
        computeGlyphBounds(aoGlyph);
        return aoGlyph.contours.size();
    }

private:
    PenPoint m_map(double aX, double aY, bool aOnCurve) const {
        return PenPoint(m_scaleX * aX + m_skewX * aY + m_translateX, m_skewY * aX + m_scaleY * aY + m_translateY, aOnCurve);
    }
    void m_ensureOpen() {
        if (!m_open) {
            m_current.push_back(m_map(0.0, 0.0, true));  // a segment before any moveTo starts at the origin
            m_open = true;
        }
    }
    void m_flush() {
        if (m_open && !m_current.empty()) {
            m_contours.push_back(m_current);
        }
        m_current.clear();
        m_open = false;
    }
    static int16_t m_quantize(double aValue) {
        const double rounded = std::floor(aValue + 0.5);
        if (rounded > 32767.0) {
            return 32767;
        }
        if (rounded < -32768.0) {
            return -32768;
        }
        return static_cast<int16_t>(rounded);
    }
};

}  // namespace ttf
}  // namespace ez
