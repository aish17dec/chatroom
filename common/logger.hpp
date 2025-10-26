#pragma once
#include <string>

void log_init(const std::string& prefix);
void log_line(const char* fmt, ...);

