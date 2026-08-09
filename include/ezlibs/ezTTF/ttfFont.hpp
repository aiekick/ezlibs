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
// the assembled font : openFromMemory chains the table parsers
// (directory, head, maxp, hhea, hmtx, loca, cmap) and serves the v1 api
// surface of the plan. the glyph POINTS (glyf parsing, the Glyph model)
// land in the next phase — this level already answers everything the
// SourceFont pane lists : count, metrics, mappings, advances

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../ezFile.hpp"  // loadFileToBin (the file side of openFromFile)
#include "ttfTypes.hpp"
#include "ttfStream.hpp"
#include "ttfTables.hpp"
#include "ttfGlyph.hpp"
#include "ttfWriter.hpp"

namespace ez {
namespace ttf {

// the font-wide numbers, resolved once at open
struct FontInfos {
    uint16_t unitsPerEm;
    uint16_t glyphCount;
    int16_t ascender;
    int16_t descender;
    int16_t lineGap;
    int16_t xMin;
    int16_t yMin;
    int16_t xMax;
    int16_t yMax;
    FontInfos() : unitsPerEm(0), glyphCount(0), ascender(0), descender(0), lineGap(0), xMin(0), yMin(0), xMax(0), yMax(0) {}
};

class Font {
private:
    FontInfos m_infos;
    HeadTable m_head;
    MaxpTable m_maxp;
    HheaTable m_hhea;
    HmtxTable m_hmtx;
    LocaTable m_loca;  // stays empty on a loca-less font (cff...) : refused earlier anyway
    CmapTable m_cmap;
    NameTable m_name;  // OPTIONAL : a nameless font opens fine, getName answers empty
    PostTable m_post;  // OPTIONAL too : getGlyphName answers empty without it
    CpalTable m_cpal;  // OPTIONAL color pair : both empty on a plain font
    ColrTable m_colr;
    std::vector<uint8_t> m_glyfBytes;  // the raw glyf table, kept : getGlyph reads its spans on demand
    std::map<GlyphIndex, std::set<CodePoint> > m_glyphIndexToCodePoints;
    std::vector<std::string> m_errors;
    bool m_opened;

public:
    Font() : m_opened(false) {}

    // parses the whole structural side (no glyph points yet). false +
    // getErrors() filled on refusal — the object then stays unusable
    // (isOpened false), never half-filled
    bool openFromMemory(const uint8_t* apBytes, std::size_t aByteCount) {
        m_reset();
        Stream stream(apBytes, aByteCount);
        SfntHeader sfntHeader;
        std::vector<TableRecord> records;
        if (!parseSfntDirectory(stream, sfntHeader, records)) {
            m_errors.push_back((sfntHeader.sfntVersion == kSfntVersionOtto) ? "unsupported flavour : OTTO (cff)" : "corrupt sfnt directory");
            return false;
        }
        // the required structural set, in dependency order
        const TableRecord* pRecord = findTableRecord(records, kTagHead);
        if (pRecord == nullptr || !m_head.parse(stream, *pRecord)) {
            m_errors.push_back("head : missing or corrupt");
            return false;
        }
        pRecord = findTableRecord(records, kTagMaxp);
        if (pRecord == nullptr || !m_maxp.parse(stream, *pRecord)) {
            m_errors.push_back("maxp : missing or corrupt");
            return false;
        }
        pRecord = findTableRecord(records, kTagHhea);
        if (pRecord == nullptr || !m_hhea.parse(stream, *pRecord)) {
            m_errors.push_back("hhea : missing or corrupt");
            return false;
        }
        pRecord = findTableRecord(records, kTagHmtx);
        if (pRecord == nullptr || !m_hmtx.parse(stream, *pRecord, m_hhea.numberOfHMetrics, m_maxp.numGlyphs)) {
            m_errors.push_back("hmtx : missing or corrupt");
            return false;
        }
        pRecord = findTableRecord(records, kTagCmap);
        if (pRecord == nullptr || !m_cmap.parse(stream, *pRecord)) {
            m_errors.push_back("cmap : missing or no supported subtable");
            return false;
        }
        // loca : required on a glyf font (ours), parsed with the head flavour
        pRecord = findTableRecord(records, kTagLoca);
        if (pRecord == nullptr || !m_loca.parse(stream, *pRecord, m_maxp.numGlyphs, m_head.indexToLocFormat)) {
            m_errors.push_back("loca : missing or corrupt");
            return false;
        }
        // glyf : the raw bytes are KEPT — getGlyph parses a span on demand
        // (the SourceFont pane lists thousands of glyphs but draws a few).
        // the last loca offset must fit the table, else the spans lie
        pRecord = findTableRecord(records, kTagGlyf);
        if (pRecord == nullptr || m_loca.offsets.back() > pRecord->length) {
            m_errors.push_back("glyf : missing or shorter than loca promises");
            return false;
        }
        stream.setReadPos(pRecord->offset);
        m_glyfBytes.resize(pRecord->length);
        if (pRecord->length != 0 && !stream.readBytes(&m_glyfBytes[0], pRecord->length)) {
            m_errors.push_back("glyf : truncated");
            return false;
        }
        // name : OPTIONAL — a corrupt or missing one degrades to empty
        // names with a note, the font still opens (unlike the structural set)
        pRecord = findTableRecord(records, kTagName);
        if (pRecord != nullptr && !m_name.parse(stream, *pRecord)) {
            m_errors.push_back("name : corrupt, names unavailable");  // note, not fatal
            m_name = NameTable();
        }
        // post : same optional rule — the glyph names are a comfort
        pRecord = findTableRecord(records, kTagPost);
        if (pRecord != nullptr && !m_post.parse(stream, *pRecord, m_maxp.numGlyphs)) {
            m_errors.push_back("post : corrupt, glyph names unavailable");  // note, not fatal
            m_post = PostTable();
        }
        // the color pair : optional, and only meaningful TOGETHER — a
        // corrupt half drops both (layers without colors lie, and back)
        pRecord = findTableRecord(records, kTagCpal);
        if (pRecord != nullptr && !m_cpal.parse(stream, *pRecord)) {
            m_errors.push_back("CPAL : corrupt, color layers unavailable");
            m_cpal = CpalTable();
        }
        pRecord = findTableRecord(records, kTagColr);
        if (pRecord != nullptr && !m_colr.parse(stream, *pRecord)) {
            m_errors.push_back("COLR : corrupt, color layers unavailable");
            m_colr = ColrTable();
        }
        if (m_colr.baseToLayers.empty() != m_cpal.palettes.empty()) {
            m_errors.push_back("COLR/CPAL : the pair is incomplete, colors dropped");
            m_cpal = CpalTable();
            m_colr = ColrTable();
        }
        // the reverse mapping : one glyph may serve several codepoints
        for (std::map<CodePoint, GlyphIndex>::const_iterator it = m_cmap.codePointToGlyphIndex.begin();  //
             it != m_cmap.codePointToGlyphIndex.end(); ++it) {
            if (it->second < m_maxp.numGlyphs) {
                m_glyphIndexToCodePoints[it->second].insert(it->first);
            } else {
                m_errors.push_back("cmap : a mapping points past numGlyphs, dropped");  // note, not fatal
            }
        }
        m_infos.unitsPerEm = m_head.unitsPerEm;
        m_infos.glyphCount = m_maxp.numGlyphs;
        m_infos.ascender = m_hhea.ascender;
        m_infos.descender = m_hhea.descender;
        m_infos.lineGap = m_hhea.lineGap;
        m_infos.xMin = m_head.xMin;
        m_infos.yMin = m_head.yMin;
        m_infos.xMax = m_head.xMax;
        m_infos.yMax = m_head.yMax;
        m_opened = true;
        return true;
    }

    // the file flavour rides ezFile : an unreadable or empty file is the
    // same refusal as a corrupt directory, with its own error line
    bool openFromFile(const std::string& aFilePath) {
        const std::vector<uint8_t> bytes = ez::file::loadFileToBin(aFilePath);
        if (bytes.empty()) {
            m_reset();
            m_errors.push_back("file : unreadable or empty : " + aFilePath);
            return false;
        }
        return openFromMemory(&bytes[0], bytes.size());
    }

    // re-emits the whole font from the parsed state (P5). the name table
    // is NOT re-emitted yet (utf8 -> utf16 re-encode pending) ; hhea is
    // written with FULL hmtx pairs and loca always long — both legal,
    // the head indexToLocFormat follows. false on a font not opened
    bool writeToMemory(std::vector<uint8_t>& aoBytes) const {
        aoBytes.clear();
        if (!m_opened) {
            return false;
        }
        std::vector<TaggedTable> tables;
        tables.resize(7);
        HeadTable head = m_head;
        head.indexToLocFormat = 1;  // the emitted loca is always long
        tables[0].tag = kTagHead;
        emitHeadTable(head, tables[0].bytes);
        tables[1].tag = kTagMaxp;
        emitMaxpTable(m_maxp, tables[1].bytes);
        tables[2].tag = kTagHhea;
        emitHheaTable(m_hhea, m_maxp.numGlyphs, tables[2].bytes);
        tables[3].tag = kTagHmtx;
        emitHmtxTable(m_hmtx, tables[3].bytes);
        tables[4].tag = kTagLoca;
        emitLocaTable(m_loca, tables[4].bytes);
        tables[5].tag = kTagCmap;
        emitCmapTable(m_cmap.codePointToGlyphIndex, tables[5].bytes);
        tables[6].tag = kTagGlyf;
        if (!m_glyfBytes.empty()) {
            tables[6].bytes.writeBytes(&m_glyfBytes[0], m_glyfBytes.size());
        }
        if (!m_post.glyphNames.empty() || m_post.underlineThickness != 0 || m_post.underlinePosition != 0) {
            TaggedTable post;
            post.tag = kTagPost;
            emitPostTable(m_post, post.bytes);
            tables.push_back(post);
        }
        if (!m_colr.baseToLayers.empty() && !m_cpal.palettes.empty()) {
            TaggedTable colr;
            colr.tag = kTagColr;
            emitColrTable(m_colr.baseToLayers, colr.bytes);
            tables.push_back(colr);
            TaggedTable cpal;
            cpal.tag = kTagCpal;
            emitCpalTable(m_cpal.palettes, cpal.bytes);
            tables.push_back(cpal);
        }
        return assembleFont(tables, aoBytes);
    }
    bool writeToFile(const std::string& aFilePath) const {
        std::vector<uint8_t> bytes;
        if (!writeToMemory(bytes)) {
            return false;
        }
        return ez::file::saveBinToFile(bytes, aFilePath);
    }

    bool isOpened() const { return m_opened; }
    const FontInfos& getInfos() const { return m_infos; }
    // the human-facing strings (kNameId*), utf-8, empty when absent
    std::string getName(uint16_t aNameId) const { return m_name.getName(aNameId); }
    // the post name of one glyph, empty without a post v1/v2 table
    std::string getGlyphName(GlyphIndex aGlyphIndex) const {
        if (static_cast<std::size_t>(aGlyphIndex) >= m_post.glyphNames.size()) {
            return std::string();
        }
        return m_post.glyphNames[aGlyphIndex];
    }
    // the color side (COLR/CPAL v0) : nullptr = not a layered base glyph.
    // paletteEntry 0xFFFF in a layer means "the current text color"
    bool hasColorLayers() const { return !m_colr.baseToLayers.empty(); }
    const std::vector<ColrLayer>* getGlyphColorLayers(GlyphIndex aGlyphIndex) const {
        const std::map<GlyphIndex, std::vector<ColrLayer> >::const_iterator it = m_colr.baseToLayers.find(aGlyphIndex);
        return (it != m_colr.baseToLayers.end()) ? &it->second : nullptr;
    }
    std::size_t getPaletteCount() const { return m_cpal.palettes.size(); }
    bool getPaletteColor(std::size_t aPaletteIdx, uint16_t aEntryIdx, ColorRgba& aoColor) const {
        if (aPaletteIdx >= m_cpal.palettes.size() || static_cast<std::size_t>(aEntryIdx) >= m_cpal.palettes[aPaletteIdx].size()) {
            return false;
        }
        aoColor = m_cpal.palettes[aPaletteIdx][aEntryIdx];
        return true;
    }
    std::size_t getGlyphCount() const { return m_infos.glyphCount; }
    const std::vector<std::string>& getErrors() const { return m_errors; }

    // 0 = notdef : the absent-codepoint answer of the format itself
    GlyphIndex getGlyphIndex(CodePoint aCodePoint) const {
        const std::map<CodePoint, GlyphIndex>::const_iterator it = m_cmap.codePointToGlyphIndex.find(aCodePoint);
        return (it != m_cmap.codePointToGlyphIndex.end()) ? it->second : 0;
    }
    const std::set<CodePoint>* getCodePoints(GlyphIndex aGlyphIndex) const {
        const std::map<GlyphIndex, std::set<CodePoint> >::const_iterator it = m_glyphIndexToCodePoints.find(aGlyphIndex);
        return (it != m_glyphIndexToCodePoints.end()) ? &it->second : nullptr;
    }
    const std::map<CodePoint, GlyphIndex>& getCodePointMap() const { return m_cmap.codePointToGlyphIndex; }

    // resolved per-glyph metrics (the hmtx trailing-run rule already applied)
    bool getAdvanceWidth(GlyphIndex aGlyphIndex, uint16_t& aoAdvance) const {
        if (static_cast<std::size_t>(aGlyphIndex) >= m_hmtx.advanceWidths.size()) {
            return false;
        }
        aoAdvance = m_hmtx.advanceWidths[aGlyphIndex];
        return true;
    }
    bool getLeftSideBearing(GlyphIndex aGlyphIndex, int16_t& aoBearing) const {
        if (static_cast<std::size_t>(aGlyphIndex) >= m_hmtx.leftSideBearings.size()) {
            return false;
        }
        aoBearing = m_hmtx.leftSideBearings[aGlyphIndex];
        return true;
    }
    // the glyf data span of a glyph (loca resolved) : an EQUAL pair is an
    // empty glyph (a space)
    bool getGlyphDataSpan(GlyphIndex aGlyphIndex, uint32_t& aoOffset, uint32_t& aoLength) const {
        if (static_cast<std::size_t>(aGlyphIndex) + 1u >= m_loca.offsets.size()) {
            return false;
        }
        aoOffset = m_loca.offsets[aGlyphIndex];
        aoLength = m_loca.offsets[aGlyphIndex + 1u] - m_loca.offsets[aGlyphIndex];
        return true;
    }
    // the RAW glyf bytes of one glyph (the builder copies and patches
    // them — composite indices are rewritten in the copy, never here).
    // aoBytes may come back null with length 0 : the empty glyph
    bool getGlyphRawSpan(GlyphIndex aGlyphIndex, const uint8_t*& aoBytes, uint32_t& aoLength) const {
        uint32_t offset = 0;
        if (!getGlyphDataSpan(aGlyphIndex, offset, aoLength)) {
            return false;
        }
        aoBytes = (aoLength == 0 || m_glyfBytes.empty()) ? nullptr : &m_glyfBytes[0] + offset;
        return true;
    }
    // parses the outline of ONE glyph on demand (contours in absolute
    // font units, or the component references of a composite). false =
    // bad index or a corrupt glyf entry
    bool getGlyph(GlyphIndex aGlyphIndex, Glyph& aoGlyph) const {
        uint32_t offset = 0;
        uint32_t length = 0;
        if (!getGlyphDataSpan(aGlyphIndex, offset, length)) {
            return false;
        }
        const uint8_t* pSpan = m_glyfBytes.empty() ? nullptr : &m_glyfBytes[0] + offset;
        return parseGlyphData((length == 0) ? nullptr : pSpan, length, aoGlyph);
    }

private:
    void m_reset() {
        m_infos = FontInfos();
        m_head = HeadTable();
        m_maxp = MaxpTable();
        m_hhea = HheaTable();
        m_hmtx = HmtxTable();
        m_loca = LocaTable();
        m_cmap = CmapTable();
        m_name = NameTable();
        m_post = PostTable();
        m_cpal = CpalTable();
        m_colr = ColrTable();
        m_glyfBytes.clear();
        m_glyphIndexToCodePoints.clear();
        m_errors.clear();
        m_opened = false;
    }
};

}  // namespace ttf
}  // namespace ez
