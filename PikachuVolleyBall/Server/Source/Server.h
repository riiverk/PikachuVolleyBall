#pragma once
#include "Common.h"
#include "RoomManager.h"
#include "ClientSession.h"
#include <vector>

class Server
{
public:
    Server();
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    bool InitServer(u_short port);
    void EnableConsoleExit();
    void AcceptLoop();
    void CloseServer();
    u_short GetPort() const;
    int GetLastError() const;
    std::size_t GetClientCount() const;
    bool IsAccepting() const;
    std::size_t GetAcceptedCount() const;
private:
    bool IsStopping() const;
    void SetError(int error);

    SOCKET listenSocket_ = INVALID_SOCKET;
    bool winsockStarted_ = false;
    u_short port_ = 0;
    int lastError_ = 0;
    bool stopping_ = true;
    std::size_t acceptedCount_ = 0;
    mutable CRITICAL_SECTION stateLock_;
    CRITICAL_SECTION closeLock_;
    HANDLE acceptStopped_ = nullptr;
    bool accepting_ = false;
    bool closing_ = false;
    bool consoleExitEnabled_ = false;
    // The server deletes each session after the accept loop has stopped.
    std::vector<ClientSession*> clients_;
    RoomManager rooms_;
};
