#include <TestEzMcpHttp.h>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMcp/http.hpp>

#include <cstdio>

using ez::mcp::Http;
using ez::mcp::HttpRequest;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

static ez::mcp::HttpParseStatus parseText(const std::string& aData) {
    HttpRequest request;
    return Http::parseRequest(aData, request);
}

static const char* const sFullRequest = "POST /mcp?session=1 HTTP/1.1\r\n"
                                        "Host: 127.0.0.1:7810\r\n"
                                        "Content-Type: application/json\r\n"
                                        "Content-Length: 2\r\n"
                                        "\r\n"
                                        "{}";

bool TestEzMcpHttp_Complete() {
    HttpRequest request;
    CTEST_ASSERT(Http::parseRequest(sFullRequest, request) == ez::mcp::HttpParseStatus_Complete);
    CTEST_ASSERT(request.method == "POST");
    CTEST_ASSERT_MESSAGE(request.path == "/mcp", "the query is not part of the path");
    CTEST_ASSERT(request.body == "{}");
    CTEST_ASSERT(request.headers.size() == 3U);
    CTEST_ASSERT((request.findHeader("host") != nullptr) && (*request.findHeader("host") == "127.0.0.1:7810"));
    CTEST_ASSERT(*request.findHeader("content-type") == "application/json");
    CTEST_ASSERT(request.findHeader("origin") == nullptr);
    CTEST_ASSERT_MESSAGE(Http::parseRequest(std::string(sFullRequest) + "extra", request) == ez::mcp::HttpParseStatus_Complete,
        "bytes after the body are not read");
    CTEST_ASSERT(request.body == "{}");
    CTEST_ASSERT(Http::parseRequest("GET / HTTP/1.0\r\n\r\n", request) == ez::mcp::HttpParseStatus_Complete);
    return true;
}

bool TestEzMcpHttp_HeaderCase() {
    HttpRequest request;
    CTEST_ASSERT(Http::parseRequest("POST /mcp HTTP/1.1\r\nCoNtEnT-LeNgTh: \t 3 \t\r\nORIGIN:http://evil\r\n\r\nabc", request) ==
        ez::mcp::HttpParseStatus_Complete);
    CTEST_ASSERT_MESSAGE(request.headers[0].first == "content-length", "names in lower case");
    CTEST_ASSERT_MESSAGE(*request.findHeader("content-length") == "3", "values trimmed");
    CTEST_ASSERT(*request.findHeader("origin") == "http://evil");
    CTEST_ASSERT(request.body == "abc");
    return true;
}

bool TestEzMcpHttp_Incomplete() {
    const std::string full(sFullRequest);
    for (size_t len = 0U; len < full.size(); ++len) {
        if (parseText(full.substr(0U, len)) != ez::mcp::HttpParseStatus_Incomplete) {
            printf("a prefix of %u bytes is not incomplete\n", static_cast<unsigned>(len));
            return false;
        }
    }
    CTEST_ASSERT(parseText(full) == ez::mcp::HttpParseStatus_Complete);
    return true;
}

bool TestEzMcpHttp_NoBody() {
    HttpRequest request;
    CTEST_ASSERT(Http::parseRequest("GET /mcp HTTP/1.1\r\nAccept: text/event-stream\r\n\r\n", request) == ez::mcp::HttpParseStatus_Complete);
    CTEST_ASSERT(request.method == "GET");
    CTEST_ASSERT(request.body.empty());
    CTEST_ASSERT(Http::parseRequest("POST /mcp HTTP/1.1\r\nContent-Length: 0\r\n\r\n", request) == ez::mcp::HttpParseStatus_Complete);
    CTEST_ASSERT(request.body.empty());
    return true;
}

bool TestEzMcpHttp_Invalid() {
    CTEST_ASSERT_MESSAGE(parseText("POST /mcp\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid, "no version");
    CTEST_ASSERT(parseText("POST  HTTP/1.1\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT(parseText(" /mcp HTTP/1.1\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT(parseText("POST /mcp HTTP/2\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT(parseText("POST /mcp HTTP/1.1 x\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT_MESSAGE(parseText("POST /mcp HTTP/1.1\r\nNoColon\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid, "a header without colon");
    CTEST_ASSERT(parseText("POST /mcp HTTP/1.1\r\n: value\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT_MESSAGE(parseText("POST /mcp HTTP/1.1\r\nContent-Length : 2\r\n\r\n{}") == ez::mcp::HttpParseStatus_Invalid,
        "no space before the colon");
    CTEST_ASSERT_MESSAGE(parseText("POST /mcp HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n\r\n") ==
            ez::mcp::HttpParseStatus_Invalid,
        "no chunked body");
    CTEST_ASSERT(parseText("POST /mcp HTTP/1.1\r\nContent-Length: abc\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT(parseText("POST /mcp HTTP/1.1\r\nContent-Length: -1\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT(parseText("POST /mcp HTTP/1.1\r\nContent-Length:\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT_MESSAGE(parseText("POST /mcp HTTP/1.1\r\nContent-Length: 16777217\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid,
        "a body over 16 MiB");
    CTEST_ASSERT(parseText("POST /mcp HTTP/1.1\r\nContent-Length: 9999999999\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT_MESSAGE(parseText("POST /mcp HTTP/1.1\r\nX: " + std::string(70000U, 'a')) == ez::mcp::HttpParseStatus_Invalid,
        "headers over 64 KiB, never ended");
    CTEST_ASSERT(parseText("POST /mcp HTTP/1.1\r\nX: " + std::string(70000U, 'a') + "\r\n\r\n") == ez::mcp::HttpParseStatus_Invalid);
    HttpRequest request;
    request.method = "untouched";
    CTEST_ASSERT(Http::parseRequest("BAD\r\n\r\n", request) == ez::mcp::HttpParseStatus_Invalid);
    CTEST_ASSERT_MESSAGE(request.method == "untouched", "a refused request leaves the output as it was");
    return true;
}

bool TestEzMcpHttp_Response() {
    CTEST_ASSERT(Http::makeResponse(200, "application/json", "{}") ==
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}");
    CTEST_ASSERT_MESSAGE(Http::makeResponse(202, "", "") == "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        "no content type without a body");
    const std::string notAllowed = Http::makeResponse(405, "text/plain", "x");
    CTEST_ASSERT_MESSAGE(notAllowed.find("HTTP/1.1 405 Method Not Allowed\r\n") == 0U, "the status line");
    CTEST_ASSERT_MESSAGE(notAllowed.find("\r\nAllow: POST\r\n") != std::string::npos, "a 405 says what is allowed");
    CTEST_ASSERT(Http::makeResponse(400, "text/plain", "").find(" 400 Bad Request\r\n") != std::string::npos);
    const std::string unauthorized = Http::makeResponse(401, "text/plain", "");
    CTEST_ASSERT(unauthorized.find(" 401 Unauthorized\r\n") != std::string::npos);
    CTEST_ASSERT_MESSAGE(unauthorized.find("\r\nWWW-Authenticate: Bearer\r\n") != std::string::npos, "a 401 names its scheme");
    CTEST_ASSERT(Http::makeResponse(403, "text/plain", "").find(" 403 Forbidden\r\n") != std::string::npos);
    CTEST_ASSERT(Http::makeResponse(404, "text/plain", "").find(" 404 Not Found\r\n") != std::string::npos);
    CTEST_ASSERT(Http::makeResponse(500, "text/plain", "").find(" 500 Internal Server Error\r\n") != std::string::npos);
    CTEST_ASSERT(Http::makeResponse(418, "text/plain", "").find(" 418 Unknown\r\n") != std::string::npos);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpHttp(const std::string& vTest) {
    IfTestExist(TestEzMcpHttp_Complete);
    else IfTestExist(TestEzMcpHttp_HeaderCase);
    else IfTestExist(TestEzMcpHttp_Incomplete);
    else IfTestExist(TestEzMcpHttp_NoBody);
    else IfTestExist(TestEzMcpHttp_Invalid);
    else IfTestExist(TestEzMcpHttp_Response);
    return false;
}
