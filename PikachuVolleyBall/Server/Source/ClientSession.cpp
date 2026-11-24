#include "ClientSession.h"
#include "Room.h"
#include "RoomManager.h"
#include <cstdio>
#include <cstring>

static const char* StatusToString(JoinResult r)
{
    switch (r)
    {
    case JoinResult::Waiting: return "Waiting";
	case JoinResult::Matched: return "Matched";
	case JoinResult::Full: return "Full";
	case JoinResult::Rejected: return "Rejected";
	default: return "Unknown";
    }
}

static bool IsValidJoin(const JoinRoomPacket& packet)
{
    if (packet.roomNo <= 0) return false;

    int length = 0;
    while (length < MAX_NICK && packet.nickname[length] != '\0') ++length;
    return length > 0 && length < MAX_NICK;
}

static bool IsValidChat(const ChatPacket& packet)
{
    int length = 0;
    while (length < MAX_CHAT && packet.msg[length] != '\0') ++length;
    return length > 0 && length < MAX_CHAT;
}

ClientSession::ClientSession(SOCKET socket, RoomManager* rooms) : socket_(socket), rooms_(rooms)
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
    EnterCriticalSection(&csSend_);
    LeaveCriticalSection(&csSend_);
}

DWORD WINAPI ClientSession::ClientThread(LPVOID arg)
{
    ClientSession* session = static_cast<ClientSession*>(arg);
    if (session == nullptr) return ERROR_INVALID_PARAMETER;
    // Lecture ch06: get the client address from the socket with getpeername().
    sockaddr_in clientaddr{};
    int addrlen = sizeof(clientaddr);
    char addr[INET_ADDRSTRLEN] = "unknown";
    if (getpeername(session->GetSocket(), reinterpret_cast<sockaddr*>(&clientaddr), &addrlen) == 0)
        inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
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
    session->Disconnect();
    std::printf("[TCP server] client closed: IP=%s, port=%d\n", addr, ntohs(clientaddr.sin_port));
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

bool ClientSession::IsInRoom() const
{
    EnterCriticalSection(&stateLock_);
    const bool inRoom = room_ != nullptr;
    LeaveCriticalSection(&stateLock_);
    return inRoom;
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
    if (length == 0) return true;
    // Lecture ch05: read a fixed length with recv(..., MSG_WAITALL).
    // A short count means the peer closed; 0 at a packet boundary is a normal close.
    const int count = recv(socket, buffer, length, MSG_WAITALL);
    if (count == length) return true;
    int error = WSAECONNRESET;
    if (count == SOCKET_ERROR)
    {
        error = WSAGetLastError();
        if (!IsStopping()) err_display("recv()");
    }
    else if (count == 0 && atPacketStart) error = 0;
    if (!IsStopping()) SetError(error);
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
    if (error != 0 || !stopping_) lastError_ = error;
    LeaveCriticalSection(&stateLock_);
}

void ClientSession::CloseSocket()
{
    EnterCriticalSection(&stateLock_);
    if (socket_ != INVALID_SOCKET)
    {
        // Announce the end of the stream before releasing a socket with a pending recv.
        shutdown(socket_, SD_BOTH);
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    LeaveCriticalSection(&stateLock_);
}

void ClientSession::ProcessPacket(char* buf)
{
    if (buf == nullptr) return;
    PacketHeader header;
    std::memcpy(&header, buf, sizeof(header));
    EnterCriticalSection(&stateLock_);
    Room* room = room_;
    const int index = playerIndex_;
    LeaveCriticalSection(&stateLock_);
    switch (header.type)
    {
    case PacketType::JOIN_ROOM:
    {
        if (room != nullptr || rooms_ == nullptr) break;
        JoinRoomPacket packet;
        std::memcpy(&packet, buf, sizeof(packet));
        if (!IsValidJoin(packet))
        {
            JoinResultPacket reply;
            reply.result = JoinResult::Rejected;
            packet.nickname[MAX_NICK - 1] = '\0';
            std::printf("[join] room=%d, name=%s, result=%s (invalid)\n",
                packet.roomNo, packet.nickname, StatusToString(reply.result));
            SendPacket((const char*)&reply, sizeof(reply));
            break;
        }
        int seat = -1;
        std::uint8_t result = 0;
        Room* joined = rooms_->JoinRoom(packet.roomNo, this, seat, result);
        JoinResultPacket reply;
        reply.result = static_cast<JoinResult>(result);
        std::printf("[join] room=%d, name=%s, result=%s\n",
            packet.roomNo, packet.nickname, StatusToString(reply.result));
        if (joined == nullptr)
        {
            SendPacket(reinterpret_cast<const char*>(&reply), sizeof(reply));
            break;
        }
        EnterCriticalSection(&stateLock_);
        room_ = joined;
        playerIndex_ = seat;
        std::memcpy(nickname_, packet.nickname, MAX_NICK);
        LeaveCriticalSection(&stateLock_);
        reply.myIndex = static_cast<std::uint8_t>(seat);
        if (SendPacket(reinterpret_cast<const char*>(&reply), sizeof(reply)))
            joined->ConfirmJoin(seat);
        break;
    }
    case PacketType::INPUT:
    {
        InputPacket packet;
        std::memcpy(&packet, buf, sizeof(packet));
        if (room != nullptr) room->SetInput(index, packet.keys);
        else std::printf("[session %lu] ignore INPUT without room\n", GetCurrentThreadId());
        break;
    }
    case PacketType::NEXT_STAGE:
        if (room != nullptr) room->OnNextStage(index);
        else std::printf("[session %lu] ignore NEXT_STAGE without room\n", GetCurrentThreadId());
        break;
    case PacketType::CHAT:
    {
        ChatPacket packet;
        std::memcpy(&packet, buf, sizeof(packet));
        if (!IsValidChat(packet))
        {
            std::printf("[session %lu] ignore invalid CHAT\n", GetCurrentThreadId());
            break;
        }
        if (room != nullptr) room->OnChat(index, packet.msg);
        else std::printf("[session %lu] ignore CHAT without room\n", GetCurrentThreadId());
        break;
    }
    case PacketType::LEAVE:
        Disconnect();
        break;
    default:
        std::printf("[session %lu] unexpected type=%u, disconnect\n", GetCurrentThreadId(),
            static_cast<unsigned>(header.type));
        Disconnect();
        break;
    }
}
void ClientSession::CopyNickname(char (&out)[MAX_NICK]) const
{
    EnterCriticalSection(&stateLock_);
    std::memcpy(out, nickname_, sizeof(out));
    LeaveCriticalSection(&stateLock_);
}

bool ClientSession::SendPacket(const char* buf, int len)
{
    if (buf == nullptr || len < static_cast<int>(sizeof(PacketHeader)) || len > MAX_PACKET_SIZE)
    {
        SetError(WSAEINVAL);
        return false;
    }
    PacketHeader header;
    std::memcpy(&header, buf, sizeof(header));
    if (header.size != len || PacketSize(header.type) != len)
    {
        SetError(WSAEINVAL);
        return false;
    }
    // A room worker and a session worker may send to the same socket.
    // Hold the send lock across the whole frame so their bytes do not mix.
    EnterCriticalSection(&csSend_);
    int sent = 0;
    int error = 0;
    while (sent < len)
    {
        const SOCKET socket = GetSocket();
        if (IsStopping() || socket == INVALID_SOCKET)
        {
            error = WSAENOTCONN;
            break;
        }
        const int count = send(socket, buf + sent, len - sent, 0);
        if (count <= 0)
        {
            error = count == SOCKET_ERROR ? WSAGetLastError() : WSAECONNRESET;
            if (count == SOCKET_ERROR && !IsStopping()) err_display("send()");
            EnterCriticalSection(&stateLock_);
            stopping_ = true;
            lastError_ = error;
            LeaveCriticalSection(&stateLock_);
            break;
        }
        sent += count;
    }
    LeaveCriticalSection(&csSend_);
    if (error != 0)
    {
        SetError(error);
        if (error != WSAENOTCONN) Disconnect();
        return false;
    }
    SetError(0);
    return true;
}
void ClientSession::Disconnect()
{
    EnterCriticalSection(&stateLock_);
    if (disconnected_)
    {
        LeaveCriticalSection(&stateLock_);
        return;
    }
    disconnected_ = true;
    stopping_ = true;
    Room* room = room_;
    const int index = playerIndex_;
    LeaveCriticalSection(&stateLock_);
    CloseSocket();
    if (room == nullptr) return;
    const bool onRoomThread = room->IsRoomThread();
    room->OnPlayerLeave(index);
    if (onRoomThread || rooms_ == nullptr)return;
    if (rooms_->RemoveIfEmpty(room))
    {
        EnterCriticalSection(&stateLock_);
        room_ = nullptr;
        playerIndex_ = -1;
        LeaveCriticalSection(&stateLock_);
        return;
    }
}
