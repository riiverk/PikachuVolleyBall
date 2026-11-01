#include "RoomManager.h"
#include "Room.h"

RoomManager::RoomManager() = default;
RoomManager::~RoomManager() = default;
Room* RoomManager::JoinRoom(int, ClientSession*, int& outIdx, std::uint8_t& outResult)
{
    outIdx = -1;
    outResult = static_cast<std::uint8_t>(JoinResult::Rejected);
    return nullptr; // TODO 11/9-11/10: room lookup and seat assignment.
}
void RoomManager::RemoveRoom(int) { /* TODO 11/11 */ }
