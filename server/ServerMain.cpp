#include "../common/Logger.hpp"
#include "../common/NetUtils.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

/*
 *  ServerMain.cpp
 *  ---------------
 *  Simple TCP file server for the distributed chatroom.
 *  Provides:
 *    VIEW – read chat file
 *    POST – append to chat file
 *
 *  This version adds full diagnostic logging to stdout/stderr
 *  so you can see exactly what requests arrive and how they’re handled.
 */

static std::string g_file = "chat.txt";

static void HandleView(int clientFd, const std::string &line)
{
    std::cout << "[SERVER] VIEW request received" << std::endl;
    std::ifstream file(g_file);
    if (!file.is_open())
    {
        std::cerr << "[SERVER] ERR open: cannot open " << g_file << std::endl;
        SendLine(clientFd, "ERR open");
        return;
    }

    std::ostringstream buf;
    buf << file.rdbuf();
    std::string content = buf.str();
    file.close();

    std::ostringstream header;
    header << "OK " << content.size() << "\n";
    SendLine(clientFd, header.str());
    SendAll(clientFd, content.c_str(), content.size());
    SendLine(clientFd, "."); // end marker
    std::cout << "[SERVER] VIEW served " << content.size() << " bytes" << std::endl;
}

static void HandlePost(int clientFd, const std::string &line)
{
    std::cout << "[SERVER] POST request: " << line << std::endl;
    std::ofstream file(g_file, std::ios::app);
    if (!file.is_open())
    {
        std::cerr << "[SERVER] ERR open: cannot append " << g_file << std::endl;
        SendLine(clientFd, "ERR open");
        return;
    }

    std::string msg = line.substr(5); // strip "POST "
    file << msg << "\n";
    file.close();

    SendLine(clientFd, "OK");
    std::cout << "[SERVER] APPEND successful: " << msg << std::endl;
}

int main(int argc, char **argv)
{
    std::string bindAddr = "0.0.0.0:7000";

    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--bind") && i + 1 < argc)
            bindAddr = argv[++i];
        else if (!strcmp(argv[i], "--file") && i + 1 < argc)
            g_file = argv[++i];
    }

    std::cout << "[SERVER] Starting on " << bindAddr << " using file: " << g_file << std::endl;

    int listenFd = TcpListen(bindAddr);
    if (listenFd < 0)
    {
        perror("listen");
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

        // Trim CR/LF
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        std::cout << "[SERVER] Received line: \"" << line << "\"" << std::endl;

        if (line.rfind("VIEW", 0) == 0)
        {
            HandleView(clientFd, line);
        }
        else if (line.rfind("POST ", 0) == 0)
        {
            HandlePost(clientFd, line);
        }
        else
        {
            std::cerr << "[SERVER] Unknown command: " << line << std::endl;
            SendLine(clientFd, "ERR badcmd");
        }

        ::close(clientFd);
    }

    return 0;
}
