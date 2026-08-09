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
// the raw table structures, mirrored from the format (the FontParser
// reference keeps the field-by-field truth). this phase carries the sfnt
// DIRECTORY only — the per-table structs land with their readers
// (P2 : head/maxp/hhea/hmtx/cmap/loca)

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ttfTypes.hpp"
#include "ttfStream.hpp"
#include "ttfPostNames.hpp"

namespace ez {
namespace ttf {

// one entry of the table directory : where a table lives in the file.
// offset/length are FILE-relative and BOUNDED by the caller against the
// real byte count before any table read (a corrupt directory lies)
struct TableRecord {
    TableTag tag;
    uint32_t checkSum;
    uint32_t offset;
    uint32_t length;
    TableRecord() : tag(0), checkSum(0), offset(0), length(0) {}
};

// the sfnt header : the version and the directory sizing. the search
// fields (searchRange/entrySelector/rangeShift) are READ but never
// trusted — they are derivable from numTables, and the writer recomputes
// them (a corrupt trio must not poison anything)
struct SfntHeader {
    uint32_t sfntVersion;
    uint16_t numTables;
    uint16_t searchRange;
    uint16_t entrySelector;
    uint16_t rangeShift;
    SfntHeader() : sfntVersion(0), numTables(0), searchRange(0), entrySelector(0), rangeShift(0) {}
};

// parses the sfnt header + the table directory from the stream START.
// false on : a stream error (truncation), an OTTO/cff flavour (out of
// ezTTF scope, reported as unsupported rather than corrupt by giving the
// header back filled), an unknown magic, or a directory whose records
// point past the end of the stream. the record order of the file is kept
inline bool parseSfntDirectory(Stream& aStream, SfntHeader& aoHeader, std::vector<TableRecord>& aoRecords) {
    aoRecords.clear();
    aStream.setReadPos(0);
    aoHeader.sfntVersion = aStream.readU32();
    aoHeader.numTables = aStream.readU16();
    aoHeader.searchRange = aStream.readU16();
    aoHeader.entrySelector = aStream.readU16();
    aoHeader.rangeShift = aStream.readU16();
    if (!aStream.ok()) {
        return false;
    }
    // 0x00010000 (and the rare mac 'true') are glyf-flavoured : ours.
    // OTTO is cff-flavoured : honestly refused, not "corrupt"
    const auto trueTag = makeTag('t', 'r', 'u', 'e');
    if (aoHeader.sfntVersion != kSfntVersionTrueType && aoHeader.sfntVersion != trueTag) {
        return false;
    }
    aoRecords.reserve(aoHeader.numTables);
    for (uint16_t recordIdx = 0; recordIdx < aoHeader.numTables; ++recordIdx) {
        TableRecord record;
        record.tag = aStream.readTag();
        record.checkSum = aStream.readU32();
        record.offset = aStream.readU32();
        record.length = aStream.readU32();
        if (!aStream.ok()) {
            aoRecords.clear();
            return false;  // truncated directory
        }
        // the bound check : offset + length must stay inside the stream,
        // watching the u32 overflow a hostile record can craft
        const auto endOffset = static_cast<uint64_t>(record.offset) + static_cast<uint64_t>(record.length);
        if (endOffset > static_cast<uint64_t>(aStream.getSize())) {
            aoRecords.clear();
            return false;
        }
        aoRecords.push_back(record);
    }
    return true;
}

// the record carrying aTag, nullptr when absent (the caller decides
// whether that table was required or optional)
inline const TableRecord* findTableRecord(const std::vector<TableRecord>& aRecords, TableTag aTag) {
    for (std::size_t recordIdx = 0; recordIdx < aRecords.size(); ++recordIdx) {
        if (aRecords[recordIdx].tag == aTag) {
            return &aRecords[recordIdx];
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------
// per-table structs + their parsers. one shared shape : parse(aStream,
// aRecord) seeks to the record offset, reads the fields, and answers
// false on a stream error or a table-level refusal (bad magic...). the
// caller already bounded the record against the file (parseSfntDirectory)
// ---------------------------------------------------------------------

// 'head' : the font header — upem, the global bbox, and indexToLocFormat
// (the loca flavour, needed BEFORE loca can be read)
struct HeadTable {
    Fixed version;
    Fixed fontRevision;
    uint32_t checkSumAdjustment;
    uint32_t magicNumber;
    uint16_t flags;
    uint16_t unitsPerEm;
    int64_t created;
    int64_t modified;
    int16_t xMin;
    int16_t yMin;
    int16_t xMax;
    int16_t yMax;
    uint16_t macStyle;
    uint16_t lowestRecPPEM;
    int16_t fontDirectionHint;
    int16_t indexToLocFormat;  // 0 = short loca (offsets/2), 1 = long
    int16_t glyphDataFormat;
    HeadTable()
        : checkSumAdjustment(0), magicNumber(0), flags(0), unitsPerEm(0), created(0), modified(0),  //
          xMin(0), yMin(0), xMax(0), yMax(0), macStyle(0), lowestRecPPEM(0), fontDirectionHint(0), indexToLocFormat(0), glyphDataFormat(0) {}
    bool parse(Stream& aStream, const TableRecord& aRecord) {
        aStream.setReadPos(aRecord.offset);
        version = aStream.readFixed();
        fontRevision = aStream.readFixed();
        checkSumAdjustment = aStream.readU32();
        magicNumber = aStream.readU32();
        flags = aStream.readU16();
        unitsPerEm = aStream.readU16();
        created = aStream.readI64();
        modified = aStream.readI64();
        xMin = aStream.readI16();
        yMin = aStream.readI16();
        xMax = aStream.readI16();
        yMax = aStream.readI16();
        macStyle = aStream.readU16();
        lowestRecPPEM = aStream.readU16();
        fontDirectionHint = aStream.readI16();
        indexToLocFormat = aStream.readI16();
        glyphDataFormat = aStream.readI16();
        // the magic is the head table own tripwire ; a upem of 0 would
        // poison every scale computation downstream
        return aStream.ok() && (magicNumber == kHeadMagicNumber) && (unitsPerEm != 0);
    }
};

// 'maxp' : the glyph count (version 1.0 carries the glyf profile — kept
// raw for a faithful rewrite, the writer recomputes what it can)
struct MaxpTable {
    Fixed version;
    uint16_t numGlyphs;
    uint16_t maxPoints;
    uint16_t maxContours;
    uint16_t maxCompositePoints;
    uint16_t maxCompositeContours;
    uint16_t maxZones;
    uint16_t maxTwilightPoints;
    uint16_t maxStorage;
    uint16_t maxFunctionDefs;
    uint16_t maxInstructionDefs;
    uint16_t maxStackElements;
    uint16_t maxSizeOfInstructions;
    uint16_t maxComponentElements;
    uint16_t maxComponentDepth;
    MaxpTable()
        : numGlyphs(0), maxPoints(0), maxContours(0), maxCompositePoints(0), maxCompositeContours(0), maxZones(0),  //
          maxTwilightPoints(0), maxStorage(0), maxFunctionDefs(0), maxInstructionDefs(0), maxStackElements(0),      //
          maxSizeOfInstructions(0), maxComponentElements(0), maxComponentDepth(0) {}
    bool parse(Stream& aStream, const TableRecord& aRecord) {
        aStream.setReadPos(aRecord.offset);
        version = aStream.readFixed();
        numGlyphs = aStream.readU16();
        // the 0.5 flavour (cff) stops here ; the 1.0 one carries the glyf
        // profile — read when present, harmless zeros otherwise
        if (version.raw == 0x00010000) {
            maxPoints = aStream.readU16();
            maxContours = aStream.readU16();
            maxCompositePoints = aStream.readU16();
            maxCompositeContours = aStream.readU16();
            maxZones = aStream.readU16();
            maxTwilightPoints = aStream.readU16();
            maxStorage = aStream.readU16();
            maxFunctionDefs = aStream.readU16();
            maxInstructionDefs = aStream.readU16();
            maxStackElements = aStream.readU16();
            maxSizeOfInstructions = aStream.readU16();
            maxComponentElements = aStream.readU16();
            maxComponentDepth = aStream.readU16();
        }
        return aStream.ok();
    }
};

// 'hhea' : the horizontal header — the font-wide vertical metrics and
// numberOfHMetrics, which SIZES the hmtx table
struct HheaTable {
    Fixed version;
    int16_t ascender;
    int16_t descender;
    int16_t lineGap;
    uint16_t advanceWidthMax;
    int16_t minLeftSideBearing;
    int16_t minRightSideBearing;
    int16_t xMaxExtent;
    int16_t caretSlopeRise;
    int16_t caretSlopeRun;
    int16_t caretOffset;
    int16_t metricDataFormat;
    uint16_t numberOfHMetrics;
    HheaTable()
        : ascender(0), descender(0), lineGap(0), advanceWidthMax(0), minLeftSideBearing(0), minRightSideBearing(0),  //
          xMaxExtent(0), caretSlopeRise(0), caretSlopeRun(0), caretOffset(0), metricDataFormat(0), numberOfHMetrics(0) {}
    bool parse(Stream& aStream, const TableRecord& aRecord) {
        aStream.setReadPos(aRecord.offset);
        version = aStream.readFixed();
        ascender = aStream.readI16();
        descender = aStream.readI16();
        lineGap = aStream.readI16();
        advanceWidthMax = aStream.readU16();
        minLeftSideBearing = aStream.readI16();
        minRightSideBearing = aStream.readI16();
        xMaxExtent = aStream.readI16();
        caretSlopeRise = aStream.readI16();
        caretSlopeRun = aStream.readI16();
        caretOffset = aStream.readI16();
        for (int32_t reservedIdx = 0; reservedIdx < 4; ++reservedIdx) {
            aStream.readI16();  // reserved
        }
        metricDataFormat = aStream.readI16();
        numberOfHMetrics = aStream.readU16();
        // zero would leave every glyph metric-less : a table-level refusal
        return aStream.ok() && (numberOfHMetrics != 0);
    }
};

// 'hmtx' : per-glyph advance/lsb, RESOLVED at parse — the trailing
// lsb-only run shares the advance of the LAST full pair, so the vectors
// come back numGlyphs-sized and no caller re-implements that rule
struct HmtxTable {
    std::vector<uint16_t> advanceWidths;
    std::vector<int16_t> leftSideBearings;
    bool parse(Stream& aStream, const TableRecord& aRecord, uint16_t aNumberOfHMetrics, uint16_t aNumGlyphs) {
        advanceWidths.clear();
        leftSideBearings.clear();
        if (aNumberOfHMetrics == 0 || aNumberOfHMetrics > aNumGlyphs) {
            return false;  // hhea and maxp disagree : corrupt
        }
        aStream.setReadPos(aRecord.offset);
        advanceWidths.reserve(aNumGlyphs);
        leftSideBearings.reserve(aNumGlyphs);
        uint16_t lastAdvance = 0;
        for (uint16_t metricIdx = 0; metricIdx < aNumberOfHMetrics; ++metricIdx) {
            lastAdvance = aStream.readU16();
            advanceWidths.push_back(lastAdvance);
            leftSideBearings.push_back(aStream.readI16());
        }
        for (uint16_t glyphIdx = aNumberOfHMetrics; glyphIdx < aNumGlyphs; ++glyphIdx) {
            advanceWidths.push_back(lastAdvance);
            leftSideBearings.push_back(aStream.readI16());
        }
        if (!aStream.ok()) {
            advanceWidths.clear();
            leftSideBearings.clear();
            return false;
        }
        return true;
    }
};

// 'loca' : the glyf offsets, RESOLVED to bytes at parse (the short
// flavour stores halves). numGlyphs+1 entries : entry i and i+1 bound
// the glyph i data, an equal pair means an EMPTY glyph (a space)
struct LocaTable {
    std::vector<uint32_t> offsets;
    bool parse(Stream& aStream, const TableRecord& aRecord, uint16_t aNumGlyphs, int16_t aIndexToLocFormat) {
        offsets.clear();
        aStream.setReadPos(aRecord.offset);
        const uint32_t entryCount = static_cast<uint32_t>(aNumGlyphs) + 1u;
        offsets.reserve(entryCount);
        for (uint32_t entryIdx = 0; entryIdx < entryCount; ++entryIdx) {
            if (aIndexToLocFormat == 0) {
                offsets.push_back(static_cast<uint32_t>(aStream.readU16()) * 2u);  // short : halved in the file
            } else {
                offsets.push_back(aStream.readU32());
            }
        }
        if (!aStream.ok()) {
            offsets.clear();
            return false;
        }
        // the offsets MUST be non-decreasing : a decreasing pair would give
        // a negative glyph length downstream — the corrupt-font defense
        for (uint32_t entryIdx = 1; entryIdx < entryCount; ++entryIdx) {
            if (offsets[entryIdx] < offsets[entryIdx - 1u]) {
                offsets.clear();
                return false;
            }
        }
        return true;
    }
};

// 'cmap' : codepoint -> glyph index. the encoding records are scanned,
// the BEST supported subtable wins (format 12 covers the supplementary
// planes, then 4, then 0 — the very reason CodePoint is a u32 here),
// and only that one is parsed. glyph index 0 (notdef) mappings are
// dropped : an absent codepoint already answers 0 downstream
struct CmapTable {
    std::map<CodePoint, GlyphIndex> codePointToGlyphIndex;
    bool parse(Stream& aStream, const TableRecord& aRecord) {
        codePointToGlyphIndex.clear();
        aStream.setReadPos(aRecord.offset);
        aStream.readU16();  // version, unused
        const uint16_t encodingCount = aStream.readU16();
        // scan the records, remember the best supported format offset
        uint32_t bestOffset = 0;
        int32_t bestScore = 0;
        for (uint16_t encodingIdx = 0; encodingIdx < encodingCount; ++encodingIdx) {
            aStream.readU16();  // platformID : the format decides, not the platform
            aStream.readU16();  // encodingID
            const uint32_t subtableOffset = aStream.readU32();
            if (!aStream.ok()) {
                return false;  // truncated record list
            }
            // peek the subtable format (offset is cmap-relative)
            const std::size_t savedPos = aStream.getReadPos();
            aStream.setReadPos(static_cast<std::size_t>(aRecord.offset) + subtableOffset);
            const uint16_t format = aStream.readU16();
            if (aStream.ok()) {
                const int32_t score = (format == 12u) ? 3 : (format == 4u) ? 2 : (format == 0u) ? 1 : 0;
                if (score > bestScore) {
                    bestScore = score;
                    bestOffset = subtableOffset;
                }
            }
            aStream.setReadPos(savedPos);
        }
        if (bestScore == 0) {
            return false;  // no supported subtable at all
        }
        aStream.setReadPos(static_cast<std::size_t>(aRecord.offset) + bestOffset);
        const uint16_t format = aStream.readU16();
        auto parsed = false;
        if (format == 0u) {
            parsed = m_parseFormat0(aStream);
        } else if (format == 4u) {
            parsed = m_parseFormat4(aStream);
        } else {
            parsed = m_parseFormat12(aStream);
        }
        if (!parsed) {
            codePointToGlyphIndex.clear();
        }
        return parsed;
    }

private:
    void m_add(CodePoint aCodePoint, uint32_t aGlyphIndex) {
        // notdef mappings carry no information ; indices past the u16
        // range cannot exist in a glyf font (maxp is a u16)
        if (aGlyphIndex != 0u && aGlyphIndex <= 0xFFFFu) {
            codePointToGlyphIndex[aCodePoint] = static_cast<GlyphIndex>(aGlyphIndex);
        }
    }
    bool m_parseFormat0(Stream& aStream) {
        aStream.readU16();  // length
        aStream.readU16();  // language
        for (uint32_t codePoint = 0; codePoint < 256u; ++codePoint) {
            m_add(codePoint, aStream.readU8());
        }
        return aStream.ok();
    }
    bool m_parseFormat4(Stream& aStream) {
        aStream.readU16();  // length
        aStream.readU16();  // language
        const uint16_t segCountX2 = aStream.readU16();
        const uint16_t segCount = segCountX2 / 2u;
        if (segCount == 0u) {
            return false;
        }
        aStream.readU16();  // searchRange trio : derivable, never trusted
        aStream.readU16();
        aStream.readU16();
        std::vector<uint16_t> endCodes(segCount);
        std::vector<uint16_t> startCodes(segCount);
        std::vector<uint16_t> idDeltas(segCount);
        std::vector<uint16_t> idRangeOffsets(segCount);
        for (uint16_t segIdx = 0; segIdx < segCount; ++segIdx) {
            endCodes[segIdx] = aStream.readU16();
        }
        aStream.readU16();  // reservedPad
        for (uint16_t segIdx = 0; segIdx < segCount; ++segIdx) {
            startCodes[segIdx] = aStream.readU16();
        }
        for (uint16_t segIdx = 0; segIdx < segCount; ++segIdx) {
            idDeltas[segIdx] = aStream.readU16();
        }
        // idRangeOffsets START here : the offset arithmetic of the format
        // is relative to each entry position — rebased below onto the
        // glyphIdArray that follows the four arrays
        for (uint16_t segIdx = 0; segIdx < segCount; ++segIdx) {
            idRangeOffsets[segIdx] = aStream.readU16();
        }
        if (!aStream.ok()) {
            return false;
        }
        // the trailing glyphIdArray : whatever remains of the stream is an
        // upper bound (the subtable length field of a corrupt font lies ;
        // the per-index bound check below is the real guard)
        std::vector<uint16_t> glyphIds;
        while (aStream.ok() && aStream.getReadPos() + 1u < aStream.getSize()) {
            glyphIds.push_back(aStream.readU16());
        }
        for (uint16_t segIdx = 0; segIdx < segCount; ++segIdx) {
            const uint16_t startCode = startCodes[segIdx];
            const uint16_t endCode = endCodes[segIdx];
            if (startCode > endCode) {
                continue;  // a corrupt segment : skipped, not fatal
            }
            for (uint32_t code = startCode; code <= endCode; ++code) {
                if (code == 0xFFFFu) {
                    continue;  // the terminator pseudo-codepoint
                }
                if (idRangeOffsets[segIdx] == 0u) {
                    // modulo 65536 arithmetic on purpose (the format rule)
                    m_add(code, static_cast<uint16_t>(code + idDeltas[segIdx]));
                } else {
                    // rebased : entry position + offset lands in glyphIds at
                    // offset/2 - (segCount - segIdx) + (code - startCode)
                    const int64_t glyphIdx = static_cast<int64_t>(idRangeOffsets[segIdx]) / 2 -  //
                                             (static_cast<int64_t>(segCount) - segIdx) + (static_cast<int64_t>(code) - startCode);
                    if (glyphIdx >= 0 && static_cast<std::size_t>(glyphIdx) < glyphIds.size()) {
                        const uint16_t rawGlyph = glyphIds[static_cast<std::size_t>(glyphIdx)];
                        if (rawGlyph != 0u) {
                            m_add(code, static_cast<uint16_t>(rawGlyph + idDeltas[segIdx]));
                        }
                    }
                }
            }
        }
        return true;
    }
    bool m_parseFormat12(Stream& aStream) {
        aStream.readU16();  // reserved
        aStream.readU32();  // length
        aStream.readU32();  // language
        const uint32_t groupCount = aStream.readU32();
        for (uint32_t groupIdx = 0; groupIdx < groupCount; ++groupIdx) {
            const uint32_t startChar = aStream.readU32();
            const uint32_t endChar = aStream.readU32();
            const uint32_t startGlyph = aStream.readU32();
            if (!aStream.ok()) {
                return false;  // a truncated group list is fatal (the count lied)
            }
            if (startChar > endChar || startChar > 0x10FFFFu) {
                continue;  // a corrupt group : skipped
            }
            // clamp to the unicode range : a hostile group cannot explode the map
            const uint32_t boundedEnd = (endChar > 0x10FFFFu) ? 0x10FFFFu : endChar;
            for (uint32_t code = startChar; code <= boundedEnd; ++code) {
                m_add(code, startGlyph + (code - startChar));
            }
        }
        return aStream.ok();
    }
};

// the name ids every studio screen speaks (the spec registry)
const uint16_t kNameIdCopyright = 0u;
const uint16_t kNameIdFamily = 1u;
const uint16_t kNameIdSubfamily = 2u;
const uint16_t kNameIdFullName = 4u;
const uint16_t kNameIdVersion = 5u;
const uint16_t kNameIdPostScriptName = 6u;

// 'name' : the human-facing strings. every record is decoded to UTF-8 at
// parse : windows records (platform 3) are UTF-16BE — surrogate pairs
// included — and mac roman (platform 1) rides as latin-1-ish. getName
// serves the windows flavour first (the richer one), mac as fallback
struct NameTable {
    struct Entry {
        uint16_t platformId;
        uint16_t encodingId;
        uint16_t languageId;
        uint16_t nameId;
        std::string textUtf8;
        Entry() : platformId(0), encodingId(0), languageId(0), nameId(0) {}
    };
    std::vector<Entry> entries;
    bool parse(Stream& aStream, const TableRecord& aRecord) {
        entries.clear();
        aStream.setReadPos(aRecord.offset);
        aStream.readU16();  // version (0 and 1 share what we read)
        const uint16_t recordCount = aStream.readU16();
        const uint16_t storageOffset = aStream.readU16();
        if (!aStream.ok()) {
            return false;
        }
        for (uint16_t recordIdx = 0; recordIdx < recordCount; ++recordIdx) {
            // the record header position moves : re-seek each turn, the
            // string decode below jumps into the storage
            aStream.setReadPos(static_cast<std::size_t>(aRecord.offset) + 6u + static_cast<std::size_t>(recordIdx) * 12u);
            Entry entry;
            entry.platformId = aStream.readU16();
            entry.encodingId = aStream.readU16();  // kept for the re-emission (the platform decides the decode)
            entry.languageId = aStream.readU16();
            entry.nameId = aStream.readU16();
            const uint16_t byteLength = aStream.readU16();
            const uint16_t stringOffset = aStream.readU16();
            if (!aStream.ok()) {
                entries.clear();
                return false;  // a truncated record list is fatal
            }
            aStream.setReadPos(static_cast<std::size_t>(aRecord.offset) + storageOffset + stringOffset);
            if (entry.platformId == 3u || entry.platformId == 0u) {
                // utf-16be (windows and unicode platforms)
                if ((byteLength % 2u) != 0u) {
                    continue;  // a lying length : this record only is dropped
                }
                std::string utf8;
                auto valid = true;
                for (uint16_t byteIdx = 0; byteIdx < byteLength && valid; byteIdx += 2u) {
                    uint32_t unit = aStream.readU16();
                    if (unit >= 0xD800u && unit <= 0xDBFFu) {
                        const uint32_t low = aStream.readU16();
                        byteIdx += 2u;
                        if (low < 0xDC00u || low > 0xDFFFu) {
                            valid = false;
                            break;
                        }
                        unit = 0x10000u + ((unit - 0xD800u) << 10) + (low - 0xDC00u);
                    } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
                        valid = false;
                        break;
                    }
                    // utf-8 encode (the codepoint is <= 0x10FFFF by construction)
                    if (unit < 0x80u) {
                        utf8 += static_cast<char>(unit);
                    } else if (unit < 0x800u) {
                        utf8 += static_cast<char>(0xC0u | (unit >> 6));
                        utf8 += static_cast<char>(0x80u | (unit & 0x3Fu));
                    } else if (unit < 0x10000u) {
                        utf8 += static_cast<char>(0xE0u | (unit >> 12));
                        utf8 += static_cast<char>(0x80u | ((unit >> 6) & 0x3Fu));
                        utf8 += static_cast<char>(0x80u | (unit & 0x3Fu));
                    } else {
                        utf8 += static_cast<char>(0xF0u | (unit >> 18));
                        utf8 += static_cast<char>(0x80u | ((unit >> 12) & 0x3Fu));
                        utf8 += static_cast<char>(0x80u | ((unit >> 6) & 0x3Fu));
                        utf8 += static_cast<char>(0x80u | (unit & 0x3Fu));
                    }
                }
                if (!valid || !aStream.ok()) {
                    continue;  // an unpaired surrogate or a lying offset : dropped
                }
                entry.textUtf8 = utf8;
            } else {
                // mac roman treated as latin-1 : the ascii range is exact,
                // the accents approximate — honest enough for a fallback
                std::string utf8;
                for (uint16_t byteIdx = 0; byteIdx < byteLength; ++byteIdx) {
                    const uint8_t byte = aStream.readU8();
                    if (byte < 0x80u) {
                        utf8 += static_cast<char>(byte);
                    } else {
                        utf8 += static_cast<char>(0xC0u | (byte >> 6));
                        utf8 += static_cast<char>(0x80u | (byte & 0x3Fu));
                    }
                }
                if (!aStream.ok()) {
                    continue;
                }
                entry.textUtf8 = utf8;
            }
            entries.push_back(entry);
        }
        return true;
    }
    // the windows flavour first (3), then unicode (0), then mac (1) ;
    // empty when the id is absent everywhere
    std::string getName(uint16_t aNameId) const {
        const uint16_t platformOrder[3] = {3u, 0u, 1u};
        for (int32_t orderIdx = 0; orderIdx < 3; ++orderIdx) {
            for (std::size_t entryIdx = 0; entryIdx < entries.size(); ++entryIdx) {
                if (entries[entryIdx].nameId == aNameId && entries[entryIdx].platformId == platformOrder[orderIdx]) {
                    return entries[entryIdx].textUtf8;
                }
            }
        }
        return std::string();
    }
};

// 'post' : the glyph NAMES. version 2 carries an index per glyph —
// below 258 the standard Macintosh table answers, from 258 the pascal
// strings that follow. version 1 IS the standard order. version 3
// carries no names at all (empty vector, honestly)
struct PostTable {
    Fixed version;
    Fixed italicAngle;
    int16_t underlinePosition;
    int16_t underlineThickness;
    uint32_t isFixedPitch;
    std::vector<std::string> glyphNames;  // resolved per glyph index (empty on v3)
    PostTable() : underlinePosition(0), underlineThickness(0), isFixedPitch(0) {}
    bool parse(Stream& aStream, const TableRecord& aRecord, uint16_t aNumGlyphs) {
        glyphNames.clear();
        aStream.setReadPos(aRecord.offset);
        version = aStream.readFixed();
        italicAngle = aStream.readFixed();
        underlinePosition = aStream.readI16();
        underlineThickness = aStream.readI16();
        isFixedPitch = aStream.readU32();
        for (int32_t memIdx = 0; memIdx < 4; ++memIdx) {
            aStream.readU32();  // min/max mem usage : irrelevant here
        }
        if (!aStream.ok()) {
            return false;
        }
        if (version.raw == 0x00030000) {
            return true;  // no names by design
        }
        if (version.raw == 0x00010000) {
            // the standard order itself : only legal when every glyph fits it
            if (aNumGlyphs > kMacGlyphNameCount) {
                return false;
            }
            glyphNames.reserve(aNumGlyphs);
            for (uint16_t glyphIdx = 0; glyphIdx < aNumGlyphs; ++glyphIdx) {
                glyphNames.push_back(getMacGlyphName(glyphIdx));
            }
            return true;
        }
        if (version.raw != 0x00020000) {
            return false;  // 2.5 and friends : deprecated, refused
        }
        const uint16_t glyphCount = aStream.readU16();
        if (!aStream.ok() || glyphCount != aNumGlyphs) {
            return false;  // post and maxp must agree
        }
        std::vector<uint16_t> nameIndices(glyphCount);
        uint16_t customCount = 0;
        for (uint16_t glyphIdx = 0; glyphIdx < glyphCount; ++glyphIdx) {
            nameIndices[glyphIdx] = aStream.readU16();
            if (nameIndices[glyphIdx] >= kMacGlyphNameCount) {
                ++customCount;
            }
        }
        if (!aStream.ok()) {
            return false;
        }
        // the pascal-string pool : read to the table end, then resolve.
        // a custom index must land INSIDE the pool — corrupt otherwise
        std::vector<std::string> customNames;
        customNames.reserve(customCount);
        const std::size_t tableEnd = static_cast<std::size_t>(aRecord.offset) + aRecord.length;
        while (aStream.ok() && aStream.getReadPos() < tableEnd) {
            const uint8_t length = aStream.readU8();
            customNames.push_back(aStream.readString(length));
        }
        glyphNames.reserve(glyphCount);
        for (uint16_t glyphIdx = 0; glyphIdx < glyphCount; ++glyphIdx) {
            if (nameIndices[glyphIdx] < kMacGlyphNameCount) {
                glyphNames.push_back(getMacGlyphName(nameIndices[glyphIdx]));
            } else {
                const std::size_t customIdx = static_cast<std::size_t>(nameIndices[glyphIdx]) - kMacGlyphNameCount;
                if (customIdx >= customNames.size()) {
                    glyphNames.clear();
                    return false;  // an index past the pool : corrupt
                }
                glyphNames.push_back(customNames[customIdx]);
            }
        }
        return aStream.ok();
    }
};

// 'CPAL' : the color palettes of a layered color font. one entry list
// shared by every palette, sliced by per-palette start indices. the
// spec stores BGRA bytes — served here as honest RGBA fields
struct ColorRgba {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
    ColorRgba() : red(0), green(0), blue(0), alpha(255) {}
};

struct CpalTable {
    std::vector<std::vector<ColorRgba> > palettes;
    bool parse(Stream& aStream, const TableRecord& aRecord) {
        palettes.clear();
        aStream.setReadPos(aRecord.offset);
        aStream.readU16();  // version : v0 fields are the shared prefix of v1
        const uint16_t entriesPerPalette = aStream.readU16();
        const uint16_t paletteCount = aStream.readU16();
        const uint16_t colorCount = aStream.readU16();
        const uint32_t colorsOffset = aStream.readU32();
        if (!aStream.ok()) {
            return false;
        }
        std::vector<uint16_t> startIndices(paletteCount);
        for (uint16_t paletteIdx = 0; paletteIdx < paletteCount; ++paletteIdx) {
            startIndices[paletteIdx] = aStream.readU16();
        }
        if (!aStream.ok()) {
            return false;
        }
        for (uint16_t paletteIdx = 0; paletteIdx < paletteCount; ++paletteIdx) {
            // a palette sliced past the record list is corrupt : refused
            if (static_cast<uint32_t>(startIndices[paletteIdx]) + entriesPerPalette > colorCount) {
                palettes.clear();
                return false;
            }
            aStream.setReadPos(static_cast<std::size_t>(aRecord.offset) + colorsOffset + static_cast<std::size_t>(startIndices[paletteIdx]) * 4u);
            std::vector<ColorRgba> palette;
            palette.reserve(entriesPerPalette);
            for (uint16_t entryIdx = 0; entryIdx < entriesPerPalette; ++entryIdx) {
                ColorRgba color;
                color.blue = aStream.readU8();  // the BGRA order of the format
                color.green = aStream.readU8();
                color.red = aStream.readU8();
                color.alpha = aStream.readU8();
                palette.push_back(color);
            }
            if (!aStream.ok()) {
                palettes.clear();
                return false;
            }
            palettes.push_back(palette);
        }
        return true;
    }
};

// 'COLR' v0 : a BASE glyph renders as a stack of LAYER glyphs, each
// tinted by a palette entry (0xFFFF = the text color itself)
struct ColrLayer {
    GlyphIndex glyphIndex;
    uint16_t paletteEntry;
    ColrLayer() : glyphIndex(0), paletteEntry(0) {}
};

struct ColrTable {
    std::map<GlyphIndex, std::vector<ColrLayer> > baseToLayers;
    bool parse(Stream& aStream, const TableRecord& aRecord) {
        baseToLayers.clear();
        aStream.setReadPos(aRecord.offset);
        aStream.readU16();  // version : the v0 fields are what we speak
        const uint16_t baseCount = aStream.readU16();
        const uint32_t baseOffset = aStream.readU32();
        const uint32_t layerOffset = aStream.readU32();
        const uint16_t layerCount = aStream.readU16();
        if (!aStream.ok()) {
            return false;
        }
        for (uint16_t baseIdx = 0; baseIdx < baseCount; ++baseIdx) {
            aStream.setReadPos(static_cast<std::size_t>(aRecord.offset) + baseOffset + static_cast<std::size_t>(baseIdx) * 6u);
            const uint16_t baseGlyph = aStream.readU16();
            const uint16_t firstLayer = aStream.readU16();
            const uint16_t numLayers = aStream.readU16();
            if (!aStream.ok() || static_cast<uint32_t>(firstLayer) + numLayers > layerCount) {
                baseToLayers.clear();
                return false;  // a base slicing past the layer list : corrupt
            }
            std::vector<ColrLayer> layers;
            layers.reserve(numLayers);
            aStream.setReadPos(static_cast<std::size_t>(aRecord.offset) + layerOffset + static_cast<std::size_t>(firstLayer) * 4u);
            for (uint16_t layerIdx = 0; layerIdx < numLayers; ++layerIdx) {
                ColrLayer layer;
                layer.glyphIndex = aStream.readU16();
                layer.paletteEntry = aStream.readU16();
                layers.push_back(layer);
            }
            if (!aStream.ok()) {
                baseToLayers.clear();
                return false;
            }
            baseToLayers[baseGlyph] = layers;
        }
        return true;
    }
};

}  // namespace ttf
}  // namespace ez
