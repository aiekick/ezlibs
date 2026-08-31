#pragma once

/*
MIT License

Copyright (c) 2014-2024 Stephane Cuillerdier (aka aiekick)

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

// ezBinToSrc is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// turn any binary blob into an embeddable source array :
// compress() emits the stb stream format — the exact format the
// stb_decompress embedded in Dear ImGui reads, so a formatted array is
// directly consumable by AddFontFromMemoryCompressedBase85TTF.
// the compressor is a SELF-CONTAINED port (no stb include, no link) of
// the one published in imgui misc/fonts/binary_to_compressed_c.cpp.

namespace ez {
namespace binToSrc {

namespace detail {

// the compressed-stream writer : one instance per run, no shared state
class Compressor {
private:
    static const uint32_t c_windowSize = 0x40000u;  // 256K look-behind, written in the stream header
    static const uint32_t c_hashSize = 32768u;

private:
    std::vector<uint8_t> m_output{};
    std::vector<const uint8_t*> m_hashTable{};
    uint32_t m_runningAdler{1u};

public:
    std::vector<uint8_t> run(const std::vector<uint8_t>& aBytes) {
        const auto inputLength = static_cast<uint32_t>(aBytes.size());
        m_output.clear();
        m_output.reserve(static_cast<std::size_t>(inputLength) + 512u + (inputLength >> 2u) + 4u);
        m_hashTable.assign(c_hashSize, nullptr);
        m_runningAdler = 1u;
        // stream signature and header (16 bytes) : 0x57 0xbc, a zero
        // pad, a 32-bit zero (the 64-bit length guard), the payload
        // length, the window size
        m_out8(0x57u);
        m_out8(0xbcu);
        m_out16(0u);
        m_out32(0u);
        m_out32(inputLength);
        m_out32(c_windowSize);
        if (!aBytes.empty()) {
            const uint8_t* inputBegin = aBytes.data();
            int32_t pendingLiterals = 0;
            m_compressChunk(inputBegin, inputBegin + inputLength, static_cast<int32_t>(inputLength), pendingLiterals);
            m_outLiterals(inputBegin + inputLength - pendingLiterals, pendingLiterals);
        }
        m_out16(0x05fau);  // end opcode
        m_out32(m_runningAdler);
        return m_output;
    }

private:
    void m_out8(uint32_t aValue) { m_output.push_back(static_cast<uint8_t>(aValue)); }
    void m_out16(uint32_t aValue) {
        m_out8(aValue >> 8u);
        m_out8(aValue);
    }
    void m_out24(uint32_t aValue) {
        m_out8(aValue >> 16u);
        m_out8(aValue >> 8u);
        m_out8(aValue);
    }
    void m_out32(uint32_t aValue) {
        m_out8(aValue >> 24u);
        m_out8(aValue >> 16u);
        m_out8(aValue >> 8u);
        m_out8(aValue);
    }

    // literal runs are emitted by slabs of at most 65536 bytes, each
    // with the shortest opcode able to carry its count
    void m_outLiterals(const uint8_t* apRun, int32_t aCount) {
        while (aCount > 65536) {
            m_outLiterals(apRun, 65536);
            apRun += 65536;
            aCount -= 65536;
        }
        if (aCount == 0) {
            return;
        }
        if (aCount <= 32) {
            m_out8(0x000020u + static_cast<uint32_t>(aCount) - 1u);
        } else if (aCount <= 2048) {
            m_out16(0x000800u + static_cast<uint32_t>(aCount) - 1u);
        } else {  // aCount <= 65536
            m_out24(0x070000u + static_cast<uint32_t>(aCount) - 1u);
        }
        m_output.insert(m_output.end(), apRun, apRun + aCount);
    }

    static uint32_t m_matchLength(const uint8_t* apLeft, const uint8_t* apRight, uint32_t aMaxLength) {
        uint32_t idx = 0u;
        for (idx = 0u; idx < aMaxLength; ++idx) {
            if (apLeft[idx] != apRight[idx]) {
                return idx;
            }
        }
        return idx;
    }

    // a short match is only worth its opcode when its distance stays
    // cheap to encode — the stb heuristic, kept verbatim
    static bool m_isWorthKeeping(int32_t aBest, int32_t aDistance) {
        return ((aBest > 2) && (aDistance <= 0x00100)) || ((aBest > 5) && (aDistance <= 0x04000)) || ((aBest > 7) && (aDistance <= 0x80000));
    }

    static uint32_t m_scramble(uint32_t aHash) { return (aHash + (aHash >> 16u)) & (c_hashSize - 1u); }
    static uint32_t m_hashThree(const uint8_t* apAt, uint32_t aA, uint32_t aB, uint32_t aC) {
        return (static_cast<uint32_t>(apAt[aA]) << 14u) + (static_cast<uint32_t>(apAt[aB]) << 7u) + apAt[aC];
    }
    static uint32_t m_hashFold(const uint8_t* apAt, uint32_t aHash, uint32_t aA, uint32_t aB) {
        return (aHash << 14u) + (aHash >> 18u) + (static_cast<uint32_t>(apAt[aA]) << 7u) + apAt[aB];
    }

    void m_compressChunk(const uint8_t* apStart, const uint8_t* apEnd, int32_t aLength, int32_t& aoPendingLiterals) {
        const uint8_t* literalStart = apStart - aoPendingLiterals;
        const uint8_t* cursor = apStart;
        // stop short of the end : the hash probes read up to 12 bytes
        // ahead, so the very tail always leaves as literals
        while ((cursor < apStart + aLength) && (cursor + 12 < apEnd)) {
            uint32_t matchMax = 65536u;
            if (cursor + 65536 > apEnd) {
                matchMax = static_cast<uint32_t>(apEnd - cursor);
            }
            int32_t best = 2;
            int32_t distance = 0;
            // only 4 candidate locations are probed, keyed by 4 hash
            // lengths (the LZO-inspired strategy of the original)
            const auto tryCandidate = [&](const uint8_t* apCandidate, bool aSkipAlreadyTried) {
                if (apCandidate == nullptr) {
                    return;
                }
                if (aSkipAlreadyTried && (distance == static_cast<int32_t>(cursor - apCandidate))) {
                    return;
                }
                const auto length = static_cast<int32_t>(m_matchLength(apCandidate, cursor, matchMax));
                if (length > best) {
                    const auto candidateDistance = static_cast<int32_t>(cursor - apCandidate);
                    if ((candidateDistance <= static_cast<int32_t>(c_windowSize)) && ((length > 9) || m_isWorthKeeping(length, candidateDistance))) {
                        best = length;
                        distance = candidateDistance;
                    }
                }
            };
            uint32_t hash = m_hashThree(cursor, 0u, 1u, 2u);
            const auto slotA = m_scramble(hash);
            tryCandidate(m_hashTable[slotA], false);
            hash = m_hashFold(cursor, hash, 3u, 4u);
            const auto slotB = m_scramble(hash);
            hash = m_hashFold(cursor, hash, 5u, 6u);
            tryCandidate(m_hashTable[slotB], true);
            hash = m_hashFold(cursor, hash, 7u, 8u);
            const auto slotC = m_scramble(hash);
            hash = m_hashFold(cursor, hash, 9u, 10u);
            tryCandidate(m_hashTable[slotC], true);
            hash = m_hashFold(cursor, hash, 11u, 12u);
            const auto slotD = m_scramble(hash);
            tryCandidate(m_hashTable[slotD], true);
            // the table is shared between the 4 probes : it only
            // updates AFTER all of them ran
            m_hashTable[slotA] = cursor;
            m_hashTable[slotB] = cursor;
            m_hashTable[slotC] = cursor;
            m_hashTable[slotD] = cursor;
            // the opcode ladder : each rung flushes the pending
            // literals then encodes (distance, length) at its cost
            if (best < 3) {  // fast path literal
                ++cursor;
            } else if ((best <= 0x80) && (distance <= 0x100)) {
                m_outLiterals(literalStart, static_cast<int32_t>(cursor - literalStart));
                cursor += best;
                literalStart = cursor;
                m_out8(0x80u + static_cast<uint32_t>(best) - 1u);
                m_out8(static_cast<uint32_t>(distance) - 1u);
            } else if ((best > 5) && (best <= 0x100) && (distance <= 0x4000)) {
                m_outLiterals(literalStart, static_cast<int32_t>(cursor - literalStart));
                cursor += best;
                literalStart = cursor;
                m_out16(0x4000u + static_cast<uint32_t>(distance) - 1u);
                m_out8(static_cast<uint32_t>(best) - 1u);
            } else if ((best > 7) && (best <= 0x100) && (distance <= 0x80000)) {
                m_outLiterals(literalStart, static_cast<int32_t>(cursor - literalStart));
                cursor += best;
                literalStart = cursor;
                m_out24(0x180000u + static_cast<uint32_t>(distance) - 1u);
                m_out8(static_cast<uint32_t>(best) - 1u);
            } else if ((best > 8) && (best <= 0x10000) && (distance <= 0x80000)) {
                m_outLiterals(literalStart, static_cast<int32_t>(cursor - literalStart));
                cursor += best;
                literalStart = cursor;
                m_out24(0x100000u + static_cast<uint32_t>(distance) - 1u);
                m_out16(static_cast<uint32_t>(best) - 1u);
            } else if ((best > 9) && (distance <= 0x1000000)) {
                if (best > 65536) {
                    best = 65536;
                }
                m_outLiterals(literalStart, static_cast<int32_t>(cursor - literalStart));
                cursor += best;
                literalStart = cursor;
                if (best <= 0x100) {
                    m_out8(0x06u);
                    m_out24(static_cast<uint32_t>(distance) - 1u);
                    m_out8(static_cast<uint32_t>(best) - 1u);
                } else {
                    m_out8(0x04u);
                    m_out24(static_cast<uint32_t>(distance) - 1u);
                    m_out16(static_cast<uint32_t>(best) - 1u);
                }
            } else {  // no match was a balanced tradeoff
                ++cursor;
            }
        }
        // whatever the probe loop could not reach leaves as literals
        if (cursor - apStart < aLength) {
            cursor = apStart + aLength;
        }
        aoPendingLiterals = static_cast<int32_t>(cursor - literalStart);
        m_runningAdler = m_adler32(m_runningAdler, apStart, static_cast<uint32_t>(cursor - apStart));
    }

    static uint32_t m_adler32(uint32_t aSeed, const uint8_t* apBuffer, uint32_t aLength) {
        const uint32_t c_adlerModulo = 65521u;
        uint32_t sumLow = aSeed & 0xffffu;
        uint32_t sumHigh = aSeed >> 16u;
        uint32_t blockLength = aLength % 5552u;
        while (aLength != 0u) {
            for (uint32_t idx = 0u; idx < blockLength; ++idx) {
                sumLow += apBuffer[idx];
                sumHigh += sumLow;
            }
            apBuffer += blockLength;
            sumLow %= c_adlerModulo;
            sumHigh %= c_adlerModulo;
            aLength -= blockLength;
            blockLength = 5552u;
        }
        return (sumHigh << 16u) + sumLow;
    }
};

// the base85 alphabet starts at 35 ('#') and skips 92 ('\\') so every
// code lands as-is inside a C string literal
inline char encode85Byte(uint32_t aValue) {
    const auto code = (aValue % 85u) + 35u;
    return static_cast<char>((code >= 92u) ? (code + 1u) : code);
}

}  // namespace detail

// compress a blob to the stb stream format (the one imgui reads)
inline std::vector<uint8_t> compress(const std::vector<uint8_t>& aBytes) {
    detail::Compressor compressor;
    return compressor.run(aBytes);
}

// encode bytes as C-literal-safe base85 : 4 input bytes (little-endian
// packed, zero padded) become 5 characters, the imgui Decode85 layout
inline std::string toBase85(const std::vector<uint8_t>& aBytes) {
    std::string result;
    const auto byteCount = aBytes.size();
    result.reserve(((byteCount + 3u) / 4u) * 5u);
    for (std::size_t groupIndex = 0u; groupIndex < byteCount; groupIndex += 4u) {
        uint32_t packed = 0u;
        for (std::size_t byteIndex = 0u; byteIndex < 4u; ++byteIndex) {
            if (groupIndex + byteIndex < byteCount) {
                packed |= static_cast<uint32_t>(aBytes[groupIndex + byteIndex]) << (8u * byteIndex);
            }
        }
        for (std::size_t charIndex = 0u; charIndex < 5u; ++charIndex) {
            result += detail::encode85Byte(packed);
            packed /= 85u;
        }
    }
    return result;
}

// emit the ready-to-paste source array :
//   static const char <name>[<N>+1] = "...";
// the one extra slot zero-initializes, so the array is always a valid
// C string. beyond 65536 characters the emission switches to a hex
// byte list of the same characters — compilers cap string literals,
// arrays carry no such limit. inside a string, a '?' following a '?'
// is escaped : old compilers would read "??x" as a trigraph
inline std::string formatCompressedBase85Array(const std::string& aBufferName, const std::string& aBase85) {
    const auto bufferSize = static_cast<uint32_t>(aBase85.size());
    const bool byteListMode = (bufferSize >= 65536u);
    std::string result = "static const char " + aBufferName + "[" + std::to_string(bufferSize) + "+1] =";
    result += byteListMode ? " {\n" : "\n    \"";
    char previousChar = 0;
    uint32_t lineCount = 0u;
    for (std::size_t charIndex = 0u; charIndex < aBase85.size(); ++charIndex) {
        const char current = aBase85[charIndex];
        if (byteListMode) {
            char hexBuffer[8];
            std::snprintf(hexBuffer, sizeof(hexBuffer), "0x%02x", static_cast<uint32_t>(static_cast<uint8_t>(current)));
            result += hexBuffer;
            if (charIndex + 1u < aBase85.size()) {
                result += ", ";
            }
        } else {
            if ((current == '?') && (previousChar == '?')) {
                result += '\\';
            }
            result += current;
        }
        previousChar = current;
        // a break every 140 characters (the 28-group cadence of the
        // reference emitter) keeps the lines editor-friendly
        if ((++lineCount == 140u) && (charIndex + 1u < aBase85.size())) {
            lineCount = 0u;
            result += byteListMode ? "\n" : "\"\n    \"";
        }
    }
    result += byteListMode ? "};\n" : "\";\n";
    return result;
}

}  // namespace binToSrc
}  // namespace ez
