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

// JSON-RPC 2.0 as MCP speaks it : one message per body (no batch), a request
// carries an id (a string or a number, never null), a notification has none.
// Reading classifies a body ; the writers make the answers.

#include "json.hpp"

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
