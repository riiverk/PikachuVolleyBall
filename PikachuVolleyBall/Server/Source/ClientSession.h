#pragma once
#include "Common.h"

class Room;
class RoomManager;

class ClientSession
{
public:
    explicit ClientSession(SOCKET socket = INVALID_SOCKET) : socket_(socket) {}
    ~ClientSession();
    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    bool Start();
    void Stop();
    DWORD GetLastError() const { return lastError_; }
    static DWORD WINAPI ClientThread(LPVOID arg);
    bool RecvPacket(char* buf);
    void ProcessPacket(char* buf);
    bool SendPacket(const char* buf, int len);
    void Disconnect();
private:
    SOCKET socket_ = INVALID_SOCKET;
    HANDLE thread_ = nullptr;
    DWORD threadId_ = 0;
    DWORD lastError_ = 0;
    Room* room_ = nullptr;
    int playerIndex_ = -1;
    char nickname_[MAX_NICK]{};
};
