#include "Room.h"

void Room::Start() { /* TODO 11/12: create room thread. */ }
DWORD WINAPI Room::RoomThread(LPVOID) { return ERROR_CALL_NOT_IMPLEMENTED; }
void Room::SetInput(int, std::uint8_t) { /* TODO 11/13 */ }
void Room::CopyInputs(std::uint8_t out[2]) { out[0] = out[1] = 0; /* TODO 11/13 */ }
void Room::SleepUntilNextTick() { /* TODO 11/14 */ }
void Room::Broadcast(const char*, int) { /* TODO 11/16 */ }
void Room::BroadcastGameStart(int, int) { /* TODO 11/16 */ }
void Room::BroadcastState() { /* TODO 11/17 */ }
void Room::BroadcastEvents() { /* TODO 11/19 */ }
void Room::BroadcastStageOver() { /* TODO 11/20 */ }
void Room::BroadcastGameOver(std::uint8_t) { /* TODO 11/21 */ }
void Room::OnNextStage(int) { /* TODO 11/23 */ }
bool Room::WaitNextStage() { return false; /* TODO 11/22 */ }
void Room::OnPlayerLeave(int) { /* TODO 11/16 */ }
void Room::EndByQuit() { /* TODO 11/24 */ }
void Room::OnChat(int, const char*) { /* TODO 11/24 */ }
