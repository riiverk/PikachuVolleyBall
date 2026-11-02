#include "Server.h"

Server::~Server() { CloseServer(); }

bool Server::InitServer(u_short port)
{
    if (listenSocket_ != INVALID_SOCKET)
    {
        lastError_ = WSAEALREADY;
        return false;
    }
    WSADATA data{};
    lastError_ = WSAStartup(MAKEWORD(2, 2), &data);
    if (lastError_ != 0) return false;
    winsockStarted_ = true;

    auto fail = [this]() {
        const int error = WSAGetLastError();
        CloseServer();
        lastError_ = error;
        return false;
    };
    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) return fail();

    // Do not share an active game's port with a second server process.
    const BOOL exclusive = TRUE;
    if (setsockopt(listenSocket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
        reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR) return fail();

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) return fail();
    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) return fail();

    int length = sizeof(address);
    if (getsockname(listenSocket_, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR) return fail();
    port_ = ntohs(address.sin_port); // Port 0 requests an available port for tests.
    lastError_ = 0;
    return true;
}
void Server::AcceptLoop() { /* TODO 11/3: accept and client thread creation. */ }
void Server::CloseServer()
{
    if (listenSocket_ != INVALID_SOCKET)
    {
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }
    port_ = 0;
    if (winsockStarted_)
    {
        WSACleanup();
        winsockStarted_ = false;
    }
    // Session/room thread shutdown is added when those threads are implemented.
}
