#include "ClientSession.h"

DWORD WINAPI ClientSession::ClientThread(LPVOID) { return ERROR_CALL_NOT_IMPLEMENTED; }
bool ClientSession::RecvPacket(char*) { return false; /* TODO 11/5 */ }
void ClientSession::ProcessPacket(char*) { /* TODO 11/8 */ }
bool ClientSession::SendPacket(const char*, int) { return false; /* TODO 11/6 */ }
void ClientSession::Disconnect() { /* TODO 11/7 */ }
