#include "Common.h"
#include <cstdio>

int recvn(SOCKET socket, char* buffer, int len, int flags)
{
    // PEEK would reread the same bytes instead of consuming the stream.
    if (len < 0 || (len > 0 && buffer == nullptr) || (flags & MSG_PEEK))
    {
        WSASetLastError(WSAEINVAL);
        return SOCKET_ERROR;
    }
    int received = 0;
    while (received < len)
    {
        const int count = recv(socket, buffer + received, len - received, flags);
        if (count == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEINTR) continue;
            return SOCKET_ERROR;
        }
        if (count == 0) break;
        received += count;
    }
    return received;
}

void err_display(const char* msg)
{
    LPVOID lpMsgBuf;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (char*)&lpMsgBuf, 0, NULL);
    std::printf("[%s] %s\n", msg, (char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

void err_display(int errcode)
{
    LPVOID lpMsgBuf;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, errcode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (char*)&lpMsgBuf, 0, NULL);
    std::printf("[error %d] %s\n", errcode, (char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}
