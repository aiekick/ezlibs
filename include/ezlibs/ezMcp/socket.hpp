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

// A blocking TCP socket bound to the loopback interface only (127.0.0.1) :
// listen, accept, receive, send, close. The waits are bounded, so a server
// thread can look at its stop flag between two of them. Winsock under
// Windows (link ws2_32), BSD sockets elsewhere. No exceptions.

#include "../ezOS.hpp"

#include <cstdint>
#include <cstring>
#include <string>

#ifdef WINDOWS_OS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ez {
namespace mcp {

enum SocketReceive { SocketReceive_Closed = 0, SocketReceive_Data, SocketReceive_Error, SocketReceive_Timeout };

class Socket {
private:
#ifdef WINDOWS_OS
    typedef SOCKET NativeHandle;
#else
    typedef int NativeHandle;
#endif

    NativeHandle m_handle{invalidHandle()};
    bool m_systemStarted{};  // this socket holds one Winsock startup

public:
    Socket() = default;
    ~Socket() {
        close();
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(Socket&&) = delete;

    // Listens on 127.0.0.1:aPort ; aPort 0 lets the system pick a free port,
    // read back by getPort().
    bool listenLocal(uint16_t aPort) {
        if (!open()) {
            return false;
        }
#ifdef WINDOWS_OS
        // No other process may bind the same port while it is ours.
        BOOL exclusive = TRUE;
        setsockopt(m_handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
#else
        // A restarted server gets its port back at once.
        int reuse = 1;
        setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
        sockaddr_in address = loopbackAddress(aPort);
        if ((bind(m_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) || (listen(m_handle, 4) != 0)) {
            close();
            return false;
        }
        return true;
    }

    // Connects to 127.0.0.1:aPort (the client side, for a test or a tool).
    bool connectLocal(uint16_t aPort) {
        if (!open()) {
            return false;
        }
        sockaddr_in address = loopbackAddress(aPort);
        if (connect(m_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            close();
            return false;
        }
        return true;
    }

    // Waits at most aTimeoutMs for a connection ; aoClient holds it on success.
    bool acceptClient(Socket& aoClient, int32_t aTimeoutMs) {
        if (!isValid() || (waitReadable(aTimeoutMs) <= 0)) {
            return false;
        }
        const NativeHandle handle = accept(m_handle, nullptr, nullptr);
        if (handle == invalidHandle()) {
            return false;
        }
        aoClient.close();
        aoClient.startSystem();
        aoClient.m_handle = handle;
        aoClient.noSigPipe();
        return true;
    }

    // Waits at most aTimeoutMs for bytes and appends what came to aoData.
    SocketReceive receive(std::string& aoData, int32_t aTimeoutMs) {
        if (!isValid()) {
            return SocketReceive_Error;
        }
        const int32_t ready = waitReadable(aTimeoutMs);
        if (ready == 0) {
            return SocketReceive_Timeout;
        }
        if (ready < 0) {
            return SocketReceive_Error;
        }
        char buffer[4096];
#ifdef WINDOWS_OS
        const int count = recv(m_handle, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
        const ssize_t count = recv(m_handle, buffer, sizeof(buffer), 0);
#endif
        if (count == 0) {
            return SocketReceive_Closed;
        }
        if (count < 0) {
            return SocketReceive_Error;
        }
        aoData.append(buffer, static_cast<size_t>(count));
        return SocketReceive_Data;
    }

    // Sends every byte of aData, or fails.
    bool sendAll(const std::string& aData) const {
        if (!isValid()) {
            return false;
        }
        size_t sent = 0U;
        while (sent < aData.size()) {
#ifdef WINDOWS_OS
            const int count = send(m_handle, aData.data() + sent, static_cast<int>(aData.size() - sent), 0);
#elif defined(MSG_NOSIGNAL)
            const ssize_t count = send(m_handle, aData.data() + sent, aData.size() - sent, MSG_NOSIGNAL);
#else
            const ssize_t count = send(m_handle, aData.data() + sent, aData.size() - sent, 0);
#endif
            if (count <= 0) {
                return false;
            }
            sent += static_cast<size_t>(count);
        }
        return true;
    }

    void close() {
        if (m_handle != invalidHandle()) {
#ifdef WINDOWS_OS
            closesocket(m_handle);
#else
            ::close(m_handle);
#endif
            m_handle = invalidHandle();
        }
#ifdef WINDOWS_OS
        if (m_systemStarted) {
            WSACleanup();
        }
#endif
        m_systemStarted = false;
    }

    bool isValid() const {
        return m_handle != invalidHandle();
    }

    // The local port, 0 when not bound.
    uint16_t getPort() const {
        sockaddr_in address{};
        return readLocalAddress(address) ? ntohs(address.sin_port) : static_cast<uint16_t>(0U);
    }

    // The local IPv4 address in host order (0x7F000001 for 127.0.0.1), 0 when not bound.
    uint32_t getAddress() const {
        sockaddr_in address{};
        return readLocalAddress(address) ? ntohl(address.sin_addr.s_addr) : 0U;
    }

private:
    static NativeHandle invalidHandle() {
#ifdef WINDOWS_OS
        return INVALID_SOCKET;
#else
        return -1;
#endif
    }

    static sockaddr_in loopbackAddress(uint16_t aPort) {
        sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(aPort);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        return address;
    }

    // A member under Windows (it counts this socket's startup), a no-op elsewhere.
    bool startSystem() {  // NOLINT(readability-convert-member-functions-to-static)
#ifdef WINDOWS_OS
        if (!m_systemStarted) {
            WSADATA data;
            m_systemStarted = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
        }
        return m_systemStarted;
#else
        return true;
#endif
    }

    bool open() {
        close();
        if (!startSystem()) {
            return false;
        }
        m_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_handle == invalidHandle()) {
            close();
            return false;
        }
        noSigPipe();
        return true;
    }

    // A peer that closed early never kills the process by SIGPIPE.
    void noSigPipe() {
#ifdef SO_NOSIGPIPE
        int noSignal = 1;
        setsockopt(m_handle, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif
    }

    // 1 readable, 0 timed out, -1 failed. An interrupted wait counts as timed out.
    int32_t waitReadable(int32_t aTimeoutMs) const {
        const int32_t timeoutMs = (aTimeoutMs < 0) ? 0 : aTimeoutMs;
#ifdef WINDOWS_OS
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_handle, &readSet);
        timeval timeout;
        timeout.tv_sec = static_cast<long>(timeoutMs / 1000);
        timeout.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);
        const int result = select(0, &readSet, nullptr, nullptr, &timeout);
        if (result < 0) {
            return -1;
        }
        return (result > 0) ? 1 : 0;
#else
        pollfd entry{};
        entry.fd = m_handle;
        entry.events = POLLIN;
        const int result = poll(&entry, 1, timeoutMs);
        if (result < 0) {
            return (errno == EINTR) ? 0 : -1;
        }
        return (result > 0) ? 1 : 0;
#endif
    }

    bool readLocalAddress(sockaddr_in& aoAddress) const {
        if (!isValid()) {
            return false;
        }
#ifdef WINDOWS_OS
        int size = static_cast<int>(sizeof(aoAddress));
#else
        socklen_t size = sizeof(aoAddress);
#endif
        return getsockname(m_handle, reinterpret_cast<sockaddr*>(&aoAddress), &size) == 0;
    }
};

}  // namespace mcp
}  // namespace ez
