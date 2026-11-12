#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "GameConst.h"
#include "Protocol.h"

// Blocking socket only. Returns len on success, a SHORT count on graceful EOF,
// or SOCKET_ERROR (inspect WSAGetLastError). A packet reader must reject short
// reads, including a close after receiving only part of the packet header.
// The caller owns the socket, WSAStartup/WSACleanup, and any receive timeout.
int recvn(SOCKET socket, char* buffer, int len, int flags = 0);

// Lecture ch02: print a socket error code as text with FormatMessage.
void err_display(const char* msg); // uses WSAGetLastError()
void err_display(int errcode);     // uses a saved error code
