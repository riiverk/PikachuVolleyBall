#include "RoomManager.h"
#include "Room.h"
#include <new>

RoomManager::RoomManager() { InitializeCriticalSection(&csRooms_); }
RoomManager::~RoomManager() { DeleteCriticalSection(&csRooms_); }
Room* RoomManager::JoinRoom(int roomNo, ClientSession* s, int& outIdx, std::uint8_t& outResult)
{
    outIdx = -1;
    outResult = static_cast<std::uint8_t>(JoinResult::Rejected);
    if (s == nullptr) return nullptr;
    Room* room = nullptr;
    int seat = -1;
    bool matched = false;
    EnterCriticalSection(&csRooms_);
    try
    {
        std::unique_ptr<Room>& slot = rooms_[roomNo];
        if (!slot) slot = std::make_unique<Room>();
        room = slot.get();
        seat = room->AddPlayer(s);
        matched = seat >= 0 && room->GetPlayer(1 - seat) != nullptr;
    }
    catch (const std::bad_alloc&)
    {
        room = nullptr;
    }
    LeaveCriticalSection(&csRooms_);
    if (room == nullptr) return nullptr;
    if (seat < 0)
    {
        outResult = static_cast<std::uint8_t>(JoinResult::Full);
        return nullptr;
    }
    outIdx = seat;
    outResult = static_cast<std::uint8_t>(matched ? JoinResult::Matched : JoinResult::Waiting);
    return room;
}
void RoomManager::RemoveRoom(int) { /* TODO 11/11 */ }
