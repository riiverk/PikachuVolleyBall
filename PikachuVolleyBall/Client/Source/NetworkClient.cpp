#include "NetworkClient.h"

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
void NetworkClient::SendJoinRoom(int, const char*) { /* TODO 11/4 */ }
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
