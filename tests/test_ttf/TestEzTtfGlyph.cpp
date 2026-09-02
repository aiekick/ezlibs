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

bool TestEzTtfGlyph(const std::string& vTest) {
    IfTestExist(TestEzTtfGlyph_EmitParseRoundTripsASimpleGlyph);
    else IfTestExist(TestEzTtfGlyph_EmitParseRoundTripsAComposite);
    else IfTestExist(TestEzTtfGlyph_AnEmptyGlyphEmitsNothing);
    else IfTestExist(TestEzTtfGlyph_RoundTripsEveryGlyphOfARealFont);
    return false;
}
