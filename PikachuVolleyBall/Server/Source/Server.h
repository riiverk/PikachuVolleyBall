#pragma once
#include "Common.h"
#include "RoomManager.h"
#include "ClientSession.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

class Server
{
public:
    ~Server();
    bool InitServer(u_short port);
    void AcceptLoop();
    void CloseServer();
    u_short GetPort() const;
    int GetLastError() const { return lastError_.load(); }
    std::size_t GetClientCount() const;
    bool IsAccepting() const;
    std::size_t GetAcceptedCount() const { return acceptedCount_.load(); }
private:
    SOCKET listenSocket_ = INVALID_SOCKET;
    bool winsockStarted_ = false;
    u_short port_ = 0;
    std::atomic_int lastError_{0};
    std::atomic_bool stopping_{true};
    std::atomic_size_t acceptedCount_{0};
    mutable std::mutex mutex_;
    std::condition_variable stopped_;
    bool accepting_ = false;
    bool closing_ = false;
    std::vector<std::unique_ptr<ClientSession>> clients_;
    RoomManager rooms_;
};
