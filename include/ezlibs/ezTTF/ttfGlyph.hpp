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

}  // namespace ttf
}  // namespace ez
