#include "ClientSession.h"
#include <cstdio>
#include <cstring>

ClientSession::ClientSession(SOCKET socket) : socket_(socket)
{
    InitializeCriticalSection(&stateLock_);
    InitializeCriticalSection(&csSend_);
}

ClientSession::~ClientSession()
{
    Stop();
    DeleteCriticalSection(&csSend_);
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
    CloseSocket();
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
        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
        std::printf("[session %lu] recv type=%u size=%u\n", GetCurrentThreadId(),
            static_cast<unsigned>(header->type), static_cast<unsigned>(header->size));
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
        SetError(IsStopping() ? 0 : WSAENOTSOCK);
        return false;
    }
    PacketHeader header{};
    if (!ReceiveExact(socket, reinterpret_cast<char*>(&header), sizeof(header), true)) return false;
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
    const int remaining = header.size - static_cast<int>(sizeof(header));
    if (!ReceiveExact(socket, buf + sizeof(header), remaining, false)) return false;
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

bool ClientSession::ReceiveExact(SOCKET socket, char* buffer, int length, bool atPacketStart)
{
    const int count = recvn(socket, buffer, length, 0);
    if (count == length) return true;
    int error = WSAECONNRESET;
    if (count == SOCKET_ERROR) error = WSAGetLastError();
    else if (count == 0 && atPacketStart) error = 0;
    SetError(IsStopping() ? 0 : error);
    return false;
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
        shutdown(socket_, SD_BOTH);
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    LeaveCriticalSection(&stateLock_);
}

void ClientSession::ProcessPacket(char*) { /* TODO 11/8 */ }

bool ClientSession::SendPacket(const char* buf, int len)
{
    if (buf == nullptr || len <= 0) return false;
    EnterCriticalSection(&csSend_);
    const SOCKET socket = GetSocket();
    int sent = 0;
    while (socket != INVALID_SOCKET && sent < len)
    {
        const int count = send(socket, buf + sent, len - sent, 0);
        if (count == SOCKET_ERROR) break;
        sent += count;
    }
    LeaveCriticalSection(&csSend_);
    return sent == len;
}

void ClientSession::Disconnect() { /* TODO 11/7 */ }
