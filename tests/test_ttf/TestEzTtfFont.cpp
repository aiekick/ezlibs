#include <TestEzTtfFont.h>
#include <TestEzTtfFixtures.hpp>
#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezTTF/ezTTF.hpp>

#include <cstdio>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

// a COMPLETE synthetic font, assembled the way the P5 writer will do it :
// each table composed in its own stream, the directory computed from the
// real sizes, everything concatenated. 3 glyphs (notdef, 'A', a space),
// upem 1000, ascender 800, 'A' -> glyph 1 (advance 500), space -> glyph 2
// (advance 250, EMPTY glyf span)
void buildMinimalFont(ez::ttf::Stream& aoFont) {
    ez::ttf::Fixed one;
    one.setFloat(1.0f);
    // --- the table payloads ---
    ez::ttf::Stream head;
    head.writeFixed(one);
    head.writeFixed(one);
    head.writeU32(0u);
    head.writeU32(ez::ttf::kHeadMagicNumber);
    head.writeU16(0u);
    head.writeU16(1000u);  // unitsPerEm
    head.writeI64(0);
    head.writeI64(0);
    head.writeI16(-10);  // bbox
    head.writeI16(-20);
    head.writeI16(510);
    head.writeI16(820);
    head.writeU16(0u);
    head.writeU16(9u);
    head.writeI16(2);
    head.writeI16(0);  // SHORT loca
    head.writeI16(0);
    ez::ttf::Stream maxp;
    maxp.writeFixed(one);
    maxp.writeU16(3u);  // numGlyphs
    for (uint16_t fieldIdx = 0; fieldIdx < 13u; ++fieldIdx) {
        maxp.writeU16(0u);
    }
    ez::ttf::Stream hhea;
    hhea.writeFixed(one);
    hhea.writeI16(800);   // ascender
    hhea.writeI16(-200);  // descender
    hhea.writeI16(50);    // lineGap
    hhea.writeU16(500u);
    hhea.writeI16(0);
    hhea.writeI16(0);
    hhea.writeI16(510);
    hhea.writeI16(1);
    hhea.writeI16(0);
    hhea.writeI16(0);
    for (int32_t reservedIdx = 0; reservedIdx < 4; ++reservedIdx) {
        hhea.writeI16(0);
    }
    hhea.writeI16(0);
    hhea.writeU16(2u);  // numberOfHMetrics : glyph 2 shares advance... no —
    // 2 full pairs (notdef 600, A 500), the space rides the trailing run
    // and SHARES 500 ? we want 250 : so THREE full pairs instead
    ez::ttf::Stream hmtx;
    hmtx.writeU16(600u);  // notdef
    hmtx.writeI16(0);
    hmtx.writeU16(500u);  // 'A'
    hmtx.writeI16(10);
    hmtx.writeI16(0);     // space : trailing lsb-only, advance = 500 (shared)
    // loca SHORT : 4 entries, glyph 0 = 0..10, glyph 1 = 10..30, glyph 2 EMPTY
    ez::ttf::Stream loca;
    loca.writeU16(0u);
    loca.writeU16(5u);   // *2 = 10
    loca.writeU16(15u);  // *2 = 30
    loca.writeU16(15u);  // EMPTY span
    // glyf : 10 zero bytes for notdef (an empty simple entry), then a REAL
    // triangle for glyph 1 — 20 bytes exactly, matching its loca span :
    // points (0,0) on, (100,0) on, (50,80) off (a quadratic control)
    ez::ttf::Stream glyf;
    for (uint32_t byteIdx = 0; byteIdx < 10u; ++byteIdx) {
        glyf.writeU8(0u);
    }
    glyf.writeI16(1);    // one contour
    glyf.writeI16(0);    // bbox
    glyf.writeI16(0);
    glyf.writeI16(100);
    glyf.writeI16(80);
    glyf.writeU16(2u);   // endPts : last point index
    glyf.writeU16(0u);   // no instructions
    glyf.writeU8(0x31u);  // p0 : on, x same, y same (deltas 0)
    glyf.writeU8(0x33u);  // p1 : on, x short +, y same
    glyf.writeU8(0x26u);  // p2 : OFF, y short +, x short - (bit4 clear)... x short = 0x02 -> 0x26
    glyf.writeU8(100u);  // x deltas : p1 +100
    glyf.writeU8(50u);   //            p2 -50 (short, negative by flag)
    glyf.writeU8(80u);   // y deltas : p2 +80
    // cmap : format 4, 'A' -> glyph 1, space 0x20 -> glyph 2, + terminator
    ez::ttf::Stream cmap;
    cmap.writeU16(0u);
    cmap.writeU16(1u);
    cmap.writeU16(3u);
    cmap.writeU16(1u);
    cmap.writeU32(12u);
    cmap.writeU16(4u);  // format
    cmap.writeU16(0u);
    cmap.writeU16(0u);
    cmap.writeU16(6u);  // 3 segments
    cmap.writeU16(0u);
    cmap.writeU16(0u);
    cmap.writeU16(0u);
    cmap.writeU16(0x20u);    // ends : space
    cmap.writeU16(0x41u);    //        'A'
    cmap.writeU16(0xFFFFu);  //        terminator
    cmap.writeU16(0u);
    cmap.writeU16(0x20u);  // starts
    cmap.writeU16(0x41u);
    cmap.writeU16(0xFFFFu);
    cmap.writeU16(static_cast<uint16_t>(-30));  // deltas : 0x20 - 30 = 2
    cmap.writeU16(static_cast<uint16_t>(-64));  //          0x41 - 64 = 1
    cmap.writeU16(1u);
    cmap.writeU16(0u);  // idRangeOffsets
    cmap.writeU16(0u);
    cmap.writeU16(0u);
    // --- the assembly : directory then payloads, offsets computed ---
    const ez::ttf::Stream* pTables[6] = {&head, &maxp, &hhea, &hmtx, &loca, &cmap};
    const ez::ttf::TableTag tags[6] = {ez::ttf::kTagHead, ez::ttf::kTagMaxp, ez::ttf::kTagHhea,
                                       ez::ttf::kTagHmtx, ez::ttf::kTagLoca, ez::ttf::kTagCmap};
    const uint16_t tableCount = 7u;  // the six + glyf
    aoFont.writeU32(ez::ttf::kSfntVersionTrueType);
    aoFont.writeU16(tableCount);
    aoFont.writeU16(0u);  // the search trio : never trusted, zeros are fine here
    aoFont.writeU16(0u);
    aoFont.writeU16(0u);
    uint32_t runningOffset = 12u + 16u * tableCount;
    for (int32_t tableIdx = 0; tableIdx < 6; ++tableIdx) {
        aoFont.writeTag(tags[tableIdx]);
        aoFont.writeU32(0u);  // checkSum : opaque at read time
        aoFont.writeU32(runningOffset);
        aoFont.writeU32(static_cast<uint32_t>(pTables[tableIdx]->getSize()));
        runningOffset += static_cast<uint32_t>(pTables[tableIdx]->getSize());
    }
    aoFont.writeTag(ez::ttf::kTagGlyf);
    aoFont.writeU32(0u);
    aoFont.writeU32(runningOffset);
    aoFont.writeU32(static_cast<uint32_t>(glyf.getSize()));
    for (int32_t tableIdx = 0; tableIdx < 6; ++tableIdx) {
        aoFont.appendStream(*pTables[tableIdx]);
    }
    aoFont.appendStream(glyf);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// the whole structural chain on a COMPLETE synthetic font : infos,
// mappings both ways, resolved metrics, loca spans
bool TestEzTtfFont_OpensACompleteSyntheticFont() {
    ez::ttf::Stream bytes;
    buildMinimalFont(bytes);
    ez::ttf::Font font;
    CTEST_ASSERT(font.openFromMemory(bytes.getBytes(), bytes.getSize()));
    CTEST_ASSERT(font.isOpened());
    CTEST_ASSERT(font.getGlyphCount() == 3u);
    CTEST_ASSERT(font.getInfos().unitsPerEm == 1000u);
    CTEST_ASSERT(font.getInfos().ascender == 800);
    CTEST_ASSERT(font.getInfos().descender == -200);
    // cmap both ways
    CTEST_ASSERT(font.getGlyphIndex(0x41u) == 1u);
    CTEST_ASSERT(font.getGlyphIndex(0x20u) == 2u);
    CTEST_ASSERT(font.getGlyphIndex(0x42u) == 0u);  // absent = notdef
    const std::set<ez::ttf::CodePoint>* pCodePoints = font.getCodePoints(1u);
    CTEST_ASSERT(pCodePoints != nullptr && pCodePoints->count(0x41u) == 1u);
    // metrics : the trailing-run rule gave the space the LAST full advance
    uint16_t advance = 0u;
    CTEST_ASSERT(font.getAdvanceWidth(1u, advance) && advance == 500u);
    CTEST_ASSERT(font.getAdvanceWidth(2u, advance) && advance == 500u);
    CTEST_ASSERT(!font.getAdvanceWidth(3u, advance));  // past numGlyphs
    // loca spans : glyph 1 = 10..30, the space is EMPTY
    uint32_t offset = 0u;
    uint32_t length = 0u;
    CTEST_ASSERT(font.getGlyphDataSpan(1u, offset, length) && offset == 10u && length == 20u);
    CTEST_ASSERT(font.getGlyphDataSpan(2u, offset, length) && length == 0u);
    CTEST_ASSERT(font.getErrors().empty());
    return true;
}

// a required table missing : refused with a NAMED error, never half-open
bool TestEzTtfFont_RefusesAFontMissingARequiredTable() {
    ez::ttf::Stream bytes;
    buildMinimalFont(bytes);
    // rebuild WITHOUT the cmap record : patch numTables and drop its record
    std::vector<uint8_t> raw(bytes.getBytes(), bytes.getBytes() + bytes.getSize());
    // simpler surgical strike : corrupt the cmap TAG in the directory so
    // findTableRecord misses it (record 5 tag at 12 + 5*16)
    raw[12u + 5u * 16u] = 'X';
    ez::ttf::Font font;
    CTEST_ASSERT(!font.openFromMemory(&raw[0], raw.size()));
    CTEST_ASSERT(!font.isOpened());
    CTEST_ASSERT(!font.getErrors().empty());
    CTEST_ASSERT(font.getErrors()[0].find("cmap") != std::string::npos);
    // and the object answers empty, not stale
    CTEST_ASSERT(font.getGlyphCount() == 0u);
    CTEST_ASSERT(font.getGlyphIndex(0x41u) == 0u);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// the glyph 1 outline decodes through every simple-glyph path : short
// and same deltas, positive and negative, on and off curve, the split
// on endPts. the empty span answers an empty glyph
bool TestEzTtfFont_ReadsASimpleGlyphOutline() {
    ez::ttf::Stream bytes;
    buildMinimalFont(bytes);
    ez::ttf::Font font;
    CTEST_ASSERT(font.openFromMemory(bytes.getBytes(), bytes.getSize()));
    ez::ttf::Glyph glyph;
    CTEST_ASSERT(font.getGlyph(1u, glyph));
    CTEST_ASSERT(!glyph.isComposite);
    CTEST_ASSERT(glyph.contours.size() == 1u);
    CTEST_ASSERT(glyph.contours[0].size() == 3u);
    CTEST_ASSERT(glyph.contours[0][0].x == 0 && glyph.contours[0][0].y == 0 && glyph.contours[0][0].onCurve);
    CTEST_ASSERT(glyph.contours[0][1].x == 100 && glyph.contours[0][1].y == 0 && glyph.contours[0][1].onCurve);
    CTEST_ASSERT(glyph.contours[0][2].x == 50 && glyph.contours[0][2].y == 80 && !glyph.contours[0][2].onCurve);
    CTEST_ASSERT(glyph.xMax == 100 && glyph.yMax == 80);
    // the space : an EMPTY span is a valid empty glyph
    ez::ttf::Glyph space;
    CTEST_ASSERT(font.getGlyph(2u, space));
    CTEST_ASSERT(space.isEmpty());
    CTEST_ASSERT(!font.getGlyph(3u, space));  // past numGlyphs
    return true;
}

// a composite entry : components kept as REFERENCES (offsets + scales),
// never flattened at parse — the subset lesson of the plan
bool TestEzTtfFont_ReadsACompositeEntry() {
    ez::ttf::Stream entry;
    entry.writeI16(-1);  // composite marker
    entry.writeI16(0);   // bbox
    entry.writeI16(0);
    entry.writeI16(200);
    entry.writeI16(200);
    // component 0 : glyph 5 at (10,-20), word args, MORE
    entry.writeU16(ez::ttf::kGlyfArgsAreWords | ez::ttf::kGlyfArgsAreXY | ez::ttf::kGlyfMoreComponents);
    entry.writeU16(5u);
    entry.writeI16(10);
    entry.writeI16(-20);
    // component 1 : glyph 7, byte args, uniform scale 0.5, LAST
    entry.writeU16(ez::ttf::kGlyfArgsAreXY | ez::ttf::kGlyfHasScale);
    entry.writeU16(7u);
    entry.writeU8(3u);
    entry.writeU8(static_cast<uint8_t>(-4));
    ez::ttf::F2DOT14 half;
    half.setFloat(0.5f);
    entry.writeF2DOT14(half);
    ez::ttf::Glyph glyph;
    CTEST_ASSERT(ez::ttf::parseGlyphData(entry.getBytes(), entry.getSize(), glyph));
    CTEST_ASSERT(glyph.isComposite);
    CTEST_ASSERT(glyph.components.size() == 2u);
    CTEST_ASSERT(glyph.components[0].glyphIndex == 5u);
    CTEST_ASSERT(glyph.components[0].offsetX == 10 && glyph.components[0].offsetY == -20);
    CTEST_ASSERT(glyph.components[1].glyphIndex == 7u);
    CTEST_ASSERT(glyph.components[1].offsetX == 3 && glyph.components[1].offsetY == -4);
    CTEST_ASSERT(glyph.components[1].scaleXX.raw == half.raw);
    CTEST_ASSERT(glyph.components[1].scaleYY.raw == half.raw);
    // a truncated composite : refused, empty
    ez::ttf::Glyph truncated;
    CTEST_ASSERT(!ez::ttf::parseGlyphData(entry.getBytes(), 14u, truncated));
    CTEST_ASSERT(truncated.isEmpty() && !truncated.isComposite);
    return true;
}

// the file flavour : the synthetic font goes to disk and comes back
// identical ; an absent file refuses with its own named error
bool TestEzTtfFont_OpensFromAFile() {
    const char* pFilePath = "ezTtfRoundTrip.bin";
    std::remove(pFilePath);
    ez::ttf::Stream bytes;
    buildMinimalFont(bytes);
    {
        FILE* pFile = std::fopen(pFilePath, "wb");
        CTEST_ASSERT(pFile != nullptr);
        std::fwrite(bytes.getBytes(), 1u, bytes.getSize(), pFile);
        std::fclose(pFile);
    }
    ez::ttf::Font font;
    CTEST_ASSERT(font.openFromFile(pFilePath));
    CTEST_ASSERT(font.getGlyphCount() == 3u);
    CTEST_ASSERT(font.getGlyphIndex(0x41u) == 1u);
    std::remove(pFilePath);
    ez::ttf::Font absent;
    CTEST_ASSERT(!absent.openFromFile("ezTtfDoesNotExist.bin"));
    CTEST_ASSERT(!absent.getErrors().empty());
    CTEST_ASSERT(absent.getErrors()[0].find("file") != std::string::npos);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// THE P5 pivot : open -> write -> reopen — everything survives (infos,
// both cmap directions, resolved metrics, the decoded outline). and the
// written form is a FIXED POINT : write(reopen(write(f))) == write(f)
bool TestEzTtfFont_WriteRoundTripsAndReachesAFixedPoint() {
    ez::ttf::Stream original;
    buildMinimalFont(original);
    ez::ttf::Font font;
    CTEST_ASSERT(font.openFromMemory(original.getBytes(), original.getSize()));
    std::vector<uint8_t> written;
    CTEST_ASSERT(font.writeToMemory(written));
    ez::ttf::Font reopened;
    CTEST_ASSERT(reopened.openFromMemory(&written[0], written.size()));
    CTEST_ASSERT(reopened.getGlyphCount() == 3u);
    CTEST_ASSERT(reopened.getInfos().unitsPerEm == 1000u);
    CTEST_ASSERT(reopened.getInfos().ascender == 800);
    CTEST_ASSERT(reopened.getGlyphIndex(0x41u) == 1u);
    CTEST_ASSERT(reopened.getGlyphIndex(0x20u) == 2u);
    uint16_t advance = 0u;
    CTEST_ASSERT(reopened.getAdvanceWidth(1u, advance) && advance == 500u);
    ez::ttf::Glyph glyph;
    CTEST_ASSERT(reopened.getGlyph(1u, glyph));
    CTEST_ASSERT(glyph.contours.size() == 1u && glyph.contours[0].size() == 3u);
    CTEST_ASSERT(glyph.contours[0][2].x == 50 && glyph.contours[0][2].y == 80 && !glyph.contours[0][2].onCurve);
    // the head checkSumAdjustment was really stamped (not left at zero)
    const std::size_t directoryEnd = 12u + 16u * 7u;
    auto headStamped = false;
    for (std::size_t recordIdx = 0; recordIdx < 7u; ++recordIdx) {
        ez::ttf::Stream directory(&written[0], written.size());
        directory.setReadPos(12u + recordIdx * 16u);
        if (directory.readTag() == ez::ttf::kTagHead) {
            directory.readU32();
            const uint32_t headOffset = directory.readU32();
            ez::ttf::Stream headBytes(&written[0], written.size());
            headBytes.setReadPos(headOffset + 8u);
            headStamped = (headBytes.readU32() != 0u);
        }
    }
    (void)directoryEnd;
    CTEST_ASSERT(headStamped);
    // the fixed point : a second write of the reopened font is byte-identical
    std::vector<uint8_t> writtenAgain;
    CTEST_ASSERT(reopened.writeToMemory(writtenAgain));
    CTEST_ASSERT(writtenAgain == written);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

ez::ttf::Glyph local_squareGlyph(int16_t aLeft, int16_t aBottom, int16_t aSide) {
    ez::ttf::Glyph glyph;
    std::vector<ez::ttf::GlyphPoint> contour(4);
    contour[0].x = aLeft;
    contour[0].y = aBottom;
    contour[1].x = static_cast<int16_t>(aLeft + aSide);
    contour[1].y = aBottom;
    contour[2].x = static_cast<int16_t>(aLeft + aSide);
    contour[2].y = static_cast<int16_t>(aBottom + aSide);
    contour[3].x = aLeft;
    contour[3].y = static_cast<int16_t>(aBottom + aSide);
    for (std::size_t pointIdx = 0; pointIdx < contour.size(); ++pointIdx) {
        contour[pointIdx].onCurve = true;
    }
    glyph.contours.push_back(contour);
    ez::ttf::computeGlyphBounds(glyph);
    return glyph;
}

bool local_pointIs(const ez::ttf::GlyphPoint& aPoint, int16_t aX, int16_t aY) {
    return (aPoint.x == aX) && (aPoint.y == aY);
}

}  // namespace

// the fixture composite resolves to ONE simple outline : the triangle
// carried to its component offset, the box recomputed ; a simple glyph
// flattens to its own points, an empty one to nothing, a bad index
// refuses
bool TestEzTtfFont_FlattensACompositeIntoItsPoints() {
    ez::ttf::Font font;
    CTEST_ASSERT(buildCompositeSourceFont(font));
    ez::ttf::Glyph flat;
    CTEST_ASSERT(ez::ttf::flattenComposite(font, 3u, flat));
    CTEST_ASSERT(!flat.isComposite && flat.components.empty());
    CTEST_ASSERT((flat.contours.size() == 1u) && (flat.contours[0].size() == 3u));
    CTEST_ASSERT(local_pointIs(flat.contours[0][0], 10, 20) && flat.contours[0][0].onCurve);
    CTEST_ASSERT(local_pointIs(flat.contours[0][1], 110, 20) && flat.contours[0][1].onCurve);
    CTEST_ASSERT(local_pointIs(flat.contours[0][2], 60, 100) && !flat.contours[0][2].onCurve);
    CTEST_ASSERT((flat.xMin == 10) && (flat.yMin == 20) && (flat.xMax == 110) && (flat.yMax == 100));
    ez::ttf::Glyph simple;
    CTEST_ASSERT(ez::ttf::flattenComposite(font, 1u, simple));
    CTEST_ASSERT((simple.contours.size() == 1u) && local_pointIs(simple.contours[0][1], 100, 0));
    ez::ttf::Glyph space;
    CTEST_ASSERT(ez::ttf::flattenComposite(font, 2u, space));
    CTEST_ASSERT(space.isEmpty());
    CTEST_ASSERT(!ez::ttf::flattenComposite(font, 4u, space));
    return true;
}

// a nested composite : a half-scaled square carried to (200, 0), itself
// turned by a 2x2 (the format convention : x' = xscale x + scale10 y,
// y' = scale01 x + yscale y) and lifted by 300, beside a plain square at
// (-100, -100) — the flattening resolves the tree component by component
// into two contours, in component order, the box over both ; a glyph
// that references itself is a cycle and refuses
bool TestEzTtfFont_FlattensANestedScaledComposite() {
    ez::ttf::Synthesizer synth;
    synth.setMetrics(1000u, 800, -200, 0);
    const ez::ttf::GlyphIndex squareIdx = synth.addGlyph(local_squareGlyph(0, 0, 100), 600u, 0x41u, "square");
    ez::ttf::Glyph half;
    half.isComposite = true;
    ez::ttf::GlyphComponent halfPart;
    halfPart.glyphIndex = squareIdx;
    halfPart.offsetX = 200;
    halfPart.scaleXX.setFloat(0.5f);
    halfPart.scaleYY.setFloat(0.5f);
    half.components.push_back(halfPart);
    half.xMin = 200;
    half.xMax = 250;
    half.yMax = 50;
    const ez::ttf::GlyphIndex halfIdx = synth.addGlyph(half, 600u, 0x42u, "half");
    ez::ttf::Glyph turned;
    turned.isComposite = true;
    ez::ttf::GlyphComponent turnedPart;
    turnedPart.glyphIndex = halfIdx;
    turnedPart.offsetY = 300;
    turnedPart.scaleXX.setFloat(0.0f);
    turnedPart.scaleXY.setFloat(1.0f);
    turnedPart.scaleYX.setFloat(-1.0f);
    turnedPart.scaleYY.setFloat(0.0f);
    turned.components.push_back(turnedPart);
    ez::ttf::GlyphComponent plainPart;
    plainPart.glyphIndex = squareIdx;
    plainPart.offsetX = -100;
    plainPart.offsetY = -100;
    turned.components.push_back(plainPart);
    turned.xMin = -100;
    turned.yMin = -100;
    turned.xMax = 0;
    turned.yMax = 550;
    const ez::ttf::GlyphIndex turnedIdx = synth.addGlyph(turned, 600u, 0x43u, "turned");
    ez::ttf::Glyph cycle;
    cycle.isComposite = true;
    ez::ttf::GlyphComponent selfPart;
    selfPart.glyphIndex = static_cast<ez::ttf::GlyphIndex>(turnedIdx + 1u);  // its own seat
    cycle.components.push_back(selfPart);
    const ez::ttf::GlyphIndex cycleIdx = synth.addGlyph(cycle, 600u, 0x44u, "cycle");
    CTEST_ASSERT(cycleIdx == turnedIdx + 1u);
    ez::ttf::Font font;
    CTEST_ASSERT_MESSAGE(synth.build(font), "the nested composite font failed to build");
    ez::ttf::Glyph flat;
    CTEST_ASSERT(ez::ttf::flattenComposite(font, turnedIdx, flat));
    CTEST_ASSERT(!flat.isComposite && (flat.contours.size() == 2u));
    CTEST_ASSERT(flat.contours[0].size() == 4u);
    CTEST_ASSERT(local_pointIs(flat.contours[0][0], 0, 500));
    CTEST_ASSERT(local_pointIs(flat.contours[0][1], 0, 550));
    CTEST_ASSERT(local_pointIs(flat.contours[0][2], -50, 550));
    CTEST_ASSERT(local_pointIs(flat.contours[0][3], -50, 500));
    CTEST_ASSERT(flat.contours[1].size() == 4u);
    CTEST_ASSERT(local_pointIs(flat.contours[1][0], -100, -100));
    CTEST_ASSERT(local_pointIs(flat.contours[1][2], 0, 0));
    CTEST_ASSERT((flat.xMin == -100) && (flat.yMin == -100) && (flat.xMax == 0) && (flat.yMax == 550));
    ez::ttf::Glyph looped;
    CTEST_ASSERT(!ez::ttf::flattenComposite(font, cycleIdx, looped));
    CTEST_ASSERT(looped.isEmpty());
    return true;
}

bool TestEzTtfFont(const std::string& vTest) {
    IfTestExist(TestEzTtfFont_OpensACompleteSyntheticFont);
    else IfTestExist(TestEzTtfFont_RefusesAFontMissingARequiredTable);
    else IfTestExist(TestEzTtfFont_ReadsASimpleGlyphOutline);
    else IfTestExist(TestEzTtfFont_ReadsACompositeEntry);
    else IfTestExist(TestEzTtfFont_OpensFromAFile);
    else IfTestExist(TestEzTtfFont_WriteRoundTripsAndReachesAFixedPoint);
    else IfTestExist(TestEzTtfFont_FlattensACompositeIntoItsPoints);
    else IfTestExist(TestEzTtfFont_FlattensANestedScaledComposite);
    return false;
}
