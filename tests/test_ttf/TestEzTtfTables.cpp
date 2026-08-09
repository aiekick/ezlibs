#include <TestEzTtfTables.h>
#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezTTF/ezTTF.hpp>

#include <string>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

// a SYNTHETIC two-table sfnt, composed by our own stream : the directory
// tests depend on no font file. layout : 12 header bytes, 2 records of
// 16 bytes, then the two table payloads
void buildTwoTableSfnt(ez::ttf::Stream& aoStream) {
    aoStream.writeU32(ez::ttf::kSfntVersionTrueType);
    aoStream.writeU16(2u);   // numTables
    aoStream.writeU16(32u);  // searchRange trio : stored but never trusted
    aoStream.writeU16(1u);
    aoStream.writeU16(0u);
    // records : head at 44 (4 bytes), maxp at 48 (6 bytes)
    aoStream.writeTag(ez::ttf::kTagHead);
    aoStream.writeU32(0x11111111u);  // checkSum, opaque here
    aoStream.writeU32(44u);
    aoStream.writeU32(4u);
    aoStream.writeTag(ez::ttf::kTagMaxp);
    aoStream.writeU32(0x22222222u);
    aoStream.writeU32(48u);
    aoStream.writeU32(6u);
    // the payloads (44 + 4 + 6 = 54 bytes total)
    aoStream.writeU32(0xAAAAAAAAu);
    aoStream.writeU32(0xBBBBBBBBu);
    aoStream.writeU16(0xCCCCu);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzTtfTables_ParsesAValidDirectory() {
    ez::ttf::Stream stream;
    buildTwoTableSfnt(stream);
    ez::ttf::SfntHeader header;
    std::vector<ez::ttf::TableRecord> records;
    CTEST_ASSERT(ez::ttf::parseSfntDirectory(stream, header, records));
    CTEST_ASSERT(header.sfntVersion == ez::ttf::kSfntVersionTrueType);
    CTEST_ASSERT(header.numTables == 2u);
    CTEST_ASSERT(records.size() == 2u);
    CTEST_ASSERT(records[0].tag == ez::ttf::kTagHead);
    CTEST_ASSERT(records[0].offset == 44u && records[0].length == 4u);
    CTEST_ASSERT(records[1].tag == ez::ttf::kTagMaxp);
    CTEST_ASSERT(records[1].offset == 48u && records[1].length == 6u);
    return true;
}

bool TestEzTtfTables_FindsATableByTag() {
    ez::ttf::Stream stream;
    buildTwoTableSfnt(stream);
    ez::ttf::SfntHeader header;
    std::vector<ez::ttf::TableRecord> records;
    CTEST_ASSERT(ez::ttf::parseSfntDirectory(stream, header, records));
    const ez::ttf::TableRecord* pMaxp = ez::ttf::findTableRecord(records, ez::ttf::kTagMaxp);
    CTEST_ASSERT(pMaxp != nullptr);
    CTEST_ASSERT(pMaxp->checkSum == 0x22222222u);
    CTEST_ASSERT(ez::ttf::findTableRecord(records, ez::ttf::kTagGlyf) == nullptr);
    return true;
}

// OTTO is a cff-flavoured opentype : out of the ezTTF scope, refused
// HONESTLY (the header still comes back filled so a caller can tell
// "unsupported flavour" from "corrupt")
bool TestEzTtfTables_RefusesTheOttoFlavour() {
    ez::ttf::Stream stream;
    stream.writeU32(ez::ttf::kSfntVersionOtto);
    stream.writeU16(0u);
    stream.writeU16(0u);
    stream.writeU16(0u);
    stream.writeU16(0u);
    ez::ttf::SfntHeader header;
    std::vector<ez::ttf::TableRecord> records;
    CTEST_ASSERT(!ez::ttf::parseSfntDirectory(stream, header, records));
    CTEST_ASSERT(header.sfntVersion == ez::ttf::kSfntVersionOtto);  // told apart from corrupt
    CTEST_ASSERT(stream.ok());  // refused by FLAVOUR, not by a bad read
    return true;
}

// a directory whose numTables promises more records than the stream
// holds : refused without UB, no partial record list left behind
bool TestEzTtfTables_RefusesATruncatedDirectory() {
    ez::ttf::Stream full;
    buildTwoTableSfnt(full);
    ez::ttf::Stream truncated(full.getBytes(), 20u);  // dies inside record 0
    ez::ttf::SfntHeader header;
    std::vector<ez::ttf::TableRecord> records;
    CTEST_ASSERT(!ez::ttf::parseSfntDirectory(truncated, header, records));
    CTEST_ASSERT(records.empty());
    return true;
}

// a record pointing past the end of the file (the classic hostile
// directory) : refused, even when the u32 sum overflows
bool TestEzTtfTables_RefusesARecordPastTheEnd() {
    ez::ttf::Stream stream;
    stream.writeU32(ez::ttf::kSfntVersionTrueType);
    stream.writeU16(1u);
    stream.writeU16(16u);
    stream.writeU16(0u);
    stream.writeU16(0u);
    stream.writeTag(ez::ttf::kTagHead);
    stream.writeU32(0u);
    stream.writeU32(0xFFFFFFF0u);  // offset near the u32 top...
    stream.writeU32(0x20u);        // ...so offset + length wraps if summed in u32
    ez::ttf::SfntHeader header;
    std::vector<ez::ttf::TableRecord> records;
    CTEST_ASSERT(!ez::ttf::parseSfntDirectory(stream, header, records));
    CTEST_ASSERT(records.empty());
    return true;
}

namespace {

// a SYNTHETIC head payload at aOffset of a bare stream (54 bytes), upem
// 2048, bbox (-100,-200)..(300,400), long loca
void appendHeadPayload(ez::ttf::Stream& aoStream) {
    ez::ttf::Fixed one;
    one.setFloat(1.0f);
    aoStream.writeFixed(one);                              // version
    aoStream.writeFixed(one);                              // fontRevision
    aoStream.writeU32(0u);                                 // checkSumAdjustment
    aoStream.writeU32(ez::ttf::kHeadMagicNumber);          // magic
    aoStream.writeU16(0u);                                 // flags
    aoStream.writeU16(2048u);                              // unitsPerEm
    aoStream.writeI64(0);                                  // created
    aoStream.writeI64(0);                                  // modified
    aoStream.writeI16(-100);                               // xMin
    aoStream.writeI16(-200);                               // yMin
    aoStream.writeI16(300);                                // xMax
    aoStream.writeI16(400);                                // yMax
    aoStream.writeU16(0u);                                 // macStyle
    aoStream.writeU16(9u);                                 // lowestRecPPEM
    aoStream.writeI16(2);                                  // fontDirectionHint
    aoStream.writeI16(1);                                  // indexToLocFormat : long
    aoStream.writeI16(0);                                  // glyphDataFormat
}

}  // namespace

// head : the fields land where the spec says, and the parse rides the
// record offset (the payload does NOT start at 0 here)
bool TestEzTtfTables_ParsesTheHeadTable() {
    ez::ttf::Stream stream;
    stream.writeU32(0xDEADBEEFu);  // 4 junk bytes : the table is NOT at 0
    ez::ttf::TableRecord record;
    record.tag = ez::ttf::kTagHead;
    record.offset = 4u;
    record.length = 54u;
    appendHeadPayload(stream);
    ez::ttf::HeadTable head;
    CTEST_ASSERT(head.parse(stream, record));
    CTEST_ASSERT(head.unitsPerEm == 2048u);
    CTEST_ASSERT(head.xMin == -100 && head.yMin == -200 && head.xMax == 300 && head.yMax == 400);
    CTEST_ASSERT(head.indexToLocFormat == 1);
    CTEST_ASSERT(head.version.getFloat() == 1.0f);
    return true;
}

// a wrong magic or a zero upem poison everything downstream : refused
bool TestEzTtfTables_RefusesABadHeadTable() {
    ez::ttf::Stream stream;
    appendHeadPayload(stream);
    ez::ttf::TableRecord record;
    record.tag = ez::ttf::kTagHead;
    record.offset = 0u;
    record.length = 54u;
    // corrupt the magic in place (offset 12..15 of the payload)
    ez::ttf::HeadTable head;
    {
        std::vector<uint8_t> bytes(stream.getBytes(), stream.getBytes() + stream.getSize());
        bytes[12] = 0u;
        ez::ttf::Stream badMagic(&bytes[0], bytes.size());
        CTEST_ASSERT(!head.parse(badMagic, record));
    }
    {
        std::vector<uint8_t> bytes(stream.getBytes(), stream.getBytes() + stream.getSize());
        bytes[18] = 0u;  // unitsPerEm high byte
        bytes[19] = 0u;  // unitsPerEm low byte
        ez::ttf::Stream zeroUpem(&bytes[0], bytes.size());
        CTEST_ASSERT(!head.parse(zeroUpem, record));
    }
    // a truncated table : the latched stream error refuses the parse
    ez::ttf::Stream truncated(stream.getBytes(), 20u);
    CTEST_ASSERT(!head.parse(truncated, record));
    return true;
}

// maxp 1.0 : numGlyphs plus the glyf profile ; 0.5 stops at numGlyphs
bool TestEzTtfTables_ParsesTheMaxpTable() {
    ez::ttf::Stream stream;
    ez::ttf::Fixed versionOne;
    versionOne.setFloat(1.0f);
    stream.writeFixed(versionOne);
    stream.writeU16(123u);  // numGlyphs
    for (uint16_t fieldIdx = 0; fieldIdx < 13u; ++fieldIdx) {
        stream.writeU16(static_cast<uint16_t>(fieldIdx + 1u));  // the profile
    }
    ez::ttf::TableRecord record;
    record.tag = ez::ttf::kTagMaxp;
    record.offset = 0u;
    record.length = 32u;
    ez::ttf::MaxpTable maxp;
    CTEST_ASSERT(maxp.parse(stream, record));
    CTEST_ASSERT(maxp.numGlyphs == 123u);
    CTEST_ASSERT(maxp.maxPoints == 1u);
    CTEST_ASSERT(maxp.maxComponentDepth == 13u);
    // the 0.5 flavour : only version + numGlyphs are present
    ez::ttf::Stream half;
    ez::ttf::Fixed versionHalf(0x00005000);
    half.writeFixed(versionHalf);
    half.writeU16(45u);
    ez::ttf::TableRecord halfRecord;
    halfRecord.tag = ez::ttf::kTagMaxp;
    halfRecord.offset = 0u;
    halfRecord.length = 6u;
    ez::ttf::MaxpTable maxpHalf;
    CTEST_ASSERT(maxpHalf.parse(half, halfRecord));
    CTEST_ASSERT(maxpHalf.numGlyphs == 45u);
    CTEST_ASSERT(maxpHalf.maxPoints == 0u);
    return true;
}

// hhea : the vertical metrics and the hmtx sizing field
bool TestEzTtfTables_ParsesTheHheaTable() {
    ez::ttf::Stream stream;
    ez::ttf::Fixed one;
    one.setFloat(1.0f);
    stream.writeFixed(one);
    stream.writeI16(1900);   // ascender
    stream.writeI16(-500);   // descender
    stream.writeI16(0);      // lineGap
    stream.writeU16(2500u);  // advanceWidthMax
    stream.writeI16(-50);    // minLeftSideBearing
    stream.writeI16(-60);    // minRightSideBearing
    stream.writeI16(2400);   // xMaxExtent
    stream.writeI16(1);      // caretSlopeRise
    stream.writeI16(0);      // caretSlopeRun
    stream.writeI16(0);      // caretOffset
    for (int32_t reservedIdx = 0; reservedIdx < 4; ++reservedIdx) {
        stream.writeI16(0);
    }
    stream.writeI16(0);    // metricDataFormat
    stream.writeU16(3u);   // numberOfHMetrics
    ez::ttf::TableRecord record;
    record.offset = 0u;
    record.length = 36u;
    ez::ttf::HheaTable hhea;
    CTEST_ASSERT(hhea.parse(stream, record));
    CTEST_ASSERT(hhea.ascender == 1900 && hhea.descender == -500);
    CTEST_ASSERT(hhea.numberOfHMetrics == 3u);
    // a zero numberOfHMetrics leaves every glyph metric-less : refused
    std::vector<uint8_t> bytes(stream.getBytes(), stream.getBytes() + stream.getSize());
    bytes[34] = 0u;
    bytes[35] = 0u;
    ez::ttf::Stream corrupted(&bytes[0], bytes.size());
    CTEST_ASSERT(!hhea.parse(corrupted, record));
    return true;
}

// hmtx : the trailing lsb-only run shares the LAST full advance — the
// vectors come back resolved, numGlyphs-sized
bool TestEzTtfTables_ParsesAndResolvesTheHmtxTable() {
    ez::ttf::Stream stream;
    stream.writeU16(10u);  // glyph 0 : advance 10
    stream.writeI16(1);
    stream.writeU16(20u);  // glyph 1 : advance 20 (the LAST full pair)
    stream.writeI16(2);
    stream.writeI16(3);    // glyph 2 : lsb only
    stream.writeI16(-4);   // glyph 3 : lsb only
    ez::ttf::TableRecord record;
    record.offset = 0u;
    record.length = 12u;
    ez::ttf::HmtxTable hmtx;
    CTEST_ASSERT(hmtx.parse(stream, record, 2u, 4u));
    CTEST_ASSERT(hmtx.advanceWidths.size() == 4u && hmtx.leftSideBearings.size() == 4u);
    CTEST_ASSERT(hmtx.advanceWidths[0] == 10u && hmtx.advanceWidths[1] == 20u);
    CTEST_ASSERT(hmtx.advanceWidths[2] == 20u && hmtx.advanceWidths[3] == 20u);  // shared
    CTEST_ASSERT(hmtx.leftSideBearings[3] == -4);
    // hhea and maxp disagreeing is corrupt : more metrics than glyphs
    CTEST_ASSERT(!hmtx.parse(stream, record, 5u, 4u));
    // a truncated table refuses and leaves EMPTY vectors, never partial
    ez::ttf::Stream truncated(stream.getBytes(), 6u);
    CTEST_ASSERT(!hmtx.parse(truncated, record, 2u, 4u));
    CTEST_ASSERT(hmtx.advanceWidths.empty());
    return true;
}

// loca : short offsets are halved in the file, both flavours resolve to
// bytes, and a decreasing pair is the corrupt-font tripwire
bool TestEzTtfTables_ParsesBothLocaFlavours() {
    // short flavour : 3 glyphs = 4 entries, stored /2
    ez::ttf::Stream shortStream;
    shortStream.writeU16(0u);
    shortStream.writeU16(10u);   // glyph 0 : bytes 0..20
    shortStream.writeU16(10u);   // glyph 1 : EMPTY (a space)
    shortStream.writeU16(25u);   // glyph 2 : bytes 20..50
    ez::ttf::TableRecord record;
    record.offset = 0u;
    record.length = 8u;
    ez::ttf::LocaTable loca;
    CTEST_ASSERT(loca.parse(shortStream, record, 3u, 0));
    CTEST_ASSERT(loca.offsets.size() == 4u);
    CTEST_ASSERT(loca.offsets[1] == 20u && loca.offsets[2] == 20u && loca.offsets[3] == 50u);
    // long flavour
    ez::ttf::Stream longStream;
    longStream.writeU32(0u);
    longStream.writeU32(100u);
    ez::ttf::TableRecord longRecord;
    longRecord.offset = 0u;
    longRecord.length = 8u;
    CTEST_ASSERT(loca.parse(longStream, longRecord, 1u, 1));
    CTEST_ASSERT(loca.offsets.size() == 2u && loca.offsets[1] == 100u);
    // a decreasing pair : refused, empty vectors
    ez::ttf::Stream decreasing;
    decreasing.writeU32(50u);
    decreasing.writeU32(20u);
    CTEST_ASSERT(!loca.parse(decreasing, longRecord, 1u, 1));
    CTEST_ASSERT(loca.offsets.empty());
    return true;
}

namespace {

// a format 4 subtable body (after the format u16) : two live segments —
// 'A'..'C' through idDelta (glyph = code - 30), 0x0100..0x0101 through
// idRangeOffset — plus the mandatory 0xFFFF terminator segment
void appendCmapFormat4Body(ez::ttf::Stream& aoStream) {
    aoStream.writeU16(0u);  // length : recomputed by nobody here, unused
    aoStream.writeU16(0u);  // language
    aoStream.writeU16(6u);  // segCountX2 : 3 segments
    aoStream.writeU16(0u);  // searchRange trio : never trusted
    aoStream.writeU16(0u);
    aoStream.writeU16(0u);
    aoStream.writeU16(0x43u);    // endCodes : 'C'
    aoStream.writeU16(0x0101u);  //            0x0101
    aoStream.writeU16(0xFFFFu);  //            terminator
    aoStream.writeU16(0u);       // reservedPad
    aoStream.writeU16(0x41u);    // startCodes : 'A'
    aoStream.writeU16(0x0100u);
    aoStream.writeU16(0xFFFFu);
    aoStream.writeU16(static_cast<uint16_t>(-30));  // idDeltas : code - 30
    aoStream.writeU16(0u);
    aoStream.writeU16(1u);  // terminator : delta 1 (maps 0xFFFF to 0, skipped anyway)
    // idRangeOffsets : segment 1 points into the glyphIdArray. entry 1 of
    // 3 : the array starts 2 entries after it -> offset (2+0)*2 = 4 lands
    // on glyphIds[0]
    aoStream.writeU16(0u);
    aoStream.writeU16(4u);
    aoStream.writeU16(0u);
    // glyphIdArray : the two glyphs of segment 1
    aoStream.writeU16(100u);
    aoStream.writeU16(101u);
}

}  // namespace

// format 4 : both resolution paths — idDelta (modulo 65536) and
// idRangeOffset (rebased into the trailing glyphIdArray)
bool TestEzTtfTables_ParsesACmapFormat4() {
    ez::ttf::Stream stream;
    stream.writeU16(0u);  // cmap version
    stream.writeU16(1u);  // one encoding record
    stream.writeU16(3u);  // platform 3
    stream.writeU16(1u);  // encoding 1
    stream.writeU32(12u);  // subtable offset (4 + 8)
    stream.writeU16(4u);   // format
    appendCmapFormat4Body(stream);
    ez::ttf::TableRecord record;
    record.offset = 0u;
    record.length = static_cast<uint32_t>(stream.getSize());
    ez::ttf::CmapTable cmap;
    CTEST_ASSERT(cmap.parse(stream, record));
    // the idDelta segment : 'A' 0x41 -> 0x41 - 30 = 35, 'C' -> 37
    CTEST_ASSERT(cmap.codePointToGlyphIndex.at(0x41u) == 35u);
    CTEST_ASSERT(cmap.codePointToGlyphIndex.at(0x43u) == 37u);
    // the idRangeOffset segment : 0x0100 -> glyphIds[0] = 100
    CTEST_ASSERT(cmap.codePointToGlyphIndex.at(0x0100u) == 100u);
    CTEST_ASSERT(cmap.codePointToGlyphIndex.at(0x0101u) == 101u);
    // nothing else leaked in (the terminator maps nothing)
    CTEST_ASSERT(cmap.codePointToGlyphIndex.size() == 5u);  // A B C + the two
    CTEST_ASSERT(cmap.codePointToGlyphIndex.count(0xFFFFu) == 0u);
    return true;
}

// format 12 beats format 0 when both are present, and carries the
// supplementary planes — THE reason CodePoint is a u32
bool TestEzTtfTables_PrefersFormat12AndReadsBeyondTheBmp() {
    ez::ttf::Stream stream;
    stream.writeU16(0u);
    stream.writeU16(2u);   // two encoding records
    stream.writeU16(1u);   // record 0 : a format 0 at offset 20
    stream.writeU16(0u);
    stream.writeU32(20u);
    stream.writeU16(3u);   // record 1 : a format 12 at offset 20 + 6 + 256
    stream.writeU16(10u);
    stream.writeU32(282u);
    // the format 0 subtable (6 + 256 bytes) : maps 'A' to glyph 7
    stream.writeU16(0u);  // format
    stream.writeU16(0u);  // length
    stream.writeU16(0u);  // language
    for (uint32_t codePoint = 0; codePoint < 256u; ++codePoint) {
        stream.writeU8((codePoint == 0x41u) ? 7u : 0u);
    }
    // the format 12 subtable : one group in the SMP (emoji plane)
    stream.writeU16(12u);  // format
    stream.writeU16(0u);   // reserved
    stream.writeU32(0u);   // length, unused
    stream.writeU32(0u);   // language
    stream.writeU32(1u);   // one group
    stream.writeU32(0x1F600u);  // startChar : an emoji
    stream.writeU32(0x1F602u);  // endChar
    stream.writeU32(500u);      // startGlyphID
    ez::ttf::TableRecord record;
    record.offset = 0u;
    record.length = static_cast<uint32_t>(stream.getSize());
    ez::ttf::CmapTable cmap;
    CTEST_ASSERT(cmap.parse(stream, record));
    // the format 12 won : the emoji plane is mapped, the format 0 'A' is NOT
    CTEST_ASSERT(cmap.codePointToGlyphIndex.size() == 3u);
    CTEST_ASSERT(cmap.codePointToGlyphIndex.at(0x1F600u) == 500u);
    CTEST_ASSERT(cmap.codePointToGlyphIndex.at(0x1F602u) == 502u);
    CTEST_ASSERT(cmap.codePointToGlyphIndex.count(0x41u) == 0u);
    return true;
}

// hostile cmaps : no supported subtable, a lying group count, and a
// group past the unicode range (clamped, not exploded)
bool TestEzTtfTables_RefusesOrBoundsHostileCmaps() {
    // an unsupported lone format (6) : refused whole
    ez::ttf::Stream unsupported;
    unsupported.writeU16(0u);
    unsupported.writeU16(1u);
    unsupported.writeU16(0u);
    unsupported.writeU16(3u);
    unsupported.writeU32(12u);
    unsupported.writeU16(6u);  // format 6 : not supported
    ez::ttf::TableRecord record;
    record.offset = 0u;
    record.length = static_cast<uint32_t>(unsupported.getSize());
    ez::ttf::CmapTable cmap;
    CTEST_ASSERT(!cmap.parse(unsupported, record));
    // a format 12 whose group count lies past the stream : refused, empty
    ez::ttf::Stream lying;
    lying.writeU16(0u);
    lying.writeU16(1u);
    lying.writeU16(3u);
    lying.writeU16(10u);
    lying.writeU32(12u);
    lying.writeU16(12u);
    lying.writeU16(0u);
    lying.writeU32(0u);
    lying.writeU32(0u);
    lying.writeU32(1000000u);  // one million groups in a 40 byte table
    lying.writeU32(0x41u);
    lying.writeU32(0x42u);
    lying.writeU32(1u);
    ez::ttf::TableRecord lyingRecord;
    lyingRecord.offset = 0u;
    lyingRecord.length = static_cast<uint32_t>(lying.getSize());
    CTEST_ASSERT(!cmap.parse(lying, lyingRecord));
    CTEST_ASSERT(cmap.codePointToGlyphIndex.empty());
    return true;
}

// name : windows utf-16be records decoded to utf-8 (surrogate pairs
// included), mac roman as the fallback flavour, absent ids empty
bool TestEzTtfTables_ParsesTheNameTable() {
    ez::ttf::Stream stream;
    stream.writeU16(0u);   // version
    stream.writeU16(2u);   // two records
    stream.writeU16(30u);  // storage offset : 6 + 2*12
    // record 0 : windows, family, "Ez" + e-acute + an emoji (surrogates)
    stream.writeU16(3u);       // platform
    stream.writeU16(1u);       // encoding
    stream.writeU16(0x409u);   // language
    stream.writeU16(ez::ttf::kNameIdFamily);
    stream.writeU16(10u);      // byte length : 5 utf-16 units
    stream.writeU16(0u);       // string offset
    // record 1 : mac, subfamily, "Regular" (no windows flavour for it)
    stream.writeU16(1u);
    stream.writeU16(0u);
    stream.writeU16(0u);
    stream.writeU16(ez::ttf::kNameIdSubfamily);
    stream.writeU16(7u);
    stream.writeU16(10u);
    // the storage : utf-16be then mac bytes
    stream.writeU16(0x0045u);  // 'E'
    stream.writeU16(0x007Au);  // 'z'
    stream.writeU16(0x00E9u);  // e-acute
    stream.writeU16(0xD83Du);  // U+1F600 as a surrogate pair
    stream.writeU16(0xDE00u);
    const char* pRegular = "Regular";
    stream.writeBytes(reinterpret_cast<const uint8_t*>(pRegular), 7u);
    ez::ttf::TableRecord record;
    record.offset = 0u;
    record.length = static_cast<uint32_t>(stream.getSize());
    ez::ttf::NameTable name;
    CTEST_ASSERT(name.parse(stream, record));
    CTEST_ASSERT(name.entries.size() == 2u);
    CTEST_ASSERT(name.getName(ez::ttf::kNameIdFamily) == "Ez\xC3\xA9\xF0\x9F\x98\x80");
    CTEST_ASSERT(name.getName(ez::ttf::kNameIdSubfamily) == "Regular");  // the mac fallback
    CTEST_ASSERT(name.getName(ez::ttf::kNameIdVersion).empty());
    // an unpaired surrogate drops ITS record only, the table survives
    ez::ttf::Stream broken;
    broken.writeU16(0u);
    broken.writeU16(1u);
    broken.writeU16(18u);
    broken.writeU16(3u);
    broken.writeU16(1u);
    broken.writeU16(0u);
    broken.writeU16(ez::ttf::kNameIdFamily);
    broken.writeU16(2u);
    broken.writeU16(0u);
    broken.writeU16(0xD83Du);  // a lone high surrogate
    ez::ttf::TableRecord brokenRecord;
    brokenRecord.offset = 0u;
    brokenRecord.length = static_cast<uint32_t>(broken.getSize());
    CTEST_ASSERT(name.parse(broken, brokenRecord));
    CTEST_ASSERT(name.entries.empty());
    return true;
}

// post v2 : standard indices resolve through the mac table, customs
// through the pascal pool ; v3 answers no names by design ; an index
// past the pool is corrupt. anchors of the standard table checked here
bool TestEzTtfTables_ParsesThePostTable() {
    // the standard-table anchors every tool agrees on
    CTEST_ASSERT(std::string(ez::ttf::getMacGlyphName(0)) == ".notdef");
    CTEST_ASSERT(std::string(ez::ttf::getMacGlyphName(3)) == "space");
    CTEST_ASSERT(std::string(ez::ttf::getMacGlyphName(36)) == "A");
    CTEST_ASSERT(std::string(ez::ttf::getMacGlyphName(68)) == "a");
    CTEST_ASSERT(std::string(ez::ttf::getMacGlyphName(257)) == "dcroat");
    CTEST_ASSERT(std::string(ez::ttf::getMacGlyphName(258)).empty());
    // a v2 with 3 glyphs : .notdef (0), 'A' (36), one custom (258)
    ez::ttf::Stream stream;
    ez::ttf::Fixed versionTwo(0x00020000);
    stream.writeFixed(versionTwo);
    stream.writeFixed(ez::ttf::Fixed());  // italicAngle
    stream.writeI16(-100);                // underlinePosition
    stream.writeI16(50);                  // underlineThickness
    stream.writeU32(0u);                  // isFixedPitch
    for (int32_t memIdx = 0; memIdx < 4; ++memIdx) {
        stream.writeU32(0u);
    }
    stream.writeU16(3u);    // numberOfGlyphs
    stream.writeU16(0u);    // .notdef
    stream.writeU16(36u);   // 'A'
    stream.writeU16(258u);  // custom 0
    stream.writeU8(7u);     // pascal : "ez_icon"
    const char* pCustom = "ez_icon";
    stream.writeBytes(reinterpret_cast<const uint8_t*>(pCustom), 7u);
    ez::ttf::TableRecord record;
    record.offset = 0u;
    record.length = static_cast<uint32_t>(stream.getSize());
    ez::ttf::PostTable post;
    CTEST_ASSERT(post.parse(stream, record, 3u));
    CTEST_ASSERT(post.glyphNames.size() == 3u);
    CTEST_ASSERT(post.glyphNames[0] == ".notdef");
    CTEST_ASSERT(post.glyphNames[1] == "A");
    CTEST_ASSERT(post.glyphNames[2] == "ez_icon");
    CTEST_ASSERT(post.underlinePosition == -100);
    // post and maxp disagreeing : refused
    CTEST_ASSERT(!post.parse(stream, record, 4u));
    // a v3 : no names, honestly
    ez::ttf::Stream three;
    ez::ttf::Fixed versionThree(0x00030000);
    three.writeFixed(versionThree);
    three.writeFixed(ez::ttf::Fixed());
    three.writeI16(0);
    three.writeI16(0);
    three.writeU32(0u);
    for (int32_t memIdx = 0; memIdx < 4; ++memIdx) {
        three.writeU32(0u);
    }
    ez::ttf::TableRecord threeRecord;
    threeRecord.offset = 0u;
    threeRecord.length = static_cast<uint32_t>(three.getSize());
    CTEST_ASSERT(post.parse(three, threeRecord, 3u));
    CTEST_ASSERT(post.glyphNames.empty());
    // a custom index past the pascal pool : corrupt, empty
    ez::ttf::Stream broken;
    broken.writeFixed(versionTwo);
    broken.writeFixed(ez::ttf::Fixed());
    broken.writeI16(0);
    broken.writeI16(0);
    broken.writeU32(0u);
    for (int32_t memIdx = 0; memIdx < 4; ++memIdx) {
        broken.writeU32(0u);
    }
    broken.writeU16(1u);
    broken.writeU16(300u);  // custom 42 of an EMPTY pool
    ez::ttf::TableRecord brokenRecord;
    brokenRecord.offset = 0u;
    brokenRecord.length = static_cast<uint32_t>(broken.getSize());
    CTEST_ASSERT(!post.parse(broken, brokenRecord, 1u));
    CTEST_ASSERT(post.glyphNames.empty());
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzTtfTables(const std::string& vTest) {
    IfTestExist(TestEzTtfTables_ParsesAValidDirectory);
    else IfTestExist(TestEzTtfTables_FindsATableByTag);
    else IfTestExist(TestEzTtfTables_RefusesTheOttoFlavour);
    else IfTestExist(TestEzTtfTables_RefusesATruncatedDirectory);
    else IfTestExist(TestEzTtfTables_RefusesARecordPastTheEnd);
    else IfTestExist(TestEzTtfTables_ParsesTheHeadTable);
    else IfTestExist(TestEzTtfTables_RefusesABadHeadTable);
    else IfTestExist(TestEzTtfTables_ParsesTheMaxpTable);
    else IfTestExist(TestEzTtfTables_ParsesTheHheaTable);
    else IfTestExist(TestEzTtfTables_ParsesAndResolvesTheHmtxTable);
    else IfTestExist(TestEzTtfTables_ParsesBothLocaFlavours);
    else IfTestExist(TestEzTtfTables_ParsesACmapFormat4);
    else IfTestExist(TestEzTtfTables_PrefersFormat12AndReadsBeyondTheBmp);
    else IfTestExist(TestEzTtfTables_RefusesOrBoundsHostileCmaps);
    else IfTestExist(TestEzTtfTables_ParsesTheNameTable);
    else IfTestExist(TestEzTtfTables_ParsesThePostTable);
    return false;
}
