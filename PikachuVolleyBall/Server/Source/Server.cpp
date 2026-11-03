#include "Server.h"
#include <new>

Server::~Server() { CloseServer(); }

bool Server::InitServer(u_short port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (listenSocket_ != INVALID_SOCKET || closing_ || accepting_)
    {
        lastError_ = WSAEALREADY;
        return false;
    }
    WSADATA data{};
    const int startupError = WSAStartup(MAKEWORD(2, 2), &data);
    lastError_ = startupError;
    if (startupError != 0) return false;

    SOCKET listener = INVALID_SOCKET;
    auto fail = [this, &listener]() {
        const int error = WSAGetLastError();
        if (listener != INVALID_SOCKET) closesocket(listener);
        WSACleanup();
        lastError_ = error;
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
    return true;
}

void Server::AcceptLoop()
{
    SOCKET listener;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closing_ || stopping_ || listenSocket_ == INVALID_SOCKET) return;
        if (accepting_)
        {
            lastError_ = WSAEALREADY;
            return;
        }
        accepting_ = true;
        listener = listenSocket_;
    }
    while (!stopping_)
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listener, &readable);
        timeval timeout{0, 100000};
        const int ready = select(0, &readable, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR)
        {
            lastError_ = WSAGetLastError();
            break;
        }
        if (ready == 0 || stopping_) continue;
        SOCKET accepted = accept(listener, nullptr, nullptr);
        if (accepted == INVALID_SOCKET)
        {
            const int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK || error == WSAECONNRESET) continue;
            lastError_ = error;
            break;
        }
        u_long blocking = 0;
        if (ioctlsocket(accepted, FIONBIO, &blocking) == SOCKET_ERROR)
        {
            lastError_ = WSAGetLastError();
            closesocket(accepted);
            continue;
        }
        std::unique_ptr<ClientSession> session(new (std::nothrow) ClientSession(accepted));
        if (!session)
        {
            closesocket(accepted);
            lastError_ = WSAENOBUFS;
            break;
        }
        if (stopping_) break;
        if (!session->Start())
        {
            lastError_ = static_cast<int>(session->GetLastError());
            continue;
        }
        try
        {
            std::lock_guard<std::mutex> lock(mutex_);
            clients_.push_back(std::move(session));
            ++acceptedCount_;
        }
        catch (const std::bad_alloc&)
        {
            lastError_ = WSAENOBUFS;
            break;
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        accepting_ = false;
        stopped_.notify_all();
    }
}

void Server::CloseServer()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (closing_)
    {
        stopped_.wait(lock, [this] { return !closing_; });
        return;
    }
    closing_ = true;
    stopping_ = true;
    stopped_.wait(lock, [this] { return !accepting_; });
    const SOCKET listener = listenSocket_;
    listenSocket_ = INVALID_SOCKET;
    port_ = 0;
    const bool cleanup = winsockStarted_;
    winsockStarted_ = false;
    auto clients = std::move(clients_);
    lock.unlock();
    if (listener != INVALID_SOCKET) closesocket(listener);
    clients.clear(); // Shut down sockets and join workers before WSACleanup.
    if (cleanup) WSACleanup();
    lock.lock();
    closing_ = false;
    stopped_.notify_all();
}

u_short Server::GetPort() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_;
}

std::size_t Server::GetClientCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return clients_.size();
}

bool Server::IsAccepting() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accepting_;
}
