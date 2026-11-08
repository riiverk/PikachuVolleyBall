#pragma once
#include "Common.h"

class Room;
class RoomManager;

class ClientSession
{
public:
    explicit ClientSession(SOCKET socket = INVALID_SOCKET);
    ~ClientSession();
    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    bool Start();
    void Stop();
    DWORD GetLastError() const;
    bool WaitForFinish(DWORD milliseconds) const;
    std::size_t GetReceivedCount() const;
    static DWORD WINAPI ClientThread(LPVOID arg);
    bool RecvPacket(char* buf);
    void ProcessPacket(char* buf);
    bool SendPacket(const char* buf, int len);
    void Disconnect();
private:
    SOCKET GetSocket() const;
    bool ReceiveExact(SOCKET socket, char* buffer, int length, bool atPacketStart);
    bool IsStopping() const;
    void SetError(DWORD error);
    void CloseSocket();
    SOCKET socket_ = INVALID_SOCKET;
    HANDLE thread_ = nullptr;
    DWORD threadId_ = 0;
    DWORD lastError_ = 0;
    mutable CRITICAL_SECTION stateLock_;
    CRITICAL_SECTION csSend_;
    volatile LONG disconnected_ = 0;
    bool stopping_ = false;
    std::size_t receivedCount_ = 0;
    Room* room_ = nullptr;
    int playerIndex_ = -1;
    char nickname_[MAX_NICK]{};
};
