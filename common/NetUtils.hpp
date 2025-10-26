#pragma once
#include <string>

int TcpListen(const std::string &hostPort);
int TcpConnect(const std::string &host, const std::string &port);
int TcpConnectHostPort(const std::string &hostPort);
int RecvLine(int fd, std::string &out, size_t max = 8192);
int SendAll(int fd, const void *buf, size_t len);
int SendLine(int fd, const std::string &line);
