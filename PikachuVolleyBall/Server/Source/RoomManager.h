#pragma once
#include "Protocol.h"
#include <memory>
#include <mutex>
#include <unordered_map>

class Room;
class ClientSession;

class RoomManager
{
public:
    RoomManager();
    ~RoomManager();
    Room* JoinRoom(int roomNo, ClientSession* s, int& outIdx, std::uint8_t& outResult);
    void RemoveRoom(int roomNo);
private:
    std::unordered_map<int, std::unique_ptr<Room>> rooms_;
    std::mutex mutex_;
};
