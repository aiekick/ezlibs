#include <TestEzTtfTypes.h>
#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMath/ezMath.hpp>  // ez::math::isEqual(a, b, epsilon)
#include <ezlibs/ezTTF/ezTTF.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// a tag is a big-endian fourcc : the numeric constants must equal their
// composed form, and the string view must round-trip
bool TestEzTtfTypes_TagComposeAndBack() {
    CTEST_ASSERT(ez::ttf::makeTag('h', 'e', 'a', 'd') == ez::ttf::kTagHead);
    CTEST_ASSERT(ez::ttf::kTagHead == 0x68656164u);
    CTEST_ASSERT(ez::ttf::makeTag('c', 'm', 'a', 'p') == ez::ttf::kTagCmap);
    CTEST_ASSERT(ez::ttf::makeTag('C', 'O', 'L', 'R') == ez::ttf::kTagColr);
    CTEST_ASSERT(ez::ttf::makeTag('O', 'T', 'T', 'O') == ez::ttf::kSfntVersionOtto);
    CTEST_ASSERT(ez::ttf::tagToString(ez::ttf::kTagHead) == "head");
    CTEST_ASSERT(ez::ttf::tagToString(ez::ttf::kTagCpal) == "CPAL");
    CTEST_ASSERT(ez::ttf::makeTag('l', 'o', 'c', 'a') == ez::ttf::kTagLoca);
    return true;
}

// 16.16 fixed : one raw int32, the storage truth of the table versions
bool TestEzTtfTypes_FixedRoundTrip() {
    ez::ttf::Fixed version;
    version.setFloat(1.0f);
    CTEST_ASSERT(version.raw == 0x00010000);  // the head table version 1.0
    CTEST_ASSERT(version.getFloat() == 1.0f);
    ez::ttf::Fixed negative;
    negative.setFloat(-1.5f);
    CTEST_ASSERT(negative.raw == -98304);  // 0xFFFE8000 as int32
    CTEST_ASSERT(negative.getFloat() == -1.5f);
    ez::ttf::Fixed zero;
    CTEST_ASSERT(zero.raw == 0 && zero.getFloat() == 0.0f);
    return true;
}

// 2.14 fixed : the SPEC table of examples — the negative rows are the
// ones the ttfrrw shift-and-mask decomposition read wrong
bool TestEzTtfTypes_F2Dot14SpecValues() {
    // raw 0x7FFF = 1.999939, raw 0x7000 = 1.75
    CTEST_ASSERT(ez::math::isEqual(ez::ttf::F2DOT14(static_cast<int16_t>(0x7FFF)).getFloat(), 1.999939f, 1e-5f));
    CTEST_ASSERT(ez::math::isEqual(ez::ttf::F2DOT14(static_cast<int16_t>(0x7000)).getFloat(), 1.75f, 1e-6f));
    // raw 0x0001 = 0.000061, raw 0 = 0
    CTEST_ASSERT(ez::math::isEqual(ez::ttf::F2DOT14(static_cast<int16_t>(1)).getFloat(), 0.000061f, 1e-5f));
    CTEST_ASSERT(ez::ttf::F2DOT14(static_cast<int16_t>(0)).getFloat() == 0.0f);
    // raw 0xFFFF (= -1) = -0.000061, raw 0x8000 (= -32768) = -2.0
    CTEST_ASSERT(ez::math::isEqual(ez::ttf::F2DOT14(static_cast<int16_t>(-1)).getFloat(), -0.000061f, 1e-5f));
    CTEST_ASSERT(ez::math::isEqual(ez::ttf::F2DOT14(static_cast<int16_t>(-32768)).getFloat(), -2.0f, 1e-6f));
    return true;
}

bool TestEzTtfTypes_F2Dot14SetFloatSaturates() {
    ez::ttf::F2DOT14 value;
    value.setFloat(1.75f);
    CTEST_ASSERT(value.raw == 0x7000);
    value.setFloat(-1.0f);
    CTEST_ASSERT(value.raw == -16384);
    // out of the [-2, +2) range : saturate, never wrap
    value.setFloat(3.0f);
    CTEST_ASSERT(value.raw == 32767);
    value.setFloat(-3.0f);
    CTEST_ASSERT(value.raw == -32768);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzTtfTypes(const std::string& vTest) {
    IfTestExist(TestEzTtfTypes_TagComposeAndBack);
    else IfTestExist(TestEzTtfTypes_FixedRoundTrip);
    else IfTestExist(TestEzTtfTypes_F2Dot14SpecValues);
    else IfTestExist(TestEzTtfTypes_F2Dot14SetFloatSaturates);
    return false;
}
