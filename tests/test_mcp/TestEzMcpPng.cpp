#include <TestEzMcpPng.h>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMcp/png.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using ez::mcp::Png;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

static uint32_t readBigEndian(const std::vector<uint8_t>& aBytes, size_t aPos) {
    return (static_cast<uint32_t>(aBytes[aPos]) << 24U) | (static_cast<uint32_t>(aBytes[aPos + 1U]) << 16U) |
        (static_cast<uint32_t>(aBytes[aPos + 2U]) << 8U) | static_cast<uint32_t>(aBytes[aPos + 3U]);
}

static bool decodeFailed(const char* aWhy) {
    printf("png decode : %s\n", aWhy);
    return false;
}

// An inflater independent of the writer (RFC 1951) : stored blocks (LEN
// against NLEN) and fixed Huffman blocks ; a dynamic block is refused, the
// writer never makes one. aoPos ends on the byte after the last block.
struct BitReader {
    const std::vector<uint8_t>& bytes;
    size_t pos;
    uint32_t bit{};
    BitReader(const std::vector<uint8_t>& aBytes, size_t aPos) : bytes(aBytes), pos(aPos) {
    }
    bool get(uint32_t aCount, uint32_t& aoValue) {
        aoValue = 0U;
        for (uint32_t idx = 0U; idx < aCount; ++idx) {
            if (pos >= bytes.size()) {
                return false;
            }
            aoValue |= static_cast<uint32_t>((bytes[pos] >> bit) & 1U) << idx;
            if (++bit == 8U) {
                bit = 0U;
                ++pos;
            }
        }
        return true;
    }
    // a Huffman code : its bits come most significant first
    bool code(uint32_t aCount, uint32_t& aoCode) {
        for (uint32_t idx = 0U; idx < aCount; ++idx) {
            uint32_t one = 0U;
            if (!get(1U, one)) {
                return false;
            }
            aoCode = (aoCode << 1U) | one;
        }
        return true;
    }
};

static bool fixedSymbol(BitReader& aoBits, uint32_t& aoSymbol) {
    uint32_t code = 0U;
    if (!aoBits.code(7U, code)) {
        return false;
    }
    if (code <= 0x17U) {
        aoSymbol = 256U + code;
        return true;
    }
    if (!aoBits.code(1U, code)) {
        return false;
    }
    if ((code >= 0x30U) && (code <= 0xBFU)) {
        aoSymbol = code - 0x30U;
        return true;
    }
    if ((code >= 0xC0U) && (code <= 0xC7U)) {
        aoSymbol = 280U + (code - 0xC0U);
        return true;
    }
    if (!aoBits.code(1U, code)) {
        return false;
    }
    if ((code >= 0x190U) && (code <= 0x1FFU)) {
        aoSymbol = 144U + (code - 0x190U);
        return true;
    }
    return false;
}

static bool inflate(const std::vector<uint8_t>& aZlib, size_t& aoPos, std::vector<uint8_t>& aoRaw) {
    static const uint32_t sLengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195,
        227, 258};
    static const uint32_t sLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint32_t sDistanceBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
        4097, 6145, 8193, 12289, 16385, 24577};
    static const uint32_t sDistanceExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    BitReader bits(aZlib, aoPos);
    uint32_t last = 0U;
    while (last == 0U) {
        uint32_t type = 0U;
        if (!bits.get(1U, last) || !bits.get(2U, type)) {
            return decodeFailed("truncated block header");
        }
        if (type == 0U) {
            if (bits.bit != 0U) {
                bits.bit = 0U;
                ++bits.pos;
            }
            if ((bits.pos + 4U) > aZlib.size()) {
                return decodeFailed("truncated stored block");
            }
            const uint32_t len = aZlib[bits.pos] | (static_cast<uint32_t>(aZlib[bits.pos + 1U]) << 8U);
            const uint32_t nlen = aZlib[bits.pos + 2U] | (static_cast<uint32_t>(aZlib[bits.pos + 3U]) << 8U);
            if ((len ^ 0xFFFFU) != nlen) {
                return decodeFailed("NLEN");
            }
            bits.pos += 4U;
            if ((bits.pos + len) > aZlib.size()) {
                return decodeFailed("stored block length");
            }
            aoRaw.insert(aoRaw.end(), aZlib.begin() + static_cast<std::ptrdiff_t>(bits.pos),
                aZlib.begin() + static_cast<std::ptrdiff_t>(bits.pos + len));
            bits.pos += len;
            continue;
        }
        if (type != 1U) {
            return decodeFailed("not a stored nor a fixed Huffman block");
        }
        while (true) {
            uint32_t symbol = 0U;
            if (!fixedSymbol(bits, symbol)) {
                return decodeFailed("bad literal/length code");
            }
            if (symbol < 256U) {
                aoRaw.push_back(static_cast<uint8_t>(symbol));
                continue;
            }
            if (symbol == 256U) {
                break;
            }
            if (symbol > 285U) {
                return decodeFailed("bad length symbol");
            }
            uint32_t extra = 0U;
            if (!bits.get(sLengthExtra[symbol - 257U], extra)) {
                return decodeFailed("truncated length");
            }
            const uint32_t length = sLengthBase[symbol - 257U] + extra;
            uint32_t distanceCode = 0U;
            if (!bits.code(5U, distanceCode) || (distanceCode > 29U) || !bits.get(sDistanceExtra[distanceCode], extra)) {
                return decodeFailed("bad distance");
            }
            const uint32_t distance = sDistanceBase[distanceCode] + extra;
            if (distance > aoRaw.size()) {
                return decodeFailed("distance before the start");
            }
            for (uint32_t idx = 0U; idx < length; ++idx) {
                aoRaw.push_back(aoRaw[aoRaw.size() - distance]);
            }
        }
    }
    aoPos = (bits.bit != 0U) ? (bits.pos + 1U) : bits.pos;
    return true;
}

// A reader independent of the writer : walks the chunks and checks every
// CRC, checks the header, inflates the data, checks the Adler-32, then
// removes the filter bytes.
static bool decodePng(const std::vector<uint8_t>& aPng, uint32_t& aoWidth, uint32_t& aoHeight, std::vector<uint8_t>& aoRgba) {
    static const uint8_t sSignature[8] = {0x89U, 'P', 'N', 'G', 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    if ((aPng.size() < 8U) || (std::memcmp(aPng.data(), sSignature, 8U) != 0)) {
        return decodeFailed("signature");
    }
    std::vector<uint8_t> zlib;
    std::vector<std::string> order;
    size_t pos = 8U;
    while (pos < aPng.size()) {
        if ((pos + 12U) > aPng.size()) {
            return decodeFailed("truncated chunk");
        }
        const uint32_t length = readBigEndian(aPng, pos);
        if ((pos + 12U + length) > aPng.size()) {
            return decodeFailed("chunk length");
        }
        const std::string type(reinterpret_cast<const char*>(aPng.data() + pos + 4U), 4U);
        if (Png::crc32(aPng.data() + pos + 4U, 4U + length) != readBigEndian(aPng, pos + 8U + length)) {
            return decodeFailed("crc");
        }
        order.push_back(type);
        if (type == "IHDR") {
            if (length != 13U) {
                return decodeFailed("IHDR length");
            }
            aoWidth = readBigEndian(aPng, pos + 8U);
            aoHeight = readBigEndian(aPng, pos + 12U);
            const uint8_t* pRest = aPng.data() + pos + 16U;
            if ((pRest[0] != 8U) || (pRest[1] != 6U) || (pRest[2] != 0U) || (pRest[3] != 0U) || (pRest[4] != 0U)) {
                return decodeFailed("IHDR fields");
            }
        } else if (type == "IDAT") {
            zlib.insert(zlib.end(), aPng.begin() + static_cast<std::ptrdiff_t>(pos + 8U),
                aPng.begin() + static_cast<std::ptrdiff_t>(pos + 8U + length));
        }
        pos += 12U + length;
    }
    if ((order.size() != 3U) || (order[0] != "IHDR") || (order[1] != "IDAT") || (order[2] != "IEND")) {
        return decodeFailed("chunk order");
    }
    if ((zlib.size() < 6U) || ((zlib[0] & 0x0FU) != 8U) || ((((static_cast<uint32_t>(zlib[0]) << 8U) | zlib[1]) % 31U) != 0U)) {
        return decodeFailed("zlib header");
    }
    std::vector<uint8_t> raw;
    size_t zpos = 2U;
    if (!inflate(zlib, zpos, raw)) {
        return false;
    }
    if ((zpos + 4U) != zlib.size()) {
        return decodeFailed("zlib trailer");
    }
    if (Png::adler32(raw.data(), raw.size()) != readBigEndian(zlib, zpos)) {
        return decodeFailed("adler32");
    }
    const size_t rowBytes = static_cast<size_t>(aoWidth) * 4U;
    if (raw.size() != ((rowBytes + 1U) * aoHeight)) {
        return decodeFailed("raw size");
    }
    aoRgba.clear();
    for (uint32_t row = 0U; row < aoHeight; ++row) {
        const size_t begin = row * (rowBytes + 1U);
        if (raw[begin] != 0U) {
            return decodeFailed("filter byte");
        }
        aoRgba.insert(aoRgba.end(), raw.begin() + static_cast<std::ptrdiff_t>(begin + 1U),
            raw.begin() + static_cast<std::ptrdiff_t>(begin + 1U + rowBytes));
    }
    return true;
}

bool TestEzMcpPng_Checksums() {
    const char* const digits = "123456789";
    CTEST_ASSERT_MESSAGE(Png::crc32(reinterpret_cast<const uint8_t*>(digits), 9U) == 0xCBF43926U, "the CRC-32 check value");
    CTEST_ASSERT(Png::crc32(reinterpret_cast<const uint8_t*>("IEND"), 4U) == 0xAE426082U);
    CTEST_ASSERT(Png::crc32(nullptr, 0U) == 0U);
    CTEST_ASSERT_MESSAGE(Png::adler32(reinterpret_cast<const uint8_t*>("Wikipedia"), 9U) == 0x11E60398U, "the Adler-32 example");
    CTEST_ASSERT(Png::adler32(nullptr, 0U) == 1U);
    std::vector<uint8_t> big(100000U, 0xFFU);
    uint32_t low = 1U;
    uint32_t high = 0U;
    for (size_t idx = 0U; idx < big.size(); ++idx) {
        low = (low + big[idx]) % 65521U;
        high = (high + low) % 65521U;
    }
    CTEST_ASSERT_MESSAGE(Png::adler32(big.data(), big.size()) == ((high << 16U) | low), "the modulo holds on a long run of 0xFF");
    return true;
}

bool TestEzMcpPng_Refusals() {
    CTEST_ASSERT(Png::encodeRgba(std::vector<uint8_t>(), 0U, 0U).empty());
    CTEST_ASSERT(Png::encodeRgba(std::vector<uint8_t>(4U, 0U), 0U, 1U).empty());
    CTEST_ASSERT(Png::encodeRgba(std::vector<uint8_t>(4U, 0U), 1U, 0U).empty());
    CTEST_ASSERT_MESSAGE(Png::encodeRgba(std::vector<uint8_t>(3U, 0U), 1U, 1U).empty(), "pixels missing");
    CTEST_ASSERT_MESSAGE(Png::encodeRgba(std::vector<uint8_t>(8U, 0U), 1U, 1U).empty(), "pixels too many");
    CTEST_ASSERT_MESSAGE(Png::encodeRgba(std::vector<uint8_t>(4U, 0U), 0x10000U, 0x10000U).empty(),
        "a size beyond one chunk is refused before any allocation");
    return true;
}

bool TestEzMcpPng_OnePixel() {
    const uint8_t pixel[] = {10U, 20U, 30U, 40U};
    const std::vector<uint8_t> rgba(pixel, pixel + 4);
    const std::vector<uint8_t> png = Png::encodeRgba(rgba, 1U, 1U);
    CTEST_ASSERT(!png.empty());
    uint32_t width = 0U;
    uint32_t height = 0U;
    std::vector<uint8_t> decoded;
    CTEST_ASSERT_MESSAGE(decodePng(png, width, height, decoded), "the file reads back");
    CTEST_ASSERT((width == 1U) && (height == 1U));
    CTEST_ASSERT(decoded == rgba);
    return true;
}

bool TestEzMcpPng_LargeImages() {
    // 200 x 100 : 80100 raw bytes of a pattern that repeats little
    const uint32_t width = 200U;
    const uint32_t height = 100U;
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4U);
    for (size_t idx = 0U; idx < rgba.size(); ++idx) {
        rgba[idx] = static_cast<uint8_t>((idx * 7U) + (idx / 13U));
    }
    const std::vector<uint8_t> png = Png::encodeRgba(rgba, width, height);
    uint32_t readWidth = 0U;
    uint32_t readHeight = 0U;
    std::vector<uint8_t> decoded;
    CTEST_ASSERT_MESSAGE(decodePng(png, readWidth, readHeight, decoded), "the file reads back");
    CTEST_ASSERT((readWidth == width) && (readHeight == height));
    CTEST_ASSERT_MESSAGE(decoded == rgba, "every pixel, rows top to bottom");
    // a flat image : one long run of matches
    const uint32_t boundaryWidth = 5461U;  // (5461 * 4 + 1) * 3 = 65535
    const std::vector<uint8_t> boundary(static_cast<size_t>(boundaryWidth) * 3U * 4U, 0x5AU);
    CTEST_ASSERT(decodePng(Png::encodeRgba(boundary, boundaryWidth, 3U), readWidth, readHeight, decoded) && (decoded == boundary));
    return true;
}

// a ui-like image : flat areas, framed rects, a strip of text-like noise.
// it shrinks many times, and reads back to the byte
bool TestEzMcpPng_CompressesAUi() {
    const uint32_t width = 640U;
    const uint32_t height = 480U;
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4U);
    uint32_t seed = 12345U;
    for (uint32_t row = 0U; row < height; ++row) {
        for (uint32_t col = 0U; col < width; ++col) {
            uint8_t* pPixel = &rgba[((static_cast<size_t>(row) * width) + col) * 4U];
            uint8_t shade = 40U;  // the background
            if ((col >= 60U) && (col < 300U) && (row >= 60U) && (row < 260U)) {
                shade = ((row == 60U) || (row == 259U) || (col == 60U) || (col == 299U)) ? 200U : 70U;  // a window and its frame
            }
            if ((row >= 100U) && (row < 108U) && (col >= 80U) && (col < 280U)) {
                seed = (seed * 1103515245U) + 12345U;
                shade = ((seed >> 16U) & 1U) ? 230U : 70U;  // a line of text
            }
            pPixel[0] = shade;
            pPixel[1] = static_cast<uint8_t>(shade + 10U);
            pPixel[2] = static_cast<uint8_t>(shade + 20U);
            pPixel[3] = 255U;
        }
    }
    const std::vector<uint8_t> png = Png::encodeRgba(rgba, width, height);
    uint32_t readWidth = 0U;
    uint32_t readHeight = 0U;
    std::vector<uint8_t> decoded;
    CTEST_ASSERT_MESSAGE(decodePng(png, readWidth, readHeight, decoded), "the file reads back");
    CTEST_ASSERT_MESSAGE(decoded == rgba, "to the byte");
    printf("ui 640x480 : %u bytes raw, %u bytes of PNG\n", static_cast<unsigned>(rgba.size()), static_cast<unsigned>(png.size()));
    CTEST_ASSERT_MESSAGE((png.size() * 20U) < rgba.size(), "a ui shrinks at least 20 times");
    return true;
}

// noise does not compress : it still reads back to the byte, barely bigger
bool TestEzMcpPng_NoiseReadsBack() {
    const uint32_t width = 64U;
    const uint32_t height = 64U;
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4U);
    uint32_t seed = 777U;
    for (auto& byte : rgba) {
        seed = (seed * 1103515245U) + 12345U;
        byte = static_cast<uint8_t>(seed >> 16U);
    }
    const std::vector<uint8_t> png = Png::encodeRgba(rgba, width, height);
    uint32_t readWidth = 0U;
    uint32_t readHeight = 0U;
    std::vector<uint8_t> decoded;
    CTEST_ASSERT(decodePng(png, readWidth, readHeight, decoded));
    CTEST_ASSERT(decoded == rgba);
    CTEST_ASSERT_MESSAGE(png.size() < ((rgba.size() * 115U) / 100U), "fixed codes cost at most 9 bits a byte, plus the framing");
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpPng(const std::string& vTest) {
    IfTestExist(TestEzMcpPng_Checksums);
    else IfTestExist(TestEzMcpPng_Refusals);
    else IfTestExist(TestEzMcpPng_OnePixel);
    else IfTestExist(TestEzMcpPng_LargeImages);
    else IfTestExist(TestEzMcpPng_CompressesAUi);
    else IfTestExist(TestEzMcpPng_NoiseReadsBack);
    return false;
}
