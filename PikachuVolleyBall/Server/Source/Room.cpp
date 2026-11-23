#include "Room.h"

#include "ClientSession.h"

Room::Room(int roomNo) : roomNo_(roomNo) { InitializeCriticalSection(&csInput_); }
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
  
    std::printf("[room %d] thread started\n", roomNo_);
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

    EnterCriticalSection(&room->csInput_);
    room->threadId_ = GetCurrentThreadId();
    LeaveCriticalSection(&room->csInput_);

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
		std::printf("[room %d] stage %d start, serve = %d\n", room->roomNo_, stage, serve + 1);
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
		std::printf("[room %d] stage %d over, winner = %d\n", room->roomNo_, stage, winner + 1);
        room->BroadcastStageOver();

        if (stage == GameConst::StageCount)
        {
			std::printf("[room %d] game over, winner = %d\n", room->roomNo_, winner + 1);
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

void Room::BroadcastEvents()
{
    for (const GameEvent& event : world_.PopEvents())
    {
        EventPacket packet;
        packet.kind = event.kind;
        packet.who = event.who;
        Broadcast(reinterpret_cast<const char*>(&packet), sizeof(packet));

        if (event.kind == EventKind::Score)
        {
            ScorePacket score;
            score.scorer = event.who;
            score.score[0] = event.score[0];
            score.score[1] = event.score[1];
            Broadcast(reinterpret_cast<const char*>(&score), sizeof(score));
        }
    }
}
void Room::BroadcastStageOver()
{
    const int loser = world_.GetLoser();
    if (loser < 0) return;

    StageOverPacket packet;
    packet.stage = static_cast<std::uint8_t>(stage_);
    packet.winner = static_cast<std::uint8_t>(1 - loser);
    Broadcast(reinterpret_cast<const char*>(&packet), sizeof(packet));
}
void Room::BroadcastGameOver(std::uint8_t reason)
{
    GameOverPacket packet;
    packet.reason = static_cast<GameOverReason>(reason);
    if (packet.reason == GameOverReason::Normal)
    {
        const int loser = world_.GetLoser();
        if (stage_ != GameConst::StageCount || loser < 0) return;
        packet.winner = static_cast<std::uint8_t>(1 - loser);
    }
    else if (packet.reason == GameOverReason::Quit)
    {
        EnterCriticalSection(&csInput_);
        if (left[0] != left[1])
            packet.winner = left[0] ? 1 : 0;
        LeaveCriticalSection(&csInput_);
        if (packet.winner == NO_PLAYER) return;
    }
    else return;

    Broadcast(reinterpret_cast<const char*>(&packet), sizeof(packet));
}
void Room::OnNextStage(int) { /* TODO 11/23 */ }
bool Room::WaitNextStage() { return false; /* TODO 11/22 */ }
void Room::OnPlayerLeave(int idx)
{
    if (idx < 0 || idx >= GameConst::PlayerCount) return;

    std::printf("[room %d] Player %d left\n", roomNo_, idx + 1);
    InterlockedExchange(&quitFlag_, 1);

    EnterCriticalSection(&csInput_);
    left[idx] = true;
    LeaveCriticalSection(&csInput_);
}

bool Room::AllLeft()
{
    bool all = true;
    EnterCriticalSection(&csInput_);
    for (int i = 0; i < GameConst::PlayerCount; ++i)
    {
        if (players_[i] != nullptr && !left[i]) all = false;
    }
    LeaveCriticalSection(&csInput_);
    return all;
}

bool Room::IsRoomThread()
{
    EnterCriticalSection(&csInput_);
    const bool same = (threadId_ == GetCurrentThreadId());
    LeaveCriticalSection(&csInput_);
    return same;
}
void Room::EndByQuit() { /* TODO 11/24 */ } // left가 모두 false면 아무것도 하지 않도록 할 것
void Room::OnChat(int, const char*) { /* TODO 11/24 */ }
