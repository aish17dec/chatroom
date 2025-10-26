#include "../common/NetUtils.hpp"
#include "../debug.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

static std::string g_file;

static void HandleView(int clientFd)
{
    std::ifstream file(g_file);
    if (!file.is_open())
    {
        std::ofstream create(g_file);
        create.close();
        file.open(g_file);
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    dprintf(clientFd, "OK %zu\n%s.\n", content.size(), content.c_str());

    std::cout << Timestamp() << " [SERVER] VIEW request served. File size: " << content.size() << " bytes" << std::endl;
}

static void HandlePost(int clientFd, const std::string &line)
{
    std::ofstream file(g_file, std::ios::app);
    if (!file.is_open())
    {
        dprintf(clientFd, "ERR open\n");
        std::cerr << Timestamp() << " [SERVER] ERROR: Failed to open file for POST" << std::endl;
        return;
    }

    std::string message = line.substr(5);
    file << message << "\n";
    file.close();

    dprintf(clientFd, "OK\n");
    std::cout << Timestamp() << " [SERVER] POST appended: " << message << std::endl;
}

int main(int argc, char **argv)
{
    std::string bindAddr = "0.0.0.0:7000";
    g_file = "./chat.txt";

    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--bind") && i + 1 < argc)
            bindAddr = argv[++i];
        else if (!strcmp(argv[i], "--file") && i + 1 < argc)
            g_file = argv[++i];
    }

    std::cout << Timestamp() << " [SERVER] Starting on " << bindAddr << " using file: " << g_file << std::endl;

    int listenFd = TcpListen(bindAddr);
    if (listenFd < 0)
    {
        std::cerr << Timestamp() << " [SERVER] ERROR: Failed to bind " << bindAddr << std::endl;
        return 1;
    }

    std::cout << Timestamp() << " [SERVER] Listening for connections..." << std::endl;

    while (true)
    {
        int clientFd = ::accept(listenFd, nullptr, nullptr);
        if (clientFd < 0)
        {
            std::cerr << Timestamp() << " [SERVER] WARNING: accept() failed" << std::endl;
            continue;
        }

        std::string line;
        if (RecvLine(clientFd, line) <= 0)
        {
            std::cerr << Timestamp() << " [SERVER] WARNING: Failed to receive data from client" << std::endl;
            ::close(clientFd);
            continue;
        }

        std::cout << Timestamp() << " [SERVER] Received line: \"" << line << "\"" << std::endl;

        if (line.rfind("VIEW", 0) == 0)
        {
            HandleView(clientFd);
        }
        else if (line.rfind("POST", 0) == 0)
        {
            HandlePost(clientFd, line);
        }
        else
        {
            dprintf(clientFd, "ERR unknown\n");
            std::cerr << Timestamp() << " [SERVER] ERROR: Unknown command received" << std::endl;
        }

        ::close(clientFd);
        std::cout << Timestamp() << " [SERVER] Connection closed" << std::endl;
    }

    return 0;
}
