#pragma once
#include "Common.h"

// WPARAM is the PacketType. LPARAM is a new[] copy of the whole packet; the window must delete[] it.
inline constexpr UINT WM_NET_EVENT = WM_APP + 1;

class NetworkClient
{
public:
    NetworkClient();
    ~NetworkClient();
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    void SetWindow(HWND window);
    bool Connect(const char* ip, u_short port);
    bool SendJoinRoom(int roomNo, const char* nick);
    static DWORD WINAPI RecvThread(LPVOID arg);
    void ProcessPacket(char* buf);
    void SendInput(std::uint8_t keys);
    bool SendChat(const char* msg);
    bool SendNextStage();
    // False until this connection has received its first GAME_STATE.
    bool GetLatestState(GameStatePacket& out);
    // Best-effort LEAVE, then stop/join reception and release Winsock. Owner-thread only.
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
    bool CanSend() const;
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
    bool hasLatestState_ = false;
    mutable CRITICAL_SECTION stateLock_;
    CRITICAL_SECTION sendLock_;
    bool stopping_ = true;
    bool closing_ = true;
    std::size_t receivedCount_ = 0;
};
