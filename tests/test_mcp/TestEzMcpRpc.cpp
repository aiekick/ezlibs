#include <TestEzMcpRpc.h>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMcp/jsonrpc.hpp>

using ez::mcp::JsonRpc;
using ez::mcp::JsonRpcMessage;
using ez::mcp::JsonValue;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

static bool isInvalid(const std::string& aBody, ez::mcp::JsonRpcErrorCode aCode) {
    const JsonRpcMessage message = JsonRpc::read(aBody);
    return (message.kind == ez::mcp::JsonRpcKind_Invalid) && (message.errorCode == aCode) && !message.errorMessage.empty();
}

bool TestEzMcpRpc_Request() {
    JsonRpcMessage message = JsonRpc::read("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"x\"}}");
    CTEST_ASSERT(message.kind == ez::mcp::JsonRpcKind_Request);
    CTEST_ASSERT(message.id.isInteger() && (message.id.asInteger() == 1));
    CTEST_ASSERT(message.method == "tools/call");
    CTEST_ASSERT(message.params.isObject() && (message.params.find("name")->asString() == "x"));
    CTEST_ASSERT(message.errorCode == ez::mcp::JsonRpcErrorCode_None);
    message = JsonRpc::read("{\"method\":\"ping\",\"id\":\"abc\",\"jsonrpc\":\"2.0\"}");
    CTEST_ASSERT_MESSAGE((message.kind == ez::mcp::JsonRpcKind_Request) && (message.id.asString() == "abc"), "a string id, members in any order");
    CTEST_ASSERT_MESSAGE(message.params.isNull(), "params may be absent");
    message = JsonRpc::read("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"m\",\"params\":[1,2]}");
    CTEST_ASSERT_MESSAGE((message.kind == ez::mcp::JsonRpcKind_Request) && message.params.isArray(), "params by position");
    return true;
}

bool TestEzMcpRpc_Notification() {
    const JsonRpcMessage message = JsonRpc::read("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
    CTEST_ASSERT(message.kind == ez::mcp::JsonRpcKind_Notification);
    CTEST_ASSERT(message.id.isNull());
    CTEST_ASSERT(message.method == "notifications/initialized");
    return true;
}

bool TestEzMcpRpc_Response() {
    CTEST_ASSERT(JsonRpc::read("{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":{}}").kind == ez::mcp::JsonRpcKind_Response);
    CTEST_ASSERT(JsonRpc::read("{\"jsonrpc\":\"2.0\",\"id\":3,\"error\":{\"code\":1,\"message\":\"m\"}}").kind == ez::mcp::JsonRpcKind_Response);
    return true;
}

bool TestEzMcpRpc_ParseError() {
    CTEST_ASSERT(isInvalid("{", ez::mcp::JsonRpcErrorCode_ParseError));
    CTEST_ASSERT(isInvalid("", ez::mcp::JsonRpcErrorCode_ParseError));
    const JsonRpcMessage message = JsonRpc::read("{\"id\":5,");
    CTEST_ASSERT_MESSAGE(message.id.isNull(), "no id is trusted from an unreadable text");
    CTEST_ASSERT_MESSAGE(message.errorMessage.find("offset 8") != std::string::npos, "the message says where reading stopped");
    return true;
}

bool TestEzMcpRpc_InvalidRequest() {
    CTEST_ASSERT_MESSAGE(isInvalid("[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}]", ez::mcp::JsonRpcErrorCode_InvalidRequest), "no batch");
    CTEST_ASSERT(isInvalid("42", ez::mcp::JsonRpcErrorCode_InvalidRequest));
    CTEST_ASSERT(isInvalid("{\"jsonrpc\":\"1.0\",\"id\":1,\"method\":\"ping\"}", ez::mcp::JsonRpcErrorCode_InvalidRequest));
    CTEST_ASSERT(isInvalid("{\"id\":1,\"method\":\"ping\"}", ez::mcp::JsonRpcErrorCode_InvalidRequest));
    CTEST_ASSERT_MESSAGE(isInvalid("{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"ping\"}", ez::mcp::JsonRpcErrorCode_InvalidRequest),
        "a null id is refused");
    CTEST_ASSERT(isInvalid("{\"jsonrpc\":\"2.0\",\"id\":{},\"method\":\"ping\"}", ez::mcp::JsonRpcErrorCode_InvalidRequest));
    CTEST_ASSERT(isInvalid("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":7}", ez::mcp::JsonRpcErrorCode_InvalidRequest));
    CTEST_ASSERT(isInvalid("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"m\",\"params\":\"p\"}", ez::mcp::JsonRpcErrorCode_InvalidRequest));
    CTEST_ASSERT(isInvalid("{\"jsonrpc\":\"2.0\",\"result\":{}}", ez::mcp::JsonRpcErrorCode_InvalidRequest));
    const JsonRpcMessage message = JsonRpc::read("{\"jsonrpc\":\"2.0\",\"id\":4}");
    CTEST_ASSERT(message.kind == ez::mcp::JsonRpcKind_Invalid);
    CTEST_ASSERT_MESSAGE(message.id.asInteger() == 4, "a valid id is kept, the error answers it");
    return true;
}

bool TestEzMcpRpc_Writers() {
    CTEST_ASSERT(JsonRpc::makeResult(JsonValue::makeInteger(1), JsonValue::makeObject()) == "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}");
    CTEST_ASSERT(JsonRpc::makeResult(JsonValue::makeString("a"), JsonValue::makeBool(true)) == "{\"jsonrpc\":\"2.0\",\"id\":\"a\",\"result\":true}");
    CTEST_ASSERT(JsonRpc::makeError(JsonValue(), ez::mcp::JsonRpcErrorCode_ParseError, "bad") ==
        "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"bad\"}}");
    CTEST_ASSERT(JsonRpc::makeError(JsonValue::makeInteger(9), ez::mcp::JsonRpcErrorCode_MethodNotFound, "m") ==
        "{\"jsonrpc\":\"2.0\",\"id\":9,\"error\":{\"code\":-32601,\"message\":\"m\"}}");
    // What is written reads back as a response.
    CTEST_ASSERT(JsonRpc::read(JsonRpc::makeResult(JsonValue::makeInteger(2), JsonValue::makeObject())).kind == ez::mcp::JsonRpcKind_Response);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpRpc(const std::string& vTest) {
    IfTestExist(TestEzMcpRpc_Request);
    else IfTestExist(TestEzMcpRpc_Notification);
    else IfTestExist(TestEzMcpRpc_Response);
    else IfTestExist(TestEzMcpRpc_ParseError);
    else IfTestExist(TestEzMcpRpc_InvalidRequest);
    else IfTestExist(TestEzMcpRpc_Writers);
    return false;
}
