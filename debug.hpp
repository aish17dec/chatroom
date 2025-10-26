#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

// Returns a formatted timestamp string in the format: [YYYY-MM-DD HH:MM:SS]
inline std::string Timestamp()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S]");
    return oss.str();
}

#endif // DEBUG_HPP

