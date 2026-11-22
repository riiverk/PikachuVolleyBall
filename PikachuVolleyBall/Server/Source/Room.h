#pragma once
#include "Common.h"
#include "GameWorld.h"

class ClientSession;

class Room
{
public:
    Room(int roomNo = 0);
    ~Room();
    Room(const Room&) = delete;
    Room& operator=(const Room&) = delete;
    int AddPlayer(ClientSession* s);
    ClientSession* GetPlayer(int idx) const;
    void ConfirmJoin(int idx);
    void Start();
    static DWORD WINAPI RoomThread(LPVOID arg);
    void SetInput(int idx, std::uint8_t keys);
    void CopyInputs(std::uint8_t out[2]);
    void SleepUntilNextTick();
    void Broadcast(const char* buf, int len);
    void BroadcastGameStart(int stage, int serve);
    void BroadcastState();
    void BroadcastEvents();
    void BroadcastStageOver();
    void BroadcastGameOver(std::uint8_t reason);
    void OnNextStage(int idx);
    bool WaitNextStage();
    void OnPlayerLeave(int idx);
    bool AllLeft();
    bool IsRoomThread();
    void EndByQuit();
    void OnChat(int idx, const char* msg);
private:
    friend struct RoomTestAccess;
    GameWorld world_;
    ClientSession* players_[2]{};
    bool joinReplySent_[2]{};
    std::uint8_t inputs_[2]{};
    bool nextStageReady_[2]{};
    CRITICAL_SECTION csInput_;
    volatile LONG quitFlag_ = 0;
    HANDLE thread_ = nullptr;
    DWORD threadId_ = 0;
    std::uint64_t nextTick_ = 0;
    int stage_ = 1;
    bool left[2]{};
    int roomNo_ = 0;
};
