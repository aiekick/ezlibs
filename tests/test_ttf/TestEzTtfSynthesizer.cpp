#include <TestEzTtfSynthesizer.h>

#include <cstdio>
#include <string>
#include <vector>

#include <ezlibs/ezTTF/ezTTF.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

// a square from (aLeft, aBottom) of side aSide, drawn by the pen
ez::ttf::Glyph local_square(double aLeft, double aBottom, double aSide) {
    ez::ttf::OutlinePen pen;
    pen.moveTo(aLeft, aBottom);
    pen.lineTo(aLeft + aSide, aBottom);
    pen.lineTo(aLeft + aSide, aBottom + aSide);
    pen.lineTo(aLeft, aBottom + aSide);
    pen.close();
    ez::ttf::Glyph glyph;
    pen.toGlyph(glyph);
    return glyph;
}

ez::ttf::ColorRgba local_color(uint8_t aRed, uint8_t aGreen, uint8_t aBlue) {
    ez::ttf::ColorRgba color;
    color.red = aRed;
    color.green = aGreen;
    color.blue = aBlue;
    color.alpha = 255;
    return color;
}

}  // namespace

// the whole chain : metrics, a drawn glyph on a codepoint with a name, a
// space, the font reopens and tells the same truths
bool TestEzTtfSynthesizer_BuildsAFontThatReopens() {
    ez::ttf::Synthesizer synth;
    synth.setMetrics(1000u, 800, -200, 0);
    const ez::ttf::GlyphIndex squareIdx = synth.addGlyph(local_square(50.0, 0.0, 500.0), 600u, 0x41u, "A");
    const ez::ttf::GlyphIndex spaceIdx = synth.addGlyph(ez::ttf::Glyph(), 250u, 0x20u, "space");
    CTEST_ASSERT(squareIdx == 1u);
    CTEST_ASSERT(spaceIdx == 2u);
    ez::ttf::Font font;
    CTEST_ASSERT_MESSAGE(synth.build(font), "the synthesized font failed to build");
    CTEST_ASSERT(font.isOpened());
    CTEST_ASSERT(font.getGlyphCount() == 3u);
    CTEST_ASSERT(font.getInfos().unitsPerEm == 1000u);
    CTEST_ASSERT(font.getInfos().ascender == 800);
    CTEST_ASSERT(font.getInfos().descender == -200);
    CTEST_ASSERT(font.getGlyphIndex(0x41u) == 1u);
    CTEST_ASSERT(font.getGlyphIndex(0x20u) == 2u);
    CTEST_ASSERT(font.getGlyphIndex(0x42u) == 0u);
    CTEST_ASSERT(font.getGlyphName(1u) == "A");
    CTEST_ASSERT(font.getGlyphName(0u) == ".notdef");
    uint16_t advance = 0u;
    CTEST_ASSERT(font.getAdvanceWidth(1u, advance) && (advance == 600u));
    CTEST_ASSERT(font.getAdvanceWidth(2u, advance) && (advance == 250u));
    int16_t bearing = 0;
    CTEST_ASSERT(font.getLeftSideBearing(1u, bearing) && (bearing == 50));
    ez::ttf::Glyph outline;
    CTEST_ASSERT(font.getGlyph(1u, outline));
    CTEST_ASSERT(outline.contours.size() == 1u);
    CTEST_ASSERT(outline.contours[0].size() == 4u);
    CTEST_ASSERT((outline.xMin == 50) && (outline.xMax == 550) && (outline.yMin == 0) && (outline.yMax == 500));
    ez::ttf::Glyph space;
    CTEST_ASSERT(font.getGlyph(2u, space));
    CTEST_ASSERT(space.isEmpty());
    CTEST_ASSERT((font.getInfos().xMin == 50) && (font.getInfos().yMax == 500));
    // the writer round trip : what the font writes reopens identical
    std::vector<uint8_t> bytes;
    CTEST_ASSERT(font.writeToMemory(bytes));
    ez::ttf::Font again;
    CTEST_ASSERT(again.openFromMemory(&bytes[0], bytes.size()));
    CTEST_ASSERT(again.getGlyphCount() == 3u);
    return true;
}

// a colored glyph : the base, two colored layers and a text color layer,
// in paint order — the palette deduplicates its colors
bool TestEzTtfSynthesizer_ColorLayersRoundTrip() {
    ez::ttf::Synthesizer synth;
    synth.setMetrics(1000u, 800, -200, 0);
    const ez::ttf::GlyphIndex base = synth.addGlyph(local_square(0.0, 0.0, 600.0), 700u, 0xE000u, "icon");
    const ez::ttf::GlyphIndex redLayer = synth.addGlyph(local_square(0.0, 0.0, 300.0), 700u, 0u, "");
    const ez::ttf::GlyphIndex blueLayer = synth.addGlyph(local_square(300.0, 300.0, 300.0), 700u, 0u, "");
    const ez::ttf::GlyphIndex redAgain = synth.addGlyph(local_square(300.0, 0.0, 300.0), 700u, 0u, "");
    const ez::ttf::GlyphIndex inkLayer = synth.addGlyph(local_square(0.0, 300.0, 300.0), 700u, 0u, "");
    CTEST_ASSERT(synth.addColorLayer(base, redLayer, local_color(255, 0, 0)));
    CTEST_ASSERT(synth.addColorLayer(base, blueLayer, local_color(0, 0, 255)));
    CTEST_ASSERT(synth.addColorLayer(base, redAgain, local_color(255, 0, 0)));
    CTEST_ASSERT(synth.addTextColorLayer(base, inkLayer));
    ez::ttf::Font font;
    CTEST_ASSERT_MESSAGE(synth.build(font), "the colored font failed to build");
    CTEST_ASSERT(font.getGlyphCount() == 6u);
    const std::vector<ez::ttf::ColrLayer>* pLayers = font.getGlyphColorLayers(base);
    CTEST_ASSERT(pLayers != nullptr);
    CTEST_ASSERT(pLayers->size() == 4u);
    CTEST_ASSERT((*pLayers)[0].glyphIndex == redLayer);
    CTEST_ASSERT((*pLayers)[1].glyphIndex == blueLayer);
    CTEST_ASSERT((*pLayers)[2].glyphIndex == redAgain);
    CTEST_ASSERT((*pLayers)[3].glyphIndex == inkLayer);
    CTEST_ASSERT((*pLayers)[0].paletteEntry == (*pLayers)[2].paletteEntry);
    CTEST_ASSERT((*pLayers)[0].paletteEntry != (*pLayers)[1].paletteEntry);
    CTEST_ASSERT((*pLayers)[3].paletteEntry == 0xFFFFu);
    CTEST_ASSERT(font.getPaletteEntryCount(0u) == 2u);
    ez::ttf::ColorRgba color;
    CTEST_ASSERT(font.getPaletteColor(0u, (*pLayers)[1].paletteEntry, color));
    CTEST_ASSERT((color.red == 0) && (color.blue == 255));
    CTEST_ASSERT(font.getGlyphColorLayers(redLayer) == nullptr);  // a layer is no base
    CTEST_ASSERT(font.getGlyphIndex(0xE000u) == base);
    return true;
}

// the point of it all : a synthesized font is a source like any other
// for the builder — two of them merge, and a real font joins when the
// tree provides one of the same em
bool TestEzTtfSynthesizer_MergesThroughTheBuilder() {
    ez::ttf::Synthesizer first;
    first.setMetrics(2048u, 1638, -410, 0);
    first.addGlyph(local_square(0.0, 0.0, 1000.0), 1100u, 0xE000u, "one");
    ez::ttf::Font fontOne;
    CTEST_ASSERT(first.build(fontOne));
    ez::ttf::Synthesizer second;
    second.setMetrics(2048u, 1638, -410, 0);
    second.addGlyph(local_square(100.0, 100.0, 500.0), 700u, 0xE001u, "two");
    ez::ttf::Font fontTwo;
    CTEST_ASSERT(second.build(fontTwo));
    ez::ttf::Builder builder;
    const int32_t sourceOne = builder.addSource(fontOne);
    const int32_t sourceTwo = builder.addSource(fontTwo);
    CTEST_ASSERT(builder.pickCodePoint(sourceOne, 0xE000u, 0xE000u, "one"));
    CTEST_ASSERT(builder.pickCodePoint(sourceTwo, 0xE001u, 0xE001u, "two"));
    ez::ttf::Font merged;
    CTEST_ASSERT_MESSAGE(builder.build(merged), "the merge of two synthesized fonts failed");
    CTEST_ASSERT(merged.getGlyphCount() == 3u);
    CTEST_ASSERT(merged.getGlyphIndex(0xE000u) != 0u);
    CTEST_ASSERT(merged.getGlyphIndex(0xE001u) != 0u);
    ez::ttf::Glyph two;
    CTEST_ASSERT(merged.getGlyph(merged.getGlyphIndex(0xE001u), two));
    CTEST_ASSERT((two.xMin == 100) && (two.xMax == 600));
    CTEST_ASSERT(merged.getGlyphName(merged.getGlyphIndex(0xE001u)) == "two");
#ifdef EZTTF_LOCAL_TTF
    {
        ez::ttf::Font roboto;
        if (roboto.openFromFile(EZTTF_LOCAL_TTF) && (roboto.getInfos().unitsPerEm == 2048u)) {
            ez::ttf::Builder mixed;
            const int32_t real = mixed.addSource(roboto);
            const int32_t synth = mixed.addSource(fontOne);
            CTEST_ASSERT(mixed.pickCodePoint(real, 0x41u, 0x41u, "A"));
            CTEST_ASSERT(mixed.pickCodePoint(synth, 0xE000u, 0xE000u, "one"));
            ez::ttf::Font both;
            CTEST_ASSERT_MESSAGE(mixed.build(both), "the merge of a real font and a synthesized one failed");
            CTEST_ASSERT(both.getGlyphIndex(0x41u) != 0u);
            CTEST_ASSERT(both.getGlyphIndex(0xE000u) != 0u);
            printf("real font pass : roboto and a synthesized glyph merged into %u glyphs\n", static_cast<uint32_t>(both.getGlyphCount()));
        }
    }
#endif
    return true;
}

bool TestEzTtfSynthesizer_RefusesUnknownLayers() {
    ez::ttf::Synthesizer synth;
    const ez::ttf::GlyphIndex base = synth.addGlyph(local_square(0.0, 0.0, 100.0), 100u, 0xE000u, "x");
    CTEST_ASSERT(!synth.addColorLayer(base, 9u, local_color(1, 2, 3)));
    CTEST_ASSERT(!synth.addColorLayer(9u, base, local_color(1, 2, 3)));
    CTEST_ASSERT(!synth.addTextColorLayer(base, 42u));
    ez::ttf::Synthesizer empty;
    ez::ttf::Font font;
    CTEST_ASSERT(!empty.build(font));
    CTEST_ASSERT(!empty.getErrors().empty());
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzTtfSynthesizer(const std::string& vTest) {
    IfTestExist(TestEzTtfSynthesizer_BuildsAFontThatReopens);
    else IfTestExist(TestEzTtfSynthesizer_ColorLayersRoundTrip);
    else IfTestExist(TestEzTtfSynthesizer_MergesThroughTheBuilder);
    else IfTestExist(TestEzTtfSynthesizer_RefusesUnknownLayers);
    return false;
}
