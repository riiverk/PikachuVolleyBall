#pragma once
#include "Common.h"
#include "RoomManager.h"

class Server
{
public:
    ~Server();
    bool InitServer(u_short port);
    void AcceptLoop();
    void CloseServer();
    u_short GetPort() const { return port_; }
    int GetLastError() const { return lastError_; }
private:
    SOCKET listenSocket_ = INVALID_SOCKET;
    bool winsockStarted_ = false;
    u_short port_ = 0;
    int lastError_ = 0;
    RoomManager rooms_;
};
