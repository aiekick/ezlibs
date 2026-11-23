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

// The JSON value ezMcp reads and writes : any valid JSON text is read (the
// whole grammar, \uXXXX escapes and surrogate pairs included), anything else
// is refused with a message and the offset where reading stopped ; values are
// written compact. Object keys keep the order they were written in.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace ez {
namespace mcp {

enum JsonType { JsonType_Null = 0, JsonType_Array, JsonType_Bool, JsonType_Number, JsonType_Object, JsonType_String };

struct JsonError {
    std::string message;
    size_t offset{};
};

class JsonValue {
public:
    static JsonValue makeArray() {
        JsonValue value;
        value.m_type = JsonType_Array;
        return value;
    }
    static JsonValue makeBool(bool aValue) {
        JsonValue value;
        value.m_type = JsonType_Bool;
        value.m_bool = aValue;
        return value;
    }
    static JsonValue makeInteger(int64_t aValue) {
        JsonValue value;
        value.m_type = JsonType_Number;
        value.m_integer = aValue;
        value.m_number = static_cast<double>(aValue);
        value.m_isInteger = true;
        return value;
    }
    static JsonValue makeNull() {
        return JsonValue();
    }
    static JsonValue makeNumber(double aValue) {
        JsonValue value;
        value.m_type = JsonType_Number;
        value.m_number = aValue;
        return value;
    }
    static JsonValue makeObject() {
        JsonValue value;
        value.m_type = JsonType_Object;
        return value;
    }
    static JsonValue makeString(const std::string& aValue) {
        JsonValue value;
        value.m_type = JsonType_String;
        value.m_string = aValue;
        return value;
    }

    // Reads a whole JSON text. On failure aoValue is untouched and aoError
    // says what was wrong and at which byte.
    static bool parse(const std::string& aText, JsonValue& aoValue, JsonError& aoError) {
        Cursor cursor(aText, aoError);
        JsonValue value;
        if (!parseValue(cursor, value, 0U)) {
            return false;
        }
        skipSpaces(cursor);
        if (cursor.pos != aText.size()) {
            return fail(cursor, "unexpected text after the value");
        }
        aoValue = value;
        return true;
    }

private:
    // A nesting deeper than this is refused : a hostile text cannot exhaust the stack.
    static const uint32_t kMaxDepth = 64U;

    struct Cursor {
        const std::string& text;
        JsonError& error;
        size_t pos{};
        Cursor(const std::string& aText, JsonError& aoError) : text(aText), error(aoError) {
        }
    };

    std::vector<std::string> m_keys;  // object keys, parallel to m_values
    std::vector<JsonValue> m_values;  // array items or object values
    std::string m_string;
    double m_number{};
    int64_t m_integer{};
    JsonType m_type{JsonType_Null};
    bool m_bool{};
    bool m_isInteger{};  // a number written without fraction nor exponent that fits an int64

public:
    JsonType getType() const {
        return m_type;
    }
    bool isNull() const {
        return m_type == JsonType_Null;
    }
    bool isArray() const {
        return m_type == JsonType_Array;
    }
    bool isBool() const {
        return m_type == JsonType_Bool;
    }
    bool isNumber() const {
        return m_type == JsonType_Number;
    }
    bool isInteger() const {
        return (m_type == JsonType_Number) && m_isInteger;
    }
    bool isObject() const {
        return m_type == JsonType_Object;
    }
    bool isString() const {
        return m_type == JsonType_String;
    }

    // The accessors answer a neutral value (false, 0, "") on a value of another type.
    bool asBool() const {
        return (m_type == JsonType_Bool) && m_bool;
    }
    double asNumber() const {
        return (m_type == JsonType_Number) ? m_number : 0.0;
    }
    int64_t asInteger() const {
        if (m_type != JsonType_Number) {
            return 0;
        }
        return m_isInteger ? m_integer : static_cast<int64_t>(m_number);
    }
    const std::string& asString() const {
        return (m_type == JsonType_String) ? m_string : emptyString();
    }

    // The count of items of an array, of members of an object, 0 otherwise.
    size_t size() const {
        return ((m_type == JsonType_Array) || (m_type == JsonType_Object)) ? m_values.size() : 0U;
    }
    // The item (or member value) at aIndex ; a null value when out of range.
    const JsonValue& at(size_t aIndex) const {
        return (aIndex < size()) ? m_values[aIndex] : nullValue();
    }
    // The key of the member at aIndex of an object ; "" otherwise.
    const std::string& keyAt(size_t aIndex) const {
        return ((m_type == JsonType_Object) && (aIndex < m_keys.size())) ? m_keys[aIndex] : emptyString();
    }
    // The member named aKey of an object ; nullptr when absent or not an object.
    const JsonValue* find(const std::string& aKey) const {
        if (m_type != JsonType_Object) {
            return nullptr;
        }
        for (size_t idx = 0U; idx < m_keys.size(); ++idx) {
            if (m_keys[idx] == aKey) {
                return &m_values[idx];
            }
        }
        return nullptr;
    }

    // Appends to an array ; ignored on any other type.
    JsonValue& push(const JsonValue& aValue) {
        if (m_type == JsonType_Array) {
            m_values.push_back(aValue);
        }
        return *this;
    }
    // Sets a member of an object, in place when the key exists, at the end
    // otherwise ; ignored on any other type.
    JsonValue& set(const std::string& aKey, const JsonValue& aValue) {
        if (m_type != JsonType_Object) {
            return *this;
        }
        for (size_t idx = 0U; idx < m_keys.size(); ++idx) {
            if (m_keys[idx] == aKey) {
                m_values[idx] = aValue;
                return *this;
            }
        }
        m_keys.push_back(aKey);
        m_values.push_back(aValue);
        return *this;
    }

    // The compact JSON text of the value.
    std::string dump() const {
        std::string out;
        write(out);
        return out;
    }

private:
    static const std::string& emptyString() {
        static const std::string sEmpty;
        return sEmpty;
    }
    static const JsonValue& nullValue() {
        static const JsonValue sNull;
        return sNull;
    }

    static bool fail(Cursor& aoCursor, const char* aMessage) {
        aoCursor.error.message = aMessage;
        aoCursor.error.offset = aoCursor.pos;
        return false;
    }

    static void skipSpaces(Cursor& aoCursor) {
        while (aoCursor.pos < aoCursor.text.size()) {
            const char chr = aoCursor.text[aoCursor.pos];
            if ((chr != ' ') && (chr != '\t') && (chr != '\n') && (chr != '\r')) {
                break;
            }
            ++aoCursor.pos;
        }
    }

    static bool isDigit(char aChr) {
        return (aChr >= '0') && (aChr <= '9');
    }

    static bool parseValue(Cursor& aoCursor, JsonValue& aoValue, uint32_t aDepth) {
        skipSpaces(aoCursor);
        if (aoCursor.pos >= aoCursor.text.size()) {
            return fail(aoCursor, "unexpected end of text");
        }
        const char chr = aoCursor.text[aoCursor.pos];
        if (chr == '{') {
            return parseObject(aoCursor, aoValue, aDepth);
        }
        if (chr == '[') {
            return parseArray(aoCursor, aoValue, aDepth);
        }
        if (chr == '"') {
            aoValue = makeString(std::string());
            return parseString(aoCursor, aoValue.m_string);
        }
        if (chr == 't') {
            aoValue = makeBool(true);
            return parseLiteral(aoCursor, "true");
        }
        if (chr == 'f') {
            aoValue = makeBool(false);
            return parseLiteral(aoCursor, "false");
        }
        if (chr == 'n') {
            aoValue = makeNull();
            return parseLiteral(aoCursor, "null");
        }
        if ((chr == '-') || isDigit(chr)) {
            return parseNumber(aoCursor, aoValue);
        }
        return fail(aoCursor, "unexpected character");
    }

    static bool parseLiteral(Cursor& aoCursor, const std::string& aLiteral) {
        if (aoCursor.text.compare(aoCursor.pos, aLiteral.size(), aLiteral) != 0) {
            return fail(aoCursor, "invalid literal");
        }
        aoCursor.pos += aLiteral.size();
        return true;
    }

    static bool parseObject(Cursor& aoCursor, JsonValue& aoValue, uint32_t aDepth) {
        if (aDepth >= kMaxDepth) {
            return fail(aoCursor, "nesting too deep");
        }
        ++aoCursor.pos;  // '{'
        aoValue = makeObject();
        skipSpaces(aoCursor);
        if ((aoCursor.pos < aoCursor.text.size()) && (aoCursor.text[aoCursor.pos] == '}')) {
            ++aoCursor.pos;
            return true;
        }
        while (true) {
            skipSpaces(aoCursor);
            if ((aoCursor.pos >= aoCursor.text.size()) || (aoCursor.text[aoCursor.pos] != '"')) {
                return fail(aoCursor, "expected a member name");
            }
            std::string key;
            if (!parseString(aoCursor, key)) {
                return false;
            }
            skipSpaces(aoCursor);
            if ((aoCursor.pos >= aoCursor.text.size()) || (aoCursor.text[aoCursor.pos] != ':')) {
                return fail(aoCursor, "expected ':'");
            }
            ++aoCursor.pos;
            JsonValue item;
            if (!parseValue(aoCursor, item, aDepth + 1U)) {
                return false;
            }
            aoValue.set(key, item);
            skipSpaces(aoCursor);
            if (aoCursor.pos >= aoCursor.text.size()) {
                return fail(aoCursor, "unexpected end of text");
            }
            const char chr = aoCursor.text[aoCursor.pos];
            ++aoCursor.pos;
            if (chr == '}') {
                return true;
            }
            if (chr != ',') {
                --aoCursor.pos;
                return fail(aoCursor, "expected ',' or '}'");
            }
        }
    }

    static bool parseArray(Cursor& aoCursor, JsonValue& aoValue, uint32_t aDepth) {
        if (aDepth >= kMaxDepth) {
            return fail(aoCursor, "nesting too deep");
        }
        ++aoCursor.pos;  // '['
        aoValue = makeArray();
        skipSpaces(aoCursor);
        if ((aoCursor.pos < aoCursor.text.size()) && (aoCursor.text[aoCursor.pos] == ']')) {
            ++aoCursor.pos;
            return true;
        }
        while (true) {
            JsonValue item;
            if (!parseValue(aoCursor, item, aDepth + 1U)) {
                return false;
            }
            aoValue.m_values.push_back(item);
            skipSpaces(aoCursor);
            if (aoCursor.pos >= aoCursor.text.size()) {
                return fail(aoCursor, "unexpected end of text");
            }
            const char chr = aoCursor.text[aoCursor.pos];
            ++aoCursor.pos;
            if (chr == ']') {
                return true;
            }
            if (chr != ',') {
                --aoCursor.pos;
                return fail(aoCursor, "expected ',' or ']'");
            }
        }
    }

    static bool parseHex4(Cursor& aoCursor, uint32_t& aoCode) {
        if ((aoCursor.pos + 4U) > aoCursor.text.size()) {
            return fail(aoCursor, "truncated \\u escape");
        }
        uint32_t code = 0U;
        for (size_t idx = 0U; idx < 4U; ++idx) {
            const char chr = aoCursor.text[aoCursor.pos];
            uint32_t digit = 0U;
            if (isDigit(chr)) {
                digit = static_cast<uint32_t>(chr - '0');
            } else if ((chr >= 'a') && (chr <= 'f')) {
                digit = static_cast<uint32_t>(chr - 'a') + 10U;
            } else if ((chr >= 'A') && (chr <= 'F')) {
                digit = static_cast<uint32_t>(chr - 'A') + 10U;
            } else {
                return fail(aoCursor, "invalid \\u escape");
            }
            code = (code << 4U) | digit;
            ++aoCursor.pos;
        }
        aoCode = code;
        return true;
    }

    static void appendUtf8(std::string& aoOut, uint32_t aCode) {
        if (aCode < 0x80U) {
            aoOut += static_cast<char>(aCode);
        } else if (aCode < 0x800U) {
            aoOut += static_cast<char>(0xC0U | (aCode >> 6U));
            aoOut += static_cast<char>(0x80U | (aCode & 0x3FU));
        } else if (aCode < 0x10000U) {
            aoOut += static_cast<char>(0xE0U | (aCode >> 12U));
            aoOut += static_cast<char>(0x80U | ((aCode >> 6U) & 0x3FU));
            aoOut += static_cast<char>(0x80U | (aCode & 0x3FU));
        } else {
            aoOut += static_cast<char>(0xF0U | (aCode >> 18U));
            aoOut += static_cast<char>(0x80U | ((aCode >> 12U) & 0x3FU));
            aoOut += static_cast<char>(0x80U | ((aCode >> 6U) & 0x3FU));
            aoOut += static_cast<char>(0x80U | (aCode & 0x3FU));
        }
    }

    static bool parseUnicodeEscape(Cursor& aoCursor, std::string& aoOut) {
        uint32_t code = 0U;
        if (!parseHex4(aoCursor, code)) {
            return false;
        }
        if ((code >= 0xDC00U) && (code <= 0xDFFFU)) {
            return fail(aoCursor, "lone low surrogate");
        }
        if ((code >= 0xD800U) && (code <= 0xDBFFU)) {
            if (aoCursor.text.compare(aoCursor.pos, 2U, "\\u") != 0) {
                return fail(aoCursor, "lone high surrogate");
            }
            aoCursor.pos += 2U;
            uint32_t low = 0U;
            if (!parseHex4(aoCursor, low)) {
                return false;
            }
            if ((low < 0xDC00U) || (low > 0xDFFFU)) {
                return fail(aoCursor, "lone high surrogate");
            }
            code = 0x10000U + ((code - 0xD800U) << 10U) + (low - 0xDC00U);
        }
        appendUtf8(aoOut, code);
        return true;
    }

    static bool parseString(Cursor& aoCursor, std::string& aoOut) {
        ++aoCursor.pos;  // '"'
        while (true) {
            if (aoCursor.pos >= aoCursor.text.size()) {
                return fail(aoCursor, "unterminated string");
            }
            const char chr = aoCursor.text[aoCursor.pos];
            if (chr == '"') {
                ++aoCursor.pos;
                return true;
            }
            if (static_cast<unsigned char>(chr) < 0x20U) {
                return fail(aoCursor, "control character in a string");
            }
            if (chr != '\\') {
                aoOut += chr;
                ++aoCursor.pos;
                continue;
            }
            ++aoCursor.pos;
            if (aoCursor.pos >= aoCursor.text.size()) {
                return fail(aoCursor, "unterminated string");
            }
            const char esc = aoCursor.text[aoCursor.pos];
            ++aoCursor.pos;
            switch (esc) {
                case '"':
                    aoOut += '"';
                    break;
                case '\\':
                    aoOut += '\\';
                    break;
                case '/':
                    aoOut += '/';
                    break;
                case 'b':
                    aoOut += '\b';
                    break;
                case 'f':
                    aoOut += '\f';
                    break;
                case 'n':
                    aoOut += '\n';
                    break;
                case 'r':
                    aoOut += '\r';
                    break;
                case 't':
                    aoOut += '\t';
                    break;
                case 'u': {
                    if (!parseUnicodeEscape(aoCursor, aoOut)) {
                        return false;
                    }
                    break;
                }
                default: {
                    --aoCursor.pos;
                    return fail(aoCursor, "invalid escape");
                }
            }
        }
    }

    static bool parseNumber(Cursor& aoCursor, JsonValue& aoValue) {
        const std::string& text = aoCursor.text;
        const size_t start = aoCursor.pos;
        bool negative = false;
        if (text[aoCursor.pos] == '-') {
            negative = true;
            ++aoCursor.pos;
        }
        if ((aoCursor.pos >= text.size()) || !isDigit(text[aoCursor.pos])) {
            return fail(aoCursor, "invalid number");
        }
        if (text[aoCursor.pos] == '0') {
            ++aoCursor.pos;
        } else {
            while ((aoCursor.pos < text.size()) && isDigit(text[aoCursor.pos])) {
                ++aoCursor.pos;
            }
        }
        const size_t intEnd = aoCursor.pos;
        bool isFloat = false;
        if ((aoCursor.pos < text.size()) && (text[aoCursor.pos] == '.')) {
            isFloat = true;
            ++aoCursor.pos;
            if ((aoCursor.pos >= text.size()) || !isDigit(text[aoCursor.pos])) {
                return fail(aoCursor, "invalid number");
            }
            while ((aoCursor.pos < text.size()) && isDigit(text[aoCursor.pos])) {
                ++aoCursor.pos;
            }
        }
        if ((aoCursor.pos < text.size()) && ((text[aoCursor.pos] == 'e') || (text[aoCursor.pos] == 'E'))) {
            isFloat = true;
            ++aoCursor.pos;
            if ((aoCursor.pos < text.size()) && ((text[aoCursor.pos] == '+') || (text[aoCursor.pos] == '-'))) {
                ++aoCursor.pos;
            }
            if ((aoCursor.pos >= text.size()) || !isDigit(text[aoCursor.pos])) {
                return fail(aoCursor, "invalid number");
            }
            while ((aoCursor.pos < text.size()) && isDigit(text[aoCursor.pos])) {
                ++aoCursor.pos;
            }
        }
        if (!isFloat) {
            // An integer is kept exact while it fits an int64 (the ids of JSON-RPC).
            const uint64_t limit = negative ? 9223372036854775808ULL : 9223372036854775807ULL;
            uint64_t magnitude = 0U;
            bool fits = true;
            for (size_t idx = negative ? (start + 1U) : start; idx < intEnd; ++idx) {
                const auto digit = static_cast<uint64_t>(text[idx] - '0');
                if (magnitude > ((limit - digit) / 10U)) {
                    fits = false;
                    break;
                }
                magnitude = (magnitude * 10U) + digit;
            }
            if (fits) {
                int64_t integer = 0;
                if (negative) {
                    integer = (magnitude == 9223372036854775808ULL) ? INT64_MIN : -static_cast<int64_t>(magnitude);
                } else {
                    integer = static_cast<int64_t>(magnitude);
                }
                aoValue = makeInteger(integer);
                return true;
            }
        }
        // Read with the classic locale : an application locale never changes the decimal point.
        std::istringstream stream(text.substr(start, aoCursor.pos - start));
        stream.imbue(std::locale::classic());
        double number = 0.0;
        stream >> number;
        if (stream.fail() || !std::isfinite(number)) {
            aoCursor.pos = start;
            return fail(aoCursor, "number out of range");
        }
        aoValue = makeNumber(number);
        return true;
    }

    static void writeString(std::string& aoOut, const std::string& aText) {
        static const char* const sHex = "0123456789abcdef";
        aoOut += '"';
        for (const char chr : aText) {
            switch (chr) {
                case '"':
                    aoOut += "\\\"";
                    break;
                case '\\':
                    aoOut += "\\\\";
                    break;
                case '\b':
                    aoOut += "\\b";
                    break;
                case '\f':
                    aoOut += "\\f";
                    break;
                case '\n':
                    aoOut += "\\n";
                    break;
                case '\r':
                    aoOut += "\\r";
                    break;
                case '\t':
                    aoOut += "\\t";
                    break;
                default: {
                    const auto code = static_cast<unsigned char>(chr);
                    if (code < 0x20U) {
                        aoOut += "\\u00";
                        aoOut += sHex[code >> 4U];
                        aoOut += sHex[code & 0x0FU];
                    } else {
                        aoOut += chr;
                    }
                    break;
                }
            }
        }
        aoOut += '"';
    }

    static void writeNumber(std::string& aoOut, double aNumber) {
        if (!std::isfinite(aNumber)) {
            aoOut += "null";  // JSON has no NaN nor infinity
            return;
        }
        // The shortest of 15 or 17 digits that reads back to the same double.
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream.precision(15);
        stream << aNumber;
        std::istringstream check(stream.str());
        check.imbue(std::locale::classic());
        double readBack = 0.0;
        check >> readBack;
        if (readBack != aNumber) {
            stream.str(std::string());
            stream.precision(17);
            stream << aNumber;
        }
        aoOut += stream.str();
    }

    void write(std::string& aoOut) const {
        switch (m_type) {
            case JsonType_Null:
                aoOut += "null";
                break;
            case JsonType_Bool:
                aoOut += m_bool ? "true" : "false";
                break;
            case JsonType_Number: {
                if (m_isInteger) {
                    aoOut += std::to_string(m_integer);
                } else {
                    writeNumber(aoOut, m_number);
                }
                break;
            }
            case JsonType_String:
                writeString(aoOut, m_string);
                break;
            case JsonType_Array: {
                aoOut += '[';
                for (size_t idx = 0U; idx < m_values.size(); ++idx) {
                    if (idx > 0U) {
                        aoOut += ',';
                    }
                    m_values[idx].write(aoOut);
                }
                aoOut += ']';
                break;
            }
            case JsonType_Object: {
                aoOut += '{';
                for (size_t idx = 0U; idx < m_values.size(); ++idx) {
                    if (idx > 0U) {
                        aoOut += ',';
                    }
                    writeString(aoOut, m_keys[idx]);
                    aoOut += ':';
                    m_values[idx].write(aoOut);
                }
                aoOut += '}';
                break;
            }
        }
    }
};

}  // namespace mcp
}  // namespace ez
