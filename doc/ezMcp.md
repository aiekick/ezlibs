# EzMcp

EzMcp is a small MCP server (Model Context Protocol) that a C++11 application
embeds, so that an MCP client can see it and drive it. The application
declares tools; the client lists them and calls them.

- Listens on **127.0.0.1 only**, on a port the application chooses (0 lets the
  system pick one). Nothing is exposed on the network.
- Speaks the MCP **streamable HTTP** transport with JSON answers: one
  `POST /mcp` per message. Protocol versions `2025-11-25`, `2025-06-18` and
  `2025-03-26`.
- A request that carries an `Origin` header (sent by a web page) is refused
  with 403. A `GET` gets 405: there is no event stream.
- An optional **bearer token**: with one, every request without
  `Authorization: Bearer <token>` gets 401, so no other process of the machine
  can call the tools.
- Tool calls run **on the application thread**: they wait in a queue until
  the application calls `poll()`, typically once per frame.
- No exceptions, no dependency beyond the standard library and the system
  sockets (Winsock under Windows: link `ws2_32`, MSVC links it by itself).

## Files

`include/ezlibs/ezMcp/`, one header per job:

|File|Role|
|-|-|
|`ezMcp.hpp`|The entry point: includes the others, declares `Server`, `Tool`, `ToolResult`, `ServerConfig`|
|`socket.hpp`|A TCP socket on the loopback interface, bounded waits|
|`http.hpp`|Reads one HTTP/1.1 request, writes one response (no chunked body, no keep-alive)|
|`json.hpp`|The JSON value: reads any valid JSON text, writes compact JSON, keeps the key order|
|`jsonrpc.hpp`|JSON-RPC 2.0 messages as MCP uses them (no batch)|
|`png.hpp`|Writes an RGBA 8 bits image as an uncompressed PNG|
|`base64.hpp`|Base64 encoding of an image inside a tool result|

`socket.hpp` includes `ezOS.hpp` for the platform defines.

## How to use

```cpp
#include <ezlibs/ezMcp/ezMcp.hpp>

// A tool : reads its arguments, fills its result.
static void echo(const ez::mcp::JsonValue& aArguments, ez::mcp::ToolResult& aoResult, void* apUserData) {
    const ez::mcp::JsonValue* pText = aArguments.find("text");
    if ((pText == nullptr) || !pText->isString()) {
        aoResult.addText("echo needs a text");
        aoResult.setError(true);  // the tool ran and failed : not a protocol error
        return;
    }
    aoResult.addText(pText->asString());
}

int main() {
    ez::mcp::Server server;

    ez::mcp::Tool tool;
    tool.name = "echo";
    tool.description = "Answers the text it is given";
    ez::mcp::JsonError error;
    ez::mcp::JsonValue::parse(
        R"({"type":"object","properties":{"text":{"type":"string"}},"required":["text"]})",
        tool.inputSchema, error);
    tool.functor = &echo;
    tool.userData = nullptr;  // handed back to the functor
    server.addTool(tool);

    ez::mcp::ServerConfig config;
    config.name = "myApp";
    config.version = "1.0";
    config.port = 7810;
    config.instructions = "myApp is a node editor; use echo to test the link";
    if (!server.start(config)) {
        return 1;  // the port is taken
    }
    while (appIsRunning()) {
        server.poll();  // runs the tool calls waiting since the last frame
        renderFrame();
    }
    server.stop();
    return 0;
}
```

An image goes back as a PNG:

```cpp
std::vector<uint8_t> rgba = readPixels();  // width * height * 4 bytes, rows top to bottom
aoResult.addImagePng(ez::mcp::Png::encodeRgba(rgba, width, height));
```

## The server

- `addTool(tool)` registers a tool, before or after `start()`. Refused: an
  empty or taken name, no functor, a schema that is not a JSON object. A tool
  without schema is listed with `{"type":"object"}`.
- `start(config)` listens and starts the server thread; false when the port
  is taken, the token is refused or the server already runs, and
  `getStartError()` says which.
- `poll()` runs the waiting tool calls on the calling thread and returns how
  many ran.
- `stop()` stops the thread and closes the port; a client still waiting gets
  an error at once. A stopped server can start again.
- `getPort()` reads back the port, useful with `config.port = 0`.

`initialize`, `ping` and `tools/list` are answered on the server thread.
A `tools/call` waits for `poll()` at most `config.callTimeoutMs` (5000 by
default, clamped to [10, 600000]); past that the client gets an error and the
call is never run later. A call already picked up by `poll()` is waited for
to its end, whatever its length.

The answers:

|Case|HTTP|JSON-RPC|
|-|-|-|
|A request|200|its result, or an error|
|A notification, a response of the client|202|none|
|Unreadable JSON|400|-32700|
|Not a valid message (a batch, no `jsonrpc`, a null id...)|400|-32600|
|Unknown method|200|-32601|
|Unknown tool, no tool name, arguments not an object|200|-32602|
|No `poll()` in time, server stopping|200|-32603|
|A tool that sets `setError(true)`|200|a result with `"isError":true`|
|No or wrong bearer token, when the config has one|401||
|An `Origin` header|403||
|Another path than `/mcp`|404||
|Another method than `POST`|405||

## The token

Anything that runs on the machine can reach a port of 127.0.0.1, and through
an SSH tunnel so can anything on the other machine. A token closes that door:

```cpp
config.token = ez::mcp::Server::makeToken();  // 64 hexadecimal characters, 256 bits
printf("Authorization: Bearer %s\n", config.token.c_str());  // handed to the client
```

- Empty (the default), there is no check.
- Set, every request must carry `Authorization: Bearer <token>` (the scheme in
  any case); anything else gets 401 with `WWW-Authenticate: Bearer`, before any
  other check. The comparison takes the same time wherever the tokens differ.
- `start()` refuses a token under 32 characters, or with anything else than
  visible ASCII characters (no space): it travels in a header and must not be
  guessed.
- The token comes from the application (made at start, or read from its
  settings), never written in the sources. The client sends it as a fixed
  header of the HTTP MCP server it declares.

## Connecting a client

The server listens on the machine of the application. A client on the same
machine declares it as an HTTP MCP server at `http://127.0.0.1:<port>/mcp`.
A client on another machine reaches it through an SSH tunnel opened from the
application side, for the time of the session:

```
ssh -R <port>:127.0.0.1:<port> <client machine>
```

The test executable has a serve mode to try a client or a tunnel without an
application: `EzLibs_TestMcp serve <port> <seconds> [token | auto]` serves two
tools, `echo` and `gradient` (a 64x32 PNG); with `auto` it makes a token and
prints the header to send.

```
curl -s http://127.0.0.1:7810/mcp -H "Content-Type: application/json" \
     -d '{"jsonrpc":"2.0","id":1,"method":"tools/list"}'
```

## The JSON value

`ez::mcp::JsonValue` is made by `makeNull`, `makeBool`, `makeInteger`,
`makeNumber`, `makeString`, `makeArray`, `makeObject`, read by
`JsonValue::parse(text, value, error)` and written by `dump()`.

- Reading refuses anything that is not valid JSON, and says why and at which
  byte (`JsonError::message`, `JsonError::offset`); the output value is left
  untouched. Nesting deeper than 64 levels is refused.
- `\uXXXX` escapes and surrogate pairs become UTF-8; raw UTF-8 passes through.
- An integer without fraction nor exponent stays exact while it fits an int64.
  Numbers are read and written with the classic locale, whatever the locale
  of the application.
- Object keys keep the order they were written in; `set` on an existing key
  keeps its place.
- On a value of another type, the accessors answer a neutral value (`false`,
  `0`, `""`, a null item, `nullptr` from `find`), and `push` / `set` do
  nothing.
