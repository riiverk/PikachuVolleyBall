#include "NetworkClient.h"

bool NetworkClient::Connect(const char*, u_short) { return false; /* TODO 11/2 */ }
void NetworkClient::SendJoinRoom(int, const char*) { /* TODO 11/4 */ }
DWORD WINAPI NetworkClient::RecvThread(LPVOID) { return ERROR_CALL_NOT_IMPLEMENTED; }
void NetworkClient::ProcessPacket(char*) { /* TODO 11/7 */ }
void NetworkClient::SendInput(std::uint8_t) { /* TODO 11/5 */ }
void NetworkClient::SendChat(const char*) { /* TODO 11/23 */ }
void NetworkClient::SendNextStage() { /* TODO 11/26 */ }
void NetworkClient::GetLatestState(GameStatePacket& out) { out = latestState_; /* TODO 11/18 */ }
void NetworkClient::Disconnect() { /* TODO 11/26 */ }
