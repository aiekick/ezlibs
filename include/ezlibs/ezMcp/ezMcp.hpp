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

#include "base64.hpp"
#include "http.hpp"
#include "json.hpp"
#include "jsonrpc.hpp"
#include "png.hpp"
#include "socket.hpp"

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
