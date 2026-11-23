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
