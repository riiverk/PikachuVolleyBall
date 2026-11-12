#include "Server.h"
#include <cstdio>
#include <new>

Server::Server()
{
    InitializeCriticalSection(&stateLock_);
    InitializeCriticalSection(&closeLock_);
    // Manual-reset event: signaled whenever no accept loop is running.
    acceptStopped_ = CreateEvent(nullptr, TRUE, TRUE, nullptr);
    if (acceptStopped_ == nullptr) lastError_ = static_cast<int>(::GetLastError());
}

Server::~Server()
{
    CloseServer();
    if (acceptStopped_ != nullptr) CloseHandle(acceptStopped_);
    DeleteCriticalSection(&closeLock_);
    DeleteCriticalSection(&stateLock_);
}

bool Server::InitServer(u_short port)
{
    EnterCriticalSection(&stateLock_);
    if (acceptStopped_ == nullptr)
    {
        LeaveCriticalSection(&stateLock_);
        return false;
    }
    if (listenSocket_ != INVALID_SOCKET || closing_ || accepting_)
    {
        lastError_ = WSAEALREADY;
        LeaveCriticalSection(&stateLock_);
        return false;
    }
    WSADATA data{};
    const int startupError = WSAStartup(MAKEWORD(2, 2), &data);
    lastError_ = startupError;
    if (startupError != 0)
    {
        err_display(startupError); // ch02: WSAStartup returns the error code itself.
        LeaveCriticalSection(&stateLock_);
        return false;
    }

    SOCKET listener = INVALID_SOCKET;
    auto fail = [this, &listener](const char* what) {
        const int error = WSAGetLastError();
        err_display(what);
        if (listener != INVALID_SOCKET) closesocket(listener);
        WSACleanup();
        lastError_ = error;
        LeaveCriticalSection(&stateLock_);
        return false;
    };
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return fail("socket()");

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) return fail("bind()");
    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) return fail("listen()");

    // select + nonblocking accept lets shutdown stop the loop without closing
    // a socket underneath another thread's Winsock call.
    u_long nonblocking = 1;
    if (ioctlsocket(listener, FIONBIO, &nonblocking) == SOCKET_ERROR) return fail("ioctlsocket()");

    int length = sizeof(address);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR) return fail("getsockname()");
    listenSocket_ = listener;
    winsockStarted_ = true;
    port_ = ntohs(address.sin_port); // Port 0 requests an available port for tests.
    lastError_ = 0;
    stopping_ = false;
    acceptedCount_ = 0;
    LeaveCriticalSection(&stateLock_);
    return true;
}

void Server::AcceptLoop()
{
    SOCKET listener;
    {
        EnterCriticalSection(&stateLock_);
        if (closing_ || stopping_ || listenSocket_ == INVALID_SOCKET)
        {
            LeaveCriticalSection(&stateLock_);
            return;
        }
        if (accepting_)
        {
            lastError_ = WSAEALREADY;
            LeaveCriticalSection(&stateLock_);
            return;
        }
        accepting_ = true;
        ResetEvent(acceptStopped_);
        listener = listenSocket_;
        LeaveCriticalSection(&stateLock_);
    }
    while (!IsStopping())
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listener, &readable);
        timeval timeout{0, 100000};
        const int ready = select(0, &readable, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR)
        {
            err_display("select()");
            SetError(WSAGetLastError());
            break;
        }
        if (ready == 0 || IsStopping()) continue;
        sockaddr_in clientaddr{};
        int addrlen = sizeof(clientaddr);
        SOCKET accepted = accept(listener, reinterpret_cast<sockaddr*>(&clientaddr), &addrlen);
        if (accepted == INVALID_SOCKET)
        {
            const int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK || error == WSAECONNRESET) continue;
            err_display("accept()");
            SetError(error);
            break;
        }
        // Lecture ch04/ch06: print the connected client's address.
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
        std::printf("\n[TCP server] client connected: IP=%s, port=%d\n", addr, ntohs(clientaddr.sin_port));
        if (!HasClientSlot())
        {
            // Lecture ch11: refuse the connection when the fixed client array is full.
            std::printf("[error] cannot add more client information (max %d)\n", MAX_CLIENTS);
            closesocket(accepted);
            continue;
        }
        u_long blocking = 0;
        if (ioctlsocket(accepted, FIONBIO, &blocking) == SOCKET_ERROR)
        {
            err_display("ioctlsocket()");
            SetError(WSAGetLastError());
            closesocket(accepted);
            continue;
        }
        ClientSession* session = new (std::nothrow) ClientSession(accepted, &rooms_);
        if (!session)
        {
            closesocket(accepted);
            SetError(WSAENOBUFS);
            break;
        }
        if (IsStopping())
        {
            delete session;
            break;
        }
        if (!session->Start())
        {
            err_display(static_cast<int>(session->GetLastError())); // CreateThread failed
            SetError(static_cast<int>(session->GetLastError()));
            delete session;
            continue;
        }
        EnterCriticalSection(&stateLock_);
        clients_[clientCount_++] = session;
        ++acceptedCount_;
        LeaveCriticalSection(&stateLock_);
    }
    {
        EnterCriticalSection(&stateLock_);
        accepting_ = false;
        SetEvent(acceptStopped_);
        LeaveCriticalSection(&stateLock_);
    }
}

void Server::CloseServer()
{
    // Serialize close calls, but allow the accept loop to use stateLock_.
    EnterCriticalSection(&closeLock_);
    EnterCriticalSection(&stateLock_);
    closing_ = true;
    stopping_ = true;
    LeaveCriticalSection(&stateLock_);

    if (acceptStopped_ != nullptr) WaitForSingleObject(acceptStopped_, INFINITE);

    EnterCriticalSection(&stateLock_);
    const SOCKET listener = listenSocket_;
    listenSocket_ = INVALID_SOCKET;
    port_ = 0;
    const bool cleanup = winsockStarted_;
    winsockStarted_ = false;
    ClientSession* clients[MAX_CLIENTS]{};
    const int count = clientCount_;
    for (int i = 0; i < count; ++i)
    {
        clients[i] = clients_[i];
        clients_[i] = nullptr;
    }
    clientCount_ = 0;
    LeaveCriticalSection(&stateLock_);
    if (listener != INVALID_SOCKET) closesocket(listener);
    // ClientSession's destructor closes its socket and waits for its worker.
    for (int i = 0; i < count; ++i) delete clients[i];
    if (cleanup) WSACleanup();
    EnterCriticalSection(&stateLock_);
    closing_ = false;
    LeaveCriticalSection(&stateLock_);
    LeaveCriticalSection(&closeLock_);
}

u_short Server::GetPort() const
{
    EnterCriticalSection(&stateLock_);
    const u_short port = port_;
    LeaveCriticalSection(&stateLock_);
    return port;
}

std::size_t Server::GetClientCount() const
{
    EnterCriticalSection(&stateLock_);
    const std::size_t count = static_cast<std::size_t>(clientCount_);
    LeaveCriticalSection(&stateLock_);
    return count;
}

bool Server::IsAccepting() const
{
    EnterCriticalSection(&stateLock_);
    const bool accepting = accepting_;
    LeaveCriticalSection(&stateLock_);
    return accepting;
}

int Server::GetLastError() const
{
    EnterCriticalSection(&stateLock_);
    const int error = lastError_;
    LeaveCriticalSection(&stateLock_);
    return error;
}

std::size_t Server::GetAcceptedCount() const
{
    EnterCriticalSection(&stateLock_);
    const std::size_t count = acceptedCount_;
    LeaveCriticalSection(&stateLock_);
    return count;
}

bool Server::IsStopping() const
{
    EnterCriticalSection(&stateLock_);
    const bool stopping = stopping_;
    LeaveCriticalSection(&stateLock_);
    return stopping;
}

void Server::SetError(int error)
{
    EnterCriticalSection(&stateLock_);
    lastError_ = error;
    LeaveCriticalSection(&stateLock_);
}

// Called only by the accept loop, the only thread that adds sessions.
bool Server::HasClientSlot()
{
    if (GetClientCount() < static_cast<std::size_t>(MAX_CLIENTS)) return true;
    RemoveFinishedClients();
    return GetClientCount() < static_cast<std::size_t>(MAX_CLIENTS);
}

// Lecture ch11 RemoveSocketInfo: move the last entry into the removed slot.
// A session still referenced by a room is kept until CloseServer.
void Server::RemoveFinishedClients()
{
    ClientSession* finished[MAX_CLIENTS]{};
    int finishedCount = 0;
    EnterCriticalSection(&stateLock_);
    int i = 0;
    while (i < clientCount_)
    {
        ClientSession* session = clients_[i];
        if (session->WaitForFinish(0) && !session->IsInRoom())
        {
            finished[finishedCount++] = session;
            clients_[i] = clients_[clientCount_ - 1];
            clients_[clientCount_ - 1] = nullptr;
            --clientCount_;
            continue;
        }
        ++i;
    }
    LeaveCriticalSection(&stateLock_);
    for (int j = 0; j < finishedCount; ++j) delete finished[j];
}
