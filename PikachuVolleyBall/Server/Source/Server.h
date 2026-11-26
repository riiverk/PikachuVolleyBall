#pragma once
#include "Common.h"
#include "RoomManager.h"
#include "ClientSession.h"

class Server
{
public:
    // Lecture ch11 SelectTCPServer keeps client information in a fixed array of FD_SETSIZE.
    static constexpr int MAX_CLIENTS = FD_SETSIZE;

    Server();
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    bool InitServer(u_short port);
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
    bool HasClientSlot();
    void RemoveFinishedClients();

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
    // Sessions are added by the accept loop and deleted by CloseServer or RemoveFinishedClients.
    ClientSession* clients_[MAX_CLIENTS]{};
    int clientCount_ = 0;
    RoomManager rooms_;
};
