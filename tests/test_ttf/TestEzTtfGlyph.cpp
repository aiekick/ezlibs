#include <TestEzTtfGlyph.h>

#include <cstdio>
#include <string>
#include <vector>

#include <ezlibs/ezTTF/ezTTF.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

bool local_sameGlyph(const ez::ttf::Glyph& aFirst, const ez::ttf::Glyph& aSecond) {
    if ((aFirst.isComposite != aSecond.isComposite) || (aFirst.xMin != aSecond.xMin) || (aFirst.yMin != aSecond.yMin) ||  //
        (aFirst.xMax != aSecond.xMax) || (aFirst.yMax != aSecond.yMax)) {
        return false;
    }
    if (aFirst.contours.size() != aSecond.contours.size()) {
        return false;
    }
    for (std::size_t contourIdx = 0; contourIdx < aFirst.contours.size(); ++contourIdx) {
        const std::vector<ez::ttf::GlyphPoint>& one = aFirst.contours[contourIdx];
        const std::vector<ez::ttf::GlyphPoint>& two = aSecond.contours[contourIdx];
        if (one.size() != two.size()) {
            return false;
        }
        for (std::size_t pointIdx = 0; pointIdx < one.size(); ++pointIdx) {
            if ((one[pointIdx].x != two[pointIdx].x) || (one[pointIdx].y != two[pointIdx].y) || (one[pointIdx].onCurve != two[pointIdx].onCurve)) {
                return false;
            }
        }
    }
    if (aFirst.components.size() != aSecond.components.size()) {
        return false;
    }
    for (std::size_t componentIdx = 0; componentIdx < aFirst.components.size(); ++componentIdx) {
        const ez::ttf::GlyphComponent& one = aFirst.components[componentIdx];
        const ez::ttf::GlyphComponent& two = aSecond.components[componentIdx];
        if ((one.glyphIndex != two.glyphIndex) || (one.offsetX != two.offsetX) || (one.offsetY != two.offsetY) ||  //
            (one.scaleXX.raw != two.scaleXX.raw) || (one.scaleXY.raw != two.scaleXY.raw) || (one.scaleYX.raw != two.scaleYX.raw) || (one.scaleYY.raw != two.scaleYY.raw)) {
            return false;
        }
    }
    return true;
}

bool local_roundTrips(const ez::ttf::Glyph& aGlyph) {
    ez::ttf::Stream stream;
    ez::ttf::emitGlyphData(aGlyph, stream);
    ez::ttf::Glyph back;
    if (!ez::ttf::parseGlyphData(stream.getBytes(), stream.getSize(), back)) {
        return false;
    }
    return local_sameGlyph(aGlyph, back);
}

ez::ttf::GlyphPoint local_point(int16_t aX, int16_t aY, bool aOnCurve) {
    ez::ttf::GlyphPoint point;
    point.x = aX;
    point.y = aY;
    point.onCurve = aOnCurve;
    return point;
}

// every glyph of a font, parsed, emitted, parsed again : the same model
bool local_roundTripsEveryGlyph(const ez::ttf::Font& aFont, uint32_t& aoCount) {
    aoCount = 0u;
    for (std::size_t glyphIdx = 0; glyphIdx < aFont.getGlyphCount(); ++glyphIdx) {
        ez::ttf::Glyph glyph;
        if (!aFont.getGlyph(static_cast<ez::ttf::GlyphIndex>(glyphIdx), glyph)) {
            return false;
        }
        if (!local_roundTrips(glyph)) {
            printf("round trip failed on glyph %u\n", static_cast<uint32_t>(glyphIdx));
            return false;
        }
        ++aoCount;
    }
    return true;
}

}  // namespace

// two contours : short deltas both signs, a run of same-x points (a
// vertical edge), a delta beyond a byte, off-curve points, the box
bool TestEzTtfGlyph_EmitParseRoundTripsASimpleGlyph() {
    ez::ttf::Glyph glyph;
    std::vector<ez::ttf::GlyphPoint> outer;
    outer.push_back(local_point(0, 0, true));
    outer.push_back(local_point(0, 300, true));    // same x : no x bytes
    outer.push_back(local_point(0, 700, true));    // same x again : a repeated flag
    outer.push_back(local_point(600, 700, true));  // a word delta
    outer.push_back(local_point(500, 400, false));
    outer.push_back(local_point(600, 0, true));
    outer.push_back(local_point(350, -20, true));  // negative short
    std::vector<ez::ttf::GlyphPoint> inner;
    inner.push_back(local_point(100, 100, true));
    inner.push_back(local_point(500, 100, true));
    inner.push_back(local_point(500, 600, true));
    inner.push_back(local_point(100, 600, true));
    glyph.contours.push_back(outer);
    glyph.contours.push_back(inner);
    ez::ttf::computeGlyphBounds(glyph);
    CTEST_ASSERT(glyph.xMin == 0);
    CTEST_ASSERT(glyph.yMin == -20);
    CTEST_ASSERT(glyph.xMax == 600);
    CTEST_ASSERT(glyph.yMax == 700);
    CTEST_ASSERT(local_roundTrips(glyph));
    // the repeat compression : 300 points of the same flag stay one glyph
    ez::ttf::Glyph tall;
    std::vector<ez::ttf::GlyphPoint> column;
    for (int16_t pointIdx = 0; pointIdx < 300; ++pointIdx) {
        column.push_back(local_point(10, static_cast<int16_t>(pointIdx * 2), true));
    }
    tall.contours.push_back(column);
    ez::ttf::computeGlyphBounds(tall);
    CTEST_ASSERT(local_roundTrips(tall));
    return true;
}

// the four scale flavours and both argument widths come back as they went
bool TestEzTtfGlyph_EmitParseRoundTripsAComposite() {
    ez::ttf::Glyph glyph;
    glyph.isComposite = true;
    glyph.xMin = -10;
    glyph.yMin = -20;
    glyph.xMax = 900;
    glyph.yMax = 800;
    ez::ttf::GlyphComponent plain;
    plain.glyphIndex = 4u;
    plain.offsetX = 12;
    plain.offsetY = -7;
    glyph.components.push_back(plain);
    ez::ttf::GlyphComponent scaled;
    scaled.glyphIndex = 5u;
    scaled.offsetX = 300;  // a word
    scaled.offsetY = 0;
    scaled.scaleXX.setFloat(0.5f);
    scaled.scaleYY.setFloat(0.5f);
    glyph.components.push_back(scaled);
    ez::ttf::GlyphComponent stretched;
    stretched.glyphIndex = 6u;
    stretched.scaleXX.setFloat(1.5f);
    stretched.scaleYY.setFloat(0.75f);
    glyph.components.push_back(stretched);
    ez::ttf::GlyphComponent sheared;
    sheared.glyphIndex = 7u;
    sheared.offsetX = -128;
    sheared.offsetY = 127;
    sheared.scaleXY.setFloat(0.25f);
    glyph.components.push_back(sheared);
    CTEST_ASSERT(local_roundTrips(glyph));
    ez::ttf::Stream stream;
    ez::ttf::emitGlyphData(glyph, stream);
    ez::ttf::Glyph back;
    CTEST_ASSERT(ez::ttf::parseGlyphData(stream.getBytes(), stream.getSize(), back));
    CTEST_ASSERT(back.isComposite);
    CTEST_ASSERT(back.components.size() == 4u);
    CTEST_ASSERT(back.components[1].scaleXX.raw == 0x2000);
    CTEST_ASSERT(back.components[3].scaleXY.raw == 0x1000);
    CTEST_ASSERT(back.components[3].scaleXX.raw == 0x4000);
    return true;
}

// an empty glyph is an empty span : the space of a font
bool TestEzTtfGlyph_AnEmptyGlyphEmitsNothing() {
    ez::ttf::Glyph empty;
    ez::ttf::Stream stream;
    ez::ttf::emitGlyphData(empty, stream);
    CTEST_ASSERT(stream.getSize() == 0u);
    ez::ttf::Glyph back;
    CTEST_ASSERT(ez::ttf::parseGlyphData(stream.getBytes(), stream.getSize(), back));
    CTEST_ASSERT(back.isEmpty());
    ez::ttf::computeGlyphBounds(empty);
    CTEST_ASSERT((empty.xMin == 0) && (empty.yMax == 0));
    return true;
}

// the real-world pass : the committed roboto subset always, the vendored
// roboto when the tree provides it — every glyph survives the round trip
bool TestEzTtfGlyph_RoundTripsEveryGlyphOfARealFont() {
    auto anyFont = false;
#ifdef EZTTF_RES_DIR
    {
        const std::string fixturePath = std::string(EZTTF_RES_DIR) + "robotoSubset.ttf";
        ez::ttf::Font fixture;
        if (fixture.openFromFile(fixturePath)) {
            uint32_t count = 0u;
            CTEST_ASSERT_MESSAGE(local_roundTripsEveryGlyph(fixture, count), "the fixture glyphs do not round trip");
            CTEST_ASSERT(count == fixture.getGlyphCount());
            anyFont = true;
        }
    }
#endif
#ifdef EZTTF_LOCAL_TTF
    {
        ez::ttf::Font roboto;
        if (roboto.openFromFile(EZTTF_LOCAL_TTF)) {
            uint32_t count = 0u;
            CTEST_ASSERT_MESSAGE(local_roundTripsEveryGlyph(roboto, count), "the real font glyphs do not round trip");
            CTEST_ASSERT(count > 100u);
            printf("real font pass : %u glyphs emitted and parsed back\n", count);
            anyFont = true;
        }
    }
#endif
    if (!anyFont) {
        printf("SKIPPED : no real font reachable\n");
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// the transform maps the points of a simple glyph (a scale about the
// origin, THEN the offset), quantizes them to font units (nearest,
// saturated to the int16 rail) and recomputes the box — the curve
// flags ride along ; a composite refuses, untouched
bool TestEzTtfGlyph_TransformScalesThenTranslatesASimpleGlyph() {
    ez::ttf::Glyph glyph;
    std::vector<ez::ttf::GlyphPoint> contour;
    contour.push_back(local_point(0, 0, true));
    contour.push_back(local_point(100, 0, true));
    contour.push_back(local_point(50, 80, false));
    contour.push_back(local_point(1, 1, true));  // the rounding witness
    glyph.contours.push_back(contour);
    ez::ttf::computeGlyphBounds(glyph);
    CTEST_ASSERT(ez::ttf::transformGlyph(glyph, ez::ttf::GlyphTransform::scaleThenTranslate(2.0, 0.5, 5.0, -3.0)));
    CTEST_ASSERT(glyph.contours.size() == 1u && glyph.contours[0].size() == 4u);
    CTEST_ASSERT((glyph.contours[0][0].x == 5) && (glyph.contours[0][0].y == -3) && glyph.contours[0][0].onCurve);
    CTEST_ASSERT((glyph.contours[0][1].x == 205) && (glyph.contours[0][1].y == -3) && glyph.contours[0][1].onCurve);
    CTEST_ASSERT((glyph.contours[0][2].x == 105) && (glyph.contours[0][2].y == 37) && !glyph.contours[0][2].onCurve);
    CTEST_ASSERT((glyph.contours[0][3].x == 7) && (glyph.contours[0][3].y == -2));  // 0.5 - 3 rounds to the nearest
    CTEST_ASSERT((glyph.xMin == 5) && (glyph.yMin == -3) && (glyph.xMax == 205) && (glyph.yMax == 37));
    // the saturation : a huge scale lands on the rail, never wraps
    CTEST_ASSERT(ez::ttf::transformGlyph(glyph, ez::ttf::GlyphTransform::scaleThenTranslate(1000.0, 1.0, 0.0, 0.0)));
    CTEST_ASSERT(glyph.contours[0][1].x == 32767);
    CTEST_ASSERT(glyph.xMax == 32767);
    // a composite refuses and keeps its references
    ez::ttf::Glyph composite;
    composite.isComposite = true;
    composite.components.push_back(ez::ttf::GlyphComponent());
    CTEST_ASSERT(!ez::ttf::transformGlyph(composite, ez::ttf::GlyphTransform::scaleThenTranslate(2.0, 2.0, 0.0, 0.0)));
    CTEST_ASSERT(composite.isComposite && composite.components.size() == 1u);
    return true;
}

// the component convention : the four F2DOT14 of a composite read as
// xscale, scale01, scale10, yscale — x' = xscale x + scale10 y + dx and
// y' = scale01 x + yscale y + dy (a quarter turn checked on two points) ;
// a plain component maps to itself
bool TestEzTtfGlyph_TransformFollowsTheComponentConvention() {
    ez::ttf::GlyphComponent component;
    component.offsetX = 10;
    component.offsetY = 20;
    component.scaleXX.setFloat(0.0f);
    component.scaleXY.setFloat(1.0f);
    component.scaleYX.setFloat(-1.0f);
    component.scaleYY.setFloat(0.0f);
    const ez::ttf::GlyphTransform turn = ez::ttf::GlyphTransform::fromComponent(component);
    double mappedX = 0.0;
    double mappedY = 0.0;
    turn.map(100.0, 0.0, mappedX, mappedY);
    CTEST_ASSERT((mappedX == 10.0) && (mappedY == 120.0));
    turn.map(0.0, 50.0, mappedX, mappedY);
    CTEST_ASSERT((mappedX == -40.0) && (mappedY == 20.0));
    const ez::ttf::GlyphTransform plain = ez::ttf::GlyphTransform::fromComponent(ez::ttf::GlyphComponent());
    plain.map(7.0, 9.0, mappedX, mappedY);
    CTEST_ASSERT((mappedX == 7.0) && (mappedY == 9.0));
    return true;
}

bool TestEzTtfGlyph(const std::string& vTest) {
    IfTestExist(TestEzTtfGlyph_EmitParseRoundTripsASimpleGlyph);
    else IfTestExist(TestEzTtfGlyph_EmitParseRoundTripsAComposite);
    else IfTestExist(TestEzTtfGlyph_AnEmptyGlyphEmitsNothing);
    else IfTestExist(TestEzTtfGlyph_RoundTripsEveryGlyphOfARealFont);
    else IfTestExist(TestEzTtfGlyph_TransformScalesThenTranslatesASimpleGlyph);
    else IfTestExist(TestEzTtfGlyph_TransformFollowsTheComponentConvention);
    return false;
}
