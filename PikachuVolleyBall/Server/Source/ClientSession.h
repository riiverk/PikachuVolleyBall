#pragma once
#include "Common.h"

class Room;
class RoomManager;

class ClientSession
{
public:
    static DWORD WINAPI ClientThread(LPVOID arg);
    bool RecvPacket(char* buf);
    void ProcessPacket(char* buf);
    bool SendPacket(const char* buf, int len);
    void Disconnect();
private:
    SOCKET socket_ = INVALID_SOCKET;
    HANDLE thread_ = nullptr;
    Room* room_ = nullptr;
    int playerIndex_ = -1;
    char nickname_[MAX_NICK]{};
};
