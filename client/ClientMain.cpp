#include "../common/Logger.hpp"
#include "../common/NetUtils.hpp"
#include "DME.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

static void formatTimestamp(char *buf, size_t n)
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::strftime(buf, n, "%d %b %I:%M %p", &tm);
}

static int connectPeerForever(const std::string &addr)
{
    const auto pos = addr.find(':');
    const std::string host = addr.substr(0, pos);
    const std::string port = addr.substr(pos + 1);

    int fd = -1, attempt = 0;
    while (true)
    {
        fd = TcpConnect(host, port);
        if (fd >= 0)
        {
            std::cout << "Connected to peer " << addr << " after " << attempt << " attempts.\n";
            return fd;
        }
        ++attempt;
        std::cout << "Peer not ready, retrying in 2s... (attempt " << attempt << ")\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

static void peerAcceptLoop(int listenFd, DME *dme)
{
    while (true)
    {
        int connFd = ::accept(listenFd, nullptr, nullptr);
        if (connFd < 0)
            continue;

        std::string line;
        while (RecvLine(connFd, line) > 0)
        {
            std::cout << "[CLIENT " << dme->getSelfId() << "] peer->me: " << line;
            dme->handleRaMessage(line);
        }
        ::close(connFd);
    }
}

static void userInputLoop(const std::string &userName, const std::string &serverAddr, DME *dme)
{
    std::cout << "Chat Room — DC Assignment II\n";
    std::cout << "User: " << userName << " (self=" << dme->getSelfId() << ", peer=" << dme->getPeerId() << ")\n";
    std::cout << "Commands: view | post \"text\" | quit\n";

    std::string input;
    while (true)
    {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input))
            break;

        if (input == "quit")
            break;

        // Handle VIEW command, user wants to see chat messages
        // Supports "view"
        if (input == "view" || input.rfind("view -n", 0) == 0)
        {
            int sfd = TcpConnectHostPort(serverAddr);
            if (sfd < 0)
            {
                std::cout << "Server unreachable\n";
                continue;
            }

            // Send VIEW command to server and print response lines
            // until "." line is received
            SendLine(sfd, input == "view" ? "VIEW" : input);
            std::string header;
            if (RecvLine(sfd, header) <= 0 || header.rfind("OK", 0) != 0)
            {
                std::cout << "Server error\n";
                ::close(sfd);
                continue;
            }

            std::string line;
            while (RecvLine(sfd, line) > 0)
            {
                if (line == ".\n" || line == ".\r\n")
                    break;
                std::cout << line;
            }
            ::close(sfd);
        }
        else if (input.rfind("post ", 0) == 0)
        {
            if (!dme->requestCriticalSection())
            {
                std::cout << "Could not acquire lock (peer unresponsive)\n";
                continue;
            }

            int sfd = TcpConnectHostPort(serverAddr);
            if (sfd < 0)
            {
                std::cout << "Server unreachable\n";
                dme->releaseCriticalSection();
                continue;
            }

            char ts[64];
            formatTimestamp(ts, sizeof ts);
            std::string text = input.substr(5);
            std::string payload = std::string("POST ") + ts + " " + userName + ": " + text;
            SendLine(sfd, payload);

            std::string resp;
            if (RecvLine(sfd, resp) > 0 && resp.rfind("OK", 0) == 0)
                std::cout << "(posted)\n";
            else
                std::cout << "POST failed\n";

            ::close(sfd);
            dme->releaseCriticalSection();
        }
    }
}

int main(int argc, char **argv)
{
    std::string userName = "User";
    int selfId = 1;
    int peerId = 2;
    std::string peerAddr;
    std::string serverAddr;
    std::string listenAddr;

    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--user") && i + 1 < argc)
            userName = argv[++i];
        else if (!strcmp(argv[i], "--self-id") && i + 1 < argc)
            selfId = std::atoi(argv[++i]);
        else if (!strcmp(argv[i], "--peer-id") && i + 1 < argc)
            peerId = std::atoi(argv[++i]);
        else if (!strcmp(argv[i], "--peer") && i + 1 < argc)
            peerAddr = argv[++i];
        else if (!strcmp(argv[i], "--server") && i + 1 < argc)
            serverAddr = argv[++i];
        else if (!strcmp(argv[i], "--listen") && i + 1 < argc)
            listenAddr = argv[++i];
    }

    char prefix[32];
    std::snprintf(prefix, sizeof prefix, "client-%d", selfId);
    InitLogger(prefix);

    int listenFd = TcpListen(listenAddr);
    if (listenFd < 0)
    {
        std::perror("listen peer");
        return 1;
    }

    int peerFd = connectPeerForever(peerAddr);
    if (peerFd < 0)
        return 1;

    DME dme(selfId, peerId, peerFd);

    std::thread tPeer(peerAcceptLoop, listenFd, &dme);
    std::thread tUser(userInputLoop, userName, serverAddr, &dme);
    tUser.join();
    return 0;
}
