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
// the BUILDER : a NEW font from a selection over several sources —
// subset, merge, rename, recode (the FontGenerator contract). the two
// historical rules live here :
//  - a picked COMPOSITE embarks its component glyphs RECURSIVELY, and
//    the component indices are REWRITTEN in the copied glyf bytes (the
//    FillResolvedCompositeGlyphs lesson)
//  - a codepoint picked twice : the LAST pick wins (the Baker rule)

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ttfTypes.hpp"
#include "ttfStream.hpp"
#include "ttfGlyph.hpp"
#include "ttfFont.hpp"

namespace ez {
namespace ttf {

// walks a COMPOSITE glyf entry in place and rewrites every component
// glyphIndex through aRemap (old index -> new index). false on a corrupt
// entry or a component absent from the map — the builder guarantees the
// map is complete before calling
inline bool remapCompositeGlyphIndices(std::vector<uint8_t>& aoBytes, const std::map<GlyphIndex, GlyphIndex>& aRemap) {
    if (aoBytes.size() < 10u) {
        return false;
    }
    const int16_t contourCount = static_cast<int16_t>((static_cast<uint16_t>(aoBytes[0]) << 8) | aoBytes[1]);
    if (contourCount >= 0) {
        return true;  // a simple glyph : nothing to remap
    }
    std::size_t pos = 10u;
    auto moreComponents = true;
    while (moreComponents) {
        if (pos + 4u > aoBytes.size()) {
            return false;
        }
        const uint16_t flags = static_cast<uint16_t>((static_cast<uint16_t>(aoBytes[pos]) << 8) | aoBytes[pos + 1u]);
        const uint16_t oldIndex = static_cast<uint16_t>((static_cast<uint16_t>(aoBytes[pos + 2u]) << 8) | aoBytes[pos + 3u]);
        const std::map<GlyphIndex, GlyphIndex>::const_iterator it = aRemap.find(oldIndex);
        if (it == aRemap.end()) {
            return false;  // the builder failed its own completeness contract
        }
        aoBytes[pos + 2u] = static_cast<uint8_t>(it->second >> 8);
        aoBytes[pos + 3u] = static_cast<uint8_t>(it->second & 0xFFu);
        pos += 4u;
        pos += ((flags & kGlyfArgsAreWords) != 0u) ? 4u : 2u;
        if ((flags & kGlyfHasScale) != 0u) {
            pos += 2u;
        } else if ((flags & kGlyfHasXYScale) != 0u) {
            pos += 4u;
        } else if ((flags & kGlyfHasTwoByTwo) != 0u) {
            pos += 8u;
        }
        moreComponents = (flags & kGlyfMoreComponents) != 0u;
    }
    return true;
}

class Builder {
private:
    struct Pick {
        int32_t sourceIdx;
        GlyphIndex sourceGlyph;
        CodePoint targetCodePoint;
        std::string targetName;
        Pick() : sourceIdx(0), sourceGlyph(0), targetCodePoint(0) {}
    };
    std::vector<const Font*> m_sources;  // observing : the caller keeps them alive through build
    std::vector<Pick> m_picks;
    std::vector<std::string> m_errors;

public:
    // observing registration : the font must outlive build(). -1 on an
    // unopened font (nothing can be picked from it)
    int32_t addSource(const Font& aFont) {
        if (!aFont.isOpened()) {
            return -1;
        }
        m_sources.push_back(&aFont);
        return static_cast<int32_t>(m_sources.size()) - 1;
    }
    // picks ONE source glyph into the target font under aTargetCodePoint
    // (the recode) and aTargetName (the rename, empty = nameless). the
    // LAST pick of a codepoint wins
    bool pickGlyph(int32_t aSourceIdx, GlyphIndex aSourceGlyph, CodePoint aTargetCodePoint, const std::string& aTargetName) {
        if (aSourceIdx < 0 || static_cast<std::size_t>(aSourceIdx) >= m_sources.size()) {
            return false;
        }
        if (static_cast<std::size_t>(aSourceGlyph) >= m_sources[static_cast<std::size_t>(aSourceIdx)]->getGlyphCount()) {
            return false;
        }
        Pick pick;
        pick.sourceIdx = aSourceIdx;
        pick.sourceGlyph = aSourceGlyph;
        pick.targetCodePoint = aTargetCodePoint;
        pick.targetName = aTargetName;
        m_picks.push_back(pick);
        return true;
    }
    // the codepoint flavour : resolves through the source cmap first
    bool pickCodePoint(int32_t aSourceIdx, CodePoint aSourceCodePoint, CodePoint aTargetCodePoint, const std::string& aTargetName) {
        if (aSourceIdx < 0 || static_cast<std::size_t>(aSourceIdx) >= m_sources.size()) {
            return false;
        }
        const GlyphIndex glyphIndex = m_sources[static_cast<std::size_t>(aSourceIdx)]->getGlyphIndex(aSourceCodePoint);
        if (glyphIndex == 0) {
            return false;  // absent codepoint : nothing honest to pick
        }
        return pickGlyph(aSourceIdx, glyphIndex, aTargetCodePoint, aTargetName);
    }
    const std::vector<std::string>& getErrors() const { return m_errors; }

    // assembles the new font THROUGH the writer (build -> bytes -> open :
    // the result is a real font by construction). false + errors on an
    // empty selection, mismatched upem sources, or a corrupt composite
    bool build(Font& aoFont) {
        m_errors.clear();
        if (m_sources.empty() || m_picks.empty()) {
            m_errors.push_back("build : no source or no pick");
            return false;
        }
        const Font& firstSource = *m_sources[0];
        for (std::size_t sourceIdx = 1; sourceIdx < m_sources.size(); ++sourceIdx) {
            if (m_sources[sourceIdx]->getInfos().unitsPerEm != firstSource.getInfos().unitsPerEm) {
                m_errors.push_back("build : mismatched unitsPerEm across sources (v1 refuses, no rescale)");
                return false;
            }
        }
        // the include list : (source, oldIndex) -> newIndex. notdef of the
        // FIRST source is glyph 0 of every font we build
        std::map<std::pair<int32_t, GlyphIndex>, GlyphIndex> included;
        std::vector<std::pair<int32_t, GlyphIndex> > order;
        m_include(0, 0, included, order);
        // codepoint -> pick (the LAST one wins : overwrite in pick order),
        // then include every winning glyph with its composite closure
        std::map<CodePoint, std::size_t> winners;
        for (std::size_t pickIdx = 0; pickIdx < m_picks.size(); ++pickIdx) {
            winners[m_picks[pickIdx].targetCodePoint] = pickIdx;
        }
        for (std::map<CodePoint, std::size_t>::const_iterator it = winners.begin(); it != winners.end(); ++it) {
            const Pick& pick = m_picks[it->second];
            if (!m_include(pick.sourceIdx, pick.sourceGlyph, included, order)) {
                m_errors.push_back("build : a composite closure failed (corrupt source glyph)");
                return false;
            }
            // a COLR base embarks its layers : each layer is a glyph of its
            // own (with its own composite closure), reachable through the
            // base only — it takes no codepoint of the new font
            const std::vector<ColrLayer>* pLayers = m_sources[static_cast<std::size_t>(pick.sourceIdx)]->getGlyphColorLayers(pick.sourceGlyph);
            if (pLayers != nullptr) {
                for (std::size_t layerIdx = 0; layerIdx < pLayers->size(); ++layerIdx) {
                    if (!m_include(pick.sourceIdx, (*pLayers)[layerIdx].glyphIndex, included, order)) {
                        m_errors.push_back("build : a color layer closure failed (corrupt source glyph)");
                        return false;
                    }
                }
            }
        }
        // glyf + loca + metrics + names, in the new order
        LocaTable loca;
        HmtxTable hmtx;
        PostTable post;
        std::vector<uint8_t> glyfBytes;
        loca.offsets.push_back(0u);
        auto anyName = false;
        for (std::size_t orderIdx = 0; orderIdx < order.size(); ++orderIdx) {
            const Font& source = *m_sources[static_cast<std::size_t>(order[orderIdx].first)];
            const GlyphIndex oldIndex = order[orderIdx].second;
            const uint8_t* pSpan = nullptr;
            uint32_t spanLength = 0;
            if (!source.getGlyphRawSpan(oldIndex, pSpan, spanLength)) {
                m_errors.push_back("build : a picked span vanished");
                return false;
            }
            std::vector<uint8_t> entry;
            if (spanLength != 0) {
                entry.assign(pSpan, pSpan + spanLength);
                // the composite indices are LOCAL to their source : remap
                // them through the included set of THAT source
                std::map<GlyphIndex, GlyphIndex> remap;
                for (std::map<std::pair<int32_t, GlyphIndex>, GlyphIndex>::const_iterator inc = included.begin(); inc != included.end(); ++inc) {
                    if (inc->first.first == order[orderIdx].first) {
                        remap[inc->first.second] = inc->second;
                    }
                }
                if (!remapCompositeGlyphIndices(entry, remap)) {
                    m_errors.push_back("build : composite remap failed");
                    return false;
                }
                while ((entry.size() % 2u) != 0u) {
                    entry.push_back(0u);  // 2-byte glyph alignment
                }
                glyfBytes.insert(glyfBytes.end(), entry.begin(), entry.end());
            }
            loca.offsets.push_back(static_cast<uint32_t>(glyfBytes.size()));
            uint16_t advance = 0;
            int16_t bearing = 0;
            source.getAdvanceWidth(oldIndex, advance);
            source.getLeftSideBearing(oldIndex, bearing);
            hmtx.advanceWidths.push_back(advance);
            hmtx.leftSideBearings.push_back(bearing);
            post.glyphNames.push_back(std::string());
        }
        post.glyphNames[0] = ".notdef";
        // the winner names + the cmap of the new font
        std::map<CodePoint, GlyphIndex> cmap;
        for (std::map<CodePoint, std::size_t>::const_iterator it = winners.begin(); it != winners.end(); ++it) {
            const Pick& pick = m_picks[it->second];
            const GlyphIndex newIndex = included[std::make_pair(pick.sourceIdx, pick.sourceGlyph)];
            cmap[pick.targetCodePoint] = newIndex;
            if (!pick.targetName.empty()) {
                post.glyphNames[newIndex] = pick.targetName;
                anyName = true;
            }
        }
        if (!anyName) {
            post.glyphNames.clear();  // nameless selection : an honest v3
        }
        // the header set rides the first source ; the bbox is the union of
        // the included glyph headers (empty glyphs contribute nothing)
        HeadTable head;
        head.version.setFloat(1.0f);
        head.fontRevision.setFloat(1.0f);
        head.unitsPerEm = firstSource.getInfos().unitsPerEm;
        head.lowestRecPPEM = 8u;
        head.indexToLocFormat = 1;
        auto anyBox = false;
        for (std::size_t orderIdx = 0; orderIdx < order.size(); ++orderIdx) {
            Glyph glyph;
            if (m_sources[static_cast<std::size_t>(order[orderIdx].first)]->getGlyph(order[orderIdx].second, glyph) && !glyph.isEmpty()) {
                if (!anyBox) {
                    head.xMin = glyph.xMin;
                    head.yMin = glyph.yMin;
                    head.xMax = glyph.xMax;
                    head.yMax = glyph.yMax;
                    anyBox = true;
                } else {
                    head.xMin = (glyph.xMin < head.xMin) ? glyph.xMin : head.xMin;
                    head.yMin = (glyph.yMin < head.yMin) ? glyph.yMin : head.yMin;
                    head.xMax = (glyph.xMax > head.xMax) ? glyph.xMax : head.xMax;
                    head.yMax = (glyph.yMax > head.yMax) ? glyph.yMax : head.yMax;
                }
            }
        }
        HheaTable hhea;
        hhea.ascender = firstSource.getInfos().ascender;
        hhea.descender = firstSource.getInfos().descender;
        hhea.lineGap = firstSource.getInfos().lineGap;
        uint16_t widest = 0;
        for (std::size_t glyphIdx = 0; glyphIdx < hmtx.advanceWidths.size(); ++glyphIdx) {
            widest = (hmtx.advanceWidths[glyphIdx] > widest) ? hmtx.advanceWidths[glyphIdx] : widest;
        }
        hhea.advanceWidthMax = widest;
        hhea.numberOfHMetrics = static_cast<uint16_t>(order.size());
        MaxpTable maxp;
        maxp.version.setFloat(1.0f);
        maxp.numGlyphs = static_cast<uint16_t>(order.size());
        // assemble through the writer : the result is a REAL font
        std::vector<TaggedTable> tables;
        tables.resize(7);
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
        {
            TaggedTable postTable;
            postTable.tag = kTagPost;
            emitPostTable(post, postTable.bytes);
            tables.push_back(postTable);
        }
        // the color side : every included COLR base re-emits its layers at
        // their NEW indices, and the palettes of the contributing sources
        // concatenate (palette 0 of each, in order) — every entry of a
        // source shifts by that source's offset, 0xFFFF (the text color)
        // never shifts. a pick without color emits no color table at all
        std::map<GlyphIndex, std::vector<ColrLayer> > newColr;
        std::vector<ColorRgba> mergedPalette;
        std::map<int32_t, uint16_t> paletteOffsets;
        for (std::size_t orderIdx = 0; orderIdx < order.size(); ++orderIdx) {
            const int32_t sourceIdx = order[orderIdx].first;
            const Font& source = *m_sources[static_cast<std::size_t>(sourceIdx)];
            const std::vector<ColrLayer>* pLayers = source.getGlyphColorLayers(order[orderIdx].second);
            if (pLayers == nullptr) {
                continue;
            }
            if (paletteOffsets.find(sourceIdx) == paletteOffsets.end()) {
                paletteOffsets[sourceIdx] = static_cast<uint16_t>(mergedPalette.size());
                const std::size_t entryCount = source.getPaletteEntryCount(0u);
                for (std::size_t entryIdx = 0; entryIdx < entryCount; ++entryIdx) {
                    ColorRgba color;
                    source.getPaletteColor(0u, static_cast<uint16_t>(entryIdx), color);
                    mergedPalette.push_back(color);
                }
            }
            std::vector<ColrLayer> newLayers;
            for (std::size_t layerIdx = 0; layerIdx < pLayers->size(); ++layerIdx) {
                const ColrLayer& layer = (*pLayers)[layerIdx];
                const std::map<std::pair<int32_t, GlyphIndex>, GlyphIndex>::const_iterator inc = included.find(std::make_pair(sourceIdx, layer.glyphIndex));
                if (inc == included.end()) {
                    m_errors.push_back("build : a color layer escaped the closure");
                    return false;
                }
                ColrLayer newLayer;
                newLayer.glyphIndex = inc->second;
                newLayer.paletteEntry = (layer.paletteEntry == 0xFFFFu) ? static_cast<uint16_t>(0xFFFFu) : static_cast<uint16_t>(layer.paletteEntry + paletteOffsets[sourceIdx]);
                newLayers.push_back(newLayer);
            }
            newColr[static_cast<GlyphIndex>(orderIdx)] = newLayers;
        }
        if (!newColr.empty()) {
            TaggedTable colrTable;
            colrTable.tag = kTagColr;
            emitColrTable(newColr, colrTable.bytes);
            tables.push_back(colrTable);
            std::vector<std::vector<ColorRgba> > palettes(1, mergedPalette);
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
    // includes (source, glyph) and its composite closure, depth first.
    // false on a corrupt glyph. already-included pairs answer true
    bool m_include(int32_t aSourceIdx, GlyphIndex aGlyphIndex,                          //
                   std::map<std::pair<int32_t, GlyphIndex>, GlyphIndex>& aoIncluded,    //
                   std::vector<std::pair<int32_t, GlyphIndex> >& aoOrder) {
        const std::pair<int32_t, GlyphIndex> key = std::make_pair(aSourceIdx, aGlyphIndex);
        if (aoIncluded.find(key) != aoIncluded.end()) {
            return true;
        }
        aoIncluded[key] = static_cast<GlyphIndex>(aoOrder.size());
        aoOrder.push_back(key);
        Glyph glyph;
        if (!m_sources[static_cast<std::size_t>(aSourceIdx)]->getGlyph(aGlyphIndex, glyph)) {
            return false;
        }
        for (std::size_t componentIdx = 0; componentIdx < glyph.components.size(); ++componentIdx) {
            if (!m_include(aSourceIdx, glyph.components[componentIdx].glyphIndex, aoIncluded, aoOrder)) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace ttf
}  // namespace ez
