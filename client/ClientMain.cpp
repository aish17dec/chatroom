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

/* Timestamp formatter for chat lines */
static void formatTimestamp(char *buf, size_t n)
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::strftime(buf, n, "%d %b %I:%M %p", &tm);
}

/* Connect to peer; keep retrying forever so start order doesn't matter */
static int connectPeerForever(const std::string &addr)
{
    const auto pos = addr.find(':');
    if (pos == std::string::npos)
    {
        std::cerr << "Invalid --peer " << addr << std::endl;
        return -1;
    }
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

/* Peer thread: accept RA messages and forward to DME */
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
            // Keep the newline exactly as received for the parser
            std::cout << "[CLIENT " << dme->getSelfId() << "] peer->me: " << line;
            dme->handleRaMessage(line);
        }
        ::close(connFd);
    }
}

/* User thread: handle CLI commands view / post / quit */
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

        // Non-critical read
        if (input == "view" || input.rfind("view -n", 0) == 0)
        {
            int sfd = TcpConnectHostPort(serverAddr);
            if (sfd < 0)
            {
                std::cout << "Server unreachable\n";
                continue;
            }

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
        // Critical write
        else if (input.rfind("post ", 0) == 0)
        {
            if (!dme->requestCs())
            {
                std::cout << "Could not acquire lock (peer unresponsive)\n";
                continue;
            }

            int sfd = TcpConnectHostPort(serverAddr);
            if (sfd < 0)
            {
                std::cout << "Server unreachable\n";
                dme->releaseCs();
                continue;
            }

            char ts[64];
            formatTimestamp(ts, sizeof ts);
            std::string text = input.substr(5); // after "post "
            std::string payload = std::string("POST ") + ts + " " + userName + ": " + text;
            SendLine(sfd, payload);

            std::string resp;
            if (RecvLine(sfd, resp) > 0 && resp.rfind("OK", 0) == 0)
                std::cout << "(posted)\n";
            else
                std::cout << "POST failed\n";

            ::close(sfd);
            dme->releaseCs();
        }
        else
        {
            std::cout << "Commands: view | post \"text\" | quit\n";
        }
    }
}

int main(int argc, char **argv)
{
    std::string userName = "User";
    int selfId = 1;
    int peerId = 2;
    std::string peerAddr = "127.0.0.1:8002";
    std::string serverAddr = "127.0.0.1:7000";
    std::string listenAddr = "0.0.0.0:8001";

    // Parse CLI
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

    // Logging
    char prefix[32];
    std::snprintf(prefix, sizeof prefix, "client-%d", selfId);
    InitLogger(prefix);

    // 1) Peer listen socket (for incoming RA)
    int listenFd = TcpListen(listenAddr);
    if (listenFd < 0)
    {
        std::perror("listen peer");
        return 1;
    }

    // 2) Peer connect (for outgoing RA) — retry forever
    int peerFd = connectPeerForever(peerAddr);
    if (peerFd < 0)
        return 1;

    // 3) DME object shared by both threads
    DME dme(selfId, peerId, peerFd);

    // 4) Start the two threads
    std::thread tPeer(peerAcceptLoop, listenFd, &dme);            // RA listener
    std::thread tUser(userInputLoop, userName, serverAddr, &dme); // CLI

    // 5) Wait for CLI to finish; RA thread runs as a service
    tUser.join();

    // We keep it simple: exit process (peer thread will be torn down by OS).
    return 0;
}
