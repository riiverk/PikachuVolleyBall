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

void ServerConnectTest()
{
    // No surrounding WSAStartup: both classes must own their Winsock lifetime.
    {
        Server server;
        Check(server.InitServer(0) && server.GetPort() != 0, "server must bind and listen");
        const u_short port = server.GetPort();
        Check(!server.InitServer(0) && server.GetLastError() == WSAEALREADY, "reject double initialization");
        Check(server.GetPort() == port, "double initialization must keep existing listener");

        Server duplicate;
        Check(!duplicate.InitServer(port) && duplicate.GetLastError() != 0, "busy port must fail");
        Check(duplicate.GetPort() == 0, "failed initialization must not expose a port");

        NetworkClient client;
        Check(client.Connect("127.0.0.1", port) && client.IsConnected(), "client connects to server listener");
        Check(!client.Connect("127.0.0.1", port) && client.GetLastError() == WSAEISCONN,
              "double connect must not replace a live connection");
        Check(client.IsConnected(), "double connect preserves first socket");
        client.Disconnect();
        client.Disconnect();
        Check(!client.IsConnected(), "disconnect closes transport");
        server.CloseServer();
        server.CloseServer();
        Check(server.GetPort() == 0, "close clears bound port");
        Check(server.InitServer(port), "server can reuse port after close");
        Check(client.Connect("127.0.0.1", port), "client can reconnect after disconnect");
    }
    SOCKET leftover = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    const int error = WSAGetLastError();
    if (leftover != INVALID_SOCKET) closesocket(leftover);
    Check(leftover == INVALID_SOCKET && error == WSANOTINITIALISED, "destructors must balance every WSAStartup");
    std::cout << "PASS: server listener, TCP connect, busy port, reconnect and automatic cleanup\n";
}

void ClientFailureTest()
{
    NetworkClient client;
    Check(!client.Connect(nullptr, 9000) && client.GetLastError() == WSAEINVAL, "null IP rejected");
    Check(!client.Connect("", 9000) && client.GetLastError() == WSAEINVAL, "empty IP rejected");
    Check(!client.Connect("127.0.0.1", 0) && client.GetLastError() == WSAEINVAL, "zero remote port rejected");
    Check(!client.Connect("999.0.0.1", 9000) && client.GetLastError() == WSAEINVAL, "invalid IPv4 rejected");
    Check(!client.IsConnected(), "invalid input must not leave a connected socket");

    // Reserve a local port without listening, so connection refusal is deterministic.
    Socket reserved;
    reserved.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Check(reserved.value != INVALID_SOCKET, "reserve refusal port");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Check(bind(reserved.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind refusal port");
    int length = sizeof(address);
    Check(getsockname(reserved.value, reinterpret_cast<sockaddr*>(&address), &length) == 0, "get refusal port");
    const u_short port = ntohs(address.sin_port);
    Check(!client.Connect("127.0.0.1", port) && client.GetLastError() == WSAECONNREFUSED,
          "connection refused must report the real socket error");
    Check(!client.IsConnected(), "failed connect must release its socket");
    Check(listen(reserved.value, 1) == 0, "start reserved listener");
    Check(client.Connect("127.0.0.1", port) && client.GetLastError() == 0, "retry after failed connect");
    Socket accepted;
    accepted.value = accept(reserved.value, nullptr, nullptr);
    Check(accepted.value != INVALID_SOCKET, "client must complete a real TCP handshake");
    client.Disconnect();
    std::cout << "PASS: invalid addresses, connection refusal and retry on the same client\n";
}

void PlayerInputTest()
{
    auto input = DecodePlayerInput(LEFT, 0, Action::Idle);
    Check(input.action == Action::Move && input.moveDirection == -1 && input.pressed == LEFT, "move left");
    input = DecodePlayerInput(RIGHT, LEFT, Action::Move);
    Check(input.moveDirection == 1 && input.pressed == RIGHT && input.released == LEFT, "change direction");
    input = DecodePlayerInput(LEFT | RIGHT, 0, Action::Idle);
    Check(input.moveDirection == 0 && input.action == Action::Idle, "opposite keys cancel");
    input = DecodePlayerInput(UP, 0, Action::Idle);
    Check(input.jump && input.action == Action::Jump, "new up press requests jump");
    input = DecodePlayerInput(UP, UP, Action::Idle);
    Check(!input.jump && input.pressed == 0, "holding jump must not retrigger even after landing");
    input = DecodePlayerInput(UP, 0, Action::Jump);
    Check(!input.jump, "cannot jump again in the air");
    input = DecodePlayerInput(ACTION, 0, Action::Jump);
    Check(input.spike && input.action == Action::Spike && input.slideDirection == 0, "air action requests spike");
    input = DecodePlayerInput(ACTION, ACTION, Action::Jump);
    Check(!input.spike && input.slideDirection == 0, "holding action must not retrigger");
    input = DecodePlayerInput(ACTION, ACTION, Action::Spike);
    Check(!input.spike && input.action == Action::Spike, "ongoing spike is preserved without a new request");
    input = DecodePlayerInput(LEFT | ACTION, 0, Action::Move);
    Check(input.action == Action::Slide && input.slideDirection == -1 && !input.spike, "ground action requests left slide");
    input = DecodePlayerInput(RIGHT | ACTION, 0, Action::Idle);
    Check(input.slideDirection == 1, "ground action requests right slide");
    input = DecodePlayerInput(ACTION, 0, Action::Idle);
    Check(input.action == Action::Idle, "stationary ground action has no slide direction");
    input = DecodePlayerInput(UP | ACTION, 0, Action::Idle);
    Check(input.jump && input.spike && input.action == Action::Spike, "jump and spike can be pressed together");
    for (Action locked : {Action::Slide, Action::Win, Action::Lose})
    {
        input = DecodePlayerInput(UP | ACTION | RIGHT, 0, locked);
        Check(input.action == locked && !input.jump && !input.spike && input.moveDirection == 0,
              "locked action must not be interrupted");
    }
    input = DecodePlayerInput(0xff, 0, Action::Idle);
    Check(input.held == (LEFT | RIGHT | UP | ACTION), "ignore undefined input bits");
    GameWorld world;
    std::uint8_t keys[2] = {LEFT | UP, RIGHT};
    world.Update(keys);
    Check(world.GetPlayerInput(0).jump && world.GetPlayerInput(0).moveDirection == -1, "player one input");
    Check(!world.GetPlayerInput(1).jump && world.GetPlayerInput(1).moveDirection == 1, "player two input is independent");
    world.Update(keys);
    Check(!world.GetPlayerInput(0).jump && world.GetPlayerInput(0).pressed == 0, "held input across game ticks");
    keys[0] = 0;
    world.Update(keys);
    Check(world.GetPlayerInput(0).released == (LEFT | UP), "key release tracked");
    keys[0] = UP;
    world.Update(keys);
    Check(world.GetPlayerInput(0).jump, "release and press allows another jump request");
    std::cout << "PASS: move/jump/slide/spike input, press edges, action locks and independent players\n";
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
    Check(server.InitServer(0), "server listener initialization");
    server.AcceptLoop(); server.CloseServer();
    (void)session.RecvPacket(nullptr); session.ProcessPacket(nullptr);
    (void)session.SendPacket(nullptr, 0); session.Disconnect();
    (void)ClientSession::ClientThread(nullptr); (void)Room::RoomThread(nullptr);
    int index; std::uint8_t result;
    RoomManager manager;
    Check(manager.JoinRoom(1, &session, index, result) == nullptr && index == -1, "no false room admission");
    manager.RemoveRoom(1);
    Check(!network.Connect(nullptr, SERVERPORT), "invalid connection request rejected");
    network.SendJoinRoom(1, "test"); network.SendInput(0); network.SendChat("");
    network.SendNextStage(); network.GetLatestState(state); network.ProcessPacket(nullptr);
    network.Disconnect(); (void)NetworkClient::RecvThread(nullptr);
    (void)input.OnKeyDown(VK_LEFT); (void)input.OnKeyUp(VK_LEFT); input.Reset();
    client.ChangeScene(Scene::Waiting); client.OnKey(WM_KEYDOWN, VK_LEFT);
    client.OnNetEvent(0, 0); client.OnTimer();
    (void)renderer.Initialize(nullptr); renderer.Render(state); renderer.Shutdown();
    std::cout << "PASS: remaining Client/Server class interfaces link\n";
}
}

int main()
{
    try { ServerConnectTest(); }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 1;
    int code = 0;
    try
    {
        ProtocolTest(); FragmentedTest(); CoalescedTest(); ShortAndInvalidTest();
        ClientFailureTest(); PlayerInputTest(); LinkScaffolds();
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
