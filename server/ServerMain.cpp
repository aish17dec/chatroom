#include "../common/Logger.hpp"
#include "../common/NetUtils.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/*
 *  ServerMain.cpp
 *  ----------------
 *  TCP file server for distributed chatroom (DC Assignment 2)
 *  Commands:
 *    VIEW – read chat file
 *    POST – append a new message
 *
 *  This version:
 *    - Automatically creates chat.txt if missing
 *    - Forces immediate console flush (unbuffered logs)
 *    - Adds detailed diagnostics around RecvLine()
 */

static std::string g_file = "chat.txt";

static void EnsureFileExists()
{
    std::ifstream fin(g_file);
    if (!fin.good())
    {
        std::ofstream fout(g_file, std::ios::app);
        if (!fout.is_open())
        {
            std::cerr << "[SERVER] FATAL: cannot create file " << g_file << std::endl;
            exit(1);
        }
        fout.close();
        std::cout << "[SERVER] Created missing file: " << g_file << std::endl;
    }
}

/* Handle VIEW requests */
static void HandleView(int clientFd)
{
    std::cout << "[SERVER] VIEW request received" << std::endl;
    std::cout.flush();

    EnsureFileExists();

    std::ifstream file(g_file, std::ios::in);
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
    SendLine(clientFd, ".");

    std::cout << "[SERVER] VIEW served " << content.size() << " bytes" << std::endl;
    std::cout.flush();
}

/* Handle POST requests */
static void HandlePost(int clientFd, const std::string &line)
{
    std::cout << "[SERVER] POST request: " << line << std::endl;
    std::cout.flush();

    EnsureFileExists();

    std::ofstream file(g_file, std::ios::app);
    if (!file.is_open())
    {
        std::cerr << "[SERVER] ERR open: cannot append " << g_file << std::endl;
        SendLine(clientFd, "ERR open");
        return;
    }

    std::string msg = line.substr(5);
    file << msg << "\n";
    file.close();

    SendLine(clientFd, "OK");
    std::cout << "[SERVER] APPEND successful: " << msg << std::endl;
    std::cout.flush();
}

int main(int argc, char **argv)
{
    std::ios::sync_with_stdio(false);    // disable sync overhead
    setvbuf(stdout, nullptr, _IONBF, 0); // unbuffer stdout
    setvbuf(stderr, nullptr, _IONBF, 0); // unbuffer stderr

    std::string bindAddr = "0.0.0.0:7000";
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--bind") && i + 1 < argc)
            bindAddr = argv[++i];
        else if (!strcmp(argv[i], "--file") && i + 1 < argc)
            g_file = argv[++i];
    }

    std::cout << "[SERVER] Starting on " << bindAddr << " using file: " << g_file << std::endl;
    EnsureFileExists();

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

        std::cout << "[SERVER] Connection accepted" << std::endl;
        std::cout.flush();

        std::string line;
        int bytes = RecvLine(clientFd, line);
        std::cout << "[SERVER] RecvLine returned " << bytes << " bytes: \"" << line << "\"" << std::endl;
        std::cout.flush();

        if (bytes <= 0)
        {
            ::close(clientFd);
            continue;
        }

        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        if (line.rfind("VIEW", 0) == 0)
            HandleView(clientFd);
        else if (line.rfind("POST ", 0) == 0)
            HandlePost(clientFd, line);
        else
        {
            std::cerr << "[SERVER] Unknown command: " << line << std::endl;
            SendLine(clientFd, "ERR badcmd");
        }

        ::close(clientFd);
    }

    return 0;
}
