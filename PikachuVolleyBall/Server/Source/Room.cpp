#include "Room.h"

#include "ClientSession.h"

Room::Room() { InitializeCriticalSection(&csInput_); }
Room::~Room()
{
    InterlockedExchange(&quitFlag_, 1);
    if (thread_ != nullptr)
    {
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
    }
    DeleteCriticalSection(&csInput_);
}

int Room::AddPlayer(ClientSession* s)
{
    if (s == nullptr) return -1;
    for (int i = 0; i < 2; ++i)
    {
        if (players_[i] != nullptr) continue;
        players_[i] = s;
        return i;
    }
    return -1;
}

ClientSession* Room::GetPlayer(int idx) const
{
    if (idx < 0 || idx >= 2) return nullptr;
    return players_[idx];
}

void Room::Start()
{
    if (thread_ != nullptr || players_[0] == nullptr || players_[1] == nullptr) return;

    EnterCriticalSection(&csInput_);
    for (int i = 0; i < 2; ++i)
    {
        inputs_[i] = 0;
        nextStageReady_[i] = false;
    }
    LeaveCriticalSection(&csInput_);

    world_.Reset(1, 0);

    thread_ = CreateThread(nullptr, 0, RoomThread, this, 0, nullptr);
    if (thread_ == nullptr)
    {
        std::printf("[room] CreateThread failed: %lu\n", GetLastError());
        return;
    }
  
    std::printf("[room] thread started\n");
}
void Room::ConfirmJoin(int idx)
{
    if (idx < 0 || idx >= GameConst::PlayerCount) return;
    EnterCriticalSection(&csInput_);
    const bool ready = !joinReplySent_[idx] && joinReplySent_[1 - idx];
    joinReplySent_[idx] = true;
    LeaveCriticalSection(&csInput_);
    if (!ready) return;

    // Seat zero already received Waiting. Both join replies precede GAME_START.
    JoinResultPacket matched;
    matched.result = JoinResult::Matched;
    matched.myIndex = 0;
    if (players_[0]->SendPacket(reinterpret_cast<const char*>(&matched), sizeof(matched)))
        Start();
}

DWORD WINAPI Room::RoomThread(LPVOID arg)
{
    Room* room = static_cast<Room*>(arg);
    if (room == nullptr) return 1;

    std::uint8_t inputs[2]{};
    int serve = 0;

    for (int stage = 1; stage <= GameConst::StageCount; ++stage)
    {
        if (InterlockedCompareExchange(&room->quitFlag_, 0, 0))
        {
            room->EndByQuit();
            return 0;
        }
        room->stage_ = stage;
        room->world_.Reset(stage, serve);
        room->BroadcastGameStart(stage, serve);
        room->nextTick_ = GetTickCount64();

        while (InterlockedCompareExchange(&room->quitFlag_, 0, 0) == 0 && !room->world_.IsStageOver())
        {
            room->CopyInputs(inputs);
            room->world_.Update(inputs);
            room->BroadcastState();
            room->BroadcastEvents();
            room->SleepUntilNextTick();
        }
        if (InterlockedCompareExchange(&room->quitFlag_, 0, 0))
        {
            room->EndByQuit();
            return 0;
        }

        const int winner = 1 - room->world_.GetLoser();
        room->BroadcastStageOver();

        if (stage == GameConst::StageCount)
        {
            room->BroadcastGameOver(static_cast<std::uint8_t>(GameOverReason::Normal));
            break;
        }

        serve = winner;
        if (!room->WaitNextStage())
        {
            room->EndByQuit();
            return 0;
        }
    }

 
    return 0;
}
void Room::SetInput(int idx, std::uint8_t keys)
{
	EnterCriticalSection(&csInput_);
    inputs_[idx] = keys;
    LeaveCriticalSection(&csInput_);
}
void Room::CopyInputs(std::uint8_t out[2])
{ 
	EnterCriticalSection(&csInput_);
	out[0] = inputs_[0];
	out[1] = inputs_[1];
	LeaveCriticalSection(&csInput_);
}
void Room::SleepUntilNextTick() 
{
    nextTick_ += GameConst::TickMs;
	const std::uint64_t now = GetTickCount64();
    if (now < nextTick_)
        Sleep(nextTick_ - now);
    else if (now - nextTick_ > GameConst::TickMs * 5)   
        nextTick_ = now;      
}
void Room::Broadcast(const char* buf, int len)
{
    if (buf == nullptr || len <= 0) return;
    for (ClientSession* player : players_)
    {
        if (player != nullptr) player->SendPacket(buf, len);
    }
}

void Room::BroadcastGameStart(int stage, int serve)
{
    if (stage < 1 || stage > GameConst::StageCount || serve < 0 || serve >= GameConst::PlayerCount) return;
    if (players_[0] == nullptr || players_[1] == nullptr) return;

    GameStartPacket packet;
    packet.stage = static_cast<std::uint8_t>(stage);
    packet.serveSide = static_cast<std::uint8_t>(serve);
    for (int i = 0; i < GameConst::PlayerCount; ++i)
        players_[i]->CopyNickname(packet.nick[i]);
    Broadcast(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void Room::BroadcastState()
{
    GameStatePacket packet;
    world_.FillState(packet);
    Broadcast(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void Room::BroadcastEvents() { /* TODO 11/19 */ }
void Room::BroadcastStageOver() { /* TODO 11/20 */ }
void Room::BroadcastGameOver(std::uint8_t) { /* TODO 11/21 */ }
void Room::OnNextStage(int) { /* TODO 11/23 */ }
bool Room::WaitNextStage() { return false; /* TODO 11/22 */ }
void Room::OnPlayerLeave(int idx)
{
    if (idx < 0 || idx >= GameConst::PlayerCount) return;

    EnterCriticalSection(&csInput_);
    left[idx] = true;
    LeaveCriticalSection(&csInput_);

    InterlockedExchange(&quitFlag_, 1);
    std::printf("[room] Player %d left\n", idx);
}
void Room::EndByQuit() { /* TODO 11/24 */ } // left가 모두 false면 아무것도 하지 않도록 할 것
void Room::OnChat(int, const char*) { /* TODO 11/24 */ }
