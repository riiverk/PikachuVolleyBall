#include "NetworkClient.h"
#include <cstring>

NetworkClient::~NetworkClient() { Disconnect(); }

bool NetworkClient::Connect(const char* ip, u_short port)
{
    if (IsConnected())
    {
        lastError_ = WSAEISCONN;
        return false;
    }
    if (ip == nullptr || *ip == '\0' || port == 0)
    {
        lastError_ = WSAEINVAL;
        return false;
    }
    WSADATA data{};
    lastError_ = WSAStartup(MAKEWORD(2, 2), &data);
    if (lastError_ != 0) return false;
    winsockStarted_ = true;

    auto fail = [this](int error) {
        Disconnect();
        lastError_ = error;
        return false;
    };
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    const int parsed = InetPtonA(AF_INET, ip, &address.sin_addr);
    if (parsed != 1) return fail(parsed == 0 ? WSAEINVAL : WSAGetLastError());

    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET) return fail(WSAGetLastError());
    if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
        return fail(WSAGetLastError());

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        latestState_ = GameStatePacket{};
    }
    lastError_ = 0;
    // RecvThread starts with the receive loop implementation (11/6).
    // Starting the current placeholder would immediately terminate the thread.
    return true;
}
void NetworkClient::SendJoinRoom(int roomNo, const char* nick)
{
    if (!IsConnected())
    {
        lastError_ = WSAENOTCONN;
        return;
    }
    if (roomNo <= 0 || nick == nullptr)
    {
        lastError_ = WSAEINVAL;
        return;
    }
    int length = 0;
    while (length < MAX_NICK && nick[length] != '\0') ++length;
    if (length == 0 || length == MAX_NICK ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, nick, length, nullptr, 0) == 0)
    {
        lastError_ = WSAEINVAL;
        return;
    }
    JoinRoomPacket packet;
    packet.roomNo = roomNo;
    std::memcpy(packet.nickname, nick, length);
    const char* bytes = reinterpret_cast<const char*>(&packet);
    int sent = 0;
    while (sent < sizeof(packet))
    {
        const int count = send(socket_, bytes + sent, static_cast<int>(sizeof(packet)) - sent, 0);
        if (count <= 0)
        {
            const int error = count == SOCKET_ERROR ? WSAGetLastError() : WSAECONNRESET;
            Disconnect();
            lastError_ = error;
            return;
        }
        sent += count;
    }
    lastError_ = 0;
}
DWORD WINAPI NetworkClient::RecvThread(LPVOID) { return ERROR_CALL_NOT_IMPLEMENTED; }
void NetworkClient::ProcessPacket(char*) { /* TODO 11/7 */ }
void NetworkClient::SendInput(std::uint8_t) { /* TODO 11/5 */ }
void NetworkClient::SendChat(const char*) { /* TODO 11/23 */ }
void NetworkClient::SendNextStage() { /* TODO 11/26 */ }
void NetworkClient::GetLatestState(GameStatePacket& out) { out = latestState_; /* TODO 11/18 */ }
void NetworkClient::Disconnect()
{
    if (socket_ != INVALID_SOCKET)
    {
        shutdown(socket_, SD_BOTH);
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    if (recvThread_ != nullptr)
    {
        WaitForSingleObject(recvThread_, INFINITE);
        CloseHandle(recvThread_);
        recvThread_ = nullptr;
    }
    if (winsockStarted_)
    {
        WSACleanup();
        winsockStarted_ = false;
    }
    // LEAVE is sent when packet transmission and graceful exit are implemented.
}
