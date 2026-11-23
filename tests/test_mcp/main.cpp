#include <TestEzMcpBase64.h>
#include <TestEzMcpHttp.h>
#include <TestEzMcpJson.h>
#include <TestEzMcpPng.h>
#include <TestEzMcpRpc.h>
#include <TestEzMcpServer.h>
#include <TestEzMcpSocket.h>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezLog.hpp>
#include <ezlibs/ezMcp/ezMcp.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

bool TestMcp(const std::string& vTest) {
    IfTestCollectionExist(TestEzMcpBase64);
    else IfTestCollectionExist(TestEzMcpHttp);
    else IfTestCollectionExist(TestEzMcpJson);
    else IfTestCollectionExist(TestEzMcpPng);
    else IfTestCollectionExist(TestEzMcpRpc);
    else IfTestCollectionExist(TestEzMcpServer);
    else IfTestCollectionExist(TestEzMcpSocket);
    return false;
}

////////////////////////////////////////////////////////////////////////////
//// SERVE MODE : a live server to try by hand (curl, an MCP client, a tunnel)
////////////////////////////////////////////////////////////////////////////

static void echoTool(const ez::mcp::JsonValue& aArguments, ez::mcp::ToolResult& aoResult, void* /*apUserData*/) {
    const ez::mcp::JsonValue* pText = aArguments.find("text");
    if ((pText == nullptr) || !pText->isString()) {
        aoResult.addText("echo needs a text");
        aoResult.setError(true);
        return;
    }
    aoResult.addText(pText->asString());
}

static void gradientTool(const ez::mcp::JsonValue& /*aArguments*/, ez::mcp::ToolResult& aoResult, void* /*apUserData*/) {
    const uint32_t width = 64U;
    const uint32_t height = 32U;
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4U);
    for (uint32_t row = 0U; row < height; ++row) {
        for (uint32_t col = 0U; col < width; ++col) {
            uint8_t* pPixel = &rgba[(static_cast<size_t>(row) * width + col) * 4U];
            pPixel[0] = static_cast<uint8_t>(col * 4U);
            pPixel[1] = static_cast<uint8_t>(row * 8U);
            pPixel[2] = 128U;
            pPixel[3] = 255U;
        }
    }
    aoResult.addImagePng(ez::mcp::Png::encodeRgba(rgba, width, height));
}

// Serves on 127.0.0.1:aPort for aSeconds, polling like a frame loop. A non
// empty aToken is required as a bearer token ; "auto" makes one and prints it.
static int serve(uint16_t aPort, int32_t aSeconds, const std::string& aToken) {
    ez::mcp::Server server;
    ez::mcp::Tool echo;
    echo.name = "echo";
    echo.description = "Answers the text it is given";
    ez::mcp::JsonError error;
    ez::mcp::JsonValue::parse("{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}", echo.inputSchema,
        error);
    echo.functor = &echoTool;
    server.addTool(echo);
    ez::mcp::Tool gradient;
    gradient.name = "gradient";
    gradient.description = "Answers a 64x32 PNG gradient";
    gradient.functor = &gradientTool;
    server.addTool(gradient);
    ez::mcp::ServerConfig config;
    config.name = "EzLibs_TestMcp";
    config.version = "1.0";
    config.port = aPort;
    config.token = (aToken == "auto") ? ez::mcp::Server::makeToken() : aToken;
    if (!server.start(config)) {
        printf("cannot start : %s\n", server.getStartError().c_str());
        return 1;
    }
    printf("serving on http://127.0.0.1:%u/mcp for %d s\n", static_cast<unsigned>(server.getPort()), static_cast<int>(aSeconds));
    if (!config.token.empty()) {
        printf("header : Authorization: Bearer %s\n", config.token.c_str());
    }
    fflush(stdout);
    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::seconds(aSeconds);
    while (std::chrono::steady_clock::now() < end) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    server.stop();
    return 0;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    ez::Log::initSingleton();
    if ((argc > 3) && (std::string(argv[1]) == "serve")) {
        // serve <port> <seconds> [token | auto]
        return serve(static_cast<uint16_t>(std::atoi(argv[2])), static_cast<int32_t>(std::atoi(argv[3])),
            (argc > 4) ? std::string(argv[4]) : std::string());
    }
    if (argc > 1) {
        printf("Exec test : %s\n", argv[1]);
        return TestMcp(argv[1]) ? 0 : 1;
    }
    // User testing
    return TestMcp("TestEzMcpServer_EndToEnd") ? 0 : 1;
}
