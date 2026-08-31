#include <ezlibs/ezBinToSrc.hpp>
#include <ezlibs/ezCTest.hpp>

#include <cstdint>
#include <string>
#include <vector>

////////////////////////////////////////////////////////////////////////////
//// THE REFERENCE READER //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// the INDEPENDENT judge : a faithful port of the stb_decompress and
// Decode85 that Dear ImGui embeds (imgui_draw.cpp). the writer under
// test must satisfy the READER its output claims compatibility with —
// a writer judged by its own reader proves nothing.

namespace refReader {

static uint32_t decompressLength(const std::vector<uint8_t>& aStream) {
    return (static_cast<uint32_t>(aStream[8]) << 24) + (static_cast<uint32_t>(aStream[9]) << 16) + (static_cast<uint32_t>(aStream[10]) << 8) + aStream[11];
}

struct Reader {
    const uint8_t* barrierInBegin{nullptr};
    uint8_t* barrierOutBegin{nullptr};
    uint8_t* barrierOutEnd{nullptr};
    uint8_t* dout{nullptr};

    void match(const uint8_t* apData, uint32_t aLength) {
        if (dout + aLength > barrierOutEnd) {
            dout += aLength;
            return;
        }
        if (apData < barrierOutBegin) {
            dout = barrierOutEnd + 1;
            return;
        }
        while (aLength-- != 0u) {
            *dout++ = *apData++;
        }
    }
    void literal(const uint8_t* apData, uint32_t aLength) {
        if (dout + aLength > barrierOutEnd) {
            dout += aLength;
            return;
        }
        if (apData < barrierInBegin) {
            dout = barrierOutEnd + 1;
            return;
        }
        for (uint32_t idx = 0u; idx < aLength; ++idx) {
            dout[idx] = apData[idx];
        }
        dout += aLength;
    }
    static uint32_t in2(const uint8_t* apAt) { return (static_cast<uint32_t>(apAt[0]) << 8) + apAt[1]; }
    static uint32_t in3(const uint8_t* apAt) { return (static_cast<uint32_t>(apAt[0]) << 16) + in2(apAt + 1); }
    static uint32_t in4(const uint8_t* apAt) { return (static_cast<uint32_t>(apAt[0]) << 24) + in3(apAt + 1); }

    const uint8_t* token(const uint8_t* apToken) {
        const uint8_t* cursor = apToken;
        if (*cursor >= 0x20) {
            if (*cursor >= 0x80) {
                match(dout - cursor[1] - 1, cursor[0] - 0x80u + 1u);
                cursor += 2;
            } else if (*cursor >= 0x40) {
                match(dout - (in2(cursor) - 0x4000u + 1u), cursor[2] + 1u);
                cursor += 3;
            } else {
                literal(cursor + 1, cursor[0] - 0x20u + 1u);
                cursor += 1 + (cursor[0] - 0x20 + 1);
            }
        } else {
            if (*cursor >= 0x18) {
                match(dout - (in3(cursor) - 0x180000u + 1u), cursor[3] + 1u);
                cursor += 4;
            } else if (*cursor >= 0x10) {
                match(dout - (in3(cursor) - 0x100000u + 1u), in2(cursor + 3) + 1u);
                cursor += 5;
            } else if (*cursor >= 0x08) {
                literal(cursor + 2, in2(cursor) - 0x0800u + 1u);
                cursor += 2 + (in2(cursor) - 0x0800 + 1);
            } else if (*cursor == 0x07) {
                literal(cursor + 3, in2(cursor + 1) + 1u);
                cursor += 3 + (in2(cursor + 1) + 1);
            } else if (*cursor == 0x06) {
                match(dout - (in3(cursor + 1) + 1u), cursor[4] + 1u);
                cursor += 5;
            } else if (*cursor == 0x04) {
                match(dout - (in3(cursor + 1) + 1u), in2(cursor + 4) + 1u);
                cursor += 6;
            }
        }
        return cursor;
    }
};

static uint32_t adler32(uint32_t aSeed, const uint8_t* apBuffer, uint32_t aLength) {
    const uint32_t adlerModulo = 65521u;
    uint32_t sumLow = aSeed & 0xffffu;
    uint32_t sumHigh = aSeed >> 16u;
    uint32_t blockLength = aLength % 5552u;
    while (aLength != 0u) {
        for (uint32_t idx = 0u; idx < blockLength; ++idx) {
            sumLow += apBuffer[idx];
            sumHigh += sumLow;
        }
        apBuffer += blockLength;
        sumLow %= adlerModulo;
        sumHigh %= adlerModulo;
        aLength -= blockLength;
        blockLength = 5552u;
    }
    return (sumHigh << 16u) + sumLow;
}

// returns the decompressed length, or 0 on any stream error
static uint32_t decompress(std::vector<uint8_t>& aoOutput, const std::vector<uint8_t>& aStream) {
    const uint8_t* input = aStream.data();
    if (Reader::in4(input) != 0x57bC0000u) {
        return 0u;
    }
    if (Reader::in4(input + 4) != 0u) {
        return 0u;
    }
    const auto outputLength = decompressLength(aStream);
    aoOutput.assign(static_cast<std::size_t>(outputLength) + 1u, 0u);
    Reader reader;
    reader.barrierInBegin = input;
    reader.barrierOutBegin = aoOutput.data();
    reader.barrierOutEnd = aoOutput.data() + outputLength;
    reader.dout = aoOutput.data();
    input += 16;
    for (;;) {
        const uint8_t* oldInput = input;
        input = reader.token(input);
        if (input == oldInput) {
            if ((*input == 0x05) && (input[1] == 0xfa)) {
                if (reader.dout != aoOutput.data() + outputLength) {
                    return 0u;
                }
                if (adler32(1u, aoOutput.data(), outputLength) != Reader::in4(input + 2)) {
                    return 0u;
                }
                aoOutput.resize(outputLength);
                return outputLength;
            }
            return 0u;
        }
        if (reader.dout > aoOutput.data() + outputLength) {
            return 0u;
        }
    }
}

static uint32_t decode85Byte(char aChar) {
    return (aChar >= '\\') ? static_cast<uint32_t>(aChar) - 36u : static_cast<uint32_t>(aChar) - 35u;
}

// the imgui Decode85 layout : 5 chars -> one 32-bit group, bytes out
// in little-endian order
static std::vector<uint8_t> decode85(const std::string& aBase85) {
    std::vector<uint8_t> result;
    result.reserve((aBase85.size() / 5u) * 4u);
    for (std::size_t idx = 0u; idx + 4u < aBase85.size() + 1u; idx += 5u) {
        uint32_t packed = decode85Byte(aBase85[idx]) + 85u * (decode85Byte(aBase85[idx + 1u]) + 85u * (decode85Byte(aBase85[idx + 2u]) + 85u * (decode85Byte(aBase85[idx + 3u]) + 85u * decode85Byte(aBase85[idx + 4u]))));
        result.push_back(static_cast<uint8_t>(packed & 0xffu));
        result.push_back(static_cast<uint8_t>((packed >> 8u) & 0xffu));
        result.push_back(static_cast<uint8_t>((packed >> 16u) & 0xffu));
        result.push_back(static_cast<uint8_t>((packed >> 24u) & 0xffu));
    }
    return result;
}

}  // namespace refReader

////////////////////////////////////////////////////////////////////////////
//// FIXTURES //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// a deterministic blob mixing repetition (matches) and lcg noise
// (literals) : both compressor paths get exercised
static std::vector<uint8_t> buildMixedBytes(std::size_t aRepeatedCount, std::size_t aNoiseCount) {
    std::vector<uint8_t> bytes;
    bytes.reserve(aRepeatedCount + aNoiseCount);
    const std::string motif = "the repeated motif feeds the match ladder ";
    for (std::size_t idx = 0u; idx < aRepeatedCount; ++idx) {
        bytes.push_back(static_cast<uint8_t>(motif[idx % motif.size()]));
    }
    uint32_t lcgState = 0x12345678u;
    for (std::size_t idx = 0u; idx < aNoiseCount; ++idx) {
        lcgState = lcgState * 1664525u + 1013904223u;
        bytes.push_back(static_cast<uint8_t>(lcgState >> 24u));
    }
    return bytes;
}

// re-read the characters of an emitted string literal : keep what sits
// between quotes, and unescape the '\?' the emitter had to write
static std::string parseStringLiteral(const std::string& aSource) {
    std::string content;
    bool inString = false;
    for (std::size_t idx = 0u; idx < aSource.size(); ++idx) {
        const char current = aSource[idx];
        if (current == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            if ((current == '\\') && (idx + 1u < aSource.size())) {
                content += aSource[idx + 1u];
                ++idx;
                continue;
            }
            content += current;
        }
    }
    return content;
}

////////////////////////////////////////////////////////////////////////////
//// THE LAWS //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzBinToSrc_compressRoundTripsThroughTheReferenceReader() {
    const auto bytes = buildMixedBytes(2000u, 8000u);
    const auto stream = ez::binToSrc::compress(bytes);
    CTEST_ASSERT(stream.size() > 22u);
    CTEST_ASSERT(stream[0] == 0x57u);
    CTEST_ASSERT(stream[1] == 0xbcu);
    CTEST_ASSERT(refReader::decompressLength(stream) == bytes.size());
    std::vector<uint8_t> decoded;
    CTEST_ASSERT(refReader::decompress(decoded, stream) == bytes.size());
    CTEST_ASSERT(decoded == bytes);
    // repetition must actually compress : the stream may not be a
    // literal dump of the input
    CTEST_ASSERT(stream.size() < bytes.size());
    return true;
}

bool TestEzBinToSrc_compressHandlesTheTinyAndEmptyInputs() {
    // empty : the bare stream (16 header + 2 end + 4 adler), length 0
    const auto emptyStream = ez::binToSrc::compress(std::vector<uint8_t>());
    CTEST_ASSERT(emptyStream.size() == 22u);
    CTEST_ASSERT(refReader::decompressLength(emptyStream) == 0u);
    // one byte : pure literal path
    std::vector<uint8_t> oneByte;
    oneByte.push_back(0x42u);
    const auto oneStream = ez::binToSrc::compress(oneByte);
    std::vector<uint8_t> decodedOne;
    CTEST_ASSERT(refReader::decompress(decodedOne, oneStream) == 1u);
    CTEST_ASSERT(decodedOne == oneByte);
    // thirteen bytes : below the 12-byte probe guard, still all literals
    const auto thirteen = buildMixedBytes(13u, 0u);
    const auto thirteenStream = ez::binToSrc::compress(thirteen);
    std::vector<uint8_t> decodedThirteen;
    CTEST_ASSERT(refReader::decompress(decodedThirteen, thirteenStream) == 13u);
    CTEST_ASSERT(decodedThirteen == thirteen);
    return true;
}

bool TestEzBinToSrc_base85SpeaksTheImguiAlphabet() {
    // known vectors : a zero group is five '#' (code 35), a one is '$'
    std::vector<uint8_t> zeroGroup(4u, 0u);
    CTEST_ASSERT(ez::binToSrc::toBase85(zeroGroup) == "#####");
    std::vector<uint8_t> oneGroup(4u, 0u);
    oneGroup[0] = 1u;
    CTEST_ASSERT(ez::binToSrc::toBase85(oneGroup) == "$####");
    // length law : any started 4-byte group yields 5 characters
    std::vector<uint8_t> oneByte(1u, 0xffu);
    CTEST_ASSERT(ez::binToSrc::toBase85(oneByte).size() == 5u);
    CTEST_ASSERT(ez::binToSrc::toBase85(buildMixedBytes(0u, 5u)).size() == 10u);
    // alphabet law : always literal-safe, never a backslash or a quote
    const auto noise = ez::binToSrc::toBase85(buildMixedBytes(0u, 4000u));
    for (std::size_t idx = 0u; idx < noise.size(); ++idx) {
        const char current = noise[idx];
        CTEST_ASSERT(current >= 35);
        CTEST_ASSERT(current <= 120);
        CTEST_ASSERT(current != '\\');
        CTEST_ASSERT(current != '"');
    }
    // the reference Decode85 reads back the exact bytes
    const auto bytes = buildMixedBytes(0u, 400u);
    CTEST_ASSERT(refReader::decode85(ez::binToSrc::toBase85(bytes)) == bytes);
    return true;
}

bool TestEzBinToSrc_formatEmitsAConsumableStringLiteral() {
    // the full chain a consumer walks : parse the literal, decode85,
    // stb-decompress — the exact AddFontFromMemoryCompressedBase85TTF
    // path — and the original bytes must come back identical
    const auto bytes = buildMixedBytes(1500u, 2500u);
    const auto base85 = ez::binToSrc::toBase85(ez::binToSrc::compress(bytes));
    const auto emitted = ez::binToSrc::formatCompressedBase85Array("test_compressed_data_base85", base85);
    const std::string expectedHead = "static const char test_compressed_data_base85[" + std::to_string(base85.size()) + "+1] =";
    CTEST_ASSERT(emitted.find(expectedHead) == 0u);
    CTEST_ASSERT(emitted.find("\";\n") != std::string::npos);
    const auto parsed = parseStringLiteral(emitted);
    CTEST_ASSERT(parsed == base85);
    std::vector<uint8_t> decoded;
    const auto packed = refReader::decode85(parsed);
    CTEST_ASSERT(refReader::decompress(decoded, packed) == bytes.size());
    CTEST_ASSERT(decoded == bytes);
    return true;
}

bool TestEzBinToSrc_formatEscapesTheTrigraphTwins() {
    // 2408 = 28 + 28*85 : its group opens with two '?' — the pair an
    // old compiler would read as a trigraph, so the emitter must break
    // it with a backslash, and the parse must still read '??' back
    std::vector<uint8_t> bytes(4u, 0u);
    bytes[0] = 0x68u;
    bytes[1] = 0x09u;
    const auto base85 = ez::binToSrc::toBase85(bytes);
    CTEST_ASSERT(base85 == "??###");
    const auto emitted = ez::binToSrc::formatCompressedBase85Array("twin", base85);
    CTEST_ASSERT(emitted.find("?\\?") != std::string::npos);
    CTEST_ASSERT(parseStringLiteral(emitted) == base85);
    return true;
}

bool TestEzBinToSrc_formatSwitchesToAByteListWhenHuge() {
    // compilers cap string literals at 65535 characters : at that size
    // the emission becomes a hex list of the same characters, still
    // declared [N+1] so the terminating zero survives
    const auto base85 = ez::binToSrc::toBase85(buildMixedBytes(0u, 52432u));
    CTEST_ASSERT(base85.size() == 65540u);
    const auto emitted = ez::binToSrc::formatCompressedBase85Array("huge", base85);
    CTEST_ASSERT(emitted.find("static const char huge[65540+1] = {") == 0u);
    CTEST_ASSERT(emitted.rfind("};\n") == emitted.size() - 3u);
    CTEST_ASSERT(emitted.find('"') == std::string::npos);
    // read the hex items back : the characters must be all there
    std::string readBack;
    readBack.reserve(base85.size());
    for (std::size_t idx = emitted.find('{'); idx + 3u < emitted.size(); ++idx) {
        if ((emitted[idx] == '0') && (emitted[idx + 1u] == 'x')) {
            const auto hexValue = std::stoul(emitted.substr(idx + 2u, 2u), nullptr, 16);
            readBack += static_cast<char>(hexValue);
            idx += 3u;
        }
    }
    CTEST_ASSERT(readBack == base85);
    return true;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzBinToSrc(const std::string& vTest) {
    IfTestExist(TestEzBinToSrc_compressRoundTripsThroughTheReferenceReader);
    else IfTestExist(TestEzBinToSrc_compressHandlesTheTinyAndEmptyInputs);
    else IfTestExist(TestEzBinToSrc_base85SpeaksTheImguiAlphabet);
    else IfTestExist(TestEzBinToSrc_formatEmitsAConsumableStringLiteral);
    else IfTestExist(TestEzBinToSrc_formatEscapesTheTrigraphTwins);
    else IfTestExist(TestEzBinToSrc_formatSwitchesToAByteListWhenHuge);
    return false;
}
