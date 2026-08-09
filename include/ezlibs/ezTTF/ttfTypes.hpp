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
// scalar types of the TrueType format. successor of the ttfrrw base :
// same concepts, hardened representations (see the per-type notes)

#include <cstdint>
#include <cmath>
#include <string>

namespace ez {
namespace ttf {

// a unicode codepoint. ttfrrw held a uint16_t and was thus BMP-bound :
// the supplementary planes (emojis, icon PUA-B ranges) need the full 21
// bits, and the cmap format 12 subtable that carries them
typedef uint32_t CodePoint;
// an index into the glyf storage. the format itself is 16 bit (maxp
// numGlyphs is a uint16), so this one honestly stays narrow
typedef uint16_t GlyphIndex;
typedef uint16_t PaletteIndex;

// a table tag is a big-endian fourcc, kept as its numeric form everywhere
// (the string form is for the humans)
typedef uint32_t TableTag;

inline TableTag makeTag(char aByte0, char aByte1, char aByte2, char aByte3) {
    return (static_cast<TableTag>(static_cast<uint8_t>(aByte0)) << 24) |  //
           (static_cast<TableTag>(static_cast<uint8_t>(aByte1)) << 16) |  //
           (static_cast<TableTag>(static_cast<uint8_t>(aByte2)) << 8) |   //
           static_cast<TableTag>(static_cast<uint8_t>(aByte3));
}

inline std::string tagToString(TableTag aTag) {
    std::string ret;
    ret += static_cast<char>((aTag >> 24) & 0xFFu);
    ret += static_cast<char>((aTag >> 16) & 0xFFu);
    ret += static_cast<char>((aTag >> 8) & 0xFFu);
    ret += static_cast<char>(aTag & 0xFFu);
    return ret;
}

// the table tags ezTTF knows about. const at namespace scope : internal
// linkage, safe across translation units without inline (C++11)
const TableTag kTagCmap = 0x636D6170u;  // 'cmap'
const TableTag kTagGlyf = 0x676C7966u;  // 'glyf'
const TableTag kTagHead = 0x68656164u;  // 'head'
const TableTag kTagHhea = 0x68686561u;  // 'hhea'
const TableTag kTagHmtx = 0x686D7478u;  // 'hmtx'
const TableTag kTagLoca = 0x6C6F6361u;  // 'loca'
const TableTag kTagMaxp = 0x6D617870u;  // 'maxp'
const TableTag kTagName = 0x6E616D65u;  // 'name'
const TableTag kTagPost = 0x706F7374u;  // 'post'
const TableTag kTagColr = 0x434F4C52u;  // 'COLR'
const TableTag kTagCpal = 0x4350414Cu;  // 'CPAL'

// the sfnt scaler magics : the version a plain TrueType file opens on,
// and the 'OTTO' mark of a cff-flavoured opentype (out of ezTTF scope)
const uint32_t kSfntVersionTrueType = 0x00010000u;
const uint32_t kSfntVersionOtto = 0x4F54544Fu;
// head.magicNumber, and the constant the checkSumAdjustment derives from
const uint32_t kHeadMagicNumber = 0x5F0F3CF5u;
const uint32_t kCheckSumAdjustmentBase = 0xB1B0AFBAu;

// signed 16.16 fixed point (the 'Fixed' of the spec : table versions,
// font revision). ttfrrw split it into two int16 halves — the fraction
// half then carried a SIGN, and negative values decomposed wrong. one
// raw int32 is the storage truth, the float views are derived
struct Fixed {
    int32_t raw;
    Fixed() : raw(0) {}
    explicit Fixed(int32_t aRaw) : raw(aRaw) {}
    void setFloat(float aValue) { raw = static_cast<int32_t>(std::floor(aValue * 65536.0f + 0.5f)); }
    float getFloat() const { return static_cast<float>(raw) / 65536.0f; }
};

// signed 2.14 fixed point (composite glyph scales). plain division on
// the raw : the ttfrrw shift-and-mask decomposition read negatives wrong
// (>> on a negative int16 and a signless fraction half)
struct F2DOT14 {
    int16_t raw;
    F2DOT14() : raw(0) {}
    explicit F2DOT14(int16_t aRaw) : raw(aRaw) {}
    void setFloat(float aValue) {
        // clamp to the representable [-2, +2) before rounding : an out of
        // range write must saturate, never wrap
        auto scaled = std::floor(aValue * 16384.0f + 0.5f);
        if (scaled < -32768.0f) {
            scaled = -32768.0f;
        }
        if (scaled > 32767.0f) {
            scaled = 32767.0f;
        }
        raw = static_cast<int16_t>(scaled);
    }
    float getFloat() const { return static_cast<float>(raw) / 16384.0f; }
};

}  // namespace ttf
}  // namespace ez
