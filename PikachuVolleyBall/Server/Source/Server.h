#pragma once
#include "Common.h"
#include "RoomManager.h"

class Server
{
public:
    bool InitServer(u_short port);
    void AcceptLoop();
    void CloseServer();
private:
    SOCKET listenSocket_ = INVALID_SOCKET;
    bool winsockStarted_ = false;
    RoomManager rooms_;
};
