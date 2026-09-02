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
// ez::svg : the value types of the svg reader — an affine matrix, a
// paint, the segments of a sub path, a painted shape and the document
// that holds them in PAINT ORDER. no rendering, no dom : what a glyph
// importer, a plotter or a mesher needs from an svg, nothing more

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "../ezMath/ezVec2.hpp"

namespace ez {
namespace svg {

typedef ez::math::dvec2 Point;

// the affine matrix of svg (matrix(a b c d e f) in the spec order) :
// x' = scaleX * x + skewX * y + translateX
// y' = skewY * x + scaleY * y + translateY
struct Matrix {
    double scaleX{1.0};
    double skewY{0.0};
    double skewX{0.0};
    double scaleY{1.0};
    double translateX{0.0};
    double translateY{0.0};
    Matrix() {}
    Matrix(double aScaleX, double aSkewY, double aSkewX, double aScaleY, double aTranslateX, double aTranslateY)
        : scaleX(aScaleX), skewY(aSkewY), skewX(aSkewX), scaleY(aScaleY), translateX(aTranslateX), translateY(aTranslateY) {}
    // this * aOther : aOther applies FIRST to a point, then this (the
    // svg transform list rule, the child under its parent)
    Matrix multiply(const Matrix& aOther) const {
        Matrix result;
        result.scaleX = scaleX * aOther.scaleX + skewX * aOther.skewY;
        result.skewY = skewY * aOther.scaleX + scaleY * aOther.skewY;
        result.skewX = scaleX * aOther.skewX + skewX * aOther.scaleY;
        result.scaleY = skewY * aOther.skewX + scaleY * aOther.scaleY;
        result.translateX = scaleX * aOther.translateX + skewX * aOther.translateY + translateX;
        result.translateY = skewY * aOther.translateX + scaleY * aOther.translateY + translateY;
        return result;
    }
    Point apply(const Point& aPoint) const {
        return Point(scaleX * aPoint.x + skewX * aPoint.y + translateX, skewY * aPoint.x + scaleY * aPoint.y + translateY);
    }
    bool isIdentity() const {
        return (scaleX == 1.0) && (skewY == 0.0) && (skewX == 0.0) && (scaleY == 1.0) && (translateX == 0.0) && (translateY == 0.0);
    }
    static Matrix translation(double aX, double aY) {
        return Matrix(1.0, 0.0, 0.0, 1.0, aX, aY);
    }
    static Matrix scaling(double aX, double aY) {
        return Matrix(aX, 0.0, 0.0, aY, 0.0, 0.0);
    }
    static Matrix rotation(double aDegrees) {
        const double radians = aDegrees * 3.14159265358979323846 / 180.0;
        const double cosine = std::cos(radians);
        const double sine = std::sin(radians);
        return Matrix(cosine, sine, -sine, cosine, 0.0, 0.0);
    }
    // rotate(a cx cy) = translate(cx cy) rotate(a) translate(-cx -cy)
    static Matrix rotationAround(double aDegrees, double aCenterX, double aCenterY) {
        return translation(aCenterX, aCenterY).multiply(rotation(aDegrees)).multiply(translation(-aCenterX, -aCenterY));
    }
    static Matrix skewingX(double aDegrees) {
        return Matrix(1.0, 0.0, std::tan(aDegrees * 3.14159265358979323846 / 180.0), 1.0, 0.0, 0.0);
    }
    static Matrix skewingY(double aDegrees) {
        return Matrix(1.0, std::tan(aDegrees * 3.14159265358979323846 / 180.0), 0.0, 1.0, 0.0, 0.0);
    }
};

// what fills a shape : nothing, a color, the current color of the text,
// or something this reader does not resolve (a gradient, a pattern : the
// shape is kept, the reference tells which)
enum class PaintKind { None, Color, CurrentColor, Unsupported };

struct Paint {
    PaintKind kind{PaintKind::Color};
    uint8_t red{0};
    uint8_t green{0};
    uint8_t blue{0};
    uint8_t alpha{255};
    std::string reference;  // the url(#id) target of an unsupported paint
    static Paint none() {
        Paint paint;
        paint.kind = PaintKind::None;
        return paint;
    }
    static Paint color(uint8_t aRed, uint8_t aGreen, uint8_t aBlue, uint8_t aAlpha = 255) {
        Paint paint;
        paint.kind = PaintKind::Color;
        paint.red = aRed;
        paint.green = aGreen;
        paint.blue = aBlue;
        paint.alpha = aAlpha;
        return paint;
    }
    static Paint currentColor() {
        Paint paint;
        paint.kind = PaintKind::CurrentColor;
        return paint;
    }
    static Paint unsupported(const std::string& aReference) {
        Paint paint;
        paint.kind = PaintKind::Unsupported;
        paint.reference = aReference;
        return paint;
    }
    bool isVisible() const {
        return (kind != PaintKind::None) && ((kind != PaintKind::Color) || (alpha != 0));
    }
};

enum class FillRule { NonZero, EvenOdd };

// a segment of a sub path : the start is the previous point of the chain
enum class SegmentKind { Line, Cubic };

struct Segment {
    SegmentKind kind{SegmentKind::Line};
    Point control0;
    Point control1;
    Point end;
    static Segment line(const Point& aEnd) {
        Segment segment;
        segment.kind = SegmentKind::Line;
        segment.end = aEnd;
        return segment;
    }
    static Segment cubic(const Point& aControl0, const Point& aControl1, const Point& aEnd) {
        Segment segment;
        segment.kind = SegmentKind::Cubic;
        segment.control0 = aControl0;
        segment.control1 = aControl1;
        segment.end = aEnd;
        return segment;
    }
};

struct SubPath {
    Point start;
    std::vector<Segment> segments;
    bool closed{false};
};

// one painted shape, its points ALREADY transformed into the document
// frame (svg units, y down), in the order the svg paints it
struct Shape {
    std::string id;
    Paint fill;
    FillRule fillRule{FillRule::NonZero};
    double opacity{1.0};  // the composed opacity (fill-opacity and the group opacities)
    std::vector<SubPath> subPaths;
};

struct Document {
    bool hasViewBox{false};
    double viewBoxX{0.0};
    double viewBoxY{0.0};
    double viewBoxWidth{0.0};
    double viewBoxHeight{0.0};
    double width{0.0};   // pixels, 0 = unspecified
    double height{0.0};  // pixels, 0 = unspecified
    std::vector<Shape> shapes;
    std::vector<std::string> warnings;  // what the reader ignored, counted and named
};

}  // namespace svg
}  // namespace ez
