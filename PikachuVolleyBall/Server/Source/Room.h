#pragma once
#include "Common.h"
#include "GameWorld.h"
#include <array>

class ClientSession;

class Room
{
public:
    Room();
    ~Room();
    Room(const Room&) = delete;
    Room& operator=(const Room&) = delete;
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
    void EndByQuit();
    void OnChat(int idx, const char* msg);
private:
    GameWorld world_;
    std::array<ClientSession*, 2> players_{};
    std::uint8_t inputs_[2]{};
    bool nextStageReady_[2]{};
    CRITICAL_SECTION csInput_;
    volatile LONG quitFlag_ = 0;
    HANDLE thread_ = nullptr;
};
