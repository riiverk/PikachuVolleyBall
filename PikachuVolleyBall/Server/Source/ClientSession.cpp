#include "ClientSession.h"
#include <cstring>

ClientSession::ClientSession(SOCKET socket) : socket_(socket)
{
    InitializeCriticalSection(&stateLock_);
}

ClientSession::~ClientSession()
{
    Stop();
    DeleteCriticalSection(&stateLock_);
}

bool ClientSession::Start()
{
    const SOCKET socket = GetSocket();
    if (socket == INVALID_SOCKET || thread_ != nullptr)
    {
        SetError(socket == INVALID_SOCKET ? WSAENOTSOCK : ERROR_ALREADY_EXISTS);
        return false;
    }
    EnterCriticalSection(&stateLock_);
    stopping_ = false;
    receivedCount_ = 0;
    lastError_ = 0;
    LeaveCriticalSection(&stateLock_);
    thread_ = CreateThread(nullptr, 0, ClientThread, this, 0, &threadId_);
    if (thread_ == nullptr)
    {
        SetError(::GetLastError());
        return false;
    }
    return true;
}

void ClientSession::Stop()
{
    EnterCriticalSection(&stateLock_);
    stopping_ = true;
    LeaveCriticalSection(&stateLock_);
    if (thread_ != nullptr)
    {
        if (GetCurrentThreadId() != threadId_) WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        thread_ = nullptr;
        threadId_ = 0;
    }
    CloseSocket();
}

DWORD WINAPI ClientSession::ClientThread(LPVOID arg)
{
    ClientSession* session = static_cast<ClientSession*>(arg);
    if (session == nullptr) return ERROR_INVALID_PARAMETER;
    char buffer[MAX_PACKET_SIZE]{};
    while (!session->IsStopping() && session->RecvPacket(buffer))
    {
        session->ProcessPacket(buffer);
        EnterCriticalSection(&session->stateLock_);
        ++session->receivedCount_;
        LeaveCriticalSection(&session->stateLock_);
    }
    session->Disconnect(); // Room notification is added with the leave handling.
    session->CloseSocket();
    return session->GetLastError();
}

bool ClientSession::RecvPacket(char* buf)
{
    if (buf == nullptr)
    {
        SetError(WSAEINVAL);
        return false;
    }
    const SOCKET socket = GetSocket();
    if (socket == INVALID_SOCKET)
    {
        SetError(WSAENOTSOCK);
        return false;
    }
    PacketHeader header{};
    const int count = ReceiveBytes(socket, reinterpret_cast<char*>(&header), sizeof(header));
    if (count != sizeof(header))
    {
        const int error = count == SOCKET_ERROR ? WSAGetLastError() : count == 0 ? 0 : WSAECONNRESET;
        SetError(IsStopping() ? 0 : error);
        return false;
    }
    if (header.size < sizeof(header) || header.size > MAX_PACKET_SIZE)
    {
        SetError(WSAEMSGSIZE);
        return false;
    }
    if (PacketSize(header.type) != header.size)
    {
        SetError(WSAEINVAL);
        return false;
    }
    std::memcpy(buf, &header, sizeof(header));
    const int remaining = header.size - sizeof(header);
    const int received = ReceiveBytes(socket, buf + sizeof(header), remaining);
    if (received != remaining)
    {
        const int error = received == SOCKET_ERROR ? WSAGetLastError() : WSAECONNRESET;
        SetError(IsStopping() ? 0 : error);
        return false;
    }
    SetError(0);
    return true;
}

DWORD ClientSession::GetLastError() const
{
    EnterCriticalSection(&stateLock_);
    const DWORD error = lastError_;
    LeaveCriticalSection(&stateLock_);
    return error;
}

bool ClientSession::WaitForFinish(DWORD milliseconds) const
{
    return thread_ != nullptr && WaitForSingleObject(thread_, milliseconds) == WAIT_OBJECT_0;
}

std::size_t ClientSession::GetReceivedCount() const
{
    EnterCriticalSection(&stateLock_);
    const std::size_t count = receivedCount_;
    LeaveCriticalSection(&stateLock_);
    return count;
}

SOCKET ClientSession::GetSocket() const
{
    EnterCriticalSection(&stateLock_);
    const SOCKET socket = socket_;
    LeaveCriticalSection(&stateLock_);
    return socket;
}

int ClientSession::ReceiveBytes(SOCKET socket, char* buffer, int length)
{
    int received = 0;
    while (received < length)
    {
        if (IsStopping())
        {
            WSASetLastError(WSAESHUTDOWN);
            return SOCKET_ERROR;
        }
        // Keep ownership in the receive thread. A short select wait permits
        // cancellation even while only part of a TCP packet has arrived.
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(socket, &readable);
        timeval timeout{0, 100000};
        const int ready = select(0, &readable, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR) return SOCKET_ERROR;
        if (ready == 0) continue;
        const int count = recv(socket, buffer + received, length - received, 0);
        if (count == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEINTR) continue;
            return SOCKET_ERROR;
        }
        if (count == 0) return received;
        received += count;
    }
    return received;
}

bool ClientSession::IsStopping() const
{
    EnterCriticalSection(&stateLock_);
    const bool stopping = stopping_;
    LeaveCriticalSection(&stateLock_);
    return stopping;
}

void ClientSession::SetError(DWORD error)
{
    EnterCriticalSection(&stateLock_);
    lastError_ = error;
    LeaveCriticalSection(&stateLock_);
}

void ClientSession::CloseSocket()
{
    EnterCriticalSection(&stateLock_);
    if (socket_ != INVALID_SOCKET)
    {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    LeaveCriticalSection(&stateLock_);
}

void ClientSession::ProcessPacket(char*) { /* TODO 11/8 */ }
bool ClientSession::SendPacket(const char*, int) { return false; /* TODO 11/6 */ }
void ClientSession::Disconnect() { /* TODO 11/7 */ }
