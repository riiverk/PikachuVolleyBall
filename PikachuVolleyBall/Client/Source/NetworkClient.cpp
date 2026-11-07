#include "NetworkClient.h"
#include <cstring>
#include <new>

NetworkClient::NetworkClient()
{
    InitializeCriticalSection(&stateLock_);
    InitializeCriticalSection(&sendLock_);
}

NetworkClient::~NetworkClient()
{
    Disconnect();
    DeleteCriticalSection(&sendLock_);
    DeleteCriticalSection(&stateLock_);
}

void NetworkClient::SetWindow(HWND window)
{
    EnterCriticalSection(&stateLock_);
    window_ = window;
    LeaveCriticalSection(&stateLock_);
}

bool NetworkClient::Connect(const char* ip, u_short port)
{
    if (IsConnected())
    {
        SetError(WSAEISCONN);
        return false;
    }
    // Reap the previous receive thread before a new connection can reuse its socket.
    Disconnect();
    if (ip == nullptr || *ip == '\0' || port == 0)
    {
        SetError(WSAEINVAL);
        return false;
    }
    WSADATA data{};
    const int startupError = WSAStartup(MAKEWORD(2, 2), &data);
    SetError(startupError);
    if (startupError != 0) return false;
    winsockStarted_ = true;

    auto fail = [this](int error) {
        Disconnect();
        SetError(error);
        return false;
    };
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    const int parsed = InetPtonA(AF_INET, ip, &address.sin_addr);
    if (parsed != 1) return fail(parsed == 0 ? WSAEINVAL : WSAGetLastError());

    const SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == INVALID_SOCKET) return fail(WSAGetLastError());
    if (connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        const int error = WSAGetLastError();
        closesocket(socket);
        return fail(error);
    }

    EnterCriticalSection(&stateLock_);
    socket_ = socket;
    stopping_ = false;
    latestState_ = GameStatePacket{};
    receivedCount_ = 0;
    lastError_ = 0;
    LeaveCriticalSection(&stateLock_);
    recvThread_ = CreateThread(nullptr, 0, RecvThread, this, 0, &recvThreadId_);
    if (recvThread_ == nullptr) return fail(static_cast<int>(::GetLastError()));
    return true;
}
void NetworkClient::SendJoinRoom(int roomNo, const char* nick)
{
    if (!IsConnected())
    {
        SetError(WSAENOTCONN);
        return;
    }
    if (roomNo <= 0 || nick == nullptr)
    {
        SetError(WSAEINVAL);
        return;
    }
    int length = 0;
    while (length < MAX_NICK && nick[length] != '\0') ++length;
    if (length == 0 || length == MAX_NICK ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, nick, length, nullptr, 0) == 0)
    {
        SetError(WSAEINVAL);
        return;
    }
    JoinRoomPacket packet;
    packet.roomNo = roomNo;
    std::memcpy(packet.nickname, nick, length);
    if (SendAll(reinterpret_cast<const char*>(&packet), sizeof(packet))) SetError(0);
}

bool NetworkClient::SendAll(const char* data, int length)
{
    EnterCriticalSection(&sendLock_);
    int sent = 0;
    int error = 0;
    while (sent < length)
    {
        const SOCKET socket = GetSocket();
        if (socket == INVALID_SOCKET || IsStopping())
        {
            error = WSAENOTCONN;
            break;
        }
        const int count = send(socket, data + sent, length - sent, 0);
        if (count == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) continue;
        if (count <= 0)
        {
            error = count == SOCKET_ERROR ? WSAGetLastError() : WSAECONNRESET;
            EnterCriticalSection(&stateLock_);
            stopping_ = true;
            lastError_ = error;
            LeaveCriticalSection(&stateLock_);
            break;
        }
        sent += count;
    }
    LeaveCriticalSection(&sendLock_);
    if (error != 0)
    {
        SetError(error);
        CloseSocket();
        return false;
    }
    return true;
}

DWORD WINAPI NetworkClient::RecvThread(LPVOID arg)
{
    NetworkClient* client = static_cast<NetworkClient*>(arg);
    if (client == nullptr) return ERROR_INVALID_PARAMETER;
    char buffer[MAX_PACKET_SIZE]{};
    while (!client->IsStopping() && client->RecvPacket(buffer))
    {
        client->ProcessPacket(buffer);
        EnterCriticalSection(&client->stateLock_);
        ++client->receivedCount_;
        LeaveCriticalSection(&client->stateLock_);
    }
    // The owner joins the thread and balances WSAStartup in Disconnect.
    client->CloseSocket();
    return static_cast<DWORD>(client->GetLastError());
}

bool NetworkClient::RecvPacket(char* buf)
{
    const SOCKET socket = GetSocket();
    if (socket == INVALID_SOCKET) return false;
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

bool NetworkClient::ReceiveExact(SOCKET socket, char* buffer, int length, bool atPacketStart)
{
    const int count = recvn(socket, buffer, length, 0);
    if (count == length) return true;
    const int error = count == SOCKET_ERROR ? WSAGetLastError() : (count == 0 && atPacketStart ? 0 : WSAECONNRESET);
    if (!IsStopping()) SetError(error);
    return false;
}
void NetworkClient::ProcessPacket(char* buf)
{
    if (buf == nullptr) return;
    PacketHeader header{};
    std::memcpy(&header, buf, sizeof(header));
    if (header.size == 0 || PacketSize(header.type) != header.size) return;
    switch (header.type)
    {
    case PacketType::GAME_STATE:
        // The latest state is polled by the timer, so it is stored instead of posted.
        EnterCriticalSection(&stateLock_);
        std::memcpy(&latestState_, buf, sizeof(latestState_));
        LeaveCriticalSection(&stateLock_);
        return;
    case PacketType::JOIN_RESULT:
    case PacketType::GAME_START:
    case PacketType::EVENT:
    case PacketType::SCORE:
    case PacketType::STAGE_OVER:
    case PacketType::GAME_OVER:
    case PacketType::CHAT:
        break;
    default:
        return; // Client-to-server packet types are never valid from the server.
    }
    EnterCriticalSection(&stateLock_);
    const HWND window = window_;
    LeaveCriticalSection(&stateLock_);
    if (window == nullptr) return;
    char* copy = new (std::nothrow) char[header.size];
    if (copy == nullptr) return;
    std::memcpy(copy, buf, header.size);
    if (!PostMessage(window, WM_NET_EVENT, static_cast<WPARAM>(header.type), reinterpret_cast<LPARAM>(copy)))
        delete[] copy;
}
void NetworkClient::SendInput(std::uint8_t keys)
{
    if (!IsConnected())
    {
        SetError(WSAENOTCONN);
        return;
    }
    constexpr std::uint8_t validKeys = LEFT | RIGHT | UP | ACTION;
    if ((keys & ~validKeys) != 0)
    {
        SetError(WSAEINVAL);
        return;
    }
    InputPacket packet;
    packet.keys = keys;
    if (SendAll(reinterpret_cast<const char*>(&packet), sizeof(packet))) SetError(0);
}
void NetworkClient::SendChat(const char*) { /* TODO 11/23 */ }
void NetworkClient::SendNextStage() { /* TODO 11/26 */ }
void NetworkClient::GetLatestState(GameStatePacket& out)
{
    EnterCriticalSection(&stateLock_);
    out = latestState_;
    LeaveCriticalSection(&stateLock_);
}
void NetworkClient::Disconnect()
{
    EnterCriticalSection(&stateLock_);
    stopping_ = true;
    LeaveCriticalSection(&stateLock_);
    CloseSocket();
    if (recvThread_ != nullptr)
    {
        if (GetCurrentThreadId() != recvThreadId_) WaitForSingleObject(recvThread_, INFINITE);
        CloseHandle(recvThread_);
        recvThread_ = nullptr;
        recvThreadId_ = 0;
    }
    EnterCriticalSection(&sendLock_);
    LeaveCriticalSection(&sendLock_);
    if (winsockStarted_)
    {
        WSACleanup();
        winsockStarted_ = false;
    }
    // LEAVE is sent when packet transmission and graceful exit are implemented.
}

SOCKET NetworkClient::GetSocket() const
{
    EnterCriticalSection(&stateLock_);
    const SOCKET socket = socket_;
    LeaveCriticalSection(&stateLock_);
    return socket;
}

bool NetworkClient::IsConnected() const { return GetSocket() != INVALID_SOCKET; }

bool NetworkClient::IsStopping() const
{
    EnterCriticalSection(&stateLock_);
    const bool stopping = stopping_;
    LeaveCriticalSection(&stateLock_);
    return stopping;
}

int NetworkClient::GetLastError() const
{
    EnterCriticalSection(&stateLock_);
    const int error = lastError_;
    LeaveCriticalSection(&stateLock_);
    return error;
}

void NetworkClient::SetError(int error)
{
    EnterCriticalSection(&stateLock_);
    if (error != 0 || !stopping_) lastError_ = error;
    LeaveCriticalSection(&stateLock_);
}

std::size_t NetworkClient::GetReceivedCount() const
{
    EnterCriticalSection(&stateLock_);
    const std::size_t count = receivedCount_;
    LeaveCriticalSection(&stateLock_);
    return count;
}

bool NetworkClient::WaitForReceiveEnd(DWORD milliseconds) const
{
    return recvThread_ != nullptr && WaitForSingleObject(recvThread_, milliseconds) == WAIT_OBJECT_0;
}

void NetworkClient::CloseSocket()
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
