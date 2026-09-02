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
//
// the SYNTHESIZER : a NEW font from outlines — glyphs drawn (the pen) or
// composed by hand, their metrics, their names and codepoints, and the
// COLR v0 layers of a colored glyph with its CPAL palette. the font is
// assembled THROUGH the writer and reopened (the builder law : a real
// font by construction) — what an svg importer or a generator needs to
// hand freetype something it can render, and the builder something it
// can merge

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ttfTypes.hpp"
#include "ttfStream.hpp"
#include "ttfTables.hpp"
#include "ttfGlyph.hpp"
#include "ttfWriter.hpp"
#include "ttfFont.hpp"

namespace ez {
namespace ttf {

class Synthesizer {
public:
    static const uint16_t kTextColorEntry = 0xFFFFu;  // the COLR palette entry meaning "the text color"

private:
    struct Entry {
        Glyph glyph;
        uint16_t advance;
        CodePoint codePoint;
        std::string name;
        Entry() : advance(0), codePoint(0) {}
    };
    uint16_t m_unitsPerEm{1000u};
    int16_t m_ascender{800};
    int16_t m_descender{-200};
    int16_t m_lineGap{0};
    std::vector<Entry> m_glyphs;  // glyph 0 is the notdef, posed at birth
    std::vector<ColorRgba> m_palette;
    std::map<GlyphIndex, std::vector<ColrLayer> > m_layers;
    std::vector<std::string> m_errors;

public:
    Synthesizer() {
        Entry notdef;
        notdef.name = ".notdef";
        m_glyphs.push_back(notdef);
    }
    void setMetrics(uint16_t aUnitsPerEm, int16_t aAscender, int16_t aDescender, int16_t aLineGap) {
        m_unitsPerEm = aUnitsPerEm;
        m_ascender = aAscender;
        m_descender = aDescender;
        m_lineGap = aLineGap;
    }
    // adds a glyph and answers its index. aCodePoint 0 = no cmap entry
    // (a layer, an unmapped glyph) ; the LAST glyph on a codepoint wins.
    // an empty name is legal (the post table then names it nothing)
    GlyphIndex addGlyph(const Glyph& aGlyph, uint16_t aAdvance, CodePoint aCodePoint, const std::string& aName) {
        Entry entry;
        entry.glyph = aGlyph;
        entry.advance = aAdvance;
        entry.codePoint = aCodePoint;
        entry.name = aName;
        m_glyphs.push_back(entry);
        return static_cast<GlyphIndex>(m_glyphs.size() - 1u);
    }
    // a COLR layer of aBase : aLayerGlyph painted with aColor (the palette
    // deduplicates), in the order the calls come — the paint order
    bool addColorLayer(GlyphIndex aBase, GlyphIndex aLayerGlyph, const ColorRgba& aColor) {
        if (!m_knows(aBase) || !m_knows(aLayerGlyph)) {
            return false;
        }
        ColrLayer layer;
        layer.glyphIndex = aLayerGlyph;
        layer.paletteEntry = m_paletteEntry(aColor);
        m_layers[aBase].push_back(layer);
        return true;
    }
    // a layer painted with the TEXT color (currentColor)
    bool addTextColorLayer(GlyphIndex aBase, GlyphIndex aLayerGlyph) {
        if (!m_knows(aBase) || !m_knows(aLayerGlyph)) {
            return false;
        }
        ColrLayer layer;
        layer.glyphIndex = aLayerGlyph;
        layer.paletteEntry = kTextColorEntry;
        m_layers[aBase].push_back(layer);
        return true;
    }
    std::size_t getGlyphCount() const {
        return m_glyphs.size();
    }
    const std::vector<std::string>& getErrors() const {
        return m_errors;
    }
    // assembles the font through the writer and reopens it. false with
    // readable errors on an empty selection or a refusal of the writer
    bool build(Font& aoFont) {
        m_errors.clear();
        if (m_glyphs.size() < 2u) {
            m_errors.push_back("build : no glyph beyond the notdef");
            return false;
        }
        if (m_glyphs.size() > 65535u) {
            m_errors.push_back("build : more than 65535 glyphs");
            return false;
        }
        // glyf + loca + metrics + names, in the given order
        LocaTable loca;
        HmtxTable hmtx;
        PostTable post;
        std::vector<uint8_t> glyfBytes;
        loca.offsets.push_back(0u);
        HeadTable head;
        head.version.setFloat(1.0f);
        head.fontRevision.setFloat(1.0f);
        head.unitsPerEm = m_unitsPerEm;
        head.lowestRecPPEM = 8u;
        head.indexToLocFormat = 1;
        auto anyBox = false;
        for (std::size_t glyphIdx = 0; glyphIdx < m_glyphs.size(); ++glyphIdx) {
            const Entry& entry = m_glyphs[glyphIdx];
            Stream data;
            emitGlyphData(entry.glyph, data);
            std::vector<uint8_t> bytes(data.getBytes(), data.getBytes() + data.getSize());
            while ((bytes.size() % 2u) != 0u) {
                bytes.push_back(0u);  // 2-byte glyph alignment
            }
            glyfBytes.insert(glyfBytes.end(), bytes.begin(), bytes.end());
            loca.offsets.push_back(static_cast<uint32_t>(glyfBytes.size()));
            hmtx.advanceWidths.push_back(entry.advance);
            hmtx.leftSideBearings.push_back(entry.glyph.isEmpty() ? static_cast<int16_t>(0) : entry.glyph.xMin);
            post.glyphNames.push_back(entry.name);
            if (!entry.glyph.isEmpty()) {
                if (!anyBox) {
                    head.xMin = entry.glyph.xMin;
                    head.yMin = entry.glyph.yMin;
                    head.xMax = entry.glyph.xMax;
                    head.yMax = entry.glyph.yMax;
                    anyBox = true;
                } else {
                    head.xMin = (entry.glyph.xMin < head.xMin) ? entry.glyph.xMin : head.xMin;
                    head.yMin = (entry.glyph.yMin < head.yMin) ? entry.glyph.yMin : head.yMin;
                    head.xMax = (entry.glyph.xMax > head.xMax) ? entry.glyph.xMax : head.xMax;
                    head.yMax = (entry.glyph.yMax > head.yMax) ? entry.glyph.yMax : head.yMax;
                }
            }
        }
        std::map<CodePoint, GlyphIndex> cmap;
        for (std::size_t glyphIdx = 1; glyphIdx < m_glyphs.size(); ++glyphIdx) {
            if (m_glyphs[glyphIdx].codePoint != 0u) {
                cmap[m_glyphs[glyphIdx].codePoint] = static_cast<GlyphIndex>(glyphIdx);
            }
        }
        HheaTable hhea;
        hhea.ascender = m_ascender;
        hhea.descender = m_descender;
        hhea.lineGap = m_lineGap;
        uint16_t widest = 0;
        for (std::size_t glyphIdx = 0; glyphIdx < hmtx.advanceWidths.size(); ++glyphIdx) {
            widest = (hmtx.advanceWidths[glyphIdx] > widest) ? hmtx.advanceWidths[glyphIdx] : widest;
        }
        hhea.advanceWidthMax = widest;
        hhea.numberOfHMetrics = static_cast<uint16_t>(m_glyphs.size());
        MaxpTable maxp;
        maxp.version.setFloat(1.0f);
        maxp.numGlyphs = static_cast<uint16_t>(m_glyphs.size());
        std::vector<TaggedTable> tables;
        tables.resize(8);
        tables[0].tag = kTagHead;
        emitHeadTable(head, tables[0].bytes);
        tables[1].tag = kTagMaxp;
        emitMaxpTable(maxp, tables[1].bytes);
        tables[2].tag = kTagHhea;
        emitHheaTable(hhea, maxp.numGlyphs, tables[2].bytes);
        tables[3].tag = kTagHmtx;
        emitHmtxTable(hmtx, tables[3].bytes);
        tables[4].tag = kTagLoca;
        emitLocaTable(loca, tables[4].bytes);
        tables[5].tag = kTagCmap;
        emitCmapTable(cmap, tables[5].bytes);
        tables[6].tag = kTagGlyf;
        if (!glyfBytes.empty()) {
            tables[6].bytes.writeBytes(&glyfBytes[0], glyfBytes.size());
        }
        tables[7].tag = kTagPost;
        emitPostTable(post, tables[7].bytes);
        if (!m_layers.empty()) {
            std::vector<ColorRgba> palette = m_palette;
            if (palette.empty()) {
                palette.push_back(ColorRgba());  // text color layers only : a palette still needs one entry
            }
            TaggedTable colrTable;
            colrTable.tag = kTagColr;
            emitColrTable(m_layers, colrTable.bytes);
            tables.push_back(colrTable);
            std::vector<std::vector<ColorRgba> > palettes(1, palette);
            TaggedTable cpalTable;
            cpalTable.tag = kTagCpal;
            emitCpalTable(palettes, cpalTable.bytes);
            tables.push_back(cpalTable);
        }
        std::vector<uint8_t> bytes;
        if (!assembleFont(tables, bytes)) {
            m_errors.push_back("build : assembly failed");
            return false;
        }
        if (!aoFont.openFromMemory(&bytes[0], bytes.size())) {
            m_errors.push_back("build : the assembled font does not reopen (writer bug)");
            return false;
        }
        return true;
    }

private:
    bool m_knows(GlyphIndex aGlyphIndex) const {
        return static_cast<std::size_t>(aGlyphIndex) < m_glyphs.size();
    }
    uint16_t m_paletteEntry(const ColorRgba& aColor) {
        for (std::size_t entryIdx = 0; entryIdx < m_palette.size(); ++entryIdx) {
            const ColorRgba& known = m_palette[entryIdx];
            if ((known.red == aColor.red) && (known.green == aColor.green) && (known.blue == aColor.blue) && (known.alpha == aColor.alpha)) {
                return static_cast<uint16_t>(entryIdx);
            }
        }
        m_palette.push_back(aColor);
        return static_cast<uint16_t>(m_palette.size() - 1u);
    }
};

}  // namespace ttf
}  // namespace ez
