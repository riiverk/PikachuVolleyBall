#include "ClientSession.h"

ClientSession::~ClientSession() { Stop(); }

bool ClientSession::Start()
{
    if (socket_ == INVALID_SOCKET || thread_ != nullptr)
    {
        lastError_ = socket_ == INVALID_SOCKET ? WSAENOTSOCK : ERROR_ALREADY_EXISTS;
        return false;
    }
    thread_ = CreateThread(nullptr, 0, ClientThread, this, 0, &threadId_);
    if (thread_ == nullptr)
    {
        lastError_ = ::GetLastError();
        return false;
    }
    lastError_ = 0;
    return true;
}

void ClientSession::Stop()
{
    if (socket_ != INVALID_SOCKET) shutdown(socket_, SD_BOTH);
    if (thread_ != nullptr)
    {
        if (GetCurrentThreadId() != threadId_) WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        thread_ = nullptr;
        threadId_ = 0;
    }
    if (socket_ != INVALID_SOCKET)
    {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
}

DWORD WINAPI ClientSession::ClientThread(LPVOID) { return ERROR_CALL_NOT_IMPLEMENTED; }
bool ClientSession::RecvPacket(char*) { return false; /* TODO 11/5 */ }
void ClientSession::ProcessPacket(char*) { /* TODO 11/8 */ }
bool ClientSession::SendPacket(const char*, int) { return false; /* TODO 11/6 */ }
void ClientSession::Disconnect() { /* TODO 11/7 */ }
