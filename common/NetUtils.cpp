#include <algorithm>
#include <iostream>
#include <netdb.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "../debug.hpp"
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
        std::cout << Timestamp() << " [NET] SplitHostPort(): no ':' found, host=" << host << ", port=<empty>"
                  << std::endl;
    }
    else
    {
        host = hostPort.substr(0, pos);
        port = hostPort.substr(pos + 1);
        std::cout << Timestamp() << " [NET] SplitHostPort(): host=" << host << ", port=" << port << std::endl;
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
    std::cout << Timestamp() << " [NET] TcpListen() called with hostPort=" << hostPort << std::endl;

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
        std::cerr << Timestamp() << " [NET] getaddrinfo() failed: " << gai_strerror(ret) << std::endl;
        return -1;
    }

    int listenFd = -1;
    for (auto *rp = result; rp; rp = rp->ai_next)
    {
        std::cout << Timestamp() << " [NET] Creating socket: family=" << rp->ai_family
                  << ", socktype=" << rp->ai_socktype << ", protocol=" << rp->ai_protocol << std::endl;

        listenFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listenFd < 0)
        {
            std::cerr << Timestamp() << " [NET] socket() failed: " << strerror(errno) << std::endl;
            continue;
        }

        int on = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#ifdef SO_REUSEPORT
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif
        std::cout << Timestamp() << " [NET] Attempting bind() and listen() on socket fd=" << listenFd << std::endl;
        if (bind(listenFd, rp->ai_addr, rp->ai_addrlen) == 0 && listen(listenFd, 64) == 0)
        {
            std::cout << Timestamp() << " [NET] TcpListen(): Successfully bound and listening on fd=" << listenFd
                      << std::endl;
            break;
        }

        std::cerr << Timestamp() << " [NET] bind()/listen() failed: " << strerror(errno) << std::endl;
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
        std::cerr << Timestamp() << " [NET] getaddrinfo() failed: " << gai_strerror(ret) << std::endl;
        return -1;
    }

    int sockFd = -1;
    for (auto *rp = result; rp; rp = rp->ai_next)
    {
        sockFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockFd < 0)
        {
            std::cerr << Timestamp() << " [NET] socket() failed: " << strerror(errno) << std::endl;
            continue;
        }

        std::cout << Timestamp() << " [NET] Attempting connect()" << std::endl;
        if (connect(sockFd, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            std::cout << Timestamp() << " [NET] TcpConnect(): successfully connected" << std::endl;
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
    std::cout << Timestamp() << " [NET] TcpConnectHostPort() input: " << hostPort << std::endl;
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
            std::cerr << Timestamp() << " [NET] recv() returned " << n << std::endl;
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
int SendAll(int fd, const void *buf, size_t len)
{
    const char *ptr = static_cast<const char *>(buf);
    size_t totalSent = 0;

    std::string message(ptr, len);
    std::cout << Timestamp() << " [NET][SEND] " << message << std::endl;

    while (totalSent < len)
    {
        ssize_t n = ::send(fd, ptr + totalSent, len - totalSent, 0);
        if (n <= 0)
        {
            std::cerr << Timestamp() << " [NET][ERROR] send() failed: " << strerror(errno) << std::endl;
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
    std::cout << Timestamp() << " [NET] SendLine(): sent " << msg.size() << " bytes, result=" << result << std::endl;
    return result;
}
