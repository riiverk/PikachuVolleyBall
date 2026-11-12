#pragma once
#include "Common.h"

class Room;
class ClientSession;

class RoomManager
{
public:
    // Two players per room, so the server's client limit is also enough rooms.
    static constexpr int MAX_ROOMS = FD_SETSIZE;

    RoomManager();
    ~RoomManager();
    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;
    Room* JoinRoom(int roomNo, ClientSession* s, int& outIdx, std::uint8_t& outResult);
    void RemoveRoom(int roomNo);
    std::size_t GetRoomCount() const;
private:
    int FindRoom(int roomNo) const; // call inside csRooms_

    // Lecture ch11 style fixed arrays: rooms_[i] has room number roomNos_[i].
    Room* rooms_[MAX_ROOMS]{};
    int roomNos_[MAX_ROOMS]{};
    int roomCount_ = 0;
    mutable CRITICAL_SECTION csRooms_;
};
