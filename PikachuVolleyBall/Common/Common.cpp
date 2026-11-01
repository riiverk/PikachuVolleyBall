#include "Common.h"

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
