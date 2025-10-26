#include "Logger.hpp"
#include <cstdarg>
#include <cstdio>
#include <ctime>

static FILE *g_logFile = nullptr;

void InitLogger(const std::string &prefix)
{
    if (g_logFile)
        return;
    char timestamp[32];
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    strftime(timestamp, sizeof timestamp, "%Y%m%d-%H%M%S", &tm);
    std::string name = prefix + "-" + timestamp + ".log";
    g_logFile = std::fopen(name.c_str(), "w");
    if (!g_logFile)
        g_logFile = stderr;
}

void LogLine(const char *fmt, ...)
{
    if (!g_logFile)
        g_logFile = stderr;
    char ts[32];
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    strftime(ts, sizeof ts, "%H:%M:%S", &tm);
    std::fprintf(g_logFile, "[%s] ", ts);

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_logFile, fmt, ap);
    va_end(ap);

    std::fputc('\n', g_logFile);
    std::fflush(g_logFile);
}
