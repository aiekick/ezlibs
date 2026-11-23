#include <TestEzMcpBase64.h>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMcp/base64.hpp>

#include <cstdint>
#include <vector>

using ez::mcp::Base64;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

static std::string encodeText(const std::string& aText) {
    return Base64::encode(std::vector<uint8_t>(aText.begin(), aText.end()));
}

// The test vectors of RFC 4648, section 10 : every padding case.
bool TestEzMcpBase64_Rfc4648() {
    CTEST_ASSERT(encodeText("").empty());
    CTEST_ASSERT(encodeText("f") == "Zg==");
    CTEST_ASSERT(encodeText("fo") == "Zm8=");
    CTEST_ASSERT(encodeText("foo") == "Zm9v");
    CTEST_ASSERT(encodeText("foob") == "Zm9vYg==");
    CTEST_ASSERT(encodeText("fooba") == "Zm9vYmE=");
    CTEST_ASSERT(encodeText("foobar") == "Zm9vYmFy");
    return true;
}

bool TestEzMcpBase64_Binary() {
    const uint8_t bytes[] = {0xFFU, 0xFEU, 0x00U, 0xFBU, 0xEFU, 0xBEU};
    CTEST_ASSERT_MESSAGE(Base64::encode(std::vector<uint8_t>(bytes, bytes + 6)) == "//4A++++",
        "the last two letters of the alphabet, and a zero byte");
    const uint8_t signature[] = {0x89U, 'P', 'N', 'G', 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    CTEST_ASSERT(Base64::encode(std::vector<uint8_t>(signature, signature + 8)) == "iVBORw0KGgo=");
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpBase64(const std::string& vTest) {
    IfTestExist(TestEzMcpBase64_Rfc4648);
    else IfTestExist(TestEzMcpBase64_Binary);
    return false;
}
