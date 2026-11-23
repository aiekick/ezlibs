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

// ezMcp, amalgamated : ONE header, everything ezMcp needs. Generated from
// include/ezlibs/ezMcp/ by single_include/amalgamate_ezmcp.cmake - never
// edit it by hand, edit the sources and run the script again.
// ---- begin ezOS.hpp ----


// ezOs is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

// just a file for define OS defines

#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(__WIN64__) || defined(WIN64) || defined(_WIN64) || defined(_MSC_VER)
#define WINDOWS_OS
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__EMSCRIPTEN__)
#ifdef __EMSCRIPTEN__
#define EMSCRIPTEN_OS
#endif
#define LINUX_OS
#define UNIX_OS
#else
#define APPLE_OS
#define UNIX_OS
#endif
// ---- end ezOS.hpp ----
// ---- begin ezMcp/ezMcp.hpp ----


// ezMcp is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

// ezMcp : an MCP server (Model Context Protocol) an application embeds to be
// seen and driven by an MCP client. It listens on 127.0.0.1 only and speaks
// the "streamable HTTP" transport with JSON answers : one POST /mcp per
// message, a request carrying an Origin header (a web page) is refused.
// With a token in the config, every request must carry
// "Authorization: Bearer <token>" : another process of the machine that
// reaches the port (through a tunnel, say) can call nothing.
//
// The application registers tools (name, description, JSON schema of the
// arguments, a function). The protocol messages are answered on the server
// thread ; a tools/call waits in a queue until the application calls poll(),
// once per frame, which runs the tool on the application thread : a tool can
// touch the ui, read the frame, inject an input at the right moment.
//
//   ez::mcp::Server server;
//   ez::mcp::Tool tool;
//   tool.name = "hello";
//   tool.description = "says hello";
//   tool.functor = [](const ez::mcp::JsonValue& aArgs, ez::mcp::ToolResult& aoResult) { aoResult.addText("hello"); };
//   server.addTool(tool);
//   ez::mcp::ServerConfig config;
//   config.name = "myApp";
//   config.port = 7810;
//   server.start(config);
//   while (running) { server.poll(); ...frame... }
//   server.stop();
//
// A tool may also answer LATER : inside its functor, aoResult.defer() hands
// back the token of the call, poll() does not answer it, and the application
// answers when it wants with server.complete(token, result), from its own
// thread (a few frames later, after a computation...). The client waits, as
// for any running tool.

// ---- begin ezMcp/base64.hpp ----


// ezMcp is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

// Base64 encoding (RFC 4648, standard alphabet, padded) : the bytes of an
// image inside an MCP tool result.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ez {
namespace mcp {

class Base64 {
public:
    static std::string encode(const std::vector<uint8_t>& aBytes) {
        static const char* const sAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((aBytes.size() + 2U) / 3U) * 4U);
        size_t idx = 0U;
        for (; (idx + 3U) <= aBytes.size(); idx += 3U) {
            const uint32_t triple = (static_cast<uint32_t>(aBytes[idx]) << 16U) | (static_cast<uint32_t>(aBytes[idx + 1U]) << 8U) | aBytes[idx + 2U];
            out += sAlphabet[(triple >> 18U) & 0x3FU];
            out += sAlphabet[(triple >> 12U) & 0x3FU];
            out += sAlphabet[(triple >> 6U) & 0x3FU];
            out += sAlphabet[triple & 0x3FU];
        }
        const size_t rest = aBytes.size() - idx;
        if (rest == 1U) {
            const uint32_t single = static_cast<uint32_t>(aBytes[idx]) << 16U;
            out += sAlphabet[(single >> 18U) & 0x3FU];
            out += sAlphabet[(single >> 12U) & 0x3FU];
            out += "==";
        } else if (rest == 2U) {
            const uint32_t pair = (static_cast<uint32_t>(aBytes[idx]) << 16U) | (static_cast<uint32_t>(aBytes[idx + 1U]) << 8U);
            out += sAlphabet[(pair >> 18U) & 0x3FU];
            out += sAlphabet[(pair >> 12U) & 0x3FU];
            out += sAlphabet[(pair >> 6U) & 0x3FU];
            out += '=';
        }
        return out;
    }
};

}  // namespace mcp
}  // namespace ez
// ---- end ezMcp/base64.hpp ----
// ---- begin ezMcp/http.hpp ----


// ezMcp is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

// The least of HTTP/1.1 an MCP server needs : read one request (request line,
// headers, a body sized by Content-Length) and write one response, after
// which the connection closes. No chunked bodies, no keep-alive. Pure text
// work, no network : the socket lives in socket.hpp.

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ez {
namespace mcp {

enum HttpParseStatus { HttpParseStatus_Complete = 0, HttpParseStatus_Incomplete, HttpParseStatus_Invalid };

struct HttpRequest {
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;  // names in lower case, values trimmed
    std::string method;
    std::string path;  // the target without its query

    // The value of the header aLowerName ; nullptr when absent.
    const std::string* findHeader(const std::string& aLowerName) const {
        for (const auto& header : headers) {
            if (header.first == aLowerName) {
                return &header.second;
            }
        }
        return nullptr;
    }
};

class Http {
public:
    // Reads the request at the start of aData : Incomplete while bytes are
    // missing, Invalid when the request breaks the grammar or the limits.
    static HttpParseStatus parseRequest(const std::string& aData, HttpRequest& aoRequest) {
        const size_t maxHeaderSize = 65536U;
        const size_t maxBodySize = static_cast<size_t>(16U) * 1024U * 1024U;
        const size_t headerEnd = aData.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            return (aData.size() > maxHeaderSize) ? HttpParseStatus_Invalid : HttpParseStatus_Incomplete;
        }
        if (headerEnd > maxHeaderSize) {
            return HttpParseStatus_Invalid;
        }
        HttpRequest request;
        size_t lineEnd = aData.find("\r\n");
        if (!parseRequestLine(aData.substr(0U, lineEnd), request)) {
            return HttpParseStatus_Invalid;
        }
        while (lineEnd < headerEnd) {
            const size_t lineBegin = lineEnd + 2U;
            lineEnd = aData.find("\r\n", lineBegin);
            const std::string line = aData.substr(lineBegin, lineEnd - lineBegin);
            const size_t colon = line.find(':');
            if ((colon == std::string::npos) || (colon == 0U)) {
                return HttpParseStatus_Invalid;
            }
            std::string name = line.substr(0U, colon);
            for (auto& chr : name) {
                if ((chr == ' ') || (chr == '\t')) {
                    return HttpParseStatus_Invalid;  // no space before the colon
                }
                if ((chr >= 'A') && (chr <= 'Z')) {
                    chr = static_cast<char>(chr - 'A' + 'a');
                }
            }
            request.headers.emplace_back(name, trim(line.substr(colon + 1U)));
        }
        if (request.findHeader("transfer-encoding") != nullptr) {
            return HttpParseStatus_Invalid;  // chunked bodies are not read
        }
        size_t bodySize = 0U;
        const std::string* pLength = request.findHeader("content-length");
        if (pLength != nullptr) {
            if (pLength->empty() || (pLength->size() > 9U)) {
                return HttpParseStatus_Invalid;
            }
            for (const char chr : *pLength) {
                if ((chr < '0') || (chr > '9')) {
                    return HttpParseStatus_Invalid;
                }
                bodySize = (bodySize * 10U) + static_cast<size_t>(chr - '0');
            }
            if (bodySize > maxBodySize) {
                return HttpParseStatus_Invalid;
            }
        }
        const size_t bodyBegin = headerEnd + 4U;
        if ((aData.size() - bodyBegin) < bodySize) {
            return HttpParseStatus_Incomplete;
        }
        request.body = aData.substr(bodyBegin, bodySize);
        aoRequest = request;
        return HttpParseStatus_Complete;
    }

    // A whole response that closes the connection. A 401 names its scheme, a 405 says which method is allowed.
    static std::string makeResponse(int32_t aStatus, const std::string& aContentType, const std::string& aBody) {
        std::string out = "HTTP/1.1 " + std::to_string(aStatus) + " " + reason(aStatus) + "\r\n";
        if (!aBody.empty()) {
            out += "Content-Type: " + aContentType + "\r\n";
        }
        if (aStatus == 401) {
            out += "WWW-Authenticate: Bearer\r\n";
        }
        if (aStatus == 405) {
            out += "Allow: POST\r\n";
        }
        out += "Content-Length: " + std::to_string(aBody.size()) + "\r\n";
        out += "Connection: close\r\n\r\n";
        out += aBody;
        return out;
    }

private:
    static const char* reason(int32_t aStatus) {
        switch (aStatus) {
            case 200:
                return "OK";
            case 202:
                return "Accepted";
            case 400:
                return "Bad Request";
            case 401:
                return "Unauthorized";
            case 403:
                return "Forbidden";
            case 404:
                return "Not Found";
            case 405:
                return "Method Not Allowed";
            case 500:
                return "Internal Server Error";
            default:
                return "Unknown";
        }
    }

    static std::string trim(const std::string& aText) {
        size_t begin = 0U;
        size_t end = aText.size();
        while ((begin < end) && ((aText[begin] == ' ') || (aText[begin] == '\t'))) {
            ++begin;
        }
        while ((end > begin) && ((aText[end - 1U] == ' ') || (aText[end - 1U] == '\t'))) {
            --end;
        }
        return aText.substr(begin, end - begin);
    }

    // "METHOD SP target SP HTTP/1.x"
    static bool parseRequestLine(const std::string& aLine, HttpRequest& aoRequest) {
        const size_t firstSpace = aLine.find(' ');
        if ((firstSpace == std::string::npos) || (firstSpace == 0U)) {
            return false;
        }
        const size_t secondSpace = aLine.find(' ', firstSpace + 1U);
        if ((secondSpace == std::string::npos) || (secondSpace == (firstSpace + 1U))) {
            return false;
        }
        const std::string version = aLine.substr(secondSpace + 1U);
        if ((version != "HTTP/1.1") && (version != "HTTP/1.0")) {
            return false;
        }
        aoRequest.method = aLine.substr(0U, firstSpace);
        const std::string target = aLine.substr(firstSpace + 1U, secondSpace - firstSpace - 1U);
        aoRequest.path = target.substr(0U, target.find('?'));
        return true;
    }
};

}  // namespace mcp
}  // namespace ez
// ---- end ezMcp/http.hpp ----
// ---- begin ezMcp/json.hpp ----


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
// ---- end ezMcp/json.hpp ----
// ---- begin ezMcp/jsonrpc.hpp ----


// ezMcp is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

// JSON-RPC 2.0 as MCP speaks it : one message per body (no batch), a request
// carries an id (a string or a number, never null), a notification has none.
// Reading classifies a body ; the writers make the answers.

// json.hpp : inlined above

#include <cstdint>
#include <string>

namespace ez {
namespace mcp {

enum JsonRpcErrorCode : int32_t {
    JsonRpcErrorCode_None = 0,
    JsonRpcErrorCode_InternalError = -32603,
    JsonRpcErrorCode_InvalidParams = -32602,
    JsonRpcErrorCode_InvalidRequest = -32600,
    JsonRpcErrorCode_MethodNotFound = -32601,
    JsonRpcErrorCode_ParseError = -32700
};

enum JsonRpcKind { JsonRpcKind_Invalid = 0, JsonRpcKind_Notification, JsonRpcKind_Request, JsonRpcKind_Response };

struct JsonRpcMessage {
    JsonRpcErrorCode errorCode{JsonRpcErrorCode_None};  // why an invalid message is invalid
    std::string errorMessage;
    JsonValue id;  // null for a notification, or when no valid id could be read
    JsonRpcKind kind{JsonRpcKind_Invalid};
    std::string method;
    JsonValue params;  // null when absent, else an object or an array
};

class JsonRpc {
public:
    static JsonRpcMessage read(const std::string& aBody) {
        JsonRpcMessage message;
        JsonValue root;
        JsonError error;
        if (!JsonValue::parse(aBody, root, error)) {
            return invalid(message, JsonRpcErrorCode_ParseError, "parse error : " + error.message + " at offset " + std::to_string(error.offset));
        }
        if (!root.isObject()) {
            return invalid(message, JsonRpcErrorCode_InvalidRequest, "a message is one JSON object");
        }
        const JsonValue* pId = root.find("id");
        if (pId != nullptr) {
            if (!pId->isString() && !pId->isNumber()) {
                return invalid(message, JsonRpcErrorCode_InvalidRequest, "an id is a string or a number");
            }
            message.id = *pId;
        }
        const JsonValue* pVersion = root.find("jsonrpc");
        if ((pVersion == nullptr) || (pVersion->asString() != "2.0")) {
            return invalid(message, JsonRpcErrorCode_InvalidRequest, "jsonrpc must be \"2.0\"");
        }
        const JsonValue* pMethod = root.find("method");
        if (pMethod != nullptr) {
            if (!pMethod->isString()) {
                return invalid(message, JsonRpcErrorCode_InvalidRequest, "a method is a string");
            }
            const JsonValue* pParams = root.find("params");
            if (pParams != nullptr) {
                if (!pParams->isObject() && !pParams->isArray()) {
                    return invalid(message, JsonRpcErrorCode_InvalidRequest, "params is an object or an array");
                }
                message.params = *pParams;
            }
            message.method = pMethod->asString();
            message.kind = (pId != nullptr) ? JsonRpcKind_Request : JsonRpcKind_Notification;
            return message;
        }
        if ((pId != nullptr) && ((root.find("result") != nullptr) || (root.find("error") != nullptr))) {
            message.kind = JsonRpcKind_Response;
            return message;
        }
        return invalid(message, JsonRpcErrorCode_InvalidRequest, "neither a request, a notification nor a response");
    }

    static std::string makeResult(const JsonValue& aId, const JsonValue& aResult) {
        JsonValue response = JsonValue::makeObject();
        response.set("jsonrpc", JsonValue::makeString("2.0"));
        response.set("id", aId);
        response.set("result", aResult);
        return response.dump();
    }

    static std::string makeError(const JsonValue& aId, JsonRpcErrorCode aCode, const std::string& aMessage) {
        JsonValue error = JsonValue::makeObject();
        error.set("code", JsonValue::makeInteger(static_cast<int64_t>(aCode)));
        error.set("message", JsonValue::makeString(aMessage));
        JsonValue response = JsonValue::makeObject();
        response.set("jsonrpc", JsonValue::makeString("2.0"));
        response.set("id", aId);
        response.set("error", error);
        return response.dump();
    }

private:
    static JsonRpcMessage& invalid(JsonRpcMessage& aoMessage, JsonRpcErrorCode aCode, const std::string& aMessage) {
        aoMessage.kind = JsonRpcKind_Invalid;
        aoMessage.errorCode = aCode;
        aoMessage.errorMessage = aMessage;
        return aoMessage;
    }
};

}  // namespace mcp
}  // namespace ez
// ---- end ezMcp/jsonrpc.hpp ----
// ---- begin ezMcp/png.hpp ----


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
// ---- end ezMcp/png.hpp ----
// ---- begin ezMcp/socket.hpp ----


// ezMcp is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

// A blocking TCP socket bound to the loopback interface only (127.0.0.1) :
// listen, accept, receive, send, close. The waits are bounded, so a server
// thread can look at its stop flag between two of them. Winsock under
// Windows (link ws2_32), BSD sockets elsewhere. No exceptions.

// ../ezOS.hpp : inlined above

#include <cstdint>
#include <cstring>
#include <string>

#ifdef WINDOWS_OS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ez {
namespace mcp {

enum SocketReceive { SocketReceive_Closed = 0, SocketReceive_Data, SocketReceive_Error, SocketReceive_Timeout };

class Socket {
private:
#ifdef WINDOWS_OS
    typedef SOCKET NativeHandle;
#else
    typedef int NativeHandle;
#endif

    NativeHandle m_handle{invalidHandle()};
    bool m_systemStarted{};  // this socket holds one Winsock startup

public:
    Socket() = default;
    ~Socket() {
        close();
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(Socket&&) = delete;

    // Listens on 127.0.0.1:aPort ; aPort 0 lets the system pick a free port,
    // read back by getPort().
    bool listenLocal(uint16_t aPort) {
        if (!open()) {
            return false;
        }
#ifdef WINDOWS_OS
        // No other process may bind the same port while it is ours.
        BOOL exclusive = TRUE;
        setsockopt(m_handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
#else
        // A restarted server gets its port back at once.
        int reuse = 1;
        setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
        sockaddr_in address = loopbackAddress(aPort);
        if ((bind(m_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) || (listen(m_handle, 4) != 0)) {
            close();
            return false;
        }
        return true;
    }

    // Connects to 127.0.0.1:aPort (the client side, for a test or a tool).
    bool connectLocal(uint16_t aPort) {
        if (!open()) {
            return false;
        }
        sockaddr_in address = loopbackAddress(aPort);
        if (connect(m_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            close();
            return false;
        }
        return true;
    }

    // Waits at most aTimeoutMs for a connection ; aoClient holds it on success.
    bool acceptClient(Socket& aoClient, int32_t aTimeoutMs) {
        if (!isValid() || (waitReadable(aTimeoutMs) <= 0)) {
            return false;
        }
        const NativeHandle handle = accept(m_handle, nullptr, nullptr);
        if (handle == invalidHandle()) {
            return false;
        }
        aoClient.close();
        aoClient.startSystem();
        aoClient.m_handle = handle;
        aoClient.noSigPipe();
        return true;
    }

    // Waits at most aTimeoutMs for bytes and appends what came to aoData.
    SocketReceive receive(std::string& aoData, int32_t aTimeoutMs) {
        if (!isValid()) {
            return SocketReceive_Error;
        }
        const int32_t ready = waitReadable(aTimeoutMs);
        if (ready == 0) {
            return SocketReceive_Timeout;
        }
        if (ready < 0) {
            return SocketReceive_Error;
        }
        char buffer[4096];
#ifdef WINDOWS_OS
        const int count = recv(m_handle, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
        const ssize_t count = recv(m_handle, buffer, sizeof(buffer), 0);
#endif
        if (count == 0) {
            return SocketReceive_Closed;
        }
        if (count < 0) {
            return SocketReceive_Error;
        }
        aoData.append(buffer, static_cast<size_t>(count));
        return SocketReceive_Data;
    }

    // Sends every byte of aData, or fails.
    bool sendAll(const std::string& aData) const {
        if (!isValid()) {
            return false;
        }
        size_t sent = 0U;
        while (sent < aData.size()) {
#ifdef WINDOWS_OS
            const int count = send(m_handle, aData.data() + sent, static_cast<int>(aData.size() - sent), 0);
#elif defined(MSG_NOSIGNAL)
            const ssize_t count = send(m_handle, aData.data() + sent, aData.size() - sent, MSG_NOSIGNAL);
#else
            const ssize_t count = send(m_handle, aData.data() + sent, aData.size() - sent, 0);
#endif
            if (count <= 0) {
                return false;
            }
            sent += static_cast<size_t>(count);
        }
        return true;
    }

    void close() {
        if (m_handle != invalidHandle()) {
#ifdef WINDOWS_OS
            closesocket(m_handle);
#else
            ::close(m_handle);
#endif
            m_handle = invalidHandle();
        }
#ifdef WINDOWS_OS
        if (m_systemStarted) {
            WSACleanup();
        }
#endif
        m_systemStarted = false;
    }

    bool isValid() const {
        return m_handle != invalidHandle();
    }

    // The local port, 0 when not bound.
    uint16_t getPort() const {
        sockaddr_in address{};
        return readLocalAddress(address) ? ntohs(address.sin_port) : static_cast<uint16_t>(0U);
    }

    // The local IPv4 address in host order (0x7F000001 for 127.0.0.1), 0 when not bound.
    uint32_t getAddress() const {
        sockaddr_in address{};
        return readLocalAddress(address) ? ntohl(address.sin_addr.s_addr) : 0U;
    }

private:
    static NativeHandle invalidHandle() {
#ifdef WINDOWS_OS
        return INVALID_SOCKET;
#else
        return -1;
#endif
    }

    static sockaddr_in loopbackAddress(uint16_t aPort) {
        sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(aPort);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        return address;
    }

    // A member under Windows (it counts this socket's startup), a no-op elsewhere.
    bool startSystem() {  // NOLINT(readability-convert-member-functions-to-static)
#ifdef WINDOWS_OS
        if (!m_systemStarted) {
            WSADATA data;
            m_systemStarted = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
        }
        return m_systemStarted;
#else
        return true;
#endif
    }

    bool open() {
        close();
        if (!startSystem()) {
            return false;
        }
        m_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_handle == invalidHandle()) {
            close();
            return false;
        }
        noSigPipe();
        return true;
    }

    // A peer that closed early never kills the process by SIGPIPE.
    void noSigPipe() {
#ifdef SO_NOSIGPIPE
        int noSignal = 1;
        setsockopt(m_handle, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif
    }

    // 1 readable, 0 timed out, -1 failed. An interrupted wait counts as timed out.
    int32_t waitReadable(int32_t aTimeoutMs) const {
        const int32_t timeoutMs = (aTimeoutMs < 0) ? 0 : aTimeoutMs;
#ifdef WINDOWS_OS
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_handle, &readSet);
        timeval timeout;
        timeout.tv_sec = static_cast<long>(timeoutMs / 1000);
        timeout.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);
        const int result = select(0, &readSet, nullptr, nullptr, &timeout);
        if (result < 0) {
            return -1;
        }
        return (result > 0) ? 1 : 0;
#else
        pollfd entry{};
        entry.fd = m_handle;
        entry.events = POLLIN;
        const int result = poll(&entry, 1, timeoutMs);
        if (result < 0) {
            return (errno == EINTR) ? 0 : -1;
        }
        return (result > 0) ? 1 : 0;
#endif
    }

    bool readLocalAddress(sockaddr_in& aoAddress) const {
        if (!isValid()) {
            return false;
        }
#ifdef WINDOWS_OS
        int size = static_cast<int>(sizeof(aoAddress));
#else
        socklen_t size = sizeof(aoAddress);
#endif
        return getsockname(m_handle, reinterpret_cast<sockaddr*>(&aoAddress), &size) == 0;
    }
};

}  // namespace mcp
}  // namespace ez
// ---- end ezMcp/socket.hpp ----

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace ez {
namespace mcp {

class Server;

// Names one call answered later (0 = none).
typedef uint64_t CallToken;

// What a tool answers : text and images, in the order they were added. An
// error result tells the client the tool ran and failed (bad arguments, a
// state that does not allow it).
class ToolResult {
    friend class Server;

private:
    std::vector<JsonValue> m_content;
    CallToken m_callToken{};  // the call this result answers, set by poll()
    bool m_deferred{};
    bool m_isError{};

public:
    // Inside a tool run by poll() : the answer comes later, by
    // Server::complete(token, result). Outside a poll(), nothing happens and
    // the token is 0.
    CallToken defer() {
        m_deferred = (m_callToken != 0U);
        return m_callToken;
    }
    void addText(const std::string& aText) {
        JsonValue item = JsonValue::makeObject();
        item.set("type", JsonValue::makeString("text"));
        item.set("text", JsonValue::makeString(aText));
        m_content.push_back(item);
    }
    // aPng is a PNG file (Png::encodeRgba makes one).
    void addImagePng(const std::vector<uint8_t>& aPng) {
        JsonValue item = JsonValue::makeObject();
        item.set("type", JsonValue::makeString("image"));
        item.set("data", JsonValue::makeString(Base64::encode(aPng)));
        item.set("mimeType", JsonValue::makeString("image/png"));
        m_content.push_back(item);
    }
    void setError(bool aIsError) {
        m_isError = aIsError;
    }
    JsonValue toJson() const {
        JsonValue content = JsonValue::makeArray();
        for (const auto& item : m_content) {
            content.push(item);
        }
        JsonValue result = JsonValue::makeObject();
        result.set("content", content);
        result.set("isError", JsonValue::makeBool(m_isError));
        return result;
    }
};

// The journal : one line of JSON per exchange, handed to the application.
enum LogLevel {
    LogLevel_None = 0,
    LogLevel_Bodies,    // the exchanges with their request and response bodies
    LogLevel_Exchanges  // one line per exchange : status, method, id, tool, duration, error
};

// Called on the thread that served the request (the server thread) : the
// application guards what it touches. Lines never interleave.
typedef std::function<void(const std::string& aLine)> LogFunctor;

typedef std::function<void(const JsonValue& aArguments, ToolResult& aoResult)> ToolFunctor;

// What became of a Server::complete().
enum CompleteResult {
    CompleteResult_Cancelled = 0,  // the server stopped meanwhile : the client was told
    CompleteResult_Completed,      // the answer went to the client
    CompleteResult_UnknownToken    // never handed out, or already completed
};

struct Tool {
    std::string description;
    ToolFunctor functor{};
    JsonValue inputSchema;  // a JSON schema object ; null means an object with no declared property
    std::string name;
};

struct ServerConfig {
    int32_t callTimeoutMs{5000};  // how long a tools/call waits for a poll(), clamped to [10, 600000]
    std::string instructions;     // told to the client at initialize, omitted when empty
    LogFunctor logFunctor{};      // the journal ; empty : no journal
    LogLevel logLevel{LogLevel_None};
    std::string name;     // serverInfo.name
    uint16_t port{};      // 0 : the system picks a free port, read back by getPort()
    std::string token;    // empty : no check ; else at least 32 visible characters, see Server::makeToken()
    std::string version;  // serverInfo.version
};

class Server {
public:
    // The protocol versions this server speaks, the newest first.
    static const std::vector<std::string>& getProtocolVersions() {
        static const std::vector<std::string> sVersions = {"2025-11-25", "2025-06-18", "2025-03-26"};
        return sVersions;
    }

    // A fresh token : 64 hexadecimal characters, 256 bits from std::random_device.
    static std::string makeToken() {
        static const char* const sHex = "0123456789abcdef";
        std::random_device device;
        std::string token;
        token.reserve(64U);
        for (size_t word = 0U; word < 8U; ++word) {
            const auto bits = static_cast<uint32_t>(device());
            for (uint32_t shift = 0U; shift < 32U; shift += 4U) {
                token += sHex[(bits >> shift) & 0x0FU];
            }
        }
        return token;
    }

private:
    enum CallState { CallState_Cancelled = 0, CallState_Done, CallState_Queued, CallState_Running };

    struct PendingCall {
        JsonValue arguments;
        ToolResult result;
        CallState state{CallState_Queued};
        Tool tool;
    };

    // What one request became, for the journal.
    struct Exchange {
        std::string error;  // why it was refused or failed
        JsonRpcErrorCode errorCode{JsonRpcErrorCode_None};
        JsonValue id;
        bool isError{};        // a tool that ran and said it failed
        std::string response;  // the HTTP body sent back
        JsonValue result;
        std::string rpc;  // the JSON-RPC method
        int32_t status{};
        std::string tool;
    };

    std::condition_variable m_callChanged;
    std::map<CallToken, std::shared_ptr<PendingCall>> m_deferred;  // the calls a tool deferred, until complete()
    int32_t m_callTimeoutMs{5000};
    ServerConfig m_config;
    Socket m_listener;
    std::mutex m_logMutex;
    mutable std::mutex m_mutex;
    CallToken m_nextToken{1};
    std::vector<std::shared_ptr<PendingCall>> m_pending;
    std::atomic<bool> m_running{false};
    std::string m_startError;
    std::thread m_thread;
    std::vector<Tool> m_tools;

public:
    Server() = default;
    ~Server() {
        stop();
    }
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    // Registers a tool, before or after start(). Refused : an empty or taken
    // name, no functor, a schema that is not an object.
    bool addTool(const Tool& aTool) {
        if (aTool.name.empty() || (aTool.functor == nullptr) || (!aTool.inputSchema.isNull() && !aTool.inputSchema.isObject())) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& tool : m_tools) {
            if (tool.name == aTool.name) {
                return false;
            }
        }
        m_tools.push_back(aTool);
        return true;
    }

    // Listens on 127.0.0.1:port and starts the server thread. On failure,
    // getStartError() says why.
    bool start(const ServerConfig& aConfig) {
        if (m_running) {
            m_startError = "the server already runs";
            return false;
        }
        if (!aConfig.token.empty() && !isTokenAccepted(aConfig.token)) {
            m_startError = "the token is refused : at least 32 visible characters, no space";
            return false;
        }
        m_config = aConfig;
        m_callTimeoutMs = clampTimeout(aConfig.callTimeoutMs);
        if (!m_listener.listenLocal(aConfig.port)) {
            m_startError = "cannot listen on 127.0.0.1:" + std::to_string(aConfig.port);
            return false;
        }
        m_startError.clear();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_deferred.clear();  // the cancelled calls of a previous run
        }
        m_running = true;
        m_thread = std::thread(&Server::run, this);
        return true;
    }

    // Stops the thread and closes the port ; calls still waiting are answered
    // with an error.
    void stop() {
        std::unique_lock<std::mutex> runningLock(m_mutex);
        if (!m_running && !m_thread.joinable()) {
            return;
        }
        m_running = false;
        runningLock.unlock();
        m_callChanged.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_listener.close();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.clear();
        for (auto& deferred : m_deferred) {
            deferred.second->state = CallState_Cancelled;  // complete() will say so
        }
    }

    // Why the last start() failed ; empty after a success.
    const std::string& getStartError() const {
        return m_startError;
    }

    bool isRunning() const {
        return m_running;
    }

    uint16_t getPort() const {
        return m_listener.getPort();
    }

    // Runs the tool calls waiting since the last poll, on the calling thread.
    // Returns how many ran.
    size_t poll() {
        std::vector<std::shared_ptr<PendingCall>> calls;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& pending : m_pending) {
                if (pending->state == CallState_Queued) {
                    pending->state = CallState_Running;
                    pending->result.m_callToken = m_nextToken++;
                    calls.push_back(pending);
                }
            }
            m_pending.clear();
        }
        for (const auto& pCall : calls) {
            PendingCall& call = *pCall;
            call.tool.functor(call.arguments, call.result);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (call.result.m_deferred) {
                    m_deferred[call.result.m_callToken] = pCall;  // still running : complete() answers it
                } else {
                    call.state = CallState_Done;
                }
            }
            m_callChanged.notify_all();
        }
        return calls.size();
    }

    // Answers a call a tool deferred, on the calling thread.
    CompleteResult complete(CallToken aToken, const ToolResult& aResult) {
        std::unique_lock<std::mutex> lock(m_mutex);
        const auto found = m_deferred.find(aToken);
        if (found == m_deferred.end()) {
            return CompleteResult_UnknownToken;
        }
        const std::shared_ptr<PendingCall> call = found->second;
        m_deferred.erase(found);
        if (call->state == CallState_Cancelled) {
            return CompleteResult_Cancelled;
        }
        call->result = aResult;
        call->result.m_callToken = aToken;
        call->result.m_deferred = false;
        call->state = CallState_Done;
        lock.unlock();
        m_callChanged.notify_all();
        return CompleteResult_Completed;
    }

    // True while a tool call waits for the next poll() : an application that
    // sleeps between frames wakes up for it.
    bool hasQueuedCalls() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::any_of(m_pending.begin(), m_pending.end(),
            [](const std::shared_ptr<PendingCall>& apPending) { return apPending->state == CallState_Queued; });
    }

    // The whole HTTP answer to one request : what the server thread sends
    // back. Public so a request can be served without a socket.
    std::string handleRequest(const HttpRequest& aRequest) {
        const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        Exchange exchange;
        const std::string answer = answerRequest(aRequest, exchange);
        writeLog(aRequest.body, exchange, begin);
        return answer;
    }

private:
    // A token travels in a header : visible ASCII only, no space, and long
    // enough not to be guessed.
    static bool isTokenAccepted(const std::string& aToken) {
        if (aToken.size() < 32U) {
            return false;
        }
        return std::all_of(aToken.begin(), aToken.end(), [](char aChr) { return (aChr > ' ') && (aChr <= '~'); });
    }

    // The time taken never depends on where the strings differ.
    static bool isSameSecret(const std::string& aLeft, const std::string& aRight) {
        const size_t len = (aLeft.size() > aRight.size()) ? aLeft.size() : aRight.size();
        uint32_t diff = (aLeft.size() == aRight.size()) ? 0U : 1U;
        for (size_t idx = 0U; idx < len; ++idx) {
            const auto left = static_cast<uint8_t>((idx < aLeft.size()) ? aLeft[idx] : 0);
            const auto right = static_cast<uint8_t>((idx < aRight.size()) ? aRight[idx] : 0);
            diff |= static_cast<uint32_t>(left ^ right);
        }
        return diff == 0U;
    }

    // "Authorization: Bearer <token>", the scheme in any case.
    bool isAuthorized(const HttpRequest& aRequest) const {
        const std::string* pValue = aRequest.findHeader("authorization");
        if ((pValue == nullptr) || (pValue->size() < 7U)) {
            return false;
        }
        std::string scheme = pValue->substr(0U, 7U);
        for (auto& chr : scheme) {
            if ((chr >= 'A') && (chr <= 'Z')) {
                chr = static_cast<char>(chr - 'A' + 'a');
            }
        }
        if (scheme != "bearer ") {
            return false;
        }
        size_t begin = 7U;
        while ((begin < pValue->size()) && ((*pValue)[begin] == ' ')) {
            ++begin;
        }
        return isSameSecret(pValue->substr(begin), m_config.token);
    }

    static std::string reply(Exchange& aoExchange, int32_t aStatus, const char* aContentType, const std::string& aBody) {
        aoExchange.status = aStatus;
        aoExchange.response = aBody;
        return Http::makeResponse(aStatus, aContentType, aBody);
    }

    // The UTC time to the millisecond, ISO 8601.
    static std::string utcNow() {
        const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
        const auto millis = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
        std::tm parts{};
#ifdef WINDOWS_OS
        gmtime_s(&parts, &seconds);
#else
        gmtime_r(&seconds, &parts);
#endif
        char text[32];
        const size_t len = std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%S", &parts);
        std::snprintf(text + len, sizeof(text) - len, ".%03dZ", static_cast<int>(millis));
        return text;
    }

    // An image or a sound inside a body is replaced by its size.
    static JsonValue elideMedia(const JsonValue& aValue) {
        if (aValue.isArray()) {
            JsonValue items = JsonValue::makeArray();
            for (size_t idx = 0U; idx < aValue.size(); ++idx) {
                items.push(elideMedia(aValue.at(idx)));
            }
            return items;
        }
        if (!aValue.isObject()) {
            return aValue;
        }
        JsonValue members = JsonValue::makeObject();
        for (size_t idx = 0U; idx < aValue.size(); ++idx) {
            members.set(aValue.keyAt(idx), elideMedia(aValue.at(idx)));
        }
        const JsonValue* pType = aValue.find("type");
        const JsonValue* pData = aValue.find("data");
        if ((pType != nullptr) && ((pType->asString() == "image") || (pType->asString() == "audio")) && (pData != nullptr) && pData->isString()) {
            members.set("data", JsonValue::makeString("<base64, " + std::to_string(pData->asString().size()) + " chars>"));
        }
        return members;
    }

    // A body as the journal keeps it : media elided, cut beyond 4096 bytes.
    static JsonValue logBody(const std::string& aBody) {
        const size_t maxSize = 4096U;
        std::string text = aBody;
        JsonValue parsed;
        JsonError error;
        if (JsonValue::parse(aBody, parsed, error)) {
            text = elideMedia(parsed).dump();
        }
        if (text.size() > maxSize) {
            text = text.substr(0U, maxSize) + "...(+" + std::to_string(text.size() - maxSize) + " bytes)";
        }
        return JsonValue::makeString(text);
    }

    // One line of JSON for the exchange. Headers are never written : the token stays out.
    void writeLog(const std::string& aRequestBody, const Exchange& aExchange, std::chrono::steady_clock::time_point aBegin) {
        const LogLevel level = m_config.logLevel;
        if ((m_config.logFunctor == nullptr) || ((level != LogLevel_Exchanges) && (level != LogLevel_Bodies))) {
            return;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - aBegin).count();
        JsonValue line = JsonValue::makeObject();
        line.set("time", JsonValue::makeString(utcNow()));
        line.set("status", JsonValue::makeInteger(aExchange.status));
        if (!aExchange.rpc.empty()) {
            line.set("rpc", JsonValue::makeString(aExchange.rpc));
        }
        if (!aExchange.id.isNull()) {
            line.set("id", aExchange.id);
        }
        if (!aExchange.tool.empty()) {
            line.set("tool", JsonValue::makeString(aExchange.tool));
            if (aExchange.errorCode == JsonRpcErrorCode_None) {
                line.set("isError", JsonValue::makeBool(aExchange.isError));
            }
        }
        line.set("ms", JsonValue::makeInteger(static_cast<int64_t>(elapsed)));
        if (!aExchange.error.empty()) {
            line.set("error", JsonValue::makeString(aExchange.error));
        }
        if (level == LogLevel_Bodies) {
            line.set("request", logBody(aRequestBody));
            line.set("response", logBody(aExchange.response));
        }
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_config.logFunctor(line.dump());
    }

    std::string answerRequest(const HttpRequest& aRequest, Exchange& aoExchange) {
        if (!m_config.token.empty() && !isAuthorized(aRequest)) {
            aoExchange.error = "a bearer token is required";
            return reply(aoExchange, 401, "text/plain", aoExchange.error);
        }
        if (aRequest.findHeader("origin") != nullptr) {
            aoExchange.error = "a request from a web page is refused";
            return reply(aoExchange, 403, "text/plain", aoExchange.error);
        }
        if (aRequest.path != "/mcp") {
            aoExchange.error = "the only path is /mcp";
            return reply(aoExchange, 404, "text/plain", aoExchange.error);
        }
        if (aRequest.method != "POST") {
            aoExchange.error = "the only method is POST";
            return reply(aoExchange, 405, "text/plain", aoExchange.error);
        }
        const JsonRpcMessage message = JsonRpc::read(aRequest.body);
        aoExchange.rpc = message.method;
        aoExchange.id = message.id;
        if (message.kind == JsonRpcKind_Invalid) {
            aoExchange.error = message.errorMessage;
            return reply(aoExchange, 400, "application/json", JsonRpc::makeError(message.id, message.errorCode, message.errorMessage));
        }
        if (message.kind != JsonRpcKind_Request) {
            return reply(aoExchange, 202, "", std::string());  // notifications and responses need no answer
        }
        dispatch(message, aoExchange);
        if (aoExchange.errorCode != JsonRpcErrorCode_None) {
            return reply(aoExchange, 200, "application/json", JsonRpc::makeError(message.id, aoExchange.errorCode, aoExchange.error));
        }
        return reply(aoExchange, 200, "application/json", JsonRpc::makeResult(message.id, aoExchange.result));
    }

    static int32_t clampTimeout(int32_t aTimeoutMs) {
        if (aTimeoutMs < 10) {
            return 10;
        }
        return (aTimeoutMs > 600000) ? 600000 : aTimeoutMs;
    }

    void run() {
        while (m_running) {
            Socket client;
            if (m_listener.acceptClient(client, 100)) {
                serveClient(client);
            }
        }
    }

    // One request per connection : read it whole, answer, close.
    void serveClient(Socket& aoClient) {
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        std::string data;
        while (m_running && (std::chrono::steady_clock::now() < deadline)) {
            const SocketReceive status = aoClient.receive(data, 100);
            if ((status == SocketReceive_Closed) || (status == SocketReceive_Error)) {
                return;
            }
            if (status == SocketReceive_Timeout) {
                continue;
            }
            HttpRequest request;
            const HttpParseStatus parse = Http::parseRequest(data, request);
            if (parse == HttpParseStatus_Incomplete) {
                continue;
            }
            if (parse == HttpParseStatus_Invalid) {
                Exchange exchange;
                exchange.error = "malformed HTTP request";
                aoClient.sendAll(reply(exchange, 400, "text/plain", exchange.error));
                writeLog(std::string(), exchange, std::chrono::steady_clock::now());
                return;
            }
            aoClient.sendAll(handleRequest(request));
            return;
        }
    }

    void dispatch(const JsonRpcMessage& aMessage, Exchange& aoExchange) {
        if (aMessage.method == "initialize") {
            aoExchange.result = makeInitializeResult(aMessage.params);
        } else if (aMessage.method == "ping") {
            aoExchange.result = JsonValue::makeObject();
        } else if (aMessage.method == "tools/list") {
            aoExchange.result = makeToolsList();
        } else if (aMessage.method == "tools/call") {
            callTool(aMessage.params, aoExchange);
        } else {
            aoExchange.errorCode = JsonRpcErrorCode_MethodNotFound;
            aoExchange.error = "unknown method : " + aMessage.method;
        }
    }

    JsonValue makeInitializeResult(const JsonValue& aParams) const {
        // The client's version when spoken here, else the newest one.
        const std::vector<std::string>& versions = getProtocolVersions();
        std::string version = versions.front();
        const JsonValue* pAsked = aParams.find("protocolVersion");
        if (pAsked != nullptr) {
            for (const auto& known : versions) {
                if (known == pAsked->asString()) {
                    version = known;
                }
            }
        }
        JsonValue serverInfo = JsonValue::makeObject();
        serverInfo.set("name", JsonValue::makeString(m_config.name));
        serverInfo.set("version", JsonValue::makeString(m_config.version));
        JsonValue capabilities = JsonValue::makeObject();
        capabilities.set("tools", JsonValue::makeObject());
        JsonValue result = JsonValue::makeObject();
        result.set("protocolVersion", JsonValue::makeString(version));
        result.set("capabilities", capabilities);
        result.set("serverInfo", serverInfo);
        if (!m_config.instructions.empty()) {
            result.set("instructions", JsonValue::makeString(m_config.instructions));
        }
        return result;
    }

    JsonValue makeToolsList() {
        JsonValue tools = JsonValue::makeArray();
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& tool : m_tools) {
            JsonValue item = JsonValue::makeObject();
            item.set("name", JsonValue::makeString(tool.name));
            item.set("description", JsonValue::makeString(tool.description));
            if (tool.inputSchema.isObject()) {
                item.set("inputSchema", tool.inputSchema);
            } else {
                JsonValue schema = JsonValue::makeObject();
                schema.set("type", JsonValue::makeString("object"));
                item.set("inputSchema", schema);
            }
            tools.push(item);
        }
        JsonValue result = JsonValue::makeObject();
        result.set("tools", tools);
        return result;
    }

    // Queues the call for poll() and waits : until it is done, until the
    // timeout when no poll() picked it up, or until the server stops.
    void callTool(const JsonValue& aParams, Exchange& aoExchange) {
        const JsonValue* pName = aParams.find("name");
        if ((pName == nullptr) || !pName->isString()) {
            aoExchange.errorCode = JsonRpcErrorCode_InvalidParams;
            aoExchange.error = "tools/call needs a tool name";
            return;
        }
        aoExchange.tool = pName->asString();
        const JsonValue* pArguments = aParams.find("arguments");
        if ((pArguments != nullptr) && !pArguments->isObject()) {
            aoExchange.errorCode = JsonRpcErrorCode_InvalidParams;
            aoExchange.error = "the arguments of a tool are an object";
            return;
        }
        std::shared_ptr<PendingCall> call = std::make_shared<PendingCall>();
        call->arguments = (pArguments != nullptr) ? *pArguments : JsonValue::makeObject();
        std::unique_lock<std::mutex> lock(m_mutex);
        bool found = false;
        for (const auto& tool : m_tools) {
            if (tool.name == pName->asString()) {
                call->tool = tool;
                found = true;
                break;
            }
        }
        if (!found) {
            aoExchange.errorCode = JsonRpcErrorCode_InvalidParams;
            aoExchange.error = "unknown tool : " + pName->asString();
            return;
        }
        m_pending.push_back(call);
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(m_callTimeoutMs);
        while (call->state != CallState_Done) {
            if (!m_running) {
                call->state = CallState_Cancelled;
                aoExchange.errorCode = JsonRpcErrorCode_InternalError;
                aoExchange.error = "the server stops";
                return;
            }
            if (call->state == CallState_Queued) {
                if ((m_callChanged.wait_until(lock, deadline) == std::cv_status::timeout) && (call->state == CallState_Queued)) {
                    call->state = CallState_Cancelled;
                    aoExchange.errorCode = JsonRpcErrorCode_InternalError;
                    aoExchange.error = "the application did not poll within " + std::to_string(m_callTimeoutMs) + " ms";
                    return;
                }
            } else {
                // Picked up by poll() : a running tool, or a deferred one, is waited for, whatever its length.
                m_callChanged.wait(lock);
            }
        }
        aoExchange.result = call->result.toJson();
        aoExchange.isError = aoExchange.result.find("isError")->asBool();
    }
};

}  // namespace mcp
}  // namespace ez
// ---- end ezMcp/ezMcp.hpp ----
