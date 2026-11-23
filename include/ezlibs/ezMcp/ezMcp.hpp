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
//   tool.functor = &sayHello;  // void sayHello(const JsonValue&, ToolResult&, void*)
//   server.addTool(tool);
//   ez::mcp::ServerConfig config;
//   config.name = "myApp";
//   config.port = 7810;
//   server.start(config);
//   while (running) { server.poll(); ...frame... }
//   server.stop();

#include "base64.hpp"
#include "http.hpp"
#include "json.hpp"
#include "jsonrpc.hpp"
#include "png.hpp"
#include "socket.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ez {
namespace mcp {

// What a tool answers : text and images, in the order they were added. An
// error result tells the client the tool ran and failed (bad arguments, a
// state that does not allow it).
class ToolResult {
private:
    std::vector<JsonValue> m_content;
    bool m_isError{};

public:
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

typedef void (*ToolFunctor)(const JsonValue& aArguments, ToolResult& aoResult, void* apUserData);

struct Tool {
    std::string description;
    ToolFunctor functor{};
    JsonValue inputSchema;  // a JSON schema object ; null means an object with no declared property
    std::string name;
    void* userData{};
};

struct ServerConfig {
    int32_t callTimeoutMs{5000};  // how long a tools/call waits for a poll(), clamped to [10, 600000]
    std::string instructions;     // told to the client at initialize, omitted when empty
    std::string name;             // serverInfo.name
    uint16_t port{};              // 0 : the system picks a free port, read back by getPort()
    std::string version;          // serverInfo.version
};

class Server {
public:
    // The protocol versions this server speaks, the newest first.
    static const std::vector<std::string>& getProtocolVersions() {
        static const std::vector<std::string> sVersions = {"2025-11-25", "2025-06-18", "2025-03-26"};
        return sVersions;
    }

private:
    enum CallState { CallState_Cancelled = 0, CallState_Done, CallState_Queued, CallState_Running };

    struct PendingCall {
        JsonValue arguments;
        ToolResult result;
        CallState state{CallState_Queued};
        Tool tool;
    };

    std::condition_variable m_callChanged;
    int32_t m_callTimeoutMs{5000};
    ServerConfig m_config;
    Socket m_listener;
    std::mutex m_mutex;
    std::vector<std::shared_ptr<PendingCall>> m_pending;
    std::atomic<bool> m_running{false};
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

    // Listens on 127.0.0.1:port and starts the server thread.
    bool start(const ServerConfig& aConfig) {
        if (m_running) {
            return false;
        }
        m_config = aConfig;
        m_callTimeoutMs = clampTimeout(aConfig.callTimeoutMs);
        if (!m_listener.listenLocal(aConfig.port)) {
            return false;
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
                    calls.push_back(pending);
                }
            }
            m_pending.clear();
        }
        for (const auto& pCall : calls) {
            PendingCall& call = *pCall;
            call.tool.functor(call.arguments, call.result, call.tool.userData);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                call.state = CallState_Done;
            }
            m_callChanged.notify_all();
        }
        return calls.size();
    }

    // The whole HTTP answer to one request : what the server thread sends
    // back. Public so a request can be served without a socket.
    std::string handleRequest(const HttpRequest& aRequest) {
        if (aRequest.findHeader("origin") != nullptr) {
            return Http::makeResponse(403, "text/plain", "a request from a web page is refused");
        }
        if (aRequest.path != "/mcp") {
            return Http::makeResponse(404, "text/plain", "the only path is /mcp");
        }
        if (aRequest.method != "POST") {
            return Http::makeResponse(405, "text/plain", "the only method is POST");
        }
        const JsonRpcMessage message = JsonRpc::read(aRequest.body);
        if (message.kind == JsonRpcKind_Invalid) {
            return Http::makeResponse(400, "application/json", JsonRpc::makeError(message.id, message.errorCode, message.errorMessage));
        }
        if (message.kind != JsonRpcKind_Request) {
            return Http::makeResponse(202, std::string(), std::string());  // notifications and responses need no answer
        }
        JsonValue result;
        JsonRpcErrorCode errorCode = JsonRpcErrorCode_None;
        std::string errorMessage;
        dispatch(message, result, errorCode, errorMessage);
        if (errorCode != JsonRpcErrorCode_None) {
            return Http::makeResponse(200, "application/json", JsonRpc::makeError(message.id, errorCode, errorMessage));
        }
        return Http::makeResponse(200, "application/json", JsonRpc::makeResult(message.id, result));
    }

private:
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
                aoClient.sendAll(Http::makeResponse(400, "text/plain", "malformed HTTP request"));
                return;
            }
            aoClient.sendAll(handleRequest(request));
            return;
        }
    }

    void dispatch(const JsonRpcMessage& aMessage, JsonValue& aoResult, JsonRpcErrorCode& aoErrorCode, std::string& aoErrorMessage) {
        if (aMessage.method == "initialize") {
            aoResult = makeInitializeResult(aMessage.params);
        } else if (aMessage.method == "ping") {
            aoResult = JsonValue::makeObject();
        } else if (aMessage.method == "tools/list") {
            aoResult = makeToolsList();
        } else if (aMessage.method == "tools/call") {
            callTool(aMessage.params, aoResult, aoErrorCode, aoErrorMessage);
        } else {
            aoErrorCode = JsonRpcErrorCode_MethodNotFound;
            aoErrorMessage = "unknown method : " + aMessage.method;
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
    void callTool(const JsonValue& aParams, JsonValue& aoResult, JsonRpcErrorCode& aoErrorCode, std::string& aoErrorMessage) {
        const JsonValue* pName = aParams.find("name");
        if ((pName == nullptr) || !pName->isString()) {
            aoErrorCode = JsonRpcErrorCode_InvalidParams;
            aoErrorMessage = "tools/call needs a tool name";
            return;
        }
        const JsonValue* pArguments = aParams.find("arguments");
        if ((pArguments != nullptr) && !pArguments->isObject()) {
            aoErrorCode = JsonRpcErrorCode_InvalidParams;
            aoErrorMessage = "the arguments of a tool are an object";
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
            aoErrorCode = JsonRpcErrorCode_InvalidParams;
            aoErrorMessage = "unknown tool : " + pName->asString();
            return;
        }
        m_pending.push_back(call);
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(m_callTimeoutMs);
        while (call->state != CallState_Done) {
            if (!m_running) {
                call->state = CallState_Cancelled;
                aoErrorCode = JsonRpcErrorCode_InternalError;
                aoErrorMessage = "the server stops";
                return;
            }
            if (call->state == CallState_Queued) {
                if ((m_callChanged.wait_until(lock, deadline) == std::cv_status::timeout) && (call->state == CallState_Queued)) {
                    call->state = CallState_Cancelled;
                    aoErrorCode = JsonRpcErrorCode_InternalError;
                    aoErrorMessage = "the application did not poll within " + std::to_string(m_callTimeoutMs) + " ms";
                    return;
                }
            } else {
                // Picked up by poll() : a running tool is waited for, whatever its length.
                m_callChanged.wait(lock);
            }
        }
        aoResult = call->result.toJson();
    }
};

}  // namespace mcp
}  // namespace ez
