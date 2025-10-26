#ifndef TRACE_HPP
#define TRACE_HPP

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>
#include <string>

// Utility to get current timestamp (HH:MM:SS.mmm)
inline std::string currentTimestamp()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// Core TRACE macros
#define TRACE_ENTER()                                                                          \
    do {                                                                                       \
        std::ostringstream _trace_oss;                                                         \
        _trace_oss << "[" << currentTimestamp() << "] [ENTER] ("                               \
                   << std::this_thread::get_id() << ") " << __FUNCTION__ << std::endl;          \
        std::cout << _trace_oss.str();                                                         \
    } while (0)

#define TRACE_EXIT()                                                                           \
    do {                                                                                       \
        std::ostringstream _trace_oss;                                                         \
        _trace_oss << "[" << currentTimestamp() << "] [EXIT]  ("                               \
                   << std::this_thread::get_id() << ") " << __FUNCTION__ << std::endl;          \
        std::cout << _trace_oss.str();                                                         \
    } while (0)

// Optional: print custom trace message with thread & time
#define TRACE_MSG(msg)                                                                         \
    do {                                                                                       \
        std::ostringstream _trace_oss;                                                         \
        _trace_oss << "[" << currentTimestamp() << "] [TRACE] ("                               \
                   << std::this_thread::get_id() << ") " << msg << std::endl;                   \
        std::cout << _trace_oss.str();                                                         \
    } while (0)

#endif // TRACE_HPP

