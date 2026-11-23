#include <TestEzMcpSocket.h>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezMcp/socket.hpp>

#include <atomic>
#include <chrono>
#include <thread>

using ez::mcp::Socket;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// Receives until aoData holds aExpected bytes, or the peer closes, or 2 s pass.
static void receiveAtLeast(Socket& aoSocket, std::string& aoData, size_t aExpected) {
    for (size_t round = 0U; (round < 20U) && (aoData.size() < aExpected); ++round) {
        const ez::mcp::SocketReceive status = aoSocket.receive(aoData, 100);
        if ((status == ez::mcp::SocketReceive_Closed) || (status == ez::mcp::SocketReceive_Error)) {
            return;
        }
    }
}

bool TestEzMcpSocket_LoopbackOnly() {
    Socket server;
    CTEST_ASSERT_MESSAGE(server.listenLocal(0U), "listening on a port the system picks");
    CTEST_ASSERT(server.isValid());
    CTEST_ASSERT_MESSAGE(server.getPort() != 0U, "the picked port is read back");
    CTEST_ASSERT_MESSAGE(server.getAddress() == 0x7F000001U, "bound to 127.0.0.1, never to every interface");
    server.close();
    CTEST_ASSERT(!server.isValid() && (server.getPort() == 0U) && (server.getAddress() == 0U));
    return true;
}

bool TestEzMcpSocket_Exchange() {
    Socket server;
    CTEST_ASSERT(server.listenLocal(0U));
    const uint16_t port = server.getPort();
    std::string clientReceived;
    std::atomic<bool> clientConnected{false};
    std::thread clientThread([port, &clientReceived, &clientConnected]() {
        Socket client;
        if (!client.connectLocal(port)) {
            return;
        }
        clientConnected = true;
        client.sendAll("hello");
        receiveAtLeast(client, clientReceived, 5U);
    });
    Socket peer;
    const bool accepted = server.acceptClient(peer, 2000);
    std::string serverReceived;
    if (accepted) {
        receiveAtLeast(peer, serverReceived, 5U);
        peer.sendAll("world");
    }
    clientThread.join();
    CTEST_ASSERT(clientConnected);
    CTEST_ASSERT_MESSAGE(accepted, "the connection is accepted");
    CTEST_ASSERT(peer.getAddress() == 0x7F000001U);
    CTEST_ASSERT(serverReceived == "hello");
    CTEST_ASSERT(clientReceived == "world");
    // A large message goes whole, whatever the size of one send.
    std::string big(1000000U, 'x');
    for (size_t idx = 0U; idx < big.size(); ++idx) {
        big[idx] = static_cast<char>('a' + (idx % 26U));
    }
    std::string bigReceived;
    std::thread bigThread([port, &big]() {
        Socket client;
        if (client.connectLocal(port)) {
            client.sendAll(big);
        }
    });
    Socket bigPeer;
    CTEST_ASSERT(server.acceptClient(bigPeer, 2000));
    // Bounded by time, not by reads : one read takes at most 4 KiB.
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((bigReceived.size() < big.size()) && (std::chrono::steady_clock::now() < deadline)) {
        if (bigPeer.receive(bigReceived, 100) == ez::mcp::SocketReceive_Closed) {
            break;
        }
    }
    bigThread.join();
    CTEST_ASSERT_MESSAGE(bigReceived == big, "a megabyte arrives whole and in order");
    return true;
}

bool TestEzMcpSocket_Timeouts() {
    Socket server;
    CTEST_ASSERT(server.listenLocal(0U));
    Socket nobody;
    CTEST_ASSERT_MESSAGE(!server.acceptClient(nobody, 50), "no client : the wait ends");
    CTEST_ASSERT(!nobody.isValid());
    Socket client;
    CTEST_ASSERT(client.connectLocal(server.getPort()));
    Socket peer;
    CTEST_ASSERT(server.acceptClient(peer, 2000));
    std::string data;
    CTEST_ASSERT_MESSAGE(peer.receive(data, 50) == ez::mcp::SocketReceive_Timeout, "nothing sent : a timeout");
    CTEST_ASSERT(data.empty());
    client.close();
    ez::mcp::SocketReceive status = ez::mcp::SocketReceive_Timeout;
    for (size_t round = 0U; (round < 20U) && (status == ez::mcp::SocketReceive_Timeout); ++round) {
        status = peer.receive(data, 100);
    }
    CTEST_ASSERT_MESSAGE(status == ez::mcp::SocketReceive_Closed, "the peer closed");
    CTEST_ASSERT_MESSAGE(peer.receive(data, -5) != ez::mcp::SocketReceive_Data, "a negative timeout polls once");
    return true;
}

// Sending to a peer that closed fails, and never kills the process (SIGPIPE).
bool TestEzMcpSocket_PeerGone() {
    Socket server;
    CTEST_ASSERT(server.listenLocal(0U));
    Socket client;
    CTEST_ASSERT(client.connectLocal(server.getPort()));
    Socket peer;
    CTEST_ASSERT(server.acceptClient(peer, 2000));
    peer.close();
    const std::string chunk(65536U, 'x');
    bool failed = false;
    for (size_t round = 0U; (round < 100U) && !failed; ++round) {
        failed = !client.sendAll(chunk);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CTEST_ASSERT_MESSAGE(failed, "the send fails, the process lives");
    return true;
}

bool TestEzMcpSocket_NotOpened() {
    Socket socket;
    std::string data;
    CTEST_ASSERT(!socket.isValid());
    CTEST_ASSERT(socket.receive(data, 10) == ez::mcp::SocketReceive_Error);
    CTEST_ASSERT(!socket.sendAll("x"));
    Socket client;
    CTEST_ASSERT(!socket.acceptClient(client, 10));
    CTEST_ASSERT(socket.getPort() == 0U);
    socket.close();  // closing twice is harmless
    socket.close();
    return true;
}

bool TestEzMcpSocket_PortTaken() {
    Socket first;
    CTEST_ASSERT(first.listenLocal(0U));
    Socket second;
    CTEST_ASSERT_MESSAGE(!second.listenLocal(first.getPort()), "a port in use is refused");
    CTEST_ASSERT(!second.isValid());
    Socket client;
    const uint16_t port = first.getPort();
    first.close();
    CTEST_ASSERT_MESSAGE(!client.connectLocal(port), "nobody listens any more");
    CTEST_ASSERT_MESSAGE(second.listenLocal(port), "a freed port is taken again at once");
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMcpSocket(const std::string& vTest) {
    IfTestExist(TestEzMcpSocket_LoopbackOnly);
    else IfTestExist(TestEzMcpSocket_Exchange);
    else IfTestExist(TestEzMcpSocket_Timeouts);
    else IfTestExist(TestEzMcpSocket_PeerGone);
    else IfTestExist(TestEzMcpSocket_NotOpened);
    else IfTestExist(TestEzMcpSocket_PortTaken);
    return false;
}
