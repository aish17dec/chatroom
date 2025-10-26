/**
 * @file ClientMain.cpp
 * @brief Implements the client process for the distributed chatroom
 *        using the Ricart–Agrawala (RA) algorithm for mutual exclusion.
 *
 * Each client instance performs two concurrent tasks:
 *   1. Listens for messages from its peer (REQ, REP, REL) via a TCP listener.
 *   2. Interacts with the user via a console prompt to send 'view' and 'post' commands.
 *
 * The client coordinates with its peer to ensure that only one node
 * performs a POST (write) operation to the server at a time.
 */

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

/**
 * @brief Format the current system time into a readable timestamp.
 *
 * Used to prepend messages with a consistent date and time,
 * e.g., "26 Oct 10:35 AM".
 *
 * @param buf Buffer to store the formatted timestamp.
 * @param n   Maximum number of bytes that can be written to the buffer.
 */
static void formatTimestamp(char *buf, size_t n)
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::strftime(buf, n, "%d %b %I:%M %p", &tm);
}

/**
 * @brief Continuously attempts to connect to a peer node until successful.
 *
 * In the distributed chatroom, both clients are symmetric — they may
 * start at different times. This function retries indefinitely until
 * a connection can be established, ensuring that startup order does not matter.
 *
 * @param addr Peer address in "host:port" format.
 * @return Connected socket descriptor (>= 0) on success, or -1 on fatal error.
 */
static int connectPeerForever(const std::string &addr)
{
    const auto pos = addr.find(':');
    if (pos == std::string::npos)
    {
        std::cerr << "Invalid peer address format: " << addr << std::endl;
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

/**
 * @brief Dedicated thread that listens for incoming RA messages.
 *
 * This function runs in an infinite loop, accepting connections from
 * the peer node on a listening socket. Each received line represents
 * one Ricart–Agrawala control message:
 *   - REQ <timestamp> <peer_id>
 *   - REP <peer_id>
 *   - REL <peer_id>
 *
 * These are forwarded to the DME handler for processing.
 *
 * @param listenFd The listening socket file descriptor.
 * @param dme Pointer to the shared DME (Distributed Mutual Exclusion) object.
 */
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
            std::cout << "[CLIENT " << dme->getSelfId() << "] Received from peer: " << line;
            dme->handleRaMessage(line);
        }
        ::close(connFd);
    }
}

/**
 * @brief User interface thread — handles command-line input interactively.
 *
 * Commands available to the user:
 *   - view          : Fetches chat history from the central file server.
 *   - post "text"   : Sends a message; requires mutual exclusion (RA lock).
 *   - quit          : Terminates the client gracefully.
 *
 * This thread blocks on user input while the peer listener thread
 * runs concurrently to handle RA coordination messages.
 *
 * @param userName   Name of the user displayed in chat messages.
 * @param serverAddr Address of the file server in "host:port" format.
 * @param dme        Pointer to the distributed mutual exclusion manager.
 */
static void userInputLoop(const std::string &userName, const std::string &serverAddr, DME *dme)
{
    std::cout << "--------------------------------------------------\n";
    std::cout << "Chat Room — DC Assignment II\n";
    std::cout << "User: " << userName << " (self=" << dme->getSelfId() << ", peer=" << dme->getPeerId() << ")\n";
    std::cout << "Commands: view | post \"text\" | quit\n";
    std::cout << "--------------------------------------------------\n";

    std::string input;
    while (true)
    {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input))
            break;

        if (input == "quit")
            break;

        /** Handle non-critical VIEW operation */
        if (input == "view" || input.rfind("view -n", 0) == 0)
        {
            int sfd = TcpConnectHostPort(serverAddr);
            if (sfd < 0)
            {
                std::cout << "Server unreachable.\n";
                continue;
            }

            SendLine(sfd, input == "view" ? "VIEW" : input);

            std::string header;
            if (RecvLine(sfd, header) <= 0 || header.rfind("OK", 0) != 0)
            {
                std::cout << "Server error.\n";
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

        /** Handle critical POST operation (requires lock via DME) */
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
                std::cout << "Server unreachable.\n";
                dme->releaseCs();
                continue;
            }

            char ts[64];
            formatTimestamp(ts, sizeof ts);
            std::string text = input.substr(5); // skip "post "
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

        /** Print help for invalid commands */
        else
        {
            std::cout << "Commands: view | post \"text\" | quit\n";
        }
    }
}

/**
 * @brief Entry point for the chatroom client process.
 *
 * This function performs the following:
 *   1. Parses command-line arguments for runtime configuration.
 *   2. Opens listening and outgoing sockets for RA communication.
 *   3. Spawns two threads:
 *        - Peer listener thread (for REQ/REP/REL).
 *        - User command thread (for CLI interaction).
 *   4. Waits for user termination.
 *
 * Example usage:
 * @code
 * ./bin/client --user "Lucy" \
 *   --self-id 1 \
 *   --peer-id 2 \
 *   --listen 0.0.0.0:8001 \
 *   --peer 172.31.27.84:8002 \
 *   --server 172.31.23.9:7000
 * @endcode
 *
 * @param argc Argument count.
 * @param argv Argument list.
 * @return int 0 on normal exit, 1 on error.
 */
int main(int argc, char **argv)
{
    // Display name for this client in chat messages
    std::string userName = "User";

    // Numeric ID used for Lamport timestamp tie-breaking
    int selfId = 1;

    // Numeric ID of the peer client
    int peerId = 2;

    // Peer node address used for Ricart–Agrawala message exchange
    // Format: "host:port" (e.g., "172.31.27.84:8002")
    std::string peerAddr = "127.0.0.1:8002";

    // Address of the central file server (handles VIEW and POST)
    // Format: "host:port" (e.g., "172.31.23.9:7000")
    std::string serverAddr = "127.0.0.1:7000";

    // Local listening address for incoming RA connections from the peer
    // "0.0.0.0" binds to all interfaces; port must match peer’s --peer value
    std::string listenAddr = "0.0.0.0:8001";

    // -------------------------------------------------------------
    // Parse command-line arguments
    // -------------------------------------------------------------
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

    // Initialise per-client logger (writes logs to client-<id>.log)
    char prefix[32];
    std::snprintf(prefix, sizeof prefix, "client-%d", selfId);
    InitLogger(prefix);

    // Create listening socket for incoming RA connections
    int listenFd = TcpListen(listenAddr);
    if (listenFd < 0)
    {
        std::perror("listen peer");
        return 1;
    }

    // Connect to peer (retry indefinitely until peer comes online)
    int peerFd = connectPeerForever(peerAddr);
    if (peerFd < 0)
        return 1;

    // Shared DME instance manages Ricart–Agrawala lock protocol
    DME dme(selfId, peerId, peerFd);

    // Launch two threads:
    //  - tPeer: listens for RA messages (REQ/REP/REL)
    //  - tUser: handles CLI commands and server communication
    std::thread tPeer(peerAcceptLoop, listenFd, &dme);
    std::thread tUser(userInputLoop, userName, serverAddr, &dme);

    // Wait for user thread to finish (exit on "quit" command)
    tUser.join();

    // The peer thread will terminate automatically when the process exits
    return 0;
}
