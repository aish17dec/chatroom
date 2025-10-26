#include "../common/Logger.hpp"
#include "../common/NetUtils.hpp"
#include "DME.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <sys/socket.h> // for accept(), bind(), listen(), connect()
#include <thread>
#include <unistd.h> // for close(), read(), write()

/*
 *  ClientMain.cpp
 *  ---------------
 *  Implements the user-side chat client with:
 *   - VIEW (non-critical)
 *   - POST (critical; uses DME)
 *  Communication:
 *   - To File Server: VIEW / POST commands
 *   - To Peer Node:   REQ / REP / REL messages (DME)
 */

static void formatTimestamp(char *buffer, size_t size)
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::strftime(buffer, size, "%d %b %I:%M %p", &tm);
}

/*
 * Background thread: listens for incoming RA messages
 * from the peer node and forwards them to DME::handleRaMessage().
 */
static void peerListener(int listenFd, DME *dme)
{
    while (true)
    {
        int connFd = ::accept(listenFd, nullptr, nullptr);
        if (connFd < 0)
            continue;

        std::string line;
        while (RecvLine(connFd, line) > 0)
        {
            dme->handleRaMessage(line);
        }
        ::close(connFd);
    }
}

int main(int argc, char **argv)
{
    std::string userName = "User";
    int selfId = 1;
    int peerId = 2;
    std::string peerAddress = "0.0.0.0:8002";
    std::string serverAddress = "127.0.0.1:7000";
    std::string listenAddress = "0.0.0.0:8001";

    // Parse CLI arguments
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--user") && i + 1 < argc)
            userName = argv[++i];
        else if (!strcmp(argv[i], "--self-id") && i + 1 < argc)
            selfId = std::atoi(argv[++i]);
        else if (!strcmp(argv[i], "--peer-id") && i + 1 < argc)
            peerId = std::atoi(argv[++i]);
        else if (!strcmp(argv[i], "--peer") && i + 1 < argc)
            peerAddress = argv[++i];
        else if (!strcmp(argv[i], "--server") && i + 1 < argc)
            serverAddress = argv[++i];
        else if (!strcmp(argv[i], "--listen") && i + 1 < argc)
            listenAddress = argv[++i];
    }

    // Initialise logging
    char logPrefix[32];
    std::snprintf(logPrefix, sizeof logPrefix, "client-%d", selfId);
    InitLogger(logPrefix);

    // Listen for peer RA messages
    int peerListenFd = TcpListen(listenAddress);
    if (peerListenFd < 0)
    {
        std::perror("listen peer");
        return 1;
    }

    // Connect to peer for outgoing RA messages
    int peerFd = TcpConnectHostPort(peerAddress);
    if (peerFd < 0)
    {
        std::perror("connect peer");
        return 1;
    }

    // Construct the DME object
    DME dme(selfId, peerId, peerFd);

    // Start listener thread for peer messages
    std::thread peerThread(peerListener, peerListenFd, &dme);
    peerThread.detach();

    // CLI loop
    std::cout << "Chat Room — DC Assignment II\n";
    std::cout << "User: " << userName << " (self=" << selfId << ", peer=" << peerId << ")\n";
    std::cout << "Commands: view | post \"text\" | quit\n";

    std::string input;
    while (true)
    {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input))
            break;

        if (input == "quit")
            break;

        // Handle VIEW
        if (input == "view" || input.rfind("view -n", 0) == 0)
        {
            int sfd = TcpConnectHostPort(serverAddress);
            if (sfd < 0)
            {
                std::cout << "Error: cannot reach server\n";
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

        // Handle POST (Critical Section)
        else if (input.rfind("post ", 0) == 0)
        {
            if (!dme.requestCs())
            {
                std::cout << "Could not acquire lock (peer unresponsive)\n";
                continue;
            }

            int sfd = TcpConnectHostPort(serverAddress);
            if (sfd < 0)
            {
                std::cout << "Server unreachable\n";
                dme.releaseCs();
                continue;
            }

            char timestamp[64];
            formatTimestamp(timestamp, sizeof timestamp);
            std::string text = input.substr(5);
            std::string payload = "POST " + std::string(timestamp) + " " + userName + ": " + text;
            SendLine(sfd, payload);

            std::string response;
            if (RecvLine(sfd, response) > 0 && response.rfind("OK", 0) == 0)
                std::cout << "(posted)\n";
            else
                std::cout << "POST failed\n";

            ::close(sfd);
            dme.releaseCs();
        }

        else
        {
            std::cout << "Commands: view | post \"text\" | quit\n";
        }
    }

    return 0;
}
