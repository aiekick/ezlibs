#pragma once

#include <ezlibs/ezTTF/ezTTF.hpp>

#include <cstdint>
#include <map>
#include <vector>

// the SYNTHETIC source fonts the ttf suites share : built through the
// writer, so every fixture is a real font by construction — and any suite
// (ezlibs or a consumer app) declares the exact face it needs without a
// binary asset

// a source font with a COMPOSITE : 4 glyphs — notdef (empty), a triangle
// ('A', advance 500), an empty space (0x20), and a composite ('B',
// advance 700) referencing the triangle at (10, 20).
// aWithColor adds the COLR/CPAL pair : the composite becomes a LAYERED
// base (triangle tinted by palette entry 0 = red, then entry 0xFFFF =
// the text color) over a two-entry palette (red, green)
inline bool buildCompositeSourceFont(ez::ttf::Font& aoFont, bool aWithColor = false) {
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
