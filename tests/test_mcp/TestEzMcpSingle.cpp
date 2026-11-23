#include <TestEzMcpSingle.h>

// this unit sees ONLY the amalgamation : it proves the single header stands
// alone (no include left behind, ezOS inside)
#include <ezMcp.hpp>

#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpSingle_StandsAlone() {
    ez::mcp::Server server;
    ez::mcp::Tool tool;
    tool.name = "single";
    tool.functor = [](const ez::mcp::JsonValue& /*aArguments*/, ez::mcp::ToolResult& aoResult) { aoResult.addText("one header"); };
    CTEST_ASSERT(server.addTool(tool));
    CTEST_ASSERT(server.start(ez::mcp::ServerConfig()));
    ez::mcp::HttpRequest request;
    request.method = "POST";
    request.path = "/mcp";
    request.body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}";
    const std::string answer = server.handleRequest(request);
    CTEST_ASSERT_MESSAGE(answer.find("HTTP/1.1 200 OK") == 0U, "the amalgamated server answers");
    CTEST_ASSERT(answer.find("\"result\":{}") != std::string::npos);
    CTEST_ASSERT(!ez::mcp::Png::encodeRgba(std::vector<uint8_t>(4U, 255U), 1U, 1U).empty());
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpSingle(const std::string& vTest) {
    IfTestExist(TestEzMcpSingle_StandsAlone);
    return false;
}
