#include <TestEzMcpServer.h>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMcp/ezMcp.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

using ez::mcp::HttpRequest;
using ez::mcp::JsonError;
using ez::mcp::JsonValue;
using ez::mcp::Server;
using ez::mcp::ServerConfig;
using ez::mcp::Socket;
using ez::mcp::Tool;
using ez::mcp::ToolResult;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

struct HttpAnswer {
    std::string body;
    JsonValue json;  // the body read as JSON, null when it is not
    int32_t status{};
};

// Splits a raw HTTP response into status and body.
static HttpAnswer readAnswer(const std::string& aRaw) {
    HttpAnswer answer;
    if (aRaw.compare(0U, 9U, "HTTP/1.1 ") == 0) {
        answer.status = static_cast<int32_t>(std::atoi(aRaw.substr(9U, 3U).c_str()));
    }
    const size_t headerEnd = aRaw.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        answer.body = aRaw.substr(headerEnd + 4U);
        JsonError error;
        JsonValue::parse(answer.body, answer.json, error);
    }
    return answer;
}

static HttpAnswer serve(Server& aoServer, const std::string& aMethod, const std::string& aPath, const std::string& aBody) {
    HttpRequest request;
    request.method = aMethod;
    request.path = aPath;
    request.body = aBody;
    return readAnswer(aoServer.handleRequest(request));
}

static HttpAnswer post(Server& aoServer, const std::string& aBody) {
    return serve(aoServer, "POST", "/mcp", aBody);
}

// The body of one POST /mcp sent through a real socket.
static std::string makePost(const std::string& aBody) {
    return "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\nAccept: application/json, "
           "text/event-stream\r\nContent-Length: " +
        std::to_string(aBody.size()) + "\r\n\r\n" + aBody;
}

// One raw exchange over a real connection : send, read until the server closes.
static HttpAnswer httpExchange(uint16_t aPort, const std::string& aRaw) {
    Socket client;
    if (!client.connectLocal(aPort) || !client.sendAll(aRaw)) {
        return HttpAnswer();
    }
    std::string data;
    for (size_t round = 0U; round < 100U; ++round) {
        const ez::mcp::SocketReceive status = client.receive(data, 100);
        if ((status == ez::mcp::SocketReceive_Closed) || (status == ez::mcp::SocketReceive_Error)) {
            break;
        }
    }
    return readAnswer(data);
}

static int64_t errorCode(const HttpAnswer& aAnswer) {
    const JsonValue* pError = aAnswer.json.find("error");
    return (pError != nullptr) ? pError->find("code")->asInteger() : 0;
}

static const JsonValue& result(const HttpAnswer& aAnswer) {
    static const JsonValue sNull;
    const JsonValue* pResult = aAnswer.json.find("result");
    return (pResult != nullptr) ? *pResult : sNull;
}

static void echoTool(const JsonValue& aArguments, ToolResult& aoResult, std::thread::id& aoCaller) {
    aoCaller = std::this_thread::get_id();
    const JsonValue* pText = aArguments.find("text");
    if ((pText == nullptr) || !pText->isString()) {
        aoResult.addText("echo needs a text");
        aoResult.setError(true);
        return;
    }
    aoResult.addText(pText->asString());
    const uint8_t pixels[] = {255U, 0U, 0U, 255U, 0U, 255U, 0U, 255U, 0U, 0U, 255U, 255U, 255U, 255U, 255U, 255U};
    aoResult.addImagePng(ez::mcp::Png::encodeRgba(std::vector<uint8_t>(pixels, pixels + 16), 2U, 2U));
}

static void slowTool(const JsonValue& /*aArguments*/, ToolResult& aoResult) {
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    aoResult.addText("slow done");
}

static void noopTool(const JsonValue& /*aArguments*/, ToolResult& aoResult) {
    aoResult.addText("done");
}

static Tool makeTool(const std::string& aName, const ez::mcp::ToolFunctor& aFunctor) {
    Tool tool;
    tool.name = aName;
    tool.description = "the tool " + aName;
    tool.functor = aFunctor;
    return tool;
}

// The echo tool, noting in aoCaller the thread it ran on : a lambda that captures.
static Tool makeEcho(std::thread::id& aoCaller) {
    return makeTool("echo", [&aoCaller](const JsonValue& aArguments, ToolResult& aoResult) { echoTool(aArguments, aoResult, aoCaller); });
}

static const char* const sCallEcho =
    "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"echo\",\"arguments\":{\"text\":\"bonjour\"}}}";

bool TestEzMcpServer_Initialize() {
    Server server;
    ServerConfig config;
    config.name = "testApp";
    config.version = "1.2.3";
    config.instructions = "drive the test app";
    CTEST_ASSERT(server.start(config));
    HttpAnswer answer = post(server,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{},\"clientInfo\":{"
        "\"name\":\"c\",\"version\":\"1\"}}}");
    CTEST_ASSERT(answer.status == 200);
    CTEST_ASSERT(answer.json.find("id")->asInteger() == 1);
    const JsonValue& init = result(answer);
    CTEST_ASSERT_MESSAGE(init.find("protocolVersion")->asString() == "2025-06-18", "the client's version when spoken here");
    CTEST_ASSERT(init.find("capabilities")->find("tools")->isObject());
    CTEST_ASSERT(init.find("serverInfo")->find("name")->asString() == "testApp");
    CTEST_ASSERT(init.find("serverInfo")->find("version")->asString() == "1.2.3");
    CTEST_ASSERT(init.find("instructions")->asString() == "drive the test app");
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"1999-01-01\"}}");
    CTEST_ASSERT_MESSAGE(result(answer).find("protocolVersion")->asString() == Server::getProtocolVersions().front(),
        "an unknown version gets the newest");
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"initialize\"}");
    CTEST_ASSERT_MESSAGE(result(answer).find("protocolVersion")->asString() == Server::getProtocolVersions().front(), "no params gets the newest");
    server.stop();
    Server quiet;
    CTEST_ASSERT(quiet.start(ServerConfig()));
    answer = post(quiet, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    CTEST_ASSERT_MESSAGE(result(answer).find("instructions") == nullptr, "no instructions, no member");
    return true;
}

bool TestEzMcpServer_Ping() {
    Server server;
    const HttpAnswer answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":\"p\",\"method\":\"ping\"}");
    CTEST_ASSERT(answer.status == 200);
    CTEST_ASSERT(answer.json.find("id")->asString() == "p");
    CTEST_ASSERT_MESSAGE(result(answer).isObject() && (result(answer).size() == 0U), "an empty result");
    return true;
}

bool TestEzMcpServer_ToolsList() {
    Server server;
    Tool second = makeTool("second", &noopTool);
    JsonError error;
    CTEST_ASSERT(
        JsonValue::parse("{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"number\"}},\"required\":[\"x\"]}", second.inputSchema, error));
    CTEST_ASSERT(server.addTool(makeTool("first", &noopTool)));
    CTEST_ASSERT(server.addTool(second));
    const HttpAnswer answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}");
    const JsonValue* pTools = result(answer).find("tools");
    CTEST_ASSERT((pTools != nullptr) && (pTools->size() == 2U));
    CTEST_ASSERT_MESSAGE(pTools->at(0U).find("name")->asString() == "first", "the order of registration");
    CTEST_ASSERT(pTools->at(0U).find("description")->asString() == "the tool first");
    CTEST_ASSERT_MESSAGE(pTools->at(0U).find("inputSchema")->dump() == "{\"type\":\"object\"}", "no schema : an object schema");
    CTEST_ASSERT(pTools->at(1U).find("inputSchema")->dump() == second.inputSchema.dump());
    return true;
}

bool TestEzMcpServer_AddToolRefusals() {
    Server server;
    CTEST_ASSERT_MESSAGE(!server.addTool(makeTool("", &noopTool)), "an empty name");
    CTEST_ASSERT_MESSAGE(!server.addTool(makeTool("x", ez::mcp::ToolFunctor())), "no functor");
    Tool arraySchema = makeTool("x", &noopTool);
    arraySchema.inputSchema = JsonValue::makeArray();
    CTEST_ASSERT_MESSAGE(!server.addTool(arraySchema), "a schema that is not an object");
    CTEST_ASSERT(server.addTool(makeTool("x", &noopTool)));
    CTEST_ASSERT_MESSAGE(!server.addTool(makeTool("x", &noopTool)), "a taken name");
    return true;
}

bool TestEzMcpServer_Routing() {
    Server server;
    HttpRequest fromPage;
    fromPage.method = "POST";
    fromPage.path = "/mcp";
    fromPage.body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}";
    fromPage.headers.push_back(std::make_pair(std::string("origin"), std::string("http://evil.example")));
    CTEST_ASSERT_MESSAGE(readAnswer(server.handleRequest(fromPage)).status == 403, "a web page is refused");
    CTEST_ASSERT(serve(server, "POST", "/other", "{}").status == 404);
    CTEST_ASSERT_MESSAGE(serve(server, "GET", "/mcp", "").status == 405, "no event stream");
    CTEST_ASSERT(serve(server, "DELETE", "/mcp", "").status == 405);
    HttpAnswer answer = post(server, "{oops");
    CTEST_ASSERT((answer.status == 400) && (errorCode(answer) == -32700));
    CTEST_ASSERT_MESSAGE(answer.json.find("id")->isNull(), "a parse error answers a null id");
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":7}");
    CTEST_ASSERT((answer.status == 400) && (errorCode(answer) == -32600));
    CTEST_ASSERT(answer.json.find("id")->asInteger() == 7);
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
    CTEST_ASSERT_MESSAGE((answer.status == 202) && answer.body.empty(), "a notification is accepted, nothing answered");
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":9,\"result\":{}}");
    CTEST_ASSERT_MESSAGE(answer.status == 202, "a response of the client is accepted");
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"resources/list\"}");
    CTEST_ASSERT((answer.status == 200) && (errorCode(answer) == -32601));
    return true;
}

bool TestEzMcpServer_CallErrors() {
    Server server;
    std::thread::id caller;
    CTEST_ASSERT(server.addTool(makeEcho(caller)));
    CTEST_ASSERT(server.start(ServerConfig()));
    HttpAnswer answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"nope\"}}");
    CTEST_ASSERT_MESSAGE(errorCode(answer) == -32602, "an unknown tool");
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{}}");
    CTEST_ASSERT_MESSAGE(errorCode(answer) == -32602, "no tool name");
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":5}}");
    CTEST_ASSERT(errorCode(answer) == -32602);
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"echo\",\"arguments\":[1]}}");
    CTEST_ASSERT_MESSAGE(errorCode(answer) == -32602, "arguments that are not an object");
    answer = post(server, "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\"}");
    CTEST_ASSERT(errorCode(answer) == -32602);
    CTEST_ASSERT_MESSAGE(server.poll() == 0U, "nothing refused reached the queue");
    server.stop();
    answer = post(server, sCallEcho);
    CTEST_ASSERT_MESSAGE(errorCode(answer) == -32603, "a stopped server runs no tool");
    return true;
}

bool TestEzMcpServer_CallTimeout() {
    Server server;
    std::thread::id caller;
    CTEST_ASSERT(server.addTool(makeEcho(caller)));
    ServerConfig config;
    config.callTimeoutMs = 1;  // clamped to 10
    CTEST_ASSERT(server.start(config));
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    const HttpAnswer answer = post(server, sCallEcho);  // no poll() : nobody runs it
    const int64_t elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
    CTEST_ASSERT(errorCode(answer) == -32603);
    CTEST_ASSERT_MESSAGE(answer.json.find("error")->find("message")->asString().find("within 10 ms") != std::string::npos,
        "the timeout of the config is clamped");
    CTEST_ASSERT(elapsedMs >= 9);
    CTEST_ASSERT_MESSAGE(server.poll() == 0U, "a call given up is never run later");
    CTEST_ASSERT(caller == std::thread::id());
    return true;
}

bool TestEzMcpServer_EndToEnd() {
    Server server;
    std::thread::id toolThread;
    CTEST_ASSERT(server.addTool(makeEcho(toolThread)));
    ServerConfig config;
    config.name = "endToEnd";
    CTEST_ASSERT(server.start(config));
    CTEST_ASSERT(server.isRunning());
    const uint16_t port = server.getPort();
    CTEST_ASSERT(port != 0U);
    std::vector<HttpAnswer> answers;
    std::atomic<bool> clientDone{false};
    std::thread clientThread([port, &answers, &clientDone]() {
        answers.push_back(
            httpExchange(port, makePost("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\"}}")));
        answers.push_back(httpExchange(port, makePost("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}")));
        answers.push_back(httpExchange(port, makePost("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}")));
        answers.push_back(httpExchange(port, makePost(sCallEcho)));
        answers.push_back(httpExchange(port, makePost("{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"echo\"}}")));
        clientDone = true;
    });
    // The application loop : one poll per frame.
    size_t ran = 0U;
    for (size_t frame = 0U; (frame < 5000U) && !clientDone; ++frame) {
        ran += server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    clientThread.join();
    server.stop();
    CTEST_ASSERT(!server.isRunning());
    CTEST_ASSERT(answers.size() == 5U);
    CTEST_ASSERT_MESSAGE(ran == 2U, "the two calls ran in poll()");
    CTEST_ASSERT_MESSAGE(toolThread == std::this_thread::get_id(), "on the thread that polls");
    CTEST_ASSERT((answers[0].status == 200) && (result(answers[0]).find("serverInfo")->find("name")->asString() == "endToEnd"));
    CTEST_ASSERT(result(answers[0]).find("protocolVersion")->asString() == "2025-11-25");
    CTEST_ASSERT(answers[1].status == 202);
    CTEST_ASSERT(result(answers[2]).find("tools")->at(0U).find("name")->asString() == "echo");
    const JsonValue& call = result(answers[3]);
    CTEST_ASSERT(!call.find("isError")->asBool());
    const JsonValue* pContent = call.find("content");
    CTEST_ASSERT((pContent != nullptr) && (pContent->size() == 2U));
    CTEST_ASSERT(pContent->at(0U).find("type")->asString() == "text");
    CTEST_ASSERT(pContent->at(0U).find("text")->asString() == "bonjour");
    CTEST_ASSERT(pContent->at(1U).find("type")->asString() == "image");
    CTEST_ASSERT(pContent->at(1U).find("mimeType")->asString() == "image/png");
    CTEST_ASSERT_MESSAGE(pContent->at(1U).find("data")->asString().compare(0U, 11U, "iVBORw0KGgo") == 0, "base64 of a PNG signature");
    const JsonValue& failed = result(answers[4]);
    CTEST_ASSERT_MESSAGE(failed.find("isError")->asBool(), "a tool that fails answers a result with isError, not a protocol error");
    CTEST_ASSERT(failed.find("content")->at(0U).find("text")->asString() == "echo needs a text");
    return true;
}

static HttpAnswer serveWithAuth(Server& aoServer, const char* apAuthorization, const char* apOrigin) {
    HttpRequest request;
    request.method = "POST";
    request.path = "/mcp";
    request.body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}";
    if (apAuthorization != nullptr) {
        request.headers.emplace_back("authorization", apAuthorization);
    }
    if (apOrigin != nullptr) {
        request.headers.emplace_back("origin", apOrigin);
    }
    return readAnswer(aoServer.handleRequest(request));
}

bool TestEzMcpServer_MakeToken() {
    const std::string first = Server::makeToken();
    const std::string second = Server::makeToken();
    CTEST_ASSERT(first.size() == 64U);
    CTEST_ASSERT_MESSAGE(first.find_first_not_of("0123456789abcdef") == std::string::npos, "hexadecimal only");
    CTEST_ASSERT_MESSAGE(first != second, "two tokens differ");
    ServerConfig config;
    config.token = first;
    Server server;
    CTEST_ASSERT_MESSAGE(server.start(config), "a made token is accepted");
    CTEST_ASSERT(server.getStartError().empty());
    return true;
}

bool TestEzMcpServer_TokenRefusedAtStart() {
    Server server;
    ServerConfig config;
    config.token = "short";
    CTEST_ASSERT_MESSAGE(!server.start(config), "a token under 32 characters");
    CTEST_ASSERT(server.getStartError().find("token") != std::string::npos);
    CTEST_ASSERT(!server.isRunning());
    config.token = std::string(31U, 'a');
    CTEST_ASSERT(!server.start(config));
    config.token = std::string(20U, 'a') + " " + std::string(20U, 'b');
    CTEST_ASSERT_MESSAGE(!server.start(config), "a space");
    config.token = std::string(40U, 'a') + "\n";
    CTEST_ASSERT_MESSAGE(!server.start(config), "a control character");
    config.token = std::string(40U, 'a') + "\xC3\xA9";
    CTEST_ASSERT_MESSAGE(!server.start(config), "a byte beyond ASCII");
    config.token = std::string(32U, 'a');
    CTEST_ASSERT_MESSAGE(server.start(config), "32 visible characters");
    CTEST_ASSERT(server.getStartError().empty());
    CTEST_ASSERT(!server.start(config));
    CTEST_ASSERT(server.getStartError() == "the server already runs");
    ServerConfig taken;
    taken.port = server.getPort();
    Server second;
    CTEST_ASSERT(!second.start(taken));
    CTEST_ASSERT(second.getStartError().find("cannot listen") != std::string::npos);
    return true;
}

bool TestEzMcpServer_Token() {
    const std::string token = Server::makeToken();
    const std::string bearer = "Bearer " + token;
    Server server;
    ServerConfig config;
    config.token = token;
    CTEST_ASSERT(server.start(config));
    HttpAnswer answer = serveWithAuth(server, nullptr, nullptr);
    CTEST_ASSERT_MESSAGE(answer.status == 401, "no header");
    CTEST_ASSERT(server.handleRequest(HttpRequest()).find("\r\nWWW-Authenticate: Bearer\r\n") != std::string::npos);
    CTEST_ASSERT_MESSAGE(serveWithAuth(server, ("Bearer " + Server::makeToken()).c_str(), nullptr).status == 401, "another token");
    CTEST_ASSERT_MESSAGE(serveWithAuth(server, ("Bearer " + token.substr(0U, 63U)).c_str(), nullptr).status == 401, "a token cut short");
    CTEST_ASSERT_MESSAGE(serveWithAuth(server, (bearer + "x").c_str(), nullptr).status == 401, "a token too long");
    CTEST_ASSERT_MESSAGE(serveWithAuth(server, ("Basic " + token).c_str(), nullptr).status == 401, "another scheme");
    CTEST_ASSERT_MESSAGE(serveWithAuth(server, token.c_str(), nullptr).status == 401, "no scheme");
    CTEST_ASSERT(serveWithAuth(server, "Bearer", nullptr).status == 401);
    CTEST_ASSERT(serveWithAuth(server, "Bearer ", nullptr).status == 401);
    answer = serveWithAuth(server, bearer.c_str(), nullptr);
    CTEST_ASSERT_MESSAGE((answer.status == 200) && (answer.json.find("id")->asInteger() == 1), "the right token");
    CTEST_ASSERT_MESSAGE(serveWithAuth(server, ("bEaReR   " + token).c_str(), nullptr).status == 200, "the scheme in any case, spaces after it");
    CTEST_ASSERT_MESSAGE(serveWithAuth(server, nullptr, "http://evil").status == 401, "the token is checked first");
    CTEST_ASSERT_MESSAGE(serveWithAuth(server, bearer.c_str(), "http://evil").status == 403, "a web page is still refused with the token");
    // Over a real connection.
    const std::string body = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\"}";
    const std::string raw =
        "POST /mcp HTTP/1.1\r\nAuthorization: " + bearer + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    CTEST_ASSERT(httpExchange(server.getPort(), raw).status == 200);
    CTEST_ASSERT(httpExchange(server.getPort(), makePost(body)).status == 401);
    return true;
}

// What the journal received, and on which threads.
struct LogSink {
    std::vector<std::string> lines;
    std::mutex mutex;
    std::vector<std::thread::id> threads;
    ez::mcp::LogFunctor functor() {
        return [this](const std::string& aLine) {
            std::lock_guard<std::mutex> lock(mutex);
            lines.push_back(aLine);
            threads.push_back(std::this_thread::get_id());
        };
    }
    JsonValue last() {
        std::lock_guard<std::mutex> lock(mutex);
        JsonValue line;
        JsonError error;
        if (!lines.empty()) {
            JsonValue::parse(lines.back(), line, error);
        }
        return line;
    }
    size_t count() {
        std::lock_guard<std::mutex> lock(mutex);
        return lines.size();
    }
};

// One POST over a real connection while this thread polls, like an application frame loop.
static HttpAnswer postWhilePolling(Server& aoServer, const std::string& aRaw) {
    HttpAnswer answer;
    std::atomic<bool> done{false};
    const uint16_t port = aoServer.getPort();
    std::thread client([port, &aRaw, &answer, &done]() {
        answer = httpExchange(port, aRaw);
        done = true;
    });
    for (size_t frame = 0U; (frame < 5000U) && !done; ++frame) {
        aoServer.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    client.join();
    return answer;
}

static ServerConfig makeLogConfig(LogSink& aoSink, ez::mcp::LogLevel aLevel) {
    ServerConfig config;
    config.logFunctor = aoSink.functor();
    config.logLevel = aLevel;
    return config;
}

bool TestEzMcpServer_LogOff() {
    const std::string ping = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}";
    LogSink sink;
    Server silent;
    CTEST_ASSERT(silent.start(makeLogConfig(sink, ez::mcp::LogLevel_None)));
    post(silent, ping);
    CTEST_ASSERT_MESSAGE(sink.count() == 0U, "level none : nothing");
    Server unknown;
    CTEST_ASSERT(unknown.start(makeLogConfig(sink, static_cast<ez::mcp::LogLevel>(7))));
    post(unknown, ping);
    CTEST_ASSERT_MESSAGE(sink.count() == 0U, "a level out of the enum counts as none");
    ServerConfig noFunctor;
    noFunctor.logLevel = ez::mcp::LogLevel_Bodies;
    Server orphan;
    CTEST_ASSERT(orphan.start(noFunctor));
    CTEST_ASSERT_MESSAGE(post(orphan, ping).status == 200, "a level without functor serves as usual");
    return true;
}

bool TestEzMcpServer_LogExchanges() {
    LogSink sink;
    Server server;
    CTEST_ASSERT(server.start(makeLogConfig(sink, ez::mcp::LogLevel_Exchanges)));
    post(server, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}");
    CTEST_ASSERT(sink.count() == 1U);
    JsonValue line = sink.last();
    CTEST_ASSERT_MESSAGE(line.isObject(), "one line of JSON");
    const std::string time = line.find("time")->asString();
    CTEST_ASSERT_MESSAGE((time.size() == 24U) && (time[10] == 'T') && (time[19] == '.') && (time[23] == 'Z'), "ISO 8601 UTC to the millisecond");
    CTEST_ASSERT(line.find("status")->asInteger() == 200);
    CTEST_ASSERT(line.find("rpc")->asString() == "ping");
    CTEST_ASSERT(line.find("id")->asInteger() == 1);
    CTEST_ASSERT(line.find("ms")->isInteger() && (line.find("ms")->asInteger() >= 0));
    CTEST_ASSERT_MESSAGE((line.find("error") == nullptr) && (line.find("tool") == nullptr), "nothing that did not happen");
    CTEST_ASSERT_MESSAGE((line.find("request") == nullptr) && (line.find("response") == nullptr), "no body at this level");
    post(server, "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
    line = sink.last();
    CTEST_ASSERT(
        (line.find("status")->asInteger() == 202) && (line.find("rpc")->asString() == "notifications/initialized") && (line.find("id") == nullptr));
    post(server, "{\"jsonrpc\":\"2.0\",\"id\":\"a\",\"method\":\"resources/list\"}");
    line = sink.last();
    CTEST_ASSERT((line.find("status")->asInteger() == 200) && (line.find("id")->asString() == "a"));
    CTEST_ASSERT(line.find("error")->asString() == "unknown method : resources/list");
    post(server, "{oops");
    line = sink.last();
    CTEST_ASSERT((line.find("status")->asInteger() == 400) && (line.find("rpc") == nullptr));
    CTEST_ASSERT(line.find("error")->asString().find("parse error") == 0U);
    serve(server, "GET", "/mcp", "");
    CTEST_ASSERT((sink.last().find("status")->asInteger() == 405) && (sink.last().find("error")->asString() == "the only method is POST"));
    serve(server, "POST", "/x", "");
    CTEST_ASSERT(sink.last().find("status")->asInteger() == 404);
    post(server, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"nope\"}}");
    line = sink.last();
    CTEST_ASSERT_MESSAGE((line.find("tool")->asString() == "nope") && (line.find("isError") == nullptr), "a refused call has no isError");
    CTEST_ASSERT(line.find("error")->asString() == "unknown tool : nope");
    CTEST_ASSERT(sink.count() == 7U);
    return true;
}

bool TestEzMcpServer_LogToolCalls() {
    LogSink sink;
    Server server;
    std::thread::id caller;
    CTEST_ASSERT(server.addTool(makeEcho(caller)));
    CTEST_ASSERT(server.start(makeLogConfig(sink, ez::mcp::LogLevel_Exchanges)));
    CTEST_ASSERT(postWhilePolling(server, makePost(sCallEcho)).status == 200);
    JsonValue line = sink.last();
    CTEST_ASSERT((line.find("tool")->asString() == "echo") && !line.find("isError")->asBool());
    CTEST_ASSERT(line.find("rpc")->asString() == "tools/call");
    CTEST_ASSERT(
        postWhilePolling(server, makePost("{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"echo\"}}")).status == 200);
    CTEST_ASSERT_MESSAGE(sink.last().find("isError")->asBool(), "a tool that failed says so");
    CTEST_ASSERT(httpExchange(server.getPort(), "NONSENSE\r\n\r\n").status == 400);
    for (size_t round = 0U; (round < 100U) && (sink.count() < 3U); ++round) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    line = sink.last();
    CTEST_ASSERT_MESSAGE((line.find("status")->asInteger() == 400) && (line.find("error")->asString() == "malformed HTTP request"),
        "a malformed request is journaled too");
    server.stop();
    CTEST_ASSERT(sink.count() == 3U);
    for (const auto& thread : sink.threads) {
        CTEST_ASSERT_MESSAGE(thread != std::this_thread::get_id(), "called on the server thread, not the polling one");
    }
    return true;
}

bool TestEzMcpServer_LogBodies() {
    LogSink sink;
    Server server;
    std::thread::id caller;
    CTEST_ASSERT(server.addTool(makeEcho(caller)));
    ServerConfig config = makeLogConfig(sink, ez::mcp::LogLevel_Bodies);
    config.token = Server::makeToken();
    CTEST_ASSERT(server.start(config));
    const std::string authorized = "POST /mcp HTTP/1.1\r\nAuthorization: Bearer " + config.token + "\r\nContent-Length: ";
    const std::string body = sCallEcho;
    CTEST_ASSERT(postWhilePolling(server, authorized + std::to_string(body.size()) + "\r\n\r\n" + body).status == 200);
    JsonValue line = sink.last();
    CTEST_ASSERT_MESSAGE(line.find("request")->asString().find("bonjour") != std::string::npos, "the request body");
    const std::string response = line.find("response")->asString();
    CTEST_ASSERT_MESSAGE(response.find("bonjour") != std::string::npos, "the response body");
    CTEST_ASSERT_MESSAGE(response.find("<base64, ") != std::string::npos, "the image replaced by its size");
    CTEST_ASSERT_MESSAGE(response.find("iVBOR") == std::string::npos, "no image data");
    CTEST_ASSERT_MESSAGE(response.find("\"mimeType\":\"image/png\"") != std::string::npos, "the rest of the image item stays");
    const std::string longBody = "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"echo\",\"arguments\":{\"text\":\"" +
        std::string(10000U, 'z') + "\"}}}";
    CTEST_ASSERT(postWhilePolling(server, authorized + std::to_string(longBody.size()) + "\r\n\r\n" + longBody).status == 200);
    line = sink.last();
    const std::string cut = line.find("request")->asString();
    CTEST_ASSERT_MESSAGE((cut.size() < 4200U) && (cut.find("...(+") != std::string::npos), "a long body is cut and says how much");
    CTEST_ASSERT(httpExchange(server.getPort(), makePost(body)).status == 401);
    CTEST_ASSERT(sink.last().find("status")->asInteger() == 401);
    CTEST_ASSERT(sink.last().find("error")->asString() == "a bearer token is required");
    for (const auto& text : sink.lines) {
        CTEST_ASSERT_MESSAGE(text.find(config.token) == std::string::npos, "the token is never journaled");
    }
    return true;
}

// A tool that answers later when asked to : it keeps the token of its call.
static Tool makeLater(std::atomic<ez::mcp::CallToken>& aoToken) {
    return makeTool("later", [&aoToken](const JsonValue& aArguments, ToolResult& aoResult) {
        const JsonValue* pLater = aArguments.find("later");
        if ((pLater != nullptr) && pLater->asBool()) {
            aoToken = aoResult.defer();
            return;
        }
        aoResult.addText("at once");
    });
}

static const char* const sCallLater =
    "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/call\",\"params\":{\"name\":\"later\",\"arguments\":{\"later\":true}}}";

bool TestEzMcpServer_DeferredAnswer() {
    Server server;
    std::atomic<ez::mcp::CallToken> token{0U};
    CTEST_ASSERT(server.addTool(makeLater(token)));
    ServerConfig config;
    config.callTimeoutMs = 100;  // a deferred call outlives it : it was picked up
    CTEST_ASSERT(server.start(config));
    HttpAnswer answer;
    std::atomic<bool> done{false};
    const uint16_t port = server.getPort();
    std::thread client([port, &answer, &done]() {
        answer = httpExchange(port, makePost(sCallLater));
        done = true;
    });
    // the application loop : the tool defers, the answer comes 30 frames later
    size_t framesSinceDefer = 0U;
    ez::mcp::CompleteResult completed = ez::mcp::CompleteResult_UnknownToken;
    for (size_t frame = 0U; (frame < 5000U) && !done; ++frame) {
        server.poll();
        if (token != 0U) {
            ++framesSinceDefer;
            if (framesSinceDefer == 30U) {
                ToolResult late;
                late.addText("done later");
                completed = server.complete(token, late);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    client.join();
    CTEST_ASSERT(token != 0U);
    CTEST_ASSERT_MESSAGE(framesSinceDefer >= 30U, "nothing answered before complete()");
    CTEST_ASSERT(completed == ez::mcp::CompleteResult_Completed);
    CTEST_ASSERT_MESSAGE((answer.status == 200) && (errorCode(answer) == 0), "no timeout for a deferred call");
    CTEST_ASSERT(answer.json.find("id")->asInteger() == 7);
    CTEST_ASSERT(result(answer).find("content")->at(0U).find("text")->asString() == "done later");
    ToolResult again;
    CTEST_ASSERT_MESSAGE(server.complete(token, again) == ez::mcp::CompleteResult_UnknownToken, "a token answers once");
    CTEST_ASSERT(server.complete(0U, again) == ez::mcp::CompleteResult_UnknownToken);
    CTEST_ASSERT(server.complete(123456U, again) == ez::mcp::CompleteResult_UnknownToken);
    // the same tool answers at once when not asked to wait : the mode is chosen per call
    const HttpAnswer atOnce =
        postWhilePolling(server, makePost("{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/call\",\"params\":{\"name\":\"later\"}}"));
    CTEST_ASSERT(result(atOnce).find("content")->at(0U).find("text")->asString() == "at once");
    return true;
}

bool TestEzMcpServer_DeferredThenStop() {
    Server server;
    std::atomic<ez::mcp::CallToken> token{0U};
    CTEST_ASSERT(server.addTool(makeLater(token)));
    CTEST_ASSERT(server.start(ServerConfig()));
    HttpAnswer answer;
    const uint16_t port = server.getPort();
    std::thread client([port, &answer]() { answer = httpExchange(port, makePost(sCallLater)); });
    for (size_t frame = 0U; (frame < 2000U) && (token == 0U); ++frame) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CTEST_ASSERT(token != 0U);
    server.stop();
    client.join();
    CTEST_ASSERT_MESSAGE(errorCode(answer) == -32603, "the waiting client is told the server stops");
    ToolResult late;
    late.addText("too late");
    CTEST_ASSERT_MESSAGE(server.complete(token, late) == ez::mcp::CompleteResult_Cancelled, "complete() says the call was cancelled");
    CTEST_ASSERT(server.complete(token, late) == ez::mcp::CompleteResult_UnknownToken);
    CTEST_ASSERT(server.start(ServerConfig()));
    CTEST_ASSERT(server.complete(token, late) == ez::mcp::CompleteResult_UnknownToken);
    return true;
}

bool TestEzMcpServer_DeferOutsidePoll() {
    ToolResult alone;
    CTEST_ASSERT_MESSAGE(alone.defer() == 0U, "no call, no token");
    alone.addText("still a result");
    CTEST_ASSERT(alone.toJson().find("content")->size() == 1U);
    return true;
}

bool TestEzMcpServer_HasQueuedCalls() {
    Server server;
    std::thread::id caller;
    CTEST_ASSERT(server.addTool(makeEcho(caller)));
    ServerConfig config;
    config.callTimeoutMs = 5000;
    CTEST_ASSERT(server.start(config));
    CTEST_ASSERT(!server.hasQueuedCalls());
    HttpAnswer answer;
    const uint16_t port = server.getPort();
    std::thread client([port, &answer]() { answer = httpExchange(port, makePost(sCallEcho)); });
    bool queued = false;
    for (size_t round = 0U; (round < 300U) && !queued; ++round) {
        queued = server.hasQueuedCalls();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CTEST_ASSERT_MESSAGE(queued, "a call waits for the next poll");
    CTEST_ASSERT(server.poll() == 1U);
    CTEST_ASSERT_MESSAGE(!server.hasQueuedCalls(), "the poll took it");
    client.join();
    CTEST_ASSERT(answer.status == 200);
    return true;
}

// A tool picked up by poll() is waited for to its end, whatever the call timeout.
bool TestEzMcpServer_SlowToolOutlivesTimeout() {
    Server server;
    CTEST_ASSERT(server.addTool(makeTool("slow", &slowTool)));
    ServerConfig config;
    config.callTimeoutMs = 100;
    CTEST_ASSERT(server.start(config));
    const uint16_t port = server.getPort();
    HttpAnswer answer;
    std::atomic<bool> clientDone{false};
    std::thread clientThread([port, &answer, &clientDone]() {
        answer = httpExchange(port, makePost("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"slow\"}}"));
        clientDone = true;
    });
    size_t ran = 0U;
    for (size_t frame = 0U; (frame < 3000U) && !clientDone; ++frame) {
        ran += server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    clientThread.join();
    CTEST_ASSERT(ran == 1U);
    CTEST_ASSERT_MESSAGE((answer.status == 200) && (errorCode(answer) == 0), "no timeout once the tool runs");
    CTEST_ASSERT(result(answer).find("content")->at(0U).find("text")->asString() == "slow done");
    return true;
}

bool TestEzMcpServer_StopWhileWaiting() {
    Server server;
    std::thread::id caller;
    CTEST_ASSERT(server.addTool(makeEcho(caller)));
    ServerConfig config;
    config.callTimeoutMs = 60000;
    CTEST_ASSERT(server.start(config));
    const uint16_t port = server.getPort();
    HttpAnswer answer;
    std::thread clientThread([port, &answer]() { answer = httpExchange(port, makePost(sCallEcho)); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));  // the call waits in the queue
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    server.stop();
    const int64_t stopMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
    clientThread.join();
    CTEST_ASSERT_MESSAGE(stopMs < 2000, "stop does not wait for the call timeout");
    CTEST_ASSERT_MESSAGE((answer.status == 200) && (errorCode(answer) == -32603), "the waiting client is told the server stops");
    CTEST_ASSERT(caller == std::thread::id());
    CTEST_ASSERT_MESSAGE(server.start(config), "a stopped server starts again");
    CTEST_ASSERT(server.getPort() != 0U);
    return true;
}

bool TestEzMcpServer_MalformedHttp() {
    Server server;
    CTEST_ASSERT(server.start(ServerConfig()));
    ServerConfig taken;
    taken.port = server.getPort();
    Server second;
    CTEST_ASSERT_MESSAGE(!second.start(taken), "a port in use fails the start");
    CTEST_ASSERT(!second.isRunning());
    CTEST_ASSERT_MESSAGE(!server.start(ServerConfig()), "a running server does not start twice");
    const HttpAnswer bad = httpExchange(server.getPort(), "NONSENSE\r\n\r\n");
    CTEST_ASSERT(bad.status == 400);
    const HttpAnswer page = httpExchange(server.getPort(), "POST /mcp HTTP/1.1\r\nOrigin: http://evil\r\nContent-Length: 2\r\n\r\n{}");
    CTEST_ASSERT(page.status == 403);
    // A request sent in pieces is read whole.
    Socket client;
    CTEST_ASSERT(client.connectLocal(server.getPort()));
    const std::string raw = makePost("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}");
    CTEST_ASSERT(client.sendAll(raw.substr(0U, 20U)));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    CTEST_ASSERT(client.sendAll(raw.substr(20U)));
    std::string data;
    for (size_t round = 0U; round < 50U; ++round) {
        if (client.receive(data, 100) == ez::mcp::SocketReceive_Closed) {
            break;
        }
    }
    CTEST_ASSERT(readAnswer(data).status == 200);
    // A client that leaves without a word is let go.
    Socket leaving;
    CTEST_ASSERT(leaving.connectLocal(server.getPort()));
    leaving.close();
    CTEST_ASSERT(httpExchange(server.getPort(), makePost("{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"ping\"}")).status == 200);
    // A client that connects and says nothing does not block the next one.
    Socket silent;
    CTEST_ASSERT(silent.connectLocal(server.getPort()));
    const HttpAnswer after = httpExchange(server.getPort(), makePost("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\"}"));
    CTEST_ASSERT_MESSAGE(after.status == 200, "served once the silent one timed out");
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpServer(const std::string& vTest) {
    IfTestExist(TestEzMcpServer_Initialize);
    else IfTestExist(TestEzMcpServer_Ping);
    else IfTestExist(TestEzMcpServer_ToolsList);
    else IfTestExist(TestEzMcpServer_AddToolRefusals);
    else IfTestExist(TestEzMcpServer_Routing);
    else IfTestExist(TestEzMcpServer_CallErrors);
    else IfTestExist(TestEzMcpServer_CallTimeout);
    else IfTestExist(TestEzMcpServer_EndToEnd);
    else IfTestExist(TestEzMcpServer_MakeToken);
    else IfTestExist(TestEzMcpServer_TokenRefusedAtStart);
    else IfTestExist(TestEzMcpServer_Token);
    else IfTestExist(TestEzMcpServer_LogOff);
    else IfTestExist(TestEzMcpServer_LogExchanges);
    else IfTestExist(TestEzMcpServer_LogToolCalls);
    else IfTestExist(TestEzMcpServer_LogBodies);
    else IfTestExist(TestEzMcpServer_DeferredAnswer);
    else IfTestExist(TestEzMcpServer_DeferredThenStop);
    else IfTestExist(TestEzMcpServer_DeferOutsidePoll);
    else IfTestExist(TestEzMcpServer_HasQueuedCalls);
    else IfTestExist(TestEzMcpServer_SlowToolOutlivesTimeout);
    else IfTestExist(TestEzMcpServer_StopWhileWaiting);
    else IfTestExist(TestEzMcpServer_MalformedHttp);
    return false;
}
