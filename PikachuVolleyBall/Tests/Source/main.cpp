#include "Common.h"
#include "../../Server/Source/Server.h"
#include "../../Server/Source/ClientSession.h"
#include "../../Server/Source/Room.h"
#include "../../Client/Source/GameClient.h"
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
void Check(bool ok, const char* label)
{
    if (!ok) throw std::runtime_error(label);
}

struct Socket
{
    SOCKET value = INVALID_SOCKET;
    ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
    Socket() = default;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
};

struct Stream
{
    Socket listener, sender, receiver;
    Stream()
    {
        listener.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(listener.value != INVALID_SOCKET, "create listener");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Check(bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind loopback");
        Check(listen(listener.value, 1) == 0, "listen");
        int length = sizeof(address);
        Check(getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &length) == 0, "get port");
        sender.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(sender.value != INVALID_SOCKET, "create sender");
        Check(connect(sender.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "connect loopback");
        receiver.value = accept(listener.value, nullptr, nullptr);
        Check(receiver.value != INVALID_SOCKET, "accept");
        const DWORD timeout = 2000;
        Check(setsockopt(receiver.value, SOL_SOCKET, SO_RCVTIMEO,
              reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0, "receive timeout");
    }
    void Send(const char* data, int length)
    {
        int sent = 0;
        while (sent < length)
        {
            const int count = send(sender.value, data + sent, length - sent, 0);
            Check(count > 0, "send");
            sent += count;
        }
    }
};

template<class T> void CheckPacket(PacketType type)
{
    const T packet{};
    PacketHeader header{};
    std::memcpy(&header, &packet, sizeof(header));
    Check(header.type == type && header.size == sizeof(T), "default wire header");
    Check(PacketSize(type) == sizeof(T), "packet size lookup");
}

void ProtocolTest()
{
    CheckPacket<JoinRoomPacket>(PacketType::JOIN_ROOM);
    CheckPacket<InputPacket>(PacketType::INPUT);
    CheckPacket<NextStagePacket>(PacketType::NEXT_STAGE);
    CheckPacket<LeavePacket>(PacketType::LEAVE);
    CheckPacket<ChatPacket>(PacketType::CHAT);
    CheckPacket<JoinResultPacket>(PacketType::JOIN_RESULT);
    CheckPacket<GameStartPacket>(PacketType::GAME_START);
    CheckPacket<GameStatePacket>(PacketType::GAME_STATE);
    CheckPacket<EventPacket>(PacketType::EVENT);
    CheckPacket<ScorePacket>(PacketType::SCORE);
    CheckPacket<StageOverPacket>(PacketType::STAGE_OVER);
    CheckPacket<GameOverPacket>(PacketType::GAME_OVER);
    Check(PacketSize(static_cast<PacketType>(0)) == 0, "unknown packet type");
    InputPacket packet;
    packet.keys = LEFT | ACTION;
    const std::array<unsigned char, 4> expected{4, 0, 2, 9};
    Check(std::memcmp(&packet, expected.data(), expected.size()) == 0, "known INPUT wire bytes");
    std::cout << "PASS: all 12 packet headers, sizes and INPUT wire bytes\n";
}

void FragmentedTest()
{
    Stream stream;
    JoinRoomPacket packet;
    packet.roomNo = 123;
    std::memcpy(packet.nickname, "Pikachu", 8);
    bool sent = true;
    std::jthread writer([&] {
        try
        {
            const auto* data = reinterpret_cast<const char*>(&packet);
            for (int i = 0; i < sizeof(packet); ++i)
            {
                stream.Send(data + i, 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            shutdown(stream.sender.value, SD_SEND);
        }
        catch (...) { sent = false; shutdown(stream.sender.value, SD_SEND); }
    });
    JoinRoomPacket received{};
    const int count = recvn(stream.receiver.value, reinterpret_cast<char*>(&received), sizeof(received));
    writer.join();
    Check(sent && count == sizeof(received), "fragmented packet read");
    Check(std::memcmp(&packet, &received, sizeof(packet)) == 0, "fragmented packet content");
    char tail;
    Check(recvn(stream.receiver.value, &tail, 1) == 0, "clean EOF");
    std::cout << "PASS: byte-by-byte TCP delivery and clean EOF\n";
}

void CoalescedTest()
{
    Stream stream;
    InputPacket packets[2];
    packets[0].keys = LEFT;
    packets[1].keys = RIGHT | UP;
    stream.Send(reinterpret_cast<const char*>(packets), sizeof(packets));
    for (const auto& expected : packets)
    {
        InputPacket received;
        Check(recvn(stream.receiver.value, reinterpret_cast<char*>(&received), sizeof(received)) == sizeof(received), "coalesced read");
        Check(std::memcmp(&expected, &received, sizeof(received)) == 0, "packet boundary preserved");
    }
    std::cout << "PASS: two packets in one TCP write\n";
}

void ShortAndInvalidTest()
{
    Stream stream;
    stream.Send("ab", 2);
    shutdown(stream.sender.value, SD_SEND);
    char data[4]{};
    Check(recvn(stream.receiver.value, data, 4) == 2 && data[0] == 'a' && data[1] == 'b', "partial EOF must remain short");
    Check(recvn(INVALID_SOCKET, nullptr, 0) == 0, "zero length");
    Check(recvn(stream.receiver.value, nullptr, 1) == SOCKET_ERROR && WSAGetLastError() == WSAEINVAL, "null buffer rejected");
    Check(recvn(stream.receiver.value, data, -1) == SOCKET_ERROR && WSAGetLastError() == WSAEINVAL, "negative length rejected");
    Check(recvn(stream.receiver.value, data, 1, MSG_PEEK) == SOCKET_ERROR && WSAGetLastError() == WSAEINVAL, "peek rejected");
    Check(recvn(INVALID_SOCKET, data, 1) == SOCKET_ERROR, "socket error propagated");
    std::cout << "PASS: truncated stream, invalid arguments and socket errors\n";
}

void LinkScaffolds()
{
    // Exercise declarations from each module so missing definitions fail linking.
    Server server;
    ClientSession session;
    Room room;
    GameWorld world;
    GameClient client;
    NetworkClient network;
    InputManager input;
    Renderer renderer;
    GameStatePacket state;
    std::uint8_t keys[2]{};
    world.Reset(1, 0); world.Update(keys); world.UpdateBall(); world.CheckScore();
    world.FillState(state); (void)world.PopEvents(); (void)world.IsStageOver(); (void)world.GetLoser();
    room.SetInput(0, 0); room.CopyInputs(keys); room.Start(); room.SleepUntilNextTick();
    room.Broadcast(nullptr, 0); room.BroadcastGameStart(1, 0); room.BroadcastState();
    room.BroadcastEvents(); room.BroadcastStageOver(); room.BroadcastGameOver(0);
    room.OnNextStage(0); (void)room.WaitNextStage(); room.OnPlayerLeave(0);
    room.EndByQuit(); room.OnChat(0, "");
    Check(!server.InitServer(SERVERPORT), "scaffold must not pretend to listen");
    server.AcceptLoop(); server.CloseServer();
    (void)session.RecvPacket(nullptr); session.ProcessPacket(nullptr);
    (void)session.SendPacket(nullptr, 0); session.Disconnect();
    (void)ClientSession::ClientThread(nullptr); (void)Room::RoomThread(nullptr);
    int index; std::uint8_t result;
    RoomManager manager;
    Check(manager.JoinRoom(1, &session, index, result) == nullptr && index == -1, "no false room admission");
    manager.RemoveRoom(1);
    Check(!network.Connect("127.0.0.1", SERVERPORT), "scaffold must not pretend to connect");
    network.SendJoinRoom(1, "test"); network.SendInput(0); network.SendChat("");
    network.SendNextStage(); network.GetLatestState(state); network.ProcessPacket(nullptr);
    network.Disconnect(); (void)NetworkClient::RecvThread(nullptr);
    (void)input.OnKeyDown(VK_LEFT); (void)input.OnKeyUp(VK_LEFT); input.Reset();
    client.ChangeScene(Scene::Waiting); client.OnKey(WM_KEYDOWN, VK_LEFT);
    client.OnNetEvent(0, 0); client.OnTimer();
    (void)renderer.Initialize(nullptr); renderer.Render(state); renderer.Shutdown();
    std::cout << "PASS: Client/Server class scaffolds link; no false connection success\n";
}
}

int main()
{
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 1;
    int code = 0;
    try
    {
        ProtocolTest(); FragmentedTest(); CoalescedTest(); ShortAndInvalidTest(); LinkScaffolds();
        std::cout << "ALL FOUNDATION CHECKS PASSED\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << "; WSA=" << WSAGetLastError() << '\n';
        code = 1;
    }
    WSACleanup();
    return code;
}
