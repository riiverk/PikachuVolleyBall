#pragma once
#include "Common.h"
#include <memory>
#include <unordered_map>

class Room;
class ClientSession;

class RoomManager
{
public:
    RoomManager();
    ~RoomManager();
    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;
    Room* JoinRoom(int roomNo, ClientSession* s, int& outIdx, std::uint8_t& outResult);
    void RemoveRoom(int roomNo);
    std::size_t GetRoomCount() const;
private:
    std::unordered_map<int, std::unique_ptr<Room>> rooms_;
    mutable CRITICAL_SECTION csRooms_;
};
