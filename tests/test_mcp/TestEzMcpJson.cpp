#include <TestEzMcpJson.h>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMcp/json.hpp>

#include <cstdint>
#include <cstdio>
#include <limits>

using ez::mcp::JsonError;
using ez::mcp::JsonValue;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

static bool parseText(const std::string& aText, JsonValue& aoValue) {
    JsonError error;
    return JsonValue::parse(aText, aoValue, error);
}

// The text is refused, and reading stopped at aOffset.
static bool refusedAt(const std::string& aText, size_t aOffset) {
    JsonValue value = JsonValue::makeString("untouched");
    JsonError error;
    if (JsonValue::parse(aText, value, error)) {
        return false;
    }
    if (error.message.empty() || (error.offset != aOffset)) {
        printf("refusal of [%s] : offset %u, message %s\n", aText.c_str(), static_cast<unsigned>(error.offset), error.message.c_str());
        return false;
    }
    return value.asString() == "untouched";  // a refused text leaves the value as it was
}

bool TestEzMcpJson_ReadTypes() {
    JsonValue value;
    CTEST_ASSERT_MESSAGE(parseText(" { \"n\" : null , \"t\":true,\"f\":false,\"i\":42,\"d\":1.5,\"s\":\"x\",\"a\":[1,2],\"o\":{} } ", value),
        "a text with every type is read");
    CTEST_ASSERT(value.isObject());
    CTEST_ASSERT(value.size() == 8U);
    CTEST_ASSERT(value.find("n")->isNull());
    CTEST_ASSERT(value.find("t")->isBool() && value.find("t")->asBool());
    CTEST_ASSERT(value.find("f")->isBool() && !value.find("f")->asBool());
    CTEST_ASSERT(value.find("i")->isInteger() && (value.find("i")->asInteger() == 42));
    CTEST_ASSERT(value.find("d")->isNumber() && !value.find("d")->isInteger() && (value.find("d")->asNumber() == 1.5));
    CTEST_ASSERT(value.find("s")->isString() && (value.find("s")->asString() == "x"));
    CTEST_ASSERT(value.find("a")->isArray() && (value.find("a")->size() == 2U) && (value.find("a")->at(1U).asInteger() == 2));
    CTEST_ASSERT(value.find("o")->isObject() && (value.find("o")->size() == 0U));
    CTEST_ASSERT(value.find("missing") == nullptr);
    CTEST_ASSERT(parseText("[]", value) && value.isArray() && (value.size() == 0U));
    CTEST_ASSERT(parseText("\"\"", value) && value.isString() && value.asString().empty());
    CTEST_ASSERT(parseText("null", value) && value.isNull());
    return true;
}

bool TestEzMcpJson_RoundTrip() {
    const std::string text = "{\"n\":null,\"t\":true,\"f\":false,\"i\":-42,\"d\":1.5,\"s\":\"x y\",\"a\":[1,[2,{}],\"z\"],\"o\":{\"k\":[]}}";
    JsonValue value;
    CTEST_ASSERT(parseText(text, value));
    CTEST_ASSERT_MESSAGE(value.dump() == text, "a compact text is written back as it was read");
    JsonValue built = JsonValue::makeObject();
    built.set("list", JsonValue::makeArray().push(JsonValue::makeInteger(1)).push(JsonValue::makeString("two")));
    built.set("flag", JsonValue::makeBool(true));
    CTEST_ASSERT(built.dump() == "{\"list\":[1,\"two\"],\"flag\":true}");
    return true;
}

bool TestEzMcpJson_KeyOrder() {
    JsonValue value;
    CTEST_ASSERT(parseText("{\"z\":1,\"a\":2,\"m\":3}", value));
    CTEST_ASSERT((value.keyAt(0U) == "z") && (value.keyAt(1U) == "a") && (value.keyAt(2U) == "m"));
    CTEST_ASSERT(value.dump() == "{\"z\":1,\"a\":2,\"m\":3}");
    value.set("a", JsonValue::makeInteger(20));
    CTEST_ASSERT_MESSAGE(value.dump() == "{\"z\":1,\"a\":20,\"m\":3}", "a set on an existing key keeps its place");
    value.set("b", JsonValue::makeInteger(4));
    CTEST_ASSERT_MESSAGE(value.dump() == "{\"z\":1,\"a\":20,\"m\":3,\"b\":4}", "a new key goes at the end");
    CTEST_ASSERT_MESSAGE(parseText("{\"k\":1,\"k\":2}", value) && (value.size() == 1U) && (value.find("k")->asInteger() == 2),
        "a repeated key keeps the last value");
    return true;
}

bool TestEzMcpJson_Escapes() {
    JsonValue value;
    CTEST_ASSERT(parseText("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"", value));
    CTEST_ASSERT(value.asString() == "\"\\/\b\f\n\r\t");
    CTEST_ASSERT_MESSAGE(value.dump() == "\"\\\"\\\\/\\b\\f\\n\\r\\t\"", "every escape is written back, the slash is not escaped");
    std::string control;
    control += '\x01';
    control += '\x1f';
    CTEST_ASSERT_MESSAGE(JsonValue::makeString(control).dump() == "\"\\u0001\\u001f\"", "the other control characters are written as \\u00XX");
    return true;
}

bool TestEzMcpJson_Unicode() {
    JsonValue value;
    CTEST_ASSERT(parseText("\"\\u0041\\u00e9\\u20AC\"", value));
    CTEST_ASSERT_MESSAGE(value.asString() == "A\xC3\xA9\xE2\x82\xAC", "one, two and three bytes of UTF-8");
    CTEST_ASSERT(parseText("\"\\ud83d\\uDE00\"", value));
    CTEST_ASSERT_MESSAGE(value.asString() == "\xF0\x9F\x98\x80", "a surrogate pair is one code point of four bytes");
    CTEST_ASSERT(parseText("\"\xC3\xA9t\xC3\xA9\"", value));
    CTEST_ASSERT_MESSAGE(value.dump() == "\"\xC3\xA9t\xC3\xA9\"", "raw UTF-8 passes through");
    CTEST_ASSERT(parseText("\"\\u0000\"", value) && (value.asString().size() == 1U) && (value.asString()[0] == '\0'));
    CTEST_ASSERT(value.dump() == "\"\\u0000\"");
    return true;
}

bool TestEzMcpJson_Numbers() {
    JsonValue value;
    CTEST_ASSERT(parseText("0", value) && value.isInteger() && (value.asInteger() == 0));
    CTEST_ASSERT(parseText("-0", value) && value.isInteger() && (value.asInteger() == 0));
    CTEST_ASSERT(parseText("-12", value) && (value.asInteger() == -12) && (value.dump() == "-12"));
    CTEST_ASSERT_MESSAGE(parseText("9223372036854775807", value) && value.isInteger() && (value.asInteger() == INT64_MAX),
        "the largest int64 is exact");
    CTEST_ASSERT(value.dump() == "9223372036854775807");
    CTEST_ASSERT_MESSAGE(parseText("-9223372036854775808", value) && value.isInteger() && (value.asInteger() == INT64_MIN),
        "the smallest int64 is exact");
    CTEST_ASSERT(value.dump() == "-9223372036854775808");
    CTEST_ASSERT_MESSAGE(parseText("9223372036854775808", value) && value.isNumber() && !value.isInteger(), "beyond int64, a double");
    CTEST_ASSERT(value.asNumber() == 9223372036854775808.0);
    CTEST_ASSERT(parseText("1e3", value) && !value.isInteger() && (value.asNumber() == 1000.0));
    CTEST_ASSERT(parseText("1.25E-2", value) && (value.asNumber() == 0.0125));
    CTEST_ASSERT(parseText("-0.5", value) && (value.asNumber() == -0.5) && (value.asInteger() == 0));
    CTEST_ASSERT(parseText("2.5e+1", value) && (value.asNumber() == 25.0));
    CTEST_ASSERT_MESSAGE(JsonValue::makeNumber(0.1).dump() == "0.1", "the shortest text that reads back");
    JsonValue third;
    CTEST_ASSERT(parseText(JsonValue::makeNumber(1.0 / 3.0).dump(), third) && (third.asNumber() == (1.0 / 3.0)));
    CTEST_ASSERT(JsonValue::makeInteger(1234567890123LL).dump() == "1234567890123");
    CTEST_ASSERT_MESSAGE(JsonValue::makeNumber(std::numeric_limits<double>::infinity()).dump() == "null", "no infinity in JSON");
    CTEST_ASSERT(refusedAt("1e400", 0U));
    return true;
}

bool TestEzMcpJson_Refusals() {
    CTEST_ASSERT(refusedAt("", 0U));
    CTEST_ASSERT(refusedAt("   ", 3U));
    CTEST_ASSERT(refusedAt("[1,]", 3U));
    CTEST_ASSERT(refusedAt("{\"a\":1,}", 7U));
    CTEST_ASSERT(refusedAt("{\"a\" 1}", 5U));
    CTEST_ASSERT(refusedAt("{a:1}", 1U));
    CTEST_ASSERT(refusedAt("[1 2]", 3U));
    CTEST_ASSERT(refusedAt("[1", 2U));
    CTEST_ASSERT(refusedAt("\"abc", 4U));
    CTEST_ASSERT(refusedAt("01", 1U));
    CTEST_ASSERT(refusedAt("1.", 2U));
    CTEST_ASSERT(refusedAt("1e", 2U));
    CTEST_ASSERT(refusedAt("-", 1U));
    CTEST_ASSERT(refusedAt("+1", 0U));
    CTEST_ASSERT(refusedAt(".5", 0U));
    CTEST_ASSERT(refusedAt("tru", 0U));
    CTEST_ASSERT(refusedAt("nul", 0U));
    CTEST_ASSERT(refusedAt("True", 0U));
    CTEST_ASSERT(refusedAt("[1] x", 4U));
    CTEST_ASSERT(refusedAt("\"a\x01\"", 2U));
    CTEST_ASSERT(refusedAt("\"\\x\"", 2U));
    CTEST_ASSERT(refusedAt("\"\\u12\"", 3U));
    CTEST_ASSERT(refusedAt("\"\\u12G4\"", 5U));
    CTEST_ASSERT(refusedAt("\"\\uD800\"", 7U));
    CTEST_ASSERT(refusedAt("\"\\uD800\\u0041\"", 13U));
    CTEST_ASSERT(refusedAt("\"\\uDC00\"", 7U));
    CTEST_ASSERT(refusedAt("{\"a", 3U));
    CTEST_ASSERT(refusedAt("{\"a\":1", 6U));
    CTEST_ASSERT(refusedAt("{\"a\":1 x}", 7U));
    CTEST_ASSERT(refusedAt("\"\\uD800\\u12\"", 9U));
    CTEST_ASSERT(refusedAt("\"\\", 2U));
    return true;
}

bool TestEzMcpJson_Depth() {
    std::string deepest;
    for (size_t idx = 0U; idx < 64U; ++idx) {
        deepest = "[" + deepest + "]";
    }
    JsonValue value;
    CTEST_ASSERT_MESSAGE(parseText(deepest, value), "64 levels are read");
    CTEST_ASSERT_MESSAGE(refusedAt("[" + deepest + "]", 64U), "the 65th level is refused");
    std::string objects = "1";
    for (size_t idx = 0U; idx < 65U; ++idx) {
        objects = "{\"k\":" + objects + "}";
    }
    CTEST_ASSERT(!parseText(objects, value));
    std::string hostile(100000U, '[');
    CTEST_ASSERT_MESSAGE(refusedAt(hostile, 64U), "a hostile nesting stops early, the stack is safe");
    return true;
}

bool TestEzMcpJson_WrongTypeAccess() {
    const JsonValue number = JsonValue::makeInteger(7);
    CTEST_ASSERT(number.asString().empty() && !number.asBool() && (number.size() == 0U));
    CTEST_ASSERT(number.find("k") == nullptr);
    CTEST_ASSERT(number.at(0U).isNull() && number.keyAt(0U).empty());
    CTEST_ASSERT(JsonValue::makeString("7").asInteger() == 0);
    CTEST_ASSERT(JsonValue::makeBool(true).asNumber() == 0.0);
    JsonValue array = JsonValue::makeArray();
    array.push(JsonValue::makeInteger(1));
    CTEST_ASSERT(array.at(1U).isNull() && array.keyAt(0U).empty() && (array.find("0") == nullptr));
    array.set("k", JsonValue::makeInteger(2));
    CTEST_ASSERT_MESSAGE(array.dump() == "[1]", "a set on an array is ignored");
    JsonValue object = JsonValue::makeObject();
    object.push(JsonValue::makeInteger(1));
    CTEST_ASSERT_MESSAGE(object.dump() == "{}", "a push on an object is ignored");
    CTEST_ASSERT(JsonValue::makeNull().dump() == "null");
    CTEST_ASSERT(JsonValue::makeNumber(2.5).getType() == ez::mcp::JsonType_Number);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpJson(const std::string& vTest) {
    IfTestExist(TestEzMcpJson_ReadTypes);
    else IfTestExist(TestEzMcpJson_RoundTrip);
    else IfTestExist(TestEzMcpJson_KeyOrder);
    else IfTestExist(TestEzMcpJson_Escapes);
    else IfTestExist(TestEzMcpJson_Unicode);
    else IfTestExist(TestEzMcpJson_Numbers);
    else IfTestExist(TestEzMcpJson_Refusals);
    else IfTestExist(TestEzMcpJson_Depth);
    else IfTestExist(TestEzMcpJson_WrongTypeAccess);
    return false;
}
