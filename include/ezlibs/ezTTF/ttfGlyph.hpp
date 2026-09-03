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
// the glyf entry model : simple outlines (packed flags + deltas decoded
// to ABSOLUTE font-unit points) and composites (references KEPT as
// references — the resolution into points is a later, separate concern,
// the ttfrrw lesson : a subset must carry the component tree, not its
// flattening)

#include <cmath>
#include <cstdint>
#include <vector>

#include "ttfTypes.hpp"
#include "ttfStream.hpp"

namespace ez {
namespace ttf {

// one outline point, absolute font units. an OFF-curve point is a
// quadratic control (the TrueType curve model)
struct GlyphPoint {
    int16_t x;
    int16_t y;
    bool onCurve;
    GlyphPoint() : x(0), y(0), onCurve(false) {}
};

// one component of a composite : a referenced glyph and its placement.
// only the translate + the three scale flavours of the format are kept
// (2x2 covers them all internally)
struct GlyphComponent {
    GlyphIndex glyphIndex;
    int16_t offsetX;
    int16_t offsetY;
    F2DOT14 scaleXX;
    F2DOT14 scaleXY;
    F2DOT14 scaleYX;
    F2DOT14 scaleYY;
    GlyphComponent() : glyphIndex(0), offsetX(0), offsetY(0) {
        scaleXX.setFloat(1.0f);
        scaleYY.setFloat(1.0f);
    }
};

struct Glyph {
    bool isComposite;
    int16_t xMin;
    int16_t yMin;
    int16_t xMax;
    int16_t yMax;
    std::vector<std::vector<GlyphPoint> > contours;  // simple glyphs
    std::vector<GlyphComponent> components;          // composite glyphs
    Glyph() : isComposite(false), xMin(0), yMin(0), xMax(0), yMax(0) {}
    bool isEmpty() const { return contours.empty() && components.empty(); }
};

// the glyf entry flags
const uint8_t kGlyfOnCurve = 0x01u;
const uint8_t kGlyfXShort = 0x02u;
const uint8_t kGlyfYShort = 0x04u;
const uint8_t kGlyfRepeat = 0x08u;
const uint8_t kGlyfXSameOrPositive = 0x10u;
const uint8_t kGlyfYSameOrPositive = 0x20u;
// the composite flags
const uint16_t kGlyfArgsAreWords = 0x0001u;
const uint16_t kGlyfArgsAreXY = 0x0002u;
const uint16_t kGlyfHasScale = 0x0008u;
const uint16_t kGlyfMoreComponents = 0x0020u;
const uint16_t kGlyfHasXYScale = 0x0040u;
const uint16_t kGlyfHasTwoByTwo = 0x0080u;

// parses ONE glyf entry from its loca span. an EMPTY span is an empty
// glyph (a space) : true with nothing inside. false = corrupt entry
// (truncation, an endPts sequence that lies...), aoGlyph left empty
inline bool parseGlyphData(const uint8_t* apBytes, std::size_t aByteCount, Glyph& aoGlyph) {
    aoGlyph = Glyph();
    if (aByteCount == 0) {
        return true;  // the space-like glyph
    }
    Stream stream(apBytes, aByteCount);
    const int16_t contourCount = stream.readI16();
    aoGlyph.xMin = stream.readI16();
    aoGlyph.yMin = stream.readI16();
    aoGlyph.xMax = stream.readI16();
    aoGlyph.yMax = stream.readI16();
    if (!stream.ok()) {
        aoGlyph = Glyph();
        return false;
    }
    if (contourCount >= 0) {
        // --- simple glyph ---
        std::vector<uint16_t> endPts(static_cast<std::size_t>(contourCount));
        uint16_t pointCount = 0;
        for (int16_t contourIdx = 0; contourIdx < contourCount; ++contourIdx) {
            endPts[static_cast<std::size_t>(contourIdx)] = stream.readU16();
            // the sequence must not decrease : the point count derives from it
            if (contourIdx > 0 && endPts[static_cast<std::size_t>(contourIdx)] < endPts[static_cast<std::size_t>(contourIdx - 1)]) {
                aoGlyph = Glyph();
                return false;
            }
        }
        if (contourCount > 0) {
            pointCount = static_cast<uint16_t>(endPts[static_cast<std::size_t>(contourCount - 1)] + 1u);
        }
        const uint16_t instructionLength = stream.readU16();
        stream.setReadPos(stream.getReadPos() + instructionLength);  // hints : skipped, never used
        // packed flags (REPEAT compressed)
        std::vector<uint8_t> flags;
        flags.reserve(pointCount);
        while (flags.size() < pointCount && stream.ok()) {
            const uint8_t flag = stream.readU8();
            flags.push_back(flag);
            if ((flag & kGlyfRepeat) != 0u) {
                const uint8_t repeatCount = stream.readU8();
                for (uint8_t repeatIdx = 0; repeatIdx < repeatCount && flags.size() < pointCount; ++repeatIdx) {
                    flags.push_back(flag);
                }
            }
        }
        if (!stream.ok() || flags.size() != pointCount) {
            aoGlyph = Glyph();
            return false;
        }
        // the delta streams : x then y, absolute coordinates accumulated
        std::vector<GlyphPoint> points(pointCount);
        int32_t coord = 0;
        for (uint16_t pointIdx = 0; pointIdx < pointCount; ++pointIdx) {
            const uint8_t flag = flags[pointIdx];
            if ((flag & kGlyfXShort) != 0u) {
                const int32_t delta = stream.readU8();
                coord += ((flag & kGlyfXSameOrPositive) != 0u) ? delta : -delta;
            } else if ((flag & kGlyfXSameOrPositive) == 0u) {
                coord += stream.readI16();
            }  // else : same x, no bytes
            points[pointIdx].x = static_cast<int16_t>(coord);
            points[pointIdx].onCurve = (flag & kGlyfOnCurve) != 0u;
        }
        coord = 0;
        for (uint16_t pointIdx = 0; pointIdx < pointCount; ++pointIdx) {
            const uint8_t flag = flags[pointIdx];
            if ((flag & kGlyfYShort) != 0u) {
                const int32_t delta = stream.readU8();
                coord += ((flag & kGlyfYSameOrPositive) != 0u) ? delta : -delta;
            } else if ((flag & kGlyfYSameOrPositive) == 0u) {
                coord += stream.readI16();
            }
            points[pointIdx].y = static_cast<int16_t>(coord);
        }
        if (!stream.ok()) {
            aoGlyph = Glyph();
            return false;
        }
        // split into contours on the endPts fences
        uint16_t firstPoint = 0;
        for (int16_t contourIdx = 0; contourIdx < contourCount; ++contourIdx) {
            const uint16_t lastPoint = endPts[static_cast<std::size_t>(contourIdx)];
            std::vector<GlyphPoint> contour;
            contour.reserve(static_cast<std::size_t>(lastPoint) - firstPoint + 1u);
            for (uint16_t pointIdx = firstPoint; pointIdx <= lastPoint; ++pointIdx) {
                contour.push_back(points[pointIdx]);
            }
            aoGlyph.contours.push_back(contour);
            firstPoint = static_cast<uint16_t>(lastPoint + 1u);
        }
        return true;
    }
    // --- composite glyph ---
    aoGlyph.isComposite = true;
    auto moreComponents = true;
    while (moreComponents) {
        const uint16_t flags = stream.readU16();
        GlyphComponent component;
        component.glyphIndex = stream.readU16();
        if ((flags & kGlyfArgsAreWords) != 0u) {
            component.offsetX = stream.readI16();
            component.offsetY = stream.readI16();
        } else {
            component.offsetX = static_cast<int8_t>(stream.readU8());
            component.offsetY = static_cast<int8_t>(stream.readU8());
        }
        // point-matching args (ARGS_ARE_XY clear) are rare and kept as
        // read offsets : the resolution phase will decide what to do
        if ((flags & kGlyfHasScale) != 0u) {
            component.scaleXX = stream.readF2DOT14();
            component.scaleYY = component.scaleXX;
        } else if ((flags & kGlyfHasXYScale) != 0u) {
            component.scaleXX = stream.readF2DOT14();
            component.scaleYY = stream.readF2DOT14();
        } else if ((flags & kGlyfHasTwoByTwo) != 0u) {
            component.scaleXX = stream.readF2DOT14();
            component.scaleXY = stream.readF2DOT14();
            component.scaleYX = stream.readF2DOT14();
            component.scaleYY = stream.readF2DOT14();
        }
        if (!stream.ok()) {
            aoGlyph = Glyph();
            return false;
        }
        aoGlyph.components.push_back(component);
        moreComponents = (flags & kGlyfMoreComponents) != 0u;
    }
    return true;
}

// the bounding box of a SIMPLE glyph from its points (no point : zeros).
// a composite keeps the fields it carries : its box depends on the
// components, which live elsewhere
inline void computeGlyphBounds(Glyph& aoGlyph) {
    if (aoGlyph.isComposite) {
        return;
    }
    auto first = true;
    aoGlyph.xMin = 0;
    aoGlyph.yMin = 0;
    aoGlyph.xMax = 0;
    aoGlyph.yMax = 0;
    for (std::size_t contourIdx = 0; contourIdx < aoGlyph.contours.size(); ++contourIdx) {
        const std::vector<GlyphPoint>& contour = aoGlyph.contours[contourIdx];
        for (std::size_t pointIdx = 0; pointIdx < contour.size(); ++pointIdx) {
            const GlyphPoint& point = contour[pointIdx];
            if (first) {
                aoGlyph.xMin = point.x;
                aoGlyph.yMin = point.y;
                aoGlyph.xMax = point.x;
                aoGlyph.yMax = point.y;
                first = false;
            } else {
                aoGlyph.xMin = (point.x < aoGlyph.xMin) ? point.x : aoGlyph.xMin;
                aoGlyph.yMin = (point.y < aoGlyph.yMin) ? point.y : aoGlyph.yMin;
                aoGlyph.xMax = (point.x > aoGlyph.xMax) ? point.x : aoGlyph.xMax;
                aoGlyph.yMax = (point.y > aoGlyph.yMax) ? point.y : aoGlyph.yMax;
            }
        }
    }
}

// a font unit from a real value : rounded to the nearest, saturated to
// the int16 the format stores (the quantization the OutlinePen shares)
inline int16_t quantizeFontUnit(double aValue) {
    const double rounded = std::floor(aValue + 0.5);
    if (rounded > 32767.0) {
        return 32767;
    }
    if (rounded < -32768.0) {
        return -32768;
    }
    return static_cast<int16_t>(rounded);
}

// an affine map of an outline, font units :
//   x' = scaleXX * x + scaleYX * y + translateX
//   y' = scaleXY * x + scaleYY * y + translateY
// the composite convention of the format (xscale, scale01, scale10,
// yscale, then the offset), so a component placement is one of these.
// identity at birth
struct GlyphTransform {
    double scaleXX;
    double scaleXY;
    double scaleYX;
    double scaleYY;
    double translateX;
    double translateY;
    GlyphTransform() : scaleXX(1.0), scaleXY(0.0), scaleYX(0.0), scaleYY(1.0), translateX(0.0), translateY(0.0) {}
    // the editor map : a scale about the origin, THEN a translation
    static GlyphTransform scaleThenTranslate(double aScaleX, double aScaleY, double aTranslateX, double aTranslateY) {
        GlyphTransform transform;
        transform.scaleXX = aScaleX;
        transform.scaleYY = aScaleY;
        transform.translateX = aTranslateX;
        transform.translateY = aTranslateY;
        return transform;
    }
    // the placement of one composite component, as the format reads it
    static GlyphTransform fromComponent(const GlyphComponent& aComponent) {
        GlyphTransform transform;
        transform.scaleXX = static_cast<double>(aComponent.scaleXX.getFloat());
        transform.scaleXY = static_cast<double>(aComponent.scaleXY.getFloat());
        transform.scaleYX = static_cast<double>(aComponent.scaleYX.getFloat());
        transform.scaleYY = static_cast<double>(aComponent.scaleYY.getFloat());
        transform.translateX = static_cast<double>(aComponent.offsetX);
        transform.translateY = static_cast<double>(aComponent.offsetY);
        return transform;
    }
    void map(double aX, double aY, double& aoX, double& aoY) const {
        aoX = scaleXX * aX + scaleYX * aY + translateX;
        aoY = scaleXY * aX + scaleYY * aY + translateY;
    }
};

// maps every point of a SIMPLE glyph through aTransform (quantized to
// font units, the curve flags kept) and recomputes its bounds. a
// composite answers false, untouched : its points live in its
// components — flatten it first
inline bool transformGlyph(Glyph& aoGlyph, const GlyphTransform& aTransform) {
    if (aoGlyph.isComposite) {
        return false;
    }
    for (std::size_t contourIdx = 0; contourIdx < aoGlyph.contours.size(); ++contourIdx) {
        std::vector<GlyphPoint>& contour = aoGlyph.contours[contourIdx];
        for (std::size_t pointIdx = 0; pointIdx < contour.size(); ++pointIdx) {
            GlyphPoint& point = contour[pointIdx];
            double mappedX = 0.0;
            double mappedY = 0.0;
            aTransform.map(static_cast<double>(point.x), static_cast<double>(point.y), mappedX, mappedY);
            point.x = quantizeFontUnit(mappedX);
            point.y = quantizeFontUnit(mappedY);
        }
    }
    computeGlyphBounds(aoGlyph);
    return true;
}

// emits ONE glyf entry, the inverse of parseGlyphData : NOTHING for an
// empty glyph (an empty span is a space), the contours of a simple glyph
// with packed flags and short deltas, the components of a composite (xy
// args, words when a byte cannot hold them, the scale flavour the values
// need — the flags the model does not keep are not written). the header
// box rides as the glyph carries it : computeGlyphBounds first. the
// deltas between consecutive points must fit an int16, as the format says
inline void emitGlyphData(const Glyph& aGlyph, Stream& aoStream) {
    if (aGlyph.isEmpty()) {
        return;
    }
    if (!aGlyph.isComposite) {
        std::vector<uint8_t> flags;
        Stream xLane;
        Stream yLane;
        int32_t previousX = 0;
        int32_t previousY = 0;
        std::vector<uint16_t> endPts;
        uint32_t pointCount = 0;
        for (std::size_t contourIdx = 0; contourIdx < aGlyph.contours.size(); ++contourIdx) {
            const std::vector<GlyphPoint>& contour = aGlyph.contours[contourIdx];
            for (std::size_t pointIdx = 0; pointIdx < contour.size(); ++pointIdx) {
                const GlyphPoint& point = contour[pointIdx];
                uint8_t flag = point.onCurve ? kGlyfOnCurve : 0u;
                const int32_t deltaX = static_cast<int32_t>(point.x) - previousX;
                const int32_t deltaY = static_cast<int32_t>(point.y) - previousY;
                previousX = point.x;
                previousY = point.y;
                if (deltaX == 0) {
                    flag |= kGlyfXSameOrPositive;
                } else if ((deltaX > -256) && (deltaX < 256)) {
                    flag |= kGlyfXShort;
                    if (deltaX > 0) {
                        flag |= kGlyfXSameOrPositive;
                    }
                    xLane.writeU8(static_cast<uint8_t>((deltaX < 0) ? -deltaX : deltaX));
                } else {
                    xLane.writeI16(static_cast<int16_t>(deltaX));
                }
                if (deltaY == 0) {
                    flag |= kGlyfYSameOrPositive;
                } else if ((deltaY > -256) && (deltaY < 256)) {
                    flag |= kGlyfYShort;
                    if (deltaY > 0) {
                        flag |= kGlyfYSameOrPositive;
                    }
                    yLane.writeU8(static_cast<uint8_t>((deltaY < 0) ? -deltaY : deltaY));
                } else {
                    yLane.writeI16(static_cast<int16_t>(deltaY));
                }
                flags.push_back(flag);
                ++pointCount;
            }
            endPts.push_back(static_cast<uint16_t>(pointCount - 1u));
        }
        aoStream.writeI16(static_cast<int16_t>(aGlyph.contours.size()));
        aoStream.writeI16(aGlyph.xMin);
        aoStream.writeI16(aGlyph.yMin);
        aoStream.writeI16(aGlyph.xMax);
        aoStream.writeI16(aGlyph.yMax);
        for (std::size_t contourIdx = 0; contourIdx < endPts.size(); ++contourIdx) {
            aoStream.writeU16(endPts[contourIdx]);
        }
        aoStream.writeU16(0u);  // no instructions : the hints are not ours
        // the flags, a run of equal ones compressed with REPEAT
        std::size_t flagIdx = 0;
        while (flagIdx < flags.size()) {
            const uint8_t flag = flags[flagIdx];
            std::size_t run = 1;
            while (((flagIdx + run) < flags.size()) && (flags[flagIdx + run] == flag) && (run < 256u)) {
                ++run;
            }
            if (run == 1u) {
                aoStream.writeU8(flag);
            } else {
                aoStream.writeU8(static_cast<uint8_t>(flag | kGlyfRepeat));
                aoStream.writeU8(static_cast<uint8_t>(run - 1u));
            }
            flagIdx += run;
        }
        aoStream.writeBytes(xLane.getBytes(), xLane.getSize());
        aoStream.writeBytes(yLane.getBytes(), yLane.getSize());
        return;
    }
    aoStream.writeI16(-1);
    aoStream.writeI16(aGlyph.xMin);
    aoStream.writeI16(aGlyph.yMin);
    aoStream.writeI16(aGlyph.xMax);
    aoStream.writeI16(aGlyph.yMax);
    for (std::size_t componentIdx = 0; componentIdx < aGlyph.components.size(); ++componentIdx) {
        const GlyphComponent& component = aGlyph.components[componentIdx];
        uint16_t flags = kGlyfArgsAreXY;
        const bool words = (component.offsetX < -128) || (component.offsetX > 127) || (component.offsetY < -128) || (component.offsetY > 127);
        if (words) {
            flags |= kGlyfArgsAreWords;
        }
        const bool twoByTwo = (component.scaleXY.raw != 0) || (component.scaleYX.raw != 0);
        const bool xyScale = !twoByTwo && (component.scaleXX.raw != component.scaleYY.raw);
        const bool uniformScale = !twoByTwo && !xyScale && (component.scaleXX.raw != 0x4000);
        if (twoByTwo) {
            flags |= kGlyfHasTwoByTwo;
        } else if (xyScale) {
            flags |= kGlyfHasXYScale;
        } else if (uniformScale) {
            flags |= kGlyfHasScale;
        }
        if (componentIdx + 1u < aGlyph.components.size()) {
            flags |= kGlyfMoreComponents;
        }
        aoStream.writeU16(flags);
        aoStream.writeU16(component.glyphIndex);
        if (words) {
            aoStream.writeI16(component.offsetX);
            aoStream.writeI16(component.offsetY);
        } else {
            aoStream.writeU8(static_cast<uint8_t>(static_cast<int8_t>(component.offsetX)));
            aoStream.writeU8(static_cast<uint8_t>(static_cast<int8_t>(component.offsetY)));
        }
        if (twoByTwo) {
            aoStream.writeF2DOT14(component.scaleXX);
            aoStream.writeF2DOT14(component.scaleXY);
            aoStream.writeF2DOT14(component.scaleYX);
            aoStream.writeF2DOT14(component.scaleYY);
        } else if (xyScale) {
            aoStream.writeF2DOT14(component.scaleXX);
            aoStream.writeF2DOT14(component.scaleYY);
        } else if (uniformScale) {
            aoStream.writeF2DOT14(component.scaleXX);
        }
    }
}

}  // namespace ttf
}  // namespace ez
