#include "RoomManager.h"
#include "Room.h"
#include <cstdio>
#include <new>

RoomManager::RoomManager() { InitializeCriticalSection(&csRooms_); }

RoomManager::~RoomManager()
{
    Clear();
    DeleteCriticalSection(&csRooms_);
}

void RoomManager::Clear()
{
    while (roomCount_ > 0)
    {
        --roomCount_;
        delete rooms_[roomCount_];
        rooms_[roomCount_] = nullptr;
        roomNos_[roomCount_] = 0;
    }
}

int RoomManager::FindRoom(int roomNo) const
{
    for (int i = 0; i < roomCount_; ++i)
        if (roomNos_[i] == roomNo) return i;
    return -1;
}

Room* RoomManager::JoinRoom(int roomNo, ClientSession* s, int& outIdx, std::uint8_t& outResult)
{
    outIdx = -1;
    outResult = static_cast<std::uint8_t>(JoinResult::Rejected);
    if (s == nullptr) return nullptr;
    Room* room = nullptr;
    int seat = -1;
    bool matched = false;
    EnterCriticalSection(&csRooms_);
    int index = FindRoom(roomNo);
    if (index < 0 && roomCount_ < MAX_ROOMS)
    {
        Room* created = new (std::nothrow) Room;
        if (created != nullptr)
        {
            index = roomCount_++;
            rooms_[index] = created;
            roomNos_[index] = roomNo;
        }
    }
    if (index >= 0)
    {
        room = rooms_[index];
        seat = room->AddPlayer(s);
        matched = seat >= 0 && room->GetPlayer(1 - seat) != nullptr;
    }
    LeaveCriticalSection(&csRooms_);
    if (room == nullptr)
    {
        std::printf("[rooms] cannot create room %d\n", roomNo);
        return nullptr;
    }
    if (seat < 0)
    {
        outResult = static_cast<std::uint8_t>(JoinResult::Full);
        return nullptr;
    }
    outIdx = seat;
    outResult = static_cast<std::uint8_t>(matched ? JoinResult::Matched : JoinResult::Waiting);
    return room;
}

void RoomManager::RemoveRoom(int roomNo)
{
    Room* removed = nullptr;
    int remaining = 0;
    EnterCriticalSection(&csRooms_);
    const int index = FindRoom(roomNo);
    if (index >= 0)
    {
        // Lecture ch11 RemoveSocketInfo: move the last entry into the removed slot.
        removed = rooms_[index];
        rooms_[index] = rooms_[roomCount_ - 1];
        roomNos_[index] = roomNos_[roomCount_ - 1];
        rooms_[roomCount_ - 1] = nullptr;
        --roomCount_;
    }
    remaining = roomCount_;
    LeaveCriticalSection(&csRooms_);
    if (removed == nullptr) return;
    delete removed;
    std::printf("[rooms] removed room %d, %d left\n", roomNo, remaining);
}

bool RoomManager::RemoveIfWaiting(Room* room)
{
    if (room == nullptr) return false;

    Room* removed = nullptr;
    EnterCriticalSection(&csRooms_);
    for (int i = 0; i < roomCount_; ++i)
    {
        if (rooms_[i] != room) continue;
        if (room->GetPlayer(1) != nullptr) break;
        removed = rooms_[i];
        rooms_[i] = rooms_[roomCount_ - 1];
        roomNos_[i] = roomNos_[roomCount_ - 1];
        rooms_[roomCount_ - 1] = nullptr;
        --roomCount_;
        break;
    }
    LeaveCriticalSection(&csRooms_);
    if (removed == nullptr) return false;
    delete removed;
    std::printf("[rooms] removed waiting room\n");
    return true;
}

std::size_t RoomManager::GetRoomCount() const
{
    EnterCriticalSection(&csRooms_);
    const std::size_t count = static_cast<std::size_t>(roomCount_);
    LeaveCriticalSection(&csRooms_);
    return count;
}
