#include "../common/Logger.hpp"
#include "../common/NetUtils.hpp"
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
}

static void HandlePost(int clientFd, const std::string &line)
{
    std::ofstream file(g_file, std::ios::app);
    if (!file.is_open())
    {
        dprintf(clientFd, "ERR open\n");
        return;
    }

    file << line.substr(5) << "\n";
    file.close();
    dprintf(clientFd, "OK\n");
    LogLine("[SERVER] POST appended: %s", line.substr(5).c_str());
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

    InitLogger("server");
    std::cout << "[SERVER] Starting on " << bindAddr << " using file: " << g_file << std::endl;

    int listenFd = TcpListen(bindAddr);
    if (listenFd < 0)
    {
        std::cerr << "Failed to bind " << bindAddr << std::endl;
        return 1;
    }

    while (true)
    {
        int clientFd = ::accept(listenFd, nullptr, nullptr);
        if (clientFd < 0)
            continue;

        std::string line;
        if (RecvLine(clientFd, line) <= 0)
        {
            ::close(clientFd);
            continue;
        }

        LogLine("[SERVER] Received line: \"%s\"", line.c_str());

        if (line.rfind("VIEW", 0) == 0)
            HandleView(clientFd);
        else if (line.rfind("POST", 0) == 0)
            HandlePost(clientFd, line);
        else
            dprintf(clientFd, "ERR unknown\n");

        ::close(clientFd);
    }

    return 0;
}
