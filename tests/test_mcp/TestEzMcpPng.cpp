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

// A reader independent of the writer : walks the chunks and checks every
// CRC, checks the header, inflates the stored blocks (LEN against NLEN),
// checks the Adler-32, then removes the filter bytes.
static bool decodeStoredPng(const std::vector<uint8_t>& aPng, uint32_t& aoWidth, uint32_t& aoHeight, std::vector<uint8_t>& aoRgba) {
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
    bool last = false;
    while (!last) {
        if ((zpos + 5U) > zlib.size()) {
            return decodeFailed("truncated block");
        }
        last = (zlib[zpos] & 1U) != 0U;
        if ((zlib[zpos] & 6U) != 0U) {
            return decodeFailed("not a stored block");
        }
        const uint32_t len = zlib[zpos + 1U] | (static_cast<uint32_t>(zlib[zpos + 2U]) << 8U);
        const uint32_t nlen = zlib[zpos + 3U] | (static_cast<uint32_t>(zlib[zpos + 4U]) << 8U);
        if ((len ^ 0xFFFFU) != nlen) {
            return decodeFailed("NLEN");
        }
        zpos += 5U;
        if ((zpos + len) > zlib.size()) {
            return decodeFailed("block length");
        }
        raw.insert(raw.end(), zlib.begin() + static_cast<std::ptrdiff_t>(zpos), zlib.begin() + static_cast<std::ptrdiff_t>(zpos + len));
        zpos += len;
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
    CTEST_ASSERT_MESSAGE(decodeStoredPng(png, width, height, decoded), "the file reads back");
    CTEST_ASSERT((width == 1U) && (height == 1U));
    CTEST_ASSERT(decoded == rgba);
    return true;
}

bool TestEzMcpPng_MultiBlock() {
    // 200 x 100 : 80100 raw bytes, two stored blocks.
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
    CTEST_ASSERT_MESSAGE(decodeStoredPng(png, readWidth, readHeight, decoded), "the file reads back");
    CTEST_ASSERT((readWidth == width) && (readHeight == height));
    CTEST_ASSERT_MESSAGE(decoded == rgba, "every pixel, rows top to bottom");
    // Exactly 65535 raw bytes : one full block, the case of the boundary.
    const uint32_t boundaryWidth = 5461U;  // (5461 * 4 + 1) * 3 = 65535
    const std::vector<uint8_t> boundary(static_cast<size_t>(boundaryWidth) * 3U * 4U, 0x5AU);
    CTEST_ASSERT(decodeStoredPng(Png::encodeRgba(boundary, boundaryWidth, 3U), readWidth, readHeight, decoded) && (decoded == boundary));
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpPng(const std::string& vTest) {
    IfTestExist(TestEzMcpPng_Checksums);
    else IfTestExist(TestEzMcpPng_Refusals);
    else IfTestExist(TestEzMcpPng_OnePixel);
    else IfTestExist(TestEzMcpPng_MultiBlock);
    return false;
}
