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
// packed. The image data is stored without compression (deflate "stored"
// blocks) : a capture is encoded fast and read by any PNG reader.

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
        const size_t maxStoredBlock = 65535U;
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

        // The zlib stream : header, stored blocks, adler32 of the raw data.
        std::vector<uint8_t> zlib;
        zlib.reserve(raw.size() + (((raw.size() / maxStoredBlock) + 1U) * 5U) + 6U);
        zlib.push_back(0x78U);  // deflate, 32K window
        zlib.push_back(0x01U);  // no preset dictionary, check bits
        size_t pos = 0U;
        do {
            const size_t len = ((raw.size() - pos) > maxStoredBlock) ? maxStoredBlock : (raw.size() - pos);
            const bool last = (pos + len) == raw.size();
            zlib.push_back(last ? 1U : 0U);  // BFINAL, BTYPE 00
            zlib.push_back(static_cast<uint8_t>(len & 0xFFU));
            zlib.push_back(static_cast<uint8_t>((len >> 8U) & 0xFFU));
            zlib.push_back(static_cast<uint8_t>(~len & 0xFFU));
            zlib.push_back(static_cast<uint8_t>((~len >> 8U) & 0xFFU));
            zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
            pos += len;
        } while (pos < raw.size());
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
