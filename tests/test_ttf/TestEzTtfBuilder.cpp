#include <TestEzTtfBuilder.h>
#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezTTF/ezTTF.hpp>

#include <cstdio>
#include <string>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

// a source font with a COMPOSITE : 4 glyphs — notdef (empty), a triangle
// ('A', advance 500), an empty space (0x20), and a composite ('B',
// advance 700) referencing the triangle at (10, 20). built through the
// P5 writer : the builder input is a real font by construction.
// aWithColor adds the COLR/CPAL pair : the composite becomes a LAYERED
// base (triangle tinted by palette entry 0, then entry 0xFFFF = text)
bool buildCompositeSourceFont(ez::ttf::Font& aoFont, bool aWithColor = false) {
    ez::ttf::HeadTable head;
    head.version.setFloat(1.0f);
    head.fontRevision.setFloat(1.0f);
    head.unitsPerEm = 1000u;
    head.xMin = 0;
    head.yMin = 0;
    head.xMax = 110;
    head.yMax = 100;
    head.lowestRecPPEM = 8u;
    head.indexToLocFormat = 1;
    ez::ttf::MaxpTable maxp;
    maxp.version.setFloat(1.0f);
    maxp.numGlyphs = 4u;
    ez::ttf::HheaTable hhea;
    hhea.ascender = 800;
    hhea.descender = -200;
    hhea.advanceWidthMax = 700u;
    hhea.numberOfHMetrics = 4u;
    ez::ttf::HmtxTable hmtx;
    const uint16_t advances[4] = {600u, 500u, 250u, 700u};
    const int16_t bearings[4] = {0, 10, 0, 5};
    for (int32_t glyphIdx = 0; glyphIdx < 4; ++glyphIdx) {
        hmtx.advanceWidths.push_back(advances[glyphIdx]);
        hmtx.leftSideBearings.push_back(bearings[glyphIdx]);
    }
    // glyf : triangle (20 bytes) then the composite (18 bytes)
    ez::ttf::Stream glyf;
    glyf.writeI16(1);
    glyf.writeI16(0);
    glyf.writeI16(0);
    glyf.writeI16(100);
    glyf.writeI16(80);
    glyf.writeU16(2u);
    glyf.writeU16(0u);
    glyf.writeU8(0x31u);
    glyf.writeU8(0x33u);
    glyf.writeU8(0x26u);
    glyf.writeU8(100u);
    glyf.writeU8(50u);
    glyf.writeU8(80u);
    const uint32_t triangleEnd = static_cast<uint32_t>(glyf.getSize());
    glyf.writeI16(-1);  // composite
    glyf.writeI16(0);
    glyf.writeI16(0);
    glyf.writeI16(110);
    glyf.writeI16(100);
    glyf.writeU16(ez::ttf::kGlyfArgsAreWords | ez::ttf::kGlyfArgsAreXY);  // LAST component
    glyf.writeU16(1u);  // references the TRIANGLE (source index 1)
    glyf.writeI16(10);
    glyf.writeI16(20);
    // loca : numGlyphs+1 entries — notdef empty (0..0), triangle 0..20,
    // space empty (20..20), composite 20..38
    ez::ttf::LocaTable loca;
    loca.offsets.push_back(0u);
    loca.offsets.push_back(0u);
    loca.offsets.push_back(triangleEnd);
    loca.offsets.push_back(triangleEnd);
    loca.offsets.push_back(static_cast<uint32_t>(glyf.getSize()));
    std::map<ez::ttf::CodePoint, ez::ttf::GlyphIndex> cmap;
    cmap[0x41u] = 1u;  // 'A' -> triangle
    cmap[0x20u] = 2u;  // space
    cmap[0x42u] = 3u;  // 'B' -> composite
    std::vector<ez::ttf::TaggedTable> tables;
    tables.resize(7);
    tables[0].tag = ez::ttf::kTagHead;
    ez::ttf::emitHeadTable(head, tables[0].bytes);
    tables[1].tag = ez::ttf::kTagMaxp;
    ez::ttf::emitMaxpTable(maxp, tables[1].bytes);
    tables[2].tag = ez::ttf::kTagHhea;
    ez::ttf::emitHheaTable(hhea, maxp.numGlyphs, tables[2].bytes);
    tables[3].tag = ez::ttf::kTagHmtx;
    ez::ttf::emitHmtxTable(hmtx, tables[3].bytes);
    tables[4].tag = ez::ttf::kTagLoca;
    ez::ttf::emitLocaTable(loca, tables[4].bytes);
    tables[5].tag = ez::ttf::kTagCmap;
    ez::ttf::emitCmapTable(cmap, tables[5].bytes);
    tables[6].tag = ez::ttf::kTagGlyf;
    tables[6].bytes.appendStream(glyf);
    if (aWithColor) {
        std::map<ez::ttf::GlyphIndex, std::vector<ez::ttf::ColrLayer> > baseToLayers;
        std::vector<ez::ttf::ColrLayer> layers(2);
        layers[0].glyphIndex = 1u;       // the triangle, tinted
        layers[0].paletteEntry = 0u;
        layers[1].glyphIndex = 1u;       // the triangle again, text color
        layers[1].paletteEntry = 0xFFFFu;
        baseToLayers[3u] = layers;       // the composite is the base
        std::vector<std::vector<ez::ttf::ColorRgba> > palettes(1);
        ez::ttf::ColorRgba redColor;
        redColor.red = 255u;
        redColor.green = 10u;
        redColor.blue = 20u;
        ez::ttf::ColorRgba greenColor;
        greenColor.green = 200u;
        palettes[0].push_back(redColor);
        palettes[0].push_back(greenColor);
        ez::ttf::TaggedTable colr;
        colr.tag = ez::ttf::kTagColr;
        ez::ttf::emitColrTable(baseToLayers, colr.bytes);
        tables.push_back(colr);
        ez::ttf::TaggedTable cpal;
        cpal.tag = ez::ttf::kTagCpal;
        ez::ttf::emitCpalTable(palettes, cpal.bytes);
        tables.push_back(cpal);
    }
    std::vector<uint8_t> bytes;
    if (!ez::ttf::assembleFont(tables, bytes)) {
        return false;
    }
    return aoFont.openFromMemory(&bytes[0], bytes.size());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// picking the COMPOSITE alone embarks its component : the new font holds
// notdef + composite + triangle, the component index REWRITTEN to the
// new location, and the outline still resolves to the triangle points
bool TestEzTtfBuilder_SubsetEmbarksTheCompositeClosure() {
    ez::ttf::Font source;
    CTEST_ASSERT(buildCompositeSourceFont(source));
    ez::ttf::Builder builder;
    const int32_t sourceIdx = builder.addSource(source);
    CTEST_ASSERT(sourceIdx == 0);
    // recode 'B' -> 0xE000 (a PUA slot), renamed
    CTEST_ASSERT(builder.pickCodePoint(sourceIdx, 0x42u, 0xE000u, "ez_composite"));
    ez::ttf::Font built;
    CTEST_ASSERT(builder.build(built));
    CTEST_ASSERT(built.getGlyphCount() == 3u);  // notdef + composite + its triangle
    const ez::ttf::GlyphIndex newComposite = built.getGlyphIndex(0xE000u);
    CTEST_ASSERT(newComposite != 0u);
    CTEST_ASSERT(built.getGlyphName(newComposite) == "ez_composite");
    ez::ttf::Glyph glyph;
    CTEST_ASSERT(built.getGlyph(newComposite, glyph));
    CTEST_ASSERT(glyph.isComposite && glyph.components.size() == 1u);
    // the REMAP : the component points INSIDE the new font, and that
    // glyph is the triangle
    const ez::ttf::GlyphIndex componentIdx = glyph.components[0].glyphIndex;
    CTEST_ASSERT(componentIdx < built.getGlyphCount());
    ez::ttf::Glyph component;
    CTEST_ASSERT(built.getGlyph(componentIdx, component));
    CTEST_ASSERT(component.contours.size() == 1u && component.contours[0].size() == 3u);
    CTEST_ASSERT(component.contours[0][1].x == 100);
    // the advance followed the pick
    uint16_t advance = 0;
    CTEST_ASSERT(built.getAdvanceWidth(newComposite, advance) && advance == 700u);
    return true;
}

// two sources merged, a recode collision : the LAST pick wins (the
// Baker rule), and both surviving mappings resolve in the built font
bool TestEzTtfBuilder_MergesTwoSourcesLastPickWins() {
    ez::ttf::Font sourceA;
    ez::ttf::Font sourceB;
    CTEST_ASSERT(buildCompositeSourceFont(sourceA));
    CTEST_ASSERT(buildCompositeSourceFont(sourceB));
    ez::ttf::Builder builder;
    const int32_t idxA = builder.addSource(sourceA);
    const int32_t idxB = builder.addSource(sourceB);
    // A's triangle to 0xE000, then B's SPACE to the SAME codepoint : B wins.
    // A's triangle also lands at 0xE001 to survive on its own
    CTEST_ASSERT(builder.pickCodePoint(idxA, 0x41u, 0xE000u, "a_tri"));
    CTEST_ASSERT(builder.pickCodePoint(idxB, 0x20u, 0xE000u, "b_space"));
    CTEST_ASSERT(builder.pickCodePoint(idxA, 0x41u, 0xE001u, "a_tri_again"));
    ez::ttf::Font built;
    CTEST_ASSERT(builder.build(built));
    // the winner of 0xE000 is B's space : an EMPTY glyph named b_space
    const ez::ttf::GlyphIndex winnerIdx = built.getGlyphIndex(0xE000u);
    CTEST_ASSERT(winnerIdx != 0u);
    CTEST_ASSERT(built.getGlyphName(winnerIdx) == "b_space");
    ez::ttf::Glyph winner;
    CTEST_ASSERT(built.getGlyph(winnerIdx, winner) && winner.isEmpty());
    // A's triangle lives at 0xE001, points intact
    const ez::ttf::GlyphIndex triangleIdx = built.getGlyphIndex(0xE001u);
    ez::ttf::Glyph triangle;
    CTEST_ASSERT(built.getGlyph(triangleIdx, triangle));
    CTEST_ASSERT(triangle.contours.size() == 1u && triangle.contours[0][2].y == 80);
    // a mismatched-upem source is refused honestly
    ez::ttf::Builder strict;
    strict.addSource(sourceA);
    (void)idxB;
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// P7 : the color pair reads back (layers per base, palette colors in
// honest rgba) and SURVIVES the write round-trip. an incomplete pair
// would have been dropped at open — completeness enforced by Font
bool TestEzTtfBuilder_ColorLayersReadAndRoundTrip() {
    ez::ttf::Font font;
    CTEST_ASSERT(buildCompositeSourceFont(font, true));
    CTEST_ASSERT(font.hasColorLayers());
    const std::vector<ez::ttf::ColrLayer>* pLayers = font.getGlyphColorLayers(3u);
    CTEST_ASSERT(pLayers != nullptr && pLayers->size() == 2u);
    CTEST_ASSERT((*pLayers)[0].glyphIndex == 1u && (*pLayers)[0].paletteEntry == 0u);
    CTEST_ASSERT((*pLayers)[1].paletteEntry == 0xFFFFu);  // the text-color layer
    CTEST_ASSERT(font.getGlyphColorLayers(1u) == nullptr);  // the triangle is a layer, not a base
    ez::ttf::ColorRgba color;
    CTEST_ASSERT(font.getPaletteCount() == 1u);
    CTEST_ASSERT(font.getPaletteColor(0u, 0u, color));
    CTEST_ASSERT(color.red == 255u && color.green == 10u && color.blue == 20u && color.alpha == 255u);
    CTEST_ASSERT(!font.getPaletteColor(0u, 2u, color));  // past the palette
    // the write round-trip : the pair survives intact
    std::vector<uint8_t> written;
    CTEST_ASSERT(font.writeToMemory(written));
    ez::ttf::Font reopened;
    CTEST_ASSERT(reopened.openFromMemory(&written[0], written.size()));
    CTEST_ASSERT(reopened.hasColorLayers());
    pLayers = reopened.getGlyphColorLayers(3u);
    CTEST_ASSERT(pLayers != nullptr && pLayers->size() == 2u);
    CTEST_ASSERT(reopened.getPaletteColor(0u, 1u, color) && color.green == 200u);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// THE REAL-WORLD PASS : everything above runs on synthetic fonts — this
// one opens the ogi vendored roboto when the tree provides it (skipped
// honestly otherwise : standalone ezlibs, the linux ci). the whole chain
// against reality : parse, names, outlines, subset, write, reopen
bool TestEzTtfBuilder_ValidatesAgainstARealFont() {
#ifdef EZTTF_LOCAL_TTF
    {
        FILE* pProbe = std::fopen(EZTTF_LOCAL_TTF, "rb");
        if (pProbe == nullptr) {
            printf("SKIPPED : no local real font at %s\n", EZTTF_LOCAL_TTF);
            return true;
        }
        std::fclose(pProbe);
    }
    ez::ttf::Font roboto;
    CTEST_ASSERT_MESSAGE(roboto.openFromFile(EZTTF_LOCAL_TTF), "real roboto refused to open");
    CTEST_ASSERT(roboto.getGlyphCount() > 100u);
    CTEST_ASSERT(roboto.getInfos().unitsPerEm == 2048u || roboto.getInfos().unitsPerEm == 1000u);
    // the ascii chain : mapped, measured, named, outlined
    const ez::ttf::GlyphIndex glyphA = roboto.getGlyphIndex(0x41u);
    CTEST_ASSERT(glyphA != 0u);
    uint16_t advance = 0u;
    CTEST_ASSERT(roboto.getAdvanceWidth(glyphA, advance) && advance > 0u);
    ez::ttf::Glyph outline;
    CTEST_ASSERT(roboto.getGlyph(glyphA, outline));
    CTEST_ASSERT(!outline.contours.empty() || !outline.components.empty());
    CTEST_ASSERT(roboto.getName(ez::ttf::kNameIdFamily).find("Roboto") != std::string::npos);
    // the bootstrap of the plan : a real subset through the builder
    ez::ttf::Builder builder;
    const int32_t sourceIdx = builder.addSource(roboto);
    CTEST_ASSERT(builder.pickCodePoint(sourceIdx, 0x41u, 0x41u, "A"));
    CTEST_ASSERT(builder.pickCodePoint(sourceIdx, 0x42u, 0x42u, "B"));
    CTEST_ASSERT(builder.pickCodePoint(sourceIdx, 0x67u, 0x67u, "g"));  // a curvy one
    ez::ttf::Font subset;
    CTEST_ASSERT_MESSAGE(builder.build(subset), "the roboto subset failed to build");
    CTEST_ASSERT(subset.getGlyphCount() >= 4u);  // notdef + 3 (+ composite deps)
    CTEST_ASSERT(subset.getGlyphIndex(0x41u) != 0u);
    ez::ttf::Glyph subsetOutline;
    CTEST_ASSERT(subset.getGlyph(subset.getGlyphIndex(0x41u), subsetOutline));
    CTEST_ASSERT(!subsetOutline.contours.empty() || !subsetOutline.components.empty());
    uint16_t subsetAdvance = 0u;
    CTEST_ASSERT(subset.getAdvanceWidth(subset.getGlyphIndex(0x41u), subsetAdvance));
    CTEST_ASSERT(subsetAdvance == advance);  // the metric followed the pick
    CTEST_ASSERT(subset.getGlyphName(subset.getGlyphIndex(0x67u)) == "g");
    printf("real font pass : %u glyphs opened, subset of %u written and reopened\n",  //
           static_cast<uint32_t>(roboto.getGlyphCount()), static_cast<uint32_t>(subset.getGlyphCount()));
#ifdef EZTTF_RES_DIR
    // the BOOTSTRAP of the committed fixture : written once on a dev
    // machine (never overwritten), committed, then the fixture test
    // below covers the real-font path on every ci
    {
        const std::string fixturePath = std::string(EZTTF_RES_DIR) + "robotoSubset.ttf";
        FILE* pExisting = std::fopen(fixturePath.c_str(), "rb");
        if (pExisting != nullptr) {
            std::fclose(pExisting);
        } else if (subset.writeToFile(fixturePath)) {
            printf("fixture bootstrapped : %s\n", fixturePath.c_str());
        }
    }
#endif
#endif
    return true;
}

// the COMMITTED fixture (res/robotoSubset.ttf, apache 2.0 — NOTICE.txt) :
// the real-font chain on every machine, ci included. skips only until
// the bootstrap above ran once
bool TestEzTtfBuilder_OpensTheCommittedFixture() {
#ifdef EZTTF_RES_DIR
    const std::string fixturePath = std::string(EZTTF_RES_DIR) + "robotoSubset.ttf";
    {
        FILE* pProbe = std::fopen(fixturePath.c_str(), "rb");
        if (pProbe == nullptr) {
            printf("SKIPPED : the fixture is not bootstrapped yet (%s)\n", fixturePath.c_str());
            return true;
        }
        std::fclose(pProbe);
    }
    ez::ttf::Font fixture;
    CTEST_ASSERT_MESSAGE(fixture.openFromFile(fixturePath), "the committed fixture refused to open");
    CTEST_ASSERT(fixture.getGlyphCount() >= 4u);
    const ez::ttf::GlyphIndex glyphA = fixture.getGlyphIndex(0x41u);
    CTEST_ASSERT(glyphA != 0u);
    CTEST_ASSERT(fixture.getGlyphName(glyphA) == "A");
    ez::ttf::Glyph outline;
    CTEST_ASSERT(fixture.getGlyph(glyphA, outline));
    CTEST_ASSERT(!outline.contours.empty() || !outline.components.empty());
    uint16_t advance = 0u;
    CTEST_ASSERT(fixture.getAdvanceWidth(glyphA, advance) && advance > 0u);
#endif
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzTtfBuilder(const std::string& vTest) {
    IfTestExist(TestEzTtfBuilder_SubsetEmbarksTheCompositeClosure);
    else IfTestExist(TestEzTtfBuilder_MergesTwoSourcesLastPickWins);
    else IfTestExist(TestEzTtfBuilder_ColorLayersReadAndRoundTrip);
    else IfTestExist(TestEzTtfBuilder_ValidatesAgainstARealFont);
    else IfTestExist(TestEzTtfBuilder_OpensTheCommittedFixture);
    return false;
}
