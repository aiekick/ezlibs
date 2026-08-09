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
// the table EMISSION side : each helper composes one table stream from
// its parsed struct, and assembleFont packs them into a valid sfnt —
// directory sorted by tag, per-table checksums on padded 4-byte units,
// and head.checkSumAdjustment stamped LAST over the whole file (the
// 0xB1B0AFBA rule : forgetting it gets fonts refused by windows)

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ttfTypes.hpp"
#include "ttfStream.hpp"
#include "ttfTables.hpp"

namespace ez {
namespace ttf {

// one assembled table waiting for the pack
struct TaggedTable {
    TableTag tag;
    Stream bytes;
    TaggedTable() : tag(0) {}
};

inline void emitHeadTable(const HeadTable& aHead, Stream& aoStream) {
    aoStream.writeFixed(aHead.version);
    aoStream.writeFixed(aHead.fontRevision);
    aoStream.writeU32(0u);  // checkSumAdjustment : stamped by assembleFont
    aoStream.writeU32(kHeadMagicNumber);
    aoStream.writeU16(aHead.flags);
    aoStream.writeU16(aHead.unitsPerEm);
    aoStream.writeI64(aHead.created);
    aoStream.writeI64(aHead.modified);
    aoStream.writeI16(aHead.xMin);
    aoStream.writeI16(aHead.yMin);
    aoStream.writeI16(aHead.xMax);
    aoStream.writeI16(aHead.yMax);
    aoStream.writeU16(aHead.macStyle);
    aoStream.writeU16(aHead.lowestRecPPEM);
    aoStream.writeI16(aHead.fontDirectionHint);
    aoStream.writeI16(aHead.indexToLocFormat);
    aoStream.writeI16(aHead.glyphDataFormat);
}

inline void emitMaxpTable(const MaxpTable& aMaxp, Stream& aoStream) {
    Fixed versionOne(0x00010000);
    aoStream.writeFixed(versionOne);  // the glyf profile flavour, always
    aoStream.writeU16(aMaxp.numGlyphs);
    aoStream.writeU16(aMaxp.maxPoints);
    aoStream.writeU16(aMaxp.maxContours);
    aoStream.writeU16(aMaxp.maxCompositePoints);
    aoStream.writeU16(aMaxp.maxCompositeContours);
    aoStream.writeU16(aMaxp.maxZones);
    aoStream.writeU16(aMaxp.maxTwilightPoints);
    aoStream.writeU16(aMaxp.maxStorage);
    aoStream.writeU16(aMaxp.maxFunctionDefs);
    aoStream.writeU16(aMaxp.maxInstructionDefs);
    aoStream.writeU16(aMaxp.maxStackElements);
    aoStream.writeU16(aMaxp.maxSizeOfInstructions);
    aoStream.writeU16(aMaxp.maxComponentElements);
    aoStream.writeU16(aMaxp.maxComponentDepth);
}

// FULL pairs for every glyph on purpose (numberOfHMetrics = numGlyphs) :
// the trailing-run compression is an optimization the reader resolves
// anyway, and the writer stays simple and always correct
inline void emitHheaTable(const HheaTable& aHhea, uint16_t aNumGlyphs, Stream& aoStream) {
    Fixed versionOne(0x00010000);
    aoStream.writeFixed(versionOne);
    aoStream.writeI16(aHhea.ascender);
    aoStream.writeI16(aHhea.descender);
    aoStream.writeI16(aHhea.lineGap);
    aoStream.writeU16(aHhea.advanceWidthMax);
    aoStream.writeI16(aHhea.minLeftSideBearing);
    aoStream.writeI16(aHhea.minRightSideBearing);
    aoStream.writeI16(aHhea.xMaxExtent);
    aoStream.writeI16(aHhea.caretSlopeRise);
    aoStream.writeI16(aHhea.caretSlopeRun);
    aoStream.writeI16(aHhea.caretOffset);
    for (int32_t reservedIdx = 0; reservedIdx < 4; ++reservedIdx) {
        aoStream.writeI16(0);
    }
    aoStream.writeI16(0);  // metricDataFormat
    aoStream.writeU16(aNumGlyphs);
}

inline void emitHmtxTable(const HmtxTable& aHmtx, Stream& aoStream) {
    for (std::size_t glyphIdx = 0; glyphIdx < aHmtx.advanceWidths.size(); ++glyphIdx) {
        aoStream.writeU16(aHmtx.advanceWidths[glyphIdx]);
        aoStream.writeI16(aHmtx.leftSideBearings[glyphIdx]);
    }
}

// long loca always : universally valid, indexToLocFormat must say 1 (the
// head the caller emits carries it — assemble-side responsibility)
inline void emitLocaTable(const LocaTable& aLoca, Stream& aoStream) {
    for (std::size_t entryIdx = 0; entryIdx < aLoca.offsets.size(); ++entryIdx) {
        aoStream.writeU32(aLoca.offsets[entryIdx]);
    }
}

// cmap : ONE format 4 for the bmp mappings, plus ONE format 12 when any
// codepoint lives past it. the f4 segments split wherever the codes stop
// being consecutive or the (glyph - code) delta changes
inline void emitCmapTable(const std::map<CodePoint, GlyphIndex>& aMap, Stream& aoStream) {
    // split the bmp part into segments
    struct Segment {
        uint16_t startCode;
        uint16_t endCode;
        uint16_t glyphOfStart;
    };
    std::vector<Segment> segments;
    auto hasBeyondBmp = false;
    for (std::map<CodePoint, GlyphIndex>::const_iterator it = aMap.begin(); it != aMap.end(); ++it) {
        if (it->first >= 0xFFFFu) {  // 0xFFFF itself is the f4 terminator : routed to f12
            hasBeyondBmp = true;
            continue;
        }
        const uint16_t code = static_cast<uint16_t>(it->first);
        if (!segments.empty()) {
            Segment& last = segments[segments.size() - 1u];
            const auto consecutive = (code == last.endCode + 1u);
            const auto sameDelta = (static_cast<int32_t>(it->second) - code) ==  //
                                   (static_cast<int32_t>(last.glyphOfStart) - last.startCode);
            if (consecutive && sameDelta) {
                last.endCode = code;
                continue;
            }
        }
        Segment segment;
        segment.startCode = code;
        segment.endCode = code;
        segment.glyphOfStart = it->second;
        segments.push_back(segment);
    }
    const uint16_t encodingCount = hasBeyondBmp ? 2u : 1u;
    aoStream.writeU16(0u);  // version
    aoStream.writeU16(encodingCount);
    // the f4 subtable body first (sized), records after
    Stream f4;
    const uint16_t segCount = static_cast<uint16_t>(segments.size() + 1u);  // + terminator
    f4.writeU16(4u);
    f4.writeU16(static_cast<uint16_t>(16u + segCount * 8u));  // length (delta-only segments : no glyphIdArray)
    f4.writeU16(0u);                                          // language
    f4.writeU16(static_cast<uint16_t>(segCount * 2u));
    // the search trio, honestly computed (some rasterizers do use it)
    uint16_t searchRange = 2u;
    uint16_t entrySelector = 0u;
    while (static_cast<uint16_t>(searchRange * 2u) <= static_cast<uint16_t>(segCount * 2u)) {
        searchRange = static_cast<uint16_t>(searchRange * 2u);
        ++entrySelector;
    }
    f4.writeU16(searchRange);
    f4.writeU16(static_cast<uint16_t>(entrySelector - 1u));
    f4.writeU16(static_cast<uint16_t>(segCount * 2u - searchRange));
    for (std::size_t segIdx = 0; segIdx < segments.size(); ++segIdx) {
        f4.writeU16(segments[segIdx].endCode);
    }
    f4.writeU16(0xFFFFu);
    f4.writeU16(0u);  // reservedPad
    for (std::size_t segIdx = 0; segIdx < segments.size(); ++segIdx) {
        f4.writeU16(segments[segIdx].startCode);
    }
    f4.writeU16(0xFFFFu);
    for (std::size_t segIdx = 0; segIdx < segments.size(); ++segIdx) {
        // delta-only segments : glyph = code + delta, modulo 65536
        f4.writeU16(static_cast<uint16_t>(segments[segIdx].glyphOfStart - segments[segIdx].startCode));
    }
    f4.writeU16(1u);  // the terminator : maps 0xFFFF to 0 (skipped by readers)
    for (uint16_t segIdx = 0; segIdx < segCount; ++segIdx) {
        f4.writeU16(0u);  // idRangeOffsets : all delta-only
    }
    // the f12 groups (consecutive code AND glyph runs), when needed
    Stream f12;
    if (hasBeyondBmp) {
        struct Group {
            uint32_t startChar;
            uint32_t endChar;
            uint32_t startGlyph;
        };
        std::vector<Group> groups;
        for (std::map<CodePoint, GlyphIndex>::const_iterator it = aMap.begin(); it != aMap.end(); ++it) {
            if (it->first < 0xFFFFu) {
                continue;
            }
            if (!groups.empty()) {
                Group& last = groups[groups.size() - 1u];
                if (it->first == last.endChar + 1u && it->second == last.startGlyph + (it->first - last.startChar)) {
                    last.endChar = it->first;
                    continue;
                }
            }
            Group group;
            group.startChar = it->first;
            group.endChar = it->first;
            group.startGlyph = it->second;
            groups.push_back(group);
        }
        f12.writeU16(12u);
        f12.writeU16(0u);
        f12.writeU32(static_cast<uint32_t>(16u + groups.size() * 12u));
        f12.writeU32(0u);
        f12.writeU32(static_cast<uint32_t>(groups.size()));
        for (std::size_t groupIdx = 0; groupIdx < groups.size(); ++groupIdx) {
            f12.writeU32(groups[groupIdx].startChar);
            f12.writeU32(groups[groupIdx].endChar);
            f12.writeU32(groups[groupIdx].startGlyph);
        }
    }
    // the records then the subtables (offsets are cmap-relative)
    const uint32_t recordsEnd = 4u + encodingCount * 8u;
    aoStream.writeU16(3u);  // windows unicode bmp
    aoStream.writeU16(1u);
    aoStream.writeU32(recordsEnd);
    if (hasBeyondBmp) {
        aoStream.writeU16(3u);  // windows unicode full
        aoStream.writeU16(10u);
        aoStream.writeU32(recordsEnd + static_cast<uint32_t>(f4.getSize()));
    }
    aoStream.appendStream(f4);
    if (hasBeyondBmp) {
        aoStream.appendStream(f12);
    }
}

// post v2 when names exist (standard indices recovered by a reverse scan
// of the mac table), a bare v3 otherwise
inline void emitPostTable(const PostTable& aPost, Stream& aoStream) {
    const auto hasNames = !aPost.glyphNames.empty();
    Fixed version(hasNames ? 0x00020000 : 0x00030000);
    aoStream.writeFixed(version);
    aoStream.writeFixed(aPost.italicAngle);
    aoStream.writeI16(aPost.underlinePosition);
    aoStream.writeI16(aPost.underlineThickness);
    aoStream.writeU32(aPost.isFixedPitch);
    for (int32_t memIdx = 0; memIdx < 4; ++memIdx) {
        aoStream.writeU32(0u);
    }
    if (!hasNames) {
        return;
    }
    aoStream.writeU16(static_cast<uint16_t>(aPost.glyphNames.size()));
    std::vector<std::string> customNames;
    for (std::size_t glyphIdx = 0; glyphIdx < aPost.glyphNames.size(); ++glyphIdx) {
        const std::string& name = aPost.glyphNames[glyphIdx];
        auto standardIdx = -1;
        for (int32_t macIdx = 0; macIdx < kMacGlyphNameCount; ++macIdx) {
            if (name == getMacGlyphName(macIdx)) {
                standardIdx = macIdx;
                break;
            }
        }
        if (standardIdx >= 0) {
            aoStream.writeU16(static_cast<uint16_t>(standardIdx));
        } else {
            aoStream.writeU16(static_cast<uint16_t>(kMacGlyphNameCount + customNames.size()));
            customNames.push_back(name);
        }
    }
    for (std::size_t customIdx = 0; customIdx < customNames.size(); ++customIdx) {
        // a pascal string caps at 255 : longer names are truncated
        const std::string& name = customNames[customIdx];
        const std::size_t length = (name.size() > 255u) ? 255u : name.size();
        aoStream.writeU8(static_cast<uint8_t>(length));
        aoStream.writeBytes(reinterpret_cast<const uint8_t*>(name.c_str()), length);
    }
}

// CPAL v0 : every palette must carry the SAME entry count — the format
// rule ; the caller feeds palettes already shaped that way
inline void emitCpalTable(const std::vector<std::vector<ColorRgba> >& aPalettes, Stream& aoStream) {
    const uint16_t paletteCount = static_cast<uint16_t>(aPalettes.size());
    const uint16_t entriesPerPalette = aPalettes.empty() ? 0u : static_cast<uint16_t>(aPalettes[0].size());
    aoStream.writeU16(0u);  // version
    aoStream.writeU16(entriesPerPalette);
    aoStream.writeU16(paletteCount);
    aoStream.writeU16(static_cast<uint16_t>(paletteCount * entriesPerPalette));
    aoStream.writeU32(12u + static_cast<uint32_t>(paletteCount) * 2u);  // colors right after the indices
    for (uint16_t paletteIdx = 0; paletteIdx < paletteCount; ++paletteIdx) {
        aoStream.writeU16(static_cast<uint16_t>(paletteIdx * entriesPerPalette));
    }
    for (uint16_t paletteIdx = 0; paletteIdx < paletteCount; ++paletteIdx) {
        for (std::size_t entryIdx = 0; entryIdx < aPalettes[paletteIdx].size(); ++entryIdx) {
            const ColorRgba& color = aPalettes[paletteIdx][entryIdx];
            aoStream.writeU8(color.blue);  // the BGRA order of the format
            aoStream.writeU8(color.green);
            aoStream.writeU8(color.red);
            aoStream.writeU8(color.alpha);
        }
    }
}

// COLR v0 : base records SORTED by glyph id (the std::map order gives it)
inline void emitColrTable(const std::map<GlyphIndex, std::vector<ColrLayer> >& aBaseToLayers, Stream& aoStream) {
    uint32_t layerCount = 0;
    for (std::map<GlyphIndex, std::vector<ColrLayer> >::const_iterator it = aBaseToLayers.begin(); it != aBaseToLayers.end(); ++it) {
        layerCount += static_cast<uint32_t>(it->second.size());
    }
    const uint16_t baseCount = static_cast<uint16_t>(aBaseToLayers.size());
    const uint32_t baseOffset = 14u;
    const uint32_t layerOffset = baseOffset + static_cast<uint32_t>(baseCount) * 6u;
    aoStream.writeU16(0u);  // version
    aoStream.writeU16(baseCount);
    aoStream.writeU32(baseOffset);
    aoStream.writeU32(layerOffset);
    aoStream.writeU16(static_cast<uint16_t>(layerCount));
    uint16_t runningLayer = 0;
    for (std::map<GlyphIndex, std::vector<ColrLayer> >::const_iterator it = aBaseToLayers.begin(); it != aBaseToLayers.end(); ++it) {
        aoStream.writeU16(it->first);
        aoStream.writeU16(runningLayer);
        aoStream.writeU16(static_cast<uint16_t>(it->second.size()));
        runningLayer = static_cast<uint16_t>(runningLayer + it->second.size());
    }
    for (std::map<GlyphIndex, std::vector<ColrLayer> >::const_iterator it = aBaseToLayers.begin(); it != aBaseToLayers.end(); ++it) {
        for (std::size_t layerIdx = 0; layerIdx < it->second.size(); ++layerIdx) {
            aoStream.writeU16(it->second[layerIdx].glyphIndex);
            aoStream.writeU16(it->second[layerIdx].paletteEntry);
        }
    }
}

// the table checksum of the spec : the sum of the 4-byte units of the
// PADDED table (the stream is padded before this is called)
inline uint32_t computeTableCheckSum(const Stream& aTable) {
    uint32_t sum = 0;
    const uint8_t* pBytes = aTable.getBytes();
    const std::size_t byteCount = aTable.getSize();
    for (std::size_t byteIdx = 0; byteIdx + 3u < byteCount; byteIdx += 4u) {
        sum += (static_cast<uint32_t>(pBytes[byteIdx]) << 24) | (static_cast<uint32_t>(pBytes[byteIdx + 1u]) << 16) |  //
               (static_cast<uint32_t>(pBytes[byteIdx + 2u]) << 8) | pBytes[byteIdx + 3u];
    }
    return sum;
}

// packs the tables into a final sfnt : directory SORTED BY TAG, per
// table checksums, and head.checkSumAdjustment stamped over the whole
// file. aoTables is consumed (padded in place)
inline bool assembleFont(std::vector<TaggedTable>& aoTables, std::vector<uint8_t>& aoBytes) {
    aoBytes.clear();
    if (aoTables.empty()) {
        return false;
    }
    // sort by tag (the binary-search contract of the directory)
    for (std::size_t sortIdx = 1; sortIdx < aoTables.size(); ++sortIdx) {  // insertion sort : tiny n, no std::sort closure
        for (std::size_t backIdx = sortIdx; backIdx > 0 && aoTables[backIdx].tag < aoTables[backIdx - 1u].tag; --backIdx) {
            std::swap(aoTables[backIdx], aoTables[backIdx - 1u]);
        }
    }
    const uint16_t tableCount = static_cast<uint16_t>(aoTables.size());
    uint16_t searchRange = 16u;
    uint16_t entrySelector = 0u;
    while (static_cast<uint16_t>(searchRange * 2u) <= static_cast<uint16_t>(tableCount * 16u)) {
        searchRange = static_cast<uint16_t>(searchRange * 2u);
        ++entrySelector;
    }
    Stream font;
    font.writeU32(kSfntVersionTrueType);
    font.writeU16(tableCount);
    font.writeU16(searchRange);
    font.writeU16(entrySelector);
    font.writeU16(static_cast<uint16_t>(tableCount * 16u - searchRange));
    uint32_t runningOffset = 12u + 16u * static_cast<uint32_t>(tableCount);
    std::size_t headOffset = 0;
    for (std::size_t tableIdx = 0; tableIdx < aoTables.size(); ++tableIdx) {
        const uint32_t realLength = static_cast<uint32_t>(aoTables[tableIdx].bytes.getSize());
        aoTables[tableIdx].bytes.padToLong();
        font.writeTag(aoTables[tableIdx].tag);
        font.writeU32(computeTableCheckSum(aoTables[tableIdx].bytes));
        font.writeU32(runningOffset);
        font.writeU32(realLength);  // the REAL length, the pad is a file artifact
        if (aoTables[tableIdx].tag == kTagHead) {
            headOffset = runningOffset;
        }
        runningOffset += static_cast<uint32_t>(aoTables[tableIdx].bytes.getSize());
    }
    for (std::size_t tableIdx = 0; tableIdx < aoTables.size(); ++tableIdx) {
        font.appendStream(aoTables[tableIdx].bytes);
    }
    aoBytes.assign(font.getBytes(), font.getBytes() + font.getSize());
    if (headOffset != 0) {
        // the whole-file sum with the adjustment at 0, then the stamp
        Stream whole(&aoBytes[0], aoBytes.size());
        whole.padToLong();
        const uint32_t wholeSum = computeTableCheckSum(whole);
        const uint32_t adjustment = kCheckSumAdjustmentBase - wholeSum;
        aoBytes[headOffset + 8u] = static_cast<uint8_t>(adjustment >> 24);
        aoBytes[headOffset + 9u] = static_cast<uint8_t>((adjustment >> 16) & 0xFFu);
        aoBytes[headOffset + 10u] = static_cast<uint8_t>((adjustment >> 8) & 0xFFu);
        aoBytes[headOffset + 11u] = static_cast<uint8_t>(adjustment & 0xFFu);
    }
    return true;
}

}  // namespace ttf
}  // namespace ez
