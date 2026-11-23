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

// ezMcp is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

// A PNG writer for RGBA 8 bits images, rows from top to bottom, tightly
// packed. The image data is deflated : LZ77 over a 32 KiB window, the fixed
// Huffman codes of RFC 1951, one block. A ui capture shrinks 15 to 40 times
// (flat areas, repeated rows : a row repeats at the distance of one row) and
// stays fast to encode ; any PNG reader reads it.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ez {
namespace mcp {

class Png {
public:
    // The PNG file of the image ; empty when the size is zero, does not match
    // the pixels, or is too large for one PNG data chunk.
    static std::vector<uint8_t> encodeRgba(const std::vector<uint8_t>& aRgba, uint32_t aWidth, uint32_t aHeight) {
        const uint64_t maxRawSize = 0x40000000U;  // one IDAT chunk holds less than 2^31 bytes ; a capture stays far below
        std::vector<uint8_t> png;
        if ((aWidth == 0U) || (aHeight == 0U)) {
            return png;
        }
        const uint64_t rowBytes = static_cast<uint64_t>(aWidth) * 4U;
        const uint64_t rawSize = (rowBytes + 1U) * aHeight;  // a filter byte leads each row
        if ((static_cast<uint64_t>(aRgba.size()) != (rowBytes * aHeight)) || (rawSize > maxRawSize)) {
            return png;
        }

        // The raw scanlines : filter "none" then the row.
        std::vector<uint8_t> raw;
        raw.reserve(static_cast<size_t>(rawSize));
        for (uint32_t row = 0U; row < aHeight; ++row) {
            raw.push_back(0U);
            const auto begin = static_cast<size_t>(row * rowBytes);
            raw.insert(raw.end(), aRgba.begin() + static_cast<std::ptrdiff_t>(begin), aRgba.begin() + static_cast<std::ptrdiff_t>(begin + rowBytes));
        }

        // The zlib stream : header, one deflate block, adler32 of the raw data.
        std::vector<uint8_t> zlib;
        zlib.reserve((raw.size() / 8U) + 64U);
        zlib.push_back(0x78U);  // deflate, 32K window
        zlib.push_back(0x01U);  // no preset dictionary, check bits
        deflateFixed(raw, zlib);
        appendBigEndian(zlib, adler32(raw.data(), raw.size()));

        static const uint8_t sSignature[8] = {0x89U, 'P', 'N', 'G', 0x0DU, 0x0AU, 0x1AU, 0x0AU};
        png.assign(sSignature, sSignature + 8);
        std::vector<uint8_t> header;
        appendBigEndian(header, aWidth);
        appendBigEndian(header, aHeight);
        header.push_back(8U);  // bit depth
        header.push_back(6U);  // color type : RGBA
        header.push_back(0U);  // compression : deflate
        header.push_back(0U);  // filter method
        header.push_back(0U);  // no interlace
        appendChunk(png, "IHDR", header);
        appendChunk(png, "IDAT", zlib);
        appendChunk(png, "IEND", std::vector<uint8_t>());
        return png;
    }

    // CRC-32 of PNG (ISO 3309, reflected 0xEDB88320).
    static uint32_t crc32(const uint8_t* apData, size_t aSize) {
        return ~updateCrc32(0xFFFFFFFFU, apData, aSize);
    }

    // Adler-32 of zlib (RFC 1950).
    static uint32_t adler32(const uint8_t* apData, size_t aSize) {
        uint32_t low = 1U;
        uint32_t high = 0U;
        for (size_t idx = 0U; idx < aSize; ++idx) {
            low = (low + apData[idx]) % 65521U;
            high = (high + low) % 65521U;
        }
        return (high << 16U) | low;
    }

private:
    // LSB-first bit packing of deflate ; the Huffman codes go in reversed
    struct BitWriter {
        std::vector<uint8_t>& out;
        uint32_t buffer{};
        uint32_t count{};
        explicit BitWriter(std::vector<uint8_t>& aoOut) : out(aoOut) {
        }
        void put(uint32_t aBits, uint32_t aCount) {
            buffer |= aBits << count;
            count += aCount;
            while (count >= 8U) {
                out.push_back(static_cast<uint8_t>(buffer & 0xFFU));
                buffer >>= 8U;
                count -= 8U;
            }
        }
        void putCode(uint32_t aCode, uint32_t aLength) {
            uint32_t reversed = 0U;
            for (uint32_t bit = 0U; bit < aLength; ++bit) {
                reversed = (reversed << 1U) | ((aCode >> bit) & 1U);
            }
            put(reversed, aLength);
        }
        void flush() {
            if (count > 0U) {
                out.push_back(static_cast<uint8_t>(buffer & 0xFFU));
            }
            buffer = 0U;
            count = 0U;
        }
    };

    // the fixed literal/length code of RFC 1951 (3.2.6)
    static void putSymbol(BitWriter& aoBits, uint32_t aSymbol) {
        if (aSymbol < 144U) {
            aoBits.putCode(0x30U + aSymbol, 8U);
        } else if (aSymbol < 256U) {
            aoBits.putCode(0x190U + (aSymbol - 144U), 9U);
        } else if (aSymbol < 280U) {
            aoBits.putCode(aSymbol - 256U, 7U);
        } else {
            aoBits.putCode(0xC0U + (aSymbol - 280U), 8U);
        }
    }

    // a match : its length symbol and extra bits, its distance code and extra bits
    static void putMatch(BitWriter& aoBits, uint32_t aLength, uint32_t aDistance) {
        static const uint16_t sLengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163,
            195, 227, 258};
        static const uint8_t sLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const uint16_t sDistanceBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049,
            3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const uint8_t sDistanceExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        uint32_t lengthIdx = 28U;
        while (sLengthBase[lengthIdx] > aLength) {
            --lengthIdx;
        }
        putSymbol(aoBits, 257U + lengthIdx);
        aoBits.put(aLength - sLengthBase[lengthIdx], sLengthExtra[lengthIdx]);
        uint32_t distanceIdx = 29U;
        while (sDistanceBase[distanceIdx] > aDistance) {
            --distanceIdx;
        }
        aoBits.putCode(distanceIdx, 5U);
        aoBits.put(aDistance - sDistanceBase[distanceIdx], sDistanceExtra[distanceIdx]);
    }

    // one final block of fixed Huffman codes : greedy LZ77, a hash of 3 bytes
    // chained over the 32 KiB window, 32 candidates at most per position
    static void deflateFixed(const std::vector<uint8_t>& aRaw, std::vector<uint8_t>& aoOut) {
        const uint32_t windowSize = 32768U;
        const uint32_t maxMatch = 258U;
        const uint32_t maxChain = 32U;
        const size_t size = aRaw.size();
        std::vector<int32_t> head(65536U, -1);
        std::vector<int32_t> previous(windowSize, -1);
        const auto hashAt = [&aRaw](size_t aPos) {
            const uint32_t key = (static_cast<uint32_t>(aRaw[aPos]) << 16U) | (static_cast<uint32_t>(aRaw[aPos + 1U]) << 8U) | aRaw[aPos + 2U];
            return static_cast<size_t>((key * 2654435761U) >> 16U);
        };
        const auto insert = [&](size_t aPos) {
            if ((aPos + 3U) <= size) {
                const auto hash = hashAt(aPos);
                previous[aPos & (windowSize - 1U)] = head[hash];
                head[hash] = static_cast<int32_t>(aPos);
            }
        };
        BitWriter bits(aoOut);
        bits.put(1U, 1U);  // BFINAL
        bits.put(1U, 2U);  // BTYPE 01 : fixed Huffman codes
        size_t pos = 0U;
        while (pos < size) {
            uint32_t bestLength = 0U;
            uint32_t bestDistance = 0U;
            if ((pos + 3U) <= size) {
                const auto limit = static_cast<uint32_t>(((size - pos) < maxMatch) ? (size - pos) : maxMatch);
                auto candidate = head[hashAt(pos)];
                for (uint32_t depth = 0U; (candidate >= 0) && (depth < maxChain); ++depth) {
                    const auto distance = static_cast<uint32_t>(pos - static_cast<size_t>(candidate));
                    if (distance > windowSize) {
                        break;
                    }
                    uint32_t length = 0U;
                    while ((length < limit) && (aRaw[static_cast<size_t>(candidate) + length] == aRaw[pos + length])) {
                        ++length;
                    }
                    if (length > bestLength) {
                        bestLength = length;
                        bestDistance = distance;
                        if (length == limit) {
                            break;
                        }
                    }
                    candidate = previous[static_cast<size_t>(candidate) & (windowSize - 1U)];
                }
            }
            if (bestLength >= 3U) {
                putMatch(bits, bestLength, bestDistance);
                for (uint32_t step = 0U; step < bestLength; ++step) {
                    insert(pos + step);
                }
                pos += bestLength;
            } else {
                putSymbol(bits, aRaw[pos]);
                insert(pos);
                ++pos;
            }
        }
        putSymbol(bits, 256U);  // end of block
        bits.flush();
    }

    struct CrcTable {
        uint32_t values[256]{};
        CrcTable() {
            for (uint32_t idx = 0U; idx < 256U; ++idx) {
                uint32_t crc = idx;
                for (uint32_t bit = 0U; bit < 8U; ++bit) {
                    crc = ((crc & 1U) != 0U) ? (0xEDB88320U ^ (crc >> 1U)) : (crc >> 1U);
                }
                values[idx] = crc;
            }
        }
    };

    static uint32_t updateCrc32(uint32_t aCrc, const uint8_t* apData, size_t aSize) {
        static const CrcTable sTable;
        uint32_t crc = aCrc;
        for (size_t idx = 0U; idx < aSize; ++idx) {
            crc = sTable.values[(crc ^ apData[idx]) & 0xFFU] ^ (crc >> 8U);
        }
        return crc;
    }

    static void appendBigEndian(std::vector<uint8_t>& aoOut, uint32_t aValue) {
        aoOut.push_back(static_cast<uint8_t>((aValue >> 24U) & 0xFFU));
        aoOut.push_back(static_cast<uint8_t>((aValue >> 16U) & 0xFFU));
        aoOut.push_back(static_cast<uint8_t>((aValue >> 8U) & 0xFFU));
        aoOut.push_back(static_cast<uint8_t>(aValue & 0xFFU));
    }

    static void appendChunk(std::vector<uint8_t>& aoPng, const char* aType, const std::vector<uint8_t>& aData) {
        appendBigEndian(aoPng, static_cast<uint32_t>(aData.size()));
        const size_t typePos = aoPng.size();
        for (size_t idx = 0U; idx < 4U; ++idx) {
            aoPng.push_back(static_cast<uint8_t>(aType[idx]));
        }
        aoPng.insert(aoPng.end(), aData.begin(), aData.end());
        // The CRC covers the type and the data, not the length.
        appendBigEndian(aoPng, crc32(aoPng.data() + typePos, aoPng.size() - typePos));
    }
};

}  // namespace mcp
}  // namespace ez
