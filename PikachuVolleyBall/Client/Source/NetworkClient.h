#pragma once
#include "Common.h"
#include <mutex>

inline constexpr UINT WM_NET_EVENT = WM_APP + 1;

class NetworkClient
{
public:
    bool Connect(const char* ip, u_short port);
    void SendJoinRoom(int roomNo, const char* nick);
    static DWORD WINAPI RecvThread(LPVOID arg);
    void ProcessPacket(char* buf);
    void SendInput(std::uint8_t keys);
    void SendChat(const char* msg);
    void SendNextStage();
    void GetLatestState(GameStatePacket& out);
    void Disconnect();
private:
    SOCKET socket_ = INVALID_SOCKET;
    HANDLE recvThread_ = nullptr;
    HWND window_ = nullptr;
    GameStatePacket latestState_{};
    std::mutex stateMutex_;
};
