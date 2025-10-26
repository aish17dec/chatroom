#include <algorithm>
#include <iostream>
#include <netdb.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "../Trace.hpp"
#include "NetUtils.hpp"

/*
 *  NetUtils.cpp
 *  -------------
 *  Provides simple, blocking TCP utilities for this project:
 *   - TcpListen(host:port)     → create listening socket
 *   - TcpConnect(host, port)   → connect to remote host
 *   - TcpConnectHostPort()     → same, single string "host:port"
 *   - RecvLine()               → read one newline-terminated line
 *   - SendAll() / SendLine()   → send data or full line
 *
 *  These wrappers are deliberately minimalist and portable.
 */

namespace
{
// Split "host:port" into "host" and "port" strings.
void SplitHostPort(const std::string &hostPort, std::string &host, std::string &port)
{
    // DEBUG_LOG("SplitHostPort() input: %s", hostPort.c_str());
    auto pos = hostPort.rfind(':');
    if (pos == std::string::npos)
    {
        host = hostPort;
        port.clear();
        DEBUG_LOG("SplitHostPort(): no ':' found, host=%s, port=<empty>", host.c_str());
    }
    else
    {
        host = hostPort.substr(0, pos);
        port = hostPort.substr(pos + 1);
        // DEBUG_LOG("SplitHostPort(): host=%s, port=%s", host.c_str(), port.c_str());
    }
}
} // namespace

/*
 * TcpListen()
 * -----------
 * Creates a listening TCP socket bound to the given host:port.
 * Example: TcpListen("0.0.0.0:7000")
 */
int TcpListen(const std::string &hostPort)
{
    TRACE_ENTER();
    DEBUG_LOG("TcpListen() called with hostPort=%s", hostPort.c_str());

    std::string host, port;
    SplitHostPort(hostPort, host, port);

    struct addrinfo hints
    {
    };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *result = nullptr;
    int ret = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (ret != 0)
    {
        DEBUG_LOG("getaddrinfo() failed: %s", gai_strerror(ret));
        return -1;
    }

    int listenFd = -1;
    for (auto *rp = result; rp; rp = rp->ai_next)
    {
        DEBUG_LOG("Creating socket: family=%d, socktype=%d, protocol=%d", rp->ai_family, rp->ai_socktype,
                  rp->ai_protocol);
        listenFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listenFd < 0)
        {
            DEBUG_LOG("socket() failed: %s", strerror(errno));
            continue;
        }

        int on = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#ifdef SO_REUSEPORT
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif
        DEBUG_LOG("Attempting bind() and listen() on socket fd=%d", listenFd);
        if (bind(listenFd, rp->ai_addr, rp->ai_addrlen) == 0 && listen(listenFd, 64) == 0)
        {
            DEBUG_LOG("TcpListen(): Successfully bound and listening on fd=%d", listenFd);
            break;
        }

        DEBUG_LOG("bind()/listen() failed: %s", strerror(errno));
        close(listenFd);
        listenFd = -1;
    }

    freeaddrinfo(result);
    return listenFd;
}

/*
 * TcpConnect()
 * ------------
 * Opens a blocking TCP connection to the given host and port.
 * Returns socket descriptor on success, or -1 on failure.
 */
int TcpConnect(const std::string &host, const std::string &port)
{
    struct addrinfo hints
    {
    };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = nullptr;
    int ret = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (ret != 0)
    {
        DEBUG_LOG("getaddrinfo() failed: %s", gai_strerror(ret));
        return -1;
    }

    int sockFd = -1;
    for (auto *rp = result; rp; rp = rp->ai_next)
    {
        sockFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockFd < 0)
        {
            DEBUG_LOG("socket() failed: %s", strerror(errno));
            continue;
        }

        std::cout << "Attempting connect()\n";
        if (connect(sockFd, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            std::cout << "TcpConnect(): successfully connected\n";
            break;
        }

        close(sockFd);
        sockFd = -1;
    }

    freeaddrinfo(result);
    return sockFd;
}

/*
 * TcpConnectHostPort()
 * --------------------
 * Convenience wrapper for TcpConnect("host:port").
 */
int TcpConnectHostPort(const std::string &hostPort)
{
    DEBUG_LOG("TcpConnectHostPort() input: %s", hostPort.c_str());
    std::string host, port;
    SplitHostPort(hostPort, host, port);
    int fd = TcpConnect(host, port);
    return fd;
}

/*
 * RecvLine()
 * ----------
 * Reads a single '\n'-terminated line from the socket.
 * The returned string includes the newline character.
 * Returns number of bytes read, or -1 on error/EOF.
 */
int RecvLine(int fd, std::string &out, size_t max)
{
    out.clear();
    char ch;
    while (out.size() + 1 < max)
    {
        ssize_t n = ::recv(fd, &ch, 1, 0);
        if (n <= 0)
        {
            DEBUG_LOG("recv() returned %zd", n);
            return out.empty() ? -1 : static_cast<int>(out.size());
        }

        out.push_back(ch);
        if (ch == '\n')
        {
            break;
        }
    }

    return static_cast<int>(out.size());
}

/*
 * SendAll()
 * ---------
 * Sends the entire buffer over the socket, retrying if necessary.
 * Returns 0 on success, -1 on error.
 */
// int SendAll(int fd, const void *buf, size_t len)
// {
//     const char *ptr = static_cast<const char *>(buf);
//     size_t totalSent = 0;

//     while (totalSent < len)
//     {
//         ssize_t n = ::send(fd, ptr + totalSent, len - totalSent, 0);
//         if (n <= 0)
//         {
//             DEBUG_LOG("send() failed: %s", strerror(errno));
//             return -1;
//         }
//         totalSent += static_cast<size_t>(n);
//         DEBUG_LOG("SendAll(): sent=%zd, totalSent=%zu/%zu", n, totalSent, len);
//     }

//     DEBUG_LOG("SendAll(): completed successfully for fd=%d", fd);
//     return 0;
// }

int SendAll(int fd, const void *buf, size_t len)
{
    const char *ptr = static_cast<const char *>(buf);
    size_t totalSent = 0;

    // Print the full message being sent (for human debugging)
    std::string message(ptr, len);
    DEBUG_LOG("[RA][SEND] %s\n", ptr);

    while (totalSent < len)
    {
        ssize_t n = ::send(fd, ptr + totalSent, len - totalSent, 0);
        if (n <= 0)
        {
            std::cerr << "[ERROR] send() failed: " << strerror(errno) << std::endl;
            return -1;
        }
        totalSent += static_cast<size_t>(n);
    }

    return 0;
}

/*
 * SendLine()
 * ----------
 * Sends a string as a complete line, ensuring a trailing newline.
 */
int SendLine(int fd, const std::string &line)
{
    std::string msg = line;
    if (msg.empty() || msg.back() != '\n')
    {
        msg.push_back('\n');
    }
    int result = SendAll(fd, msg.data(), msg.size());
    DEBUG_LOG("SendLine(): sent %zu bytes, result=%d", msg.size(), result);
    TRACE_EXIT();
    return result;
}
