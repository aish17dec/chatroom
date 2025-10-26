#include "../common/Logger.hpp"
#include "../common/NetUtils.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/socket.h> // for accept(), bind(), listen(), connect()
#include <unistd.h>
#include <unistd.h> // for close(), read(), write()

static const char *g_chatFile = "./chat.txt";

static void HandleView(int clientFd, const std::string &line)
{
    FILE *file = std::fopen(g_chatFile, "r");
    if (!file)
    {
        SendLine(clientFd, "ERR open");
        return;
    }
    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    SendLine(clientFd, "OK " + std::to_string(size));
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, file)) > 0)
        SendAll(clientFd, buf, n);
    SendLine(clientFd, ".");
    std::fclose(file);
}

static void HandlePost(int clientFd, const std::string &line)
{
    const char *payload = line.c_str() + 5; // skip "POST "
    FILE *file = std::fopen(g_chatFile, "a");
    if (!file)
    {
        SendLine(clientFd, "ERR open");
        return;
    }
    std::fprintf(file, "%s\n", payload);
    std::fclose(file);
    SendLine(clientFd, "OK");
    LogLine("APPEND: %s", payload);
}

int main(int argc, char **argv)
{
    const char *bindAddr = "0.0.0.0:7000";
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--bind") && i + 1 < argc)
            bindAddr = argv[++i];
        else if (!strcmp(argv[i], "--file") && i + 1 < argc)
            g_chatFile = argv[++i];
    }

    InitLogger("Server");
    int listenFd = TcpListen(bindAddr);
    if (listenFd < 0)
    {
        perror("listen");
        return 1;
    }

    LogLine("LISTEN on %s; file=%s", bindAddr, g_chatFile);

    while (true)
    {
        int clientFd = ::accept(listenFd, nullptr, nullptr);
        if (clientFd < 0)
            continue;
        std::string line;
        if (RecvLine(clientFd, line) > 0)
        {
            if (line.rfind("VIEW", 0) == 0)
                HandleView(clientFd, line);
            else if (line.rfind("POST ", 0) == 0)
                HandlePost(clientFd, line);
            else
                SendLine(clientFd, "ERR badcmd");
        }
        ::close(clientFd);
    }
}
