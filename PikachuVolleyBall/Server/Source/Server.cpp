#include "Server.h"
#include <conio.h>
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
        LeaveCriticalSection(&stateLock_);
        return false;
    }

    SOCKET listener = INVALID_SOCKET;
    auto fail = [this, &listener]() {
        const int error = WSAGetLastError();
        if (listener != INVALID_SOCKET) closesocket(listener);
        WSACleanup();
        lastError_ = error;
        LeaveCriticalSection(&stateLock_);
        return false;
    };
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return fail();

    // Do not share an active game's port with a second server process.
    const BOOL exclusive = TRUE;
    if (setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
        reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR) return fail();

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) return fail();
    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) return fail();

    // select + nonblocking accept lets shutdown stop the loop without closing
    // a socket underneath another thread's Winsock call.
    u_long nonblocking = 1;
    if (ioctlsocket(listener, FIONBIO, &nonblocking) == SOCKET_ERROR) return fail();

    int length = sizeof(address);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR) return fail();
    listenSocket_ = listener;
    winsockStarted_ = true;
    port_ = ntohs(address.sin_port); // Port 0 requests an available port for tests.
    lastError_ = 0;
    stopping_ = false;
    acceptedCount_ = 0;
    LeaveCriticalSection(&stateLock_);
    return true;
}

void Server::EnableConsoleExit()
{
    DWORD mode;
    const bool hasConsole = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != FALSE;
    EnterCriticalSection(&stateLock_);
    consoleExitEnabled_ = hasConsole;
    LeaveCriticalSection(&stateLock_);
}

void Server::AcceptLoop()
{
    SOCKET listener;
    bool consoleExitEnabled;
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
        consoleExitEnabled = consoleExitEnabled_;
        LeaveCriticalSection(&stateLock_);
    }
    while (!IsStopping())
    {
        // The main thread checks Enter between socket waits; no input thread is needed.
        if (consoleExitEnabled && _kbhit())
        {
            const int key = _getch();
            if (key == '\r') break;
            if (key == 0 || key == 0xe0) _getch(); // Consume an extended key's second byte.
        }
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listener, &readable);
        timeval timeout{0, 100000};
        const int ready = select(0, &readable, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR)
        {
            SetError(WSAGetLastError());
            break;
        }
        if (ready == 0 || IsStopping()) continue;
        SOCKET accepted = accept(listener, nullptr, nullptr);
        if (accepted == INVALID_SOCKET)
        {
            const int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK || error == WSAECONNRESET) continue;
            SetError(error);
            break;
        }
        u_long blocking = 0;
        if (ioctlsocket(accepted, FIONBIO, &blocking) == SOCKET_ERROR)
        {
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
            SetError(static_cast<int>(session->GetLastError()));
            delete session;
            continue;
        }
        EnterCriticalSection(&stateLock_);
        try
        {
            clients_.push_back(session);
            ++acceptedCount_;
        }
        catch (const std::bad_alloc&)
        {
            lastError_ = WSAENOBUFS;
            LeaveCriticalSection(&stateLock_);
            delete session;
            break;
        }
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
    std::vector<ClientSession*> clients;
    clients.swap(clients_);
    LeaveCriticalSection(&stateLock_);
    if (listener != INVALID_SOCKET) closesocket(listener);
    // ClientSession's destructor shuts down its socket and waits for its worker.
    for (ClientSession* client : clients) delete client;
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
    const std::size_t count = clients_.size();
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
