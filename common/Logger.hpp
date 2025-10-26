#pragma once
#include <string>

void InitLogger(const std::string &prefix);
void LogLine(const char *fmt, ...);
