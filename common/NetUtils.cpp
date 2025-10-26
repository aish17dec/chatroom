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
    auto pos = hostPort.rfind(':');
    if (pos == std::string::npos)
    {
        host = hostPort;
        port.clear();
    }
    else
    {
        host = hostPort.substr(0, pos);
        port = hostPort.substr(pos + 1);
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
    std::string host, port;
    SplitHostPort(hostPort, host, port);

    struct addrinfo hints
    {
    };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *result = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0)
        return -1;

    int listenFd = -1;
    for (auto *rp = result; rp; rp = rp->ai_next)
    {
        listenFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listenFd < 0)
            continue;

        int on = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#ifdef SO_REUSEPORT
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif
        if (bind(listenFd, rp->ai_addr, rp->ai_addrlen) == 0 && listen(listenFd, 64) == 0)
            break;

        close(listenFd);
        listenFd = -1;
    }

    freeaddrinfo(result);

    TRACE_EXIT();
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
    TRACE_ENTER();
    struct addrinfo hints
    {
    };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0)
        return -1;

    int sockFd = -1;
    for (auto *rp = result; rp; rp = rp->ai_next)
    {
        sockFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockFd < 0)
            continue;

        if (connect(sockFd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(sockFd);
        sockFd = -1;
    }

    freeaddrinfo(result);
    TRACE_EXIT();
    return sockFd;
}

/*
 * TcpConnectHostPort()
 * --------------------
 * Convenience wrapper for TcpConnect("host:port").
 */
int TcpConnectHostPort(const std::string &hostPort)
{
    TRACE_ENTER();
    std::string host, port;
    SplitHostPort(hostPort, host, port);
    TRACE_EXIT();
    return TcpConnect(host, port);
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
    TRACE_ENTER();
    out.clear();
    char ch;
    while (out.size() + 1 < max)
    {
        TRACE_ENTER();
        ssize_t n = ::recv(fd, &ch, 1, 0);
        if (n <= 0)
            return out.empty() ? -1 : static_cast<int>(out.size());

        out.push_back(ch);
        if (ch == '\n')
            break;
    }
    return static_cast<int>(out.size());
}

/*
 * SendAll()
 * ---------
 * Sends the entire buffer over the socket, retrying if necessary.
 * Returns 0 on success, -1 on error.
 */
int SendAll(int fd, const void *buf, size_t len)
{
    TRACE_ENTER();
    const char *ptr = static_cast<const char *>(buf);
    size_t totalSent = 0;

    while (totalSent < len)
    {
        ssize_t n = ::send(fd, ptr + totalSent, len - totalSent, 0);
        if (n <= 0)
            return -1;
        totalSent += static_cast<size_t>(n);
    }

    TRACE_EXIT();
    return 0;
}

/*
 * SendLine()
 * ----------
 * Sends a string as a complete line, ensuring a trailing newline.
 */
int SendLine(int fd, const std::string &line)
{
    TRACE_ENTER();
    std::string msg = line;
    if (msg.empty() || msg.back() != '\n')
        msg.push_back('\n');
    return SendAll(fd, msg.data(), msg.size());
}
