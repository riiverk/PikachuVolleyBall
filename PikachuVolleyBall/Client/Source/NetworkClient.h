#pragma once
#include "Common.h"

inline constexpr UINT WM_NET_EVENT = WM_APP + 1;

class NetworkClient
{
public:
    NetworkClient();
    ~NetworkClient();
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    bool Connect(const char* ip, u_short port);
    void SendJoinRoom(int roomNo, const char* nick);
    static DWORD WINAPI RecvThread(LPVOID arg);
    void ProcessPacket(char* buf);
    void SendInput(std::uint8_t keys);
    void SendChat(const char* msg);
    void SendNextStage();
    void GetLatestState(GameStatePacket& out);
    void Disconnect();
    bool IsConnected() const;
    int GetLastError() const;
    std::size_t GetReceivedCount() const;
    bool WaitForReceiveEnd(DWORD milliseconds) const;
private:
    bool SendAll(const char* data, int length);
    bool RecvPacket(char* buf);
    bool ReceiveExact(SOCKET socket, char* buffer, int length, bool atPacketStart);
    SOCKET GetSocket() const;
    bool IsStopping() const;
    void SetError(int error);
    void CloseSocket();
    SOCKET socket_ = INVALID_SOCKET;
    HANDLE recvThread_ = nullptr;
    DWORD recvThreadId_ = 0;
    bool winsockStarted_ = false;
    int lastError_ = 0;
    HWND window_ = nullptr;
    GameStatePacket latestState_{};
    mutable CRITICAL_SECTION stateLock_;
    CRITICAL_SECTION sendLock_;
    bool stopping_ = true;
    std::size_t receivedCount_ = 0;
};
