#include "Common.h"
#include "../../Server/Source/Server.h"
#include "../../Server/Source/ClientSession.h"
#include "../../Server/Source/Room.h"
#include "../../Server/Source/RoomManager.h"
#include "../../Server/Source/PlayerMovement.h"
#include "../../Server/Source/BallPhysics.h"
#include "../../Client/Source/GameClient.h"
#include "../../Client/Source/ChatPanel.h"
#include "../../Client/Source/ConnectScreen.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

// Seed and inspect world state for integration tests through the existing public methods.
struct GameWorldTestAccess
{
    static void SetPhase(GameWorld& world, Phase phase) { world.state_.phase = phase; }
    static void SetBall(GameWorld& world, const BallState& ball) { world.state_.ball = ball; }
    static void SetPlayer(GameWorld& world, int index, const PlayerState& player) { world.state_.p[index] = player; }
    static int Score(const GameWorld& world, int index) { return world.scores_[index]; }
    static const std::vector<GameEvent>& Events(const GameWorld& world) { return world.events_; }

    static void FinishFirstStage(GameWorld& world, int loser)
    {
        world.stage_ = 1;
        world.scores_[loser] = 2;
        world.scores_[1 - loser] = GameConst::WinningScore;
        world.state_.phase = Phase::StageOver;
        world.state_.tick = 99;
        for (int i = 0; i < GameConst::PlayerCount; ++i)
        {
            world.state_.p[i].action = i == loser ? Action::Lose : Action::Win;
            world.state_.p[i].frame = 4;
        }
        world.events_.push_back({EventKind::Score, static_cast<std::uint8_t>(1 - loser)});
    }
    static bool StageIsFresh(const GameWorld& world, int stage, int serveSide)
    {
        return world.stage_ == stage && world.serveSide_ == serveSide &&
            world.scores_[0] == 0 && world.scores_[1] == 0 && world.events_.empty();
    }
};

struct RendererTestAccess
{
    static COLORREF Pixel(const Renderer& renderer, int x, int y)
    {
        return GetPixel(renderer.backBufferDC_, x, y);
    }
};

struct GameClientTestAccess
{
    static auto Scores(const GameClient& client) { return client.scores_; }

    static COLORREF Pixel(const GameClient& client, int x, int y)
    {
        return RendererTestAccess::Pixel(client.renderer_, x, y);
    }
};

struct RoomTestAccess
{
    static GameWorld& World(Room& room) { return room.world_; }
    static void SetStage(Room& room, int stage) { room.stage_ = stage; }
    static bool WaitForEnd(Room& room)
    {
        return WaitForSingleObject(room.thread_, 2000) == WAIT_OBJECT_0;
    }
    static void Bind(ClientSession& session, Room& room, int index)
    {
        session.room_ = &room;
        session.playerIndex_ = index;
    }
};

namespace
{
void Check(bool ok, const char* label)
{
    if (!ok) throw std::runtime_error(label);
}

void ExpectClientLeave(SOCKET peer)
{
    LeavePacket packet;
    const int received = recvn(peer, reinterpret_cast<char*>(&packet), sizeof(packet));
    Check(received == static_cast<int>(sizeof(packet)) &&
        packet.size == sizeof(packet) && packet.type == PacketType::LEAVE,
        ("LEAVE is the final whole client frame; received=" + std::to_string(received) +
            ", type=" + std::to_string(static_cast<unsigned>(packet.type))).c_str());
    char tail;
    Check(recv(peer, &tail, 1, 0) == 0, "exactly one LEAVE precedes clean EOF");
}

void AdvanceWorldBall(GameWorld& world, BallState& ball)
{
    GameWorldTestAccess::SetBall(world, ball);
    world.UpdateBall();
    GameStatePacket state;
    world.FillState(state);
    ball = state.ball;
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

struct Listener
{
    Socket socket;
    u_short port = 0;
    Listener()
    {
        socket.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(socket.value != INVALID_SOCKET, "create receive-test listener");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Check(bind(socket.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind receive-test listener");
        Check(listen(socket.value, 8) == 0, "listen for receive tests");
        int length = sizeof(address);
        Check(getsockname(socket.value, reinterpret_cast<sockaddr*>(&address), &length) == 0, "get receive-test port");
        port = ntohs(address.sin_port);
    }
    SOCKET Accept()
    {
        const SOCKET accepted = accept(socket.value, nullptr, nullptr);
        Check(accepted != INVALID_SOCKET, "accept receive-test client");
        return accepted;
    }
};

void SendBytes(SOCKET socket, const char* data, int length)
{
    int sent = 0;
    while (sent < length)
    {
        const int count = send(socket, data + sent, length - sent, 0);
        Check(count > 0, "send test bytes");
        sent += count;
    }
}

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

void GameClientJoinTest()
{
    GameClient client;
    Listener listener;
    Check(client.ConnectAndJoin(L"999.0.0.1", L"27", L"피카츄", listener.port) == JoinRequestResult::InvalidAddress,
        "reject an invalid IPv4 address");
    for (const wchar_t* room : {L"0", L"2147483648", L"27a"})
        Check(client.ConnectAndJoin(L"127.0.0.1", room, L"피카츄", listener.port) == JoinRequestResult::InvalidRoom,
            "reject room numbers outside the positive int range or with non-digits");
    for (const wchar_t* nick : {L"", L"피카츄배구팀"})
        Check(client.ConnectAndJoin(L"127.0.0.1", L"27", nick, listener.port) == JoinRequestResult::InvalidNickname,
            "reject empty nicknames or UTF-8 nicknames that do not fit in the packet");
    Check(!client.IsConnected(), "invalid form fields leave the client disconnected");

    Check(client.ConnectAndJoin(L"127.0.0.1", L"27", L"피카츄배구", listener.port) == JoinRequestResult::Sent,
        "connect and send a join request on retry");
    Socket peer;
    peer.value = listener.Accept();
    const DWORD timeout = 2000;
    Check(setsockopt(peer.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0,
        "set join request receive timeout");
    JoinRoomPacket packet;
    Check(recvn(peer.value, reinterpret_cast<char*>(&packet), sizeof(packet)) == sizeof(packet) &&
        packet.type == PacketType::JOIN_ROOM && packet.size == sizeof(packet) &&
        packet.roomNo == 27 && std::strcmp(packet.nickname, "피카츄배구") == 0,
        "send the entered room and UTF-8 nickname in one JOIN_ROOM packet");
    client.Disconnect();
    Check(!client.IsConnected(), "disconnect the online client before entering local play");
    std::cout << "PASS: game client validates the connection form and sends JOIN_ROOM\n";
}

void GameClientInputTest()
{
    GameClient client;
    Listener listener;
    client.OnKey(WM_KEYDOWN, VK_UP);
    Check(client.ConnectAndJoin(L"127.0.0.1", L"1", L"input", listener.port) == JoinRequestResult::Sent,
        "join before exercising game client input");
    Socket peer;
    peer.value = listener.Accept();
    const DWORD timeout = 2000;
    Check(setsockopt(peer.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0,
        "set input receive timeout");
    JoinRoomPacket join;
    Check(recvn(peer.value, reinterpret_cast<char*>(&join), sizeof(join)) == sizeof(join) &&
        join.type == PacketType::JOIN_ROOM, "JOIN_ROOM precedes input packets");

    for (const Scene scene : {Scene::Connect, Scene::Waiting, Scene::StageOver, Scene::Result})
    {
        client.ChangeScene(scene);
        for (const WPARAM key : std::array<WPARAM, 8>{VK_LEFT, VK_RIGHT, VK_UP, 'Z', VK_RETURN, VK_SPACE, VK_TAB, VK_ESCAPE})
        {
            client.OnKey(WM_KEYDOWN, key);
            client.OnKey(WM_KEYUP, key);
        }
    }
    client.ChangeScene(Scene::Playing);
    client.OnKey(WM_KEYUP, VK_UP);
    client.OnKey(WM_CHAR, 'Z');
    client.OnKey(WM_KEYDOWN, 'A');
    client.OnKey(WM_KEYDOWN, VK_RETURN);
    client.OnKey(WM_KEYUP, VK_RETURN);
    client.OnKey(WM_KEYDOWN, VK_TAB);
    client.OnKey(WM_KEYDOWN, VK_LEFT);
    client.OnKey(WM_KEYDOWN, VK_LEFT);
    client.OnKey(WM_KEYDOWN, 'Z');
    client.OnKey(WM_KEYUP, VK_LEFT);
    client.OnKey(WM_KEYUP, VK_LEFT);
    client.ReleaseInput();
    client.ReleaseInput();
    client.OnKey(WM_KEYDOWN, VK_RIGHT);
    client.OnKey(WM_KEYUP, VK_RIGHT);

    std::vector<std::uint8_t> expected{LEFT, LEFT | ACTION, ACTION, 0, RIGHT, 0};
    client.OnKey(WM_KEYDOWN, VK_RIGHT);
    Check(client.BeginChat() && client.IsChatting(), "opening chat releases game input");
    client.OnKey(WM_KEYDOWN, VK_LEFT);
    client.OnKey(WM_KEYDOWN, VK_UP);
    client.OnKey(WM_KEYDOWN, 'Z');
    client.EndChat();
    client.OnKey(WM_KEYUP, VK_RIGHT);
    expected.insert(expected.end(), {RIGHT, 0});
    for (const Scene scene : {Scene::Connect, Scene::Waiting, Scene::StageOver, Scene::Result})
    {
        client.OnKey(WM_KEYDOWN, VK_RIGHT);
        client.ChangeScene(scene);
        client.OnKey(WM_KEYUP, VK_RIGHT);
        client.OnKey(WM_KEYDOWN, 'Z');
        client.OnKey(WM_KEYUP, 'Z');
        client.ChangeScene(Scene::Playing);
        client.OnKey(WM_KEYUP, VK_RIGHT);
        expected.insert(expected.end(), {RIGHT, 0});
    }
    client.OnKey(WM_KEYDOWN, VK_UP);
    client.Disconnect();
    expected.insert(expected.end(), {UP, 0});

    for (const std::uint8_t keys : expected)
    {
        InputPacket packet;
        Check(recvn(peer.value, reinterpret_cast<char*>(&packet), sizeof(packet)) == sizeof(packet) &&
            packet.type == PacketType::INPUT && packet.size == sizeof(packet) && packet.keys == keys,
            "send only gameplay changes and release held keys on focus loss, scene changes and disconnect");
    }
    ExpectClientLeave(peer.value);
    std::cout << "PASS: game client routes keys by scene, suppresses repeats and releases held keys\n";
}

template<class Predicate> void WaitUntil(Predicate predicate, const char* label)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate())
    {
        Check(std::chrono::steady_clock::now() < deadline, label);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

struct AcceptRunner
{
    Server& server;
    std::thread thread;
    explicit AcceptRunner(Server& target) : server(target), thread(&Server::AcceptLoop, &target) {}
    void Finish()
    {
        server.CloseServer();
        if (thread.joinable()) thread.join();
    }
    ~AcceptRunner() { Finish(); }
};

void ServerAcceptTest()
{
    DWORD baselineHandles = 0, finalHandles = 0;
    Server server;
    for (int cycle = 0; cycle < 6; ++cycle)
    {
        Check(server.InitServer(0) && server.GetAcceptedCount() == 0, "restart listener with a fresh accept count");
        AcceptRunner running(server);
        WaitUntil([&] { return server.IsAccepting(); }, "accept loop must start");
        std::array<NetworkClient, 6> clients;
        for (auto& client : clients)
            Check(client.Connect("127.0.0.1", server.GetPort()), "connect multiple clients");
        WaitUntil([&] { return server.GetAcceptedCount() == clients.size(); }, "accept all client connections");
        Check(server.GetClientCount() == clients.size(), "accepted sessions must be owned by the server");
        // Closing with connected clients must release sessions, worker handles and sockets.
        running.Finish();
        Check(!server.IsAccepting() && server.GetClientCount() == 0, "shutdown releases all client sessions");
        for (auto& client : clients) client.Disconnect();
        if (cycle == 0)
            Check(GetProcessHandleCount(GetCurrentProcess(), &baselineHandles) != 0, "baseline handle count");
    }
    Check(GetProcessHandleCount(GetCurrentProcess(), &finalHandles) != 0, "final handle count");
    Check(finalHandles <= baselineHandles + 2, "repeated accept/close must not leak socket or thread handles");

    Check(server.InitServer(0), "idle listener initialization");
    AcceptRunner idle(server);
    WaitUntil([&] { return server.IsAccepting(); }, "idle accept loop must start");
    const auto started = std::chrono::steady_clock::now();
    idle.Finish();
    Check(std::chrono::steady_clock::now() - started < std::chrono::seconds(2), "idle accept must stop without a new connection");
    Check(server.InitServer(0), "concurrent close listener initialization");
    AcceptRunner concurrent(server);
    WaitUntil([&] { return server.IsAccepting(); }, "concurrent close loop must start");
    std::thread firstClose(&Server::CloseServer, &server);
    std::thread secondClose(&Server::CloseServer, &server);
    firstClose.join();
    secondClose.join();
    concurrent.Finish();
    Check(server.GetPort() == 0 && server.GetClientCount() == 0, "concurrent close calls complete safely");
    ClientSession invalid;
    Check(!invalid.Start() && invalid.GetLastError() == WSAENOTSOCK, "invalid session must not start a worker");
    std::cout << "PASS: 36 accepted sessions, cleanup, restart, idle and concurrent shutdown\n";
}

void HorizontalMovementTest()
{
    GameWorld world;
    GameStatePacket state;
    world.FillState(state);
    Check(state.p[0].x == 30 && state.p[1].x == 950, "player spawn positions");
    std::uint8_t keys[2] = {RIGHT, LEFT};
    world.Update(keys);
    world.FillState(state);
    Check(state.p[0].x == 40 && state.p[1].x == 940, "independent horizontal movement at normal speed");
    Check(state.p[0].y == 560 && state.p[1].y == 560, "horizontal movement must not change height");
    Check(state.p[0].action == Action::Move && state.p[1].action == Action::Move, "walking state");
    Check(state.p[0].frame == 0 && state.p[1].frame == 0, "walking starts from the first frame");
    for (int tick = 2; tick <= 101; ++tick)
    {
        world.Update(keys);
        world.FillState(state);
        if (tick == 5 || tick == 17 || tick == 29 || tick == 33)
        {
            const std::uint16_t expected = tick == 17 ? 4 : (tick == 33 ? 0 : 1);
            Check(state.p[0].frame == expected && state.p[1].frame == expected,
                "both walking animations advance, reverse and repeat");
        }
    }
    world.FillState(state);
    Check(state.p[0].x == GameConst::PlayerMaxX[0] && state.p[1].x == GameConst::PlayerMinX[1], "players cannot cross the net");
    Check(state.p[0].frame == 0 && state.p[1].frame == 0, "walking animation stops when blocked by the court edge");
    keys[0] = LEFT; keys[1] = RIGHT;
    for (int tick = 0; tick < 100; ++tick) world.Update(keys);
    world.FillState(state);
    Check(state.p[0].x == GameConst::PlayerMinX[0] && state.p[1].x == GameConst::PlayerMaxX[1], "outer court bounds");
    keys[0] = 0; keys[1] = LEFT | RIGHT;
    world.Update(keys);
    GameStatePacket stopped;
    world.FillState(stopped);
    Check(stopped.p[0].x == state.p[0].x && stopped.p[1].x == state.p[1].x, "release and opposite keys stop movement");
    Check(stopped.p[0].action == Action::Idle && stopped.p[1].action == Action::Idle, "idle state after stopping");
    Check(stopped.p[0].frame == 0 && stopped.p[1].frame == 0, "idle uses the first frame");

    PlayerState awakened;
    awakened.x = 30; awakened.y = 560; awakened.evil = 1;
    PlayerInput right; right.moveDirection = 1;
    MovePlayerHorizontally(awakened, right, 0);
    Check(awakened.x == 45, "awakened movement speed");
    awakened.x = GameConst::PlayerMaxX[0] - 2;
    MovePlayerHorizontally(awakened, right, 0);
    Check(awakened.x == GameConst::PlayerMaxX[0], "clamp the final partial step");
    for (Action locked : {Action::Slide, Action::Win, Action::Lose})
    {
        awakened.x = 30; awakened.action = locked;
        MovePlayerHorizontally(awakened, right, 0);
        Check(awakened.x == 30 && awakened.action == locked, "locked animation must block walking");
    }
    awakened.action = Action::Jump;
    MovePlayerHorizontally(awakened, right, 0);
    Check(awakened.x == 45 && awakened.action == Action::Jump && awakened.y == 560, "horizontal movement preserves airborne action");
    std::cout << "PASS: player movement, court/net bounds, awakened speed and action restrictions\n";
}

void GameClientSceneTest()
{
    GameClient client;
    Check(client.GetScene() == Scene::Connect, "game client starts at the connect scene");

    for (const Scene scene : {Scene::Waiting, Scene::Playing, Scene::StageOver, Scene::Playing, Scene::Result, Scene::Connect})
    {
        client.ChangeScene(scene);
        Check(client.GetScene() == scene, "game client switches to the requested scene");
    }
    std::cout << "PASS: game client scene transitions and return to connect\n";
}

void RendererInitializationTest()
{
    const HWND window = CreateWindowExW(0, L"STATIC", nullptr, WS_POPUP,
        0, 0, 320, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(window != nullptr, "create a hidden renderer window");
    const HANDLE process = GetCurrentProcess();
    {
        Renderer renderer;
        Check(!renderer.Initialize(nullptr), "renderer requires a valid window");
        Check(renderer.Initialize(window), "initialize the renderer and load both bitmaps");
        renderer.Shutdown();
        GdiFlush();
        const DWORD baseline = GetGuiResources(process, GR_GDIOBJECTS);
        Check(renderer.Initialize(window), "renderer can initialize after shutdown");
        GdiFlush();
        const DWORD initializedCount = GetGuiResources(process, GR_GDIOBJECTS);
        Check(initializedCount > baseline, "renderer owns GDI objects after initialization");
        Check(renderer.Initialize(window), "repeated initialization on the same window succeeds");
        GdiFlush();
        Check(GetGuiResources(process, GR_GDIOBJECTS) == initializedCount,
            "repeated initialization does not allocate more GDI objects");

        renderer.Shutdown();
        renderer.Shutdown();
        GdiFlush();
        Check(GetGuiResources(process, GR_GDIOBJECTS) == baseline, "shutdown releases the renderer's GDI objects");
    }
    const DWORD baseline = GetGuiResources(process, GR_GDIOBJECTS);
    {
        Renderer renderer;
        Check(renderer.Initialize(window), "initialize the renderer before destruction");
    }
    GdiFlush();
    Check(GetGuiResources(process, GR_GDIOBJECTS) == baseline, "destruction releases the renderer's GDI objects");
    DestroyWindow(window);
    std::cout << "PASS: renderer initialization, shutdown and GDI resource ownership\n";
}

void RendererFrameTest()
{
    const HWND window = CreateWindowExW(0, L"STATIC", nullptr, WS_POPUP,
        0, 0, 580, 415, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(window != nullptr, "create a window for the renderer frame test");
    Renderer renderer;
    Check(renderer.Initialize(window), "initialize the frame renderer");

    GameStatePacket state;
    state.phase = Phase::Playing;
    state.p[0].x = 100; state.p[0].y = 520;
    state.p[1].x = 900; state.p[1].y = 520;
    state.ball.x = 380; state.ball.y = 280;
    renderer.Render(state);

    const auto pixel = [&renderer](int x, int y) { return RendererTestAccess::Pixel(renderer, x, y); };
    const COLORREF sky = pixel(0, 0);
    const COLORREF ground = pixel(GameConst::WindowWidth - 1, GameConst::WindowHeight - 1);
    Check(sky != CLR_INVALID && ground != CLR_INVALID && sky != ground, "render sky and ground to the buffer edges");
    const COLORREF leftPlayer = pixel(185, 605);
    const COLORREF ball = pixel(430, 330);
    // The same offset hits a red cheek on the left sprite and yellow on its mirror.
    Check(pixel(159, 627) == RGB(159, 0, 0) && pixel(959, 627) == RGB(255, 255, 0),
        "the right player is mirrored");
    Check(ball == RGB(255, 247, 247), "draw the ball at its packet coordinates");
    Check(pixel(70, 70) == RGB(255, 0, 0) && pixel(1066, 70) == RGB(255, 0, 0),
        "draw an initial zero for each player");
    const COLORREF thirdDigitBackground = pixel(198, 70);

    state.p[0].action = Action::Jump;
    state.p[0].frame = 1;
    state.p[1].evil = 1;
    state.ball.x = 450;
    const COLORREF warningBackground = pixel(945, 505);
    renderer.Render(state, {8, 9});
    Check(pixel(185, 605) != leftPlayer, "select the player sprite by packet action and frame");
    Check(pixel(945, 505) != warningBackground, "draw the awakening warning");
    Check(pixel(430, 330) != ball && pixel(500, 330) == ball, "redraw the ball without leaving its previous frame");
    Check(pixel(82, 82) == RGB(0, 0, 0) && pixel(1078, 82) == RGB(255, 0, 0),
        "draw distinct scores using the sprites for eight and nine");

    renderer.Render(state, {255, 9});
    Check(pixel(82, 82) == RGB(255, 0, 0) && pixel(198, 70) == RGB(255, 0, 0) &&
        pixel(1078, 82) == RGB(255, 0, 0), "draw a three-digit score without shifting the other player's score");

    state.phase = Phase::StageOver;
    renderer.Render(state, {0, 9});
    Check(pixel(500, 330) != ball, "hide the ball after the stage ends");
    Check(pixel(198, 70) == thirdDigitBackground && pixel(1078, 82) == RGB(255, 0, 0),
        "clear old digits when a score shrinks and keep the score visible at stage end");
    DestroyWindow(window);
    std::cout << "PASS: renderer draws the court and updates player, ball and score sprites\n";
}

void InputManagerTest()
{
    InputManager input;
    Check(input.GetKeys() == 0, "initial keys are empty");
    Check(input.OnKeyDown(VK_LEFT) && input.GetKeys() == LEFT, "left key down");
    Check(!input.OnKeyDown(VK_LEFT), "key auto-repeat must not signal another state change");
    Check(input.OnKeyDown('Z') && input.GetKeys() == (LEFT | ACTION), "combined movement and action");
    Check(!input.OnKeyDown(VK_RETURN) && !input.OnKeyDown(VK_DOWN) && !input.OnKeyDown('A'), "chat and unrelated keys must not affect gameplay");
    Check(input.OnKeyUp(VK_LEFT) && input.GetKeys() == ACTION, "release only the matching key bit");
    Check(!input.OnKeyUp(VK_LEFT) && !input.OnKeyUp(VK_RETURN), "repeated and unrelated key-up events are ignored");
    Check(input.OnKeyDown(VK_UP) && input.OnKeyDown(VK_RIGHT), "up and right mappings");
    Check(input.GetKeys() == (UP | RIGHT | ACTION), "all simultaneous key bits are retained");
    input.Reset();
    Check(input.GetKeys() == 0 && !input.OnKeyUp('Z'), "reset clears every key");

    GameWorld world;
    Check(input.OnKeyDown(VK_RIGHT), "press right after reset");
    std::uint8_t keys[2] = {input.GetKeys(), 0};
    world.Update(keys);
    Check(!input.OnKeyDown(VK_RIGHT), "holding right changes position but not the key mask");
    world.Update(keys);
    GameStatePacket state;
    world.FillState(state);
    Check(state.p[0].x == 50 && state.p[1].x == 950, "client key state drives server movement over successive ticks");
    input.OnKeyUp(VK_RIGHT); keys[0] = input.GetKeys();
    world.Update(keys); world.FillState(state);
    Check(state.p[0].x == 50 && state.p[0].action == Action::Idle, "release stops server movement");
    std::cout << "PASS: client key mapping, repeat suppression, combined keys and movement integration\n";
}

void JumpTest()
{
    GameWorld world;
    GameStatePacket state;
    std::uint8_t keys[2] = {UP | RIGHT, 0};
    world.Update(keys);
    world.FillState(state);
    Check(state.p[0].y == 535 && state.p[0].x == 40 && state.p[0].action == Action::Jump,
          "jump starts with the original upward velocity and permits horizontal movement");
    Check(state.p[1].y == 560 && state.p[1].action == Action::Idle, "other player stays grounded");
    Check(state.p[0].frame == 0, "jump starts in the takeoff pose");
    std::array<bool, 5> jumpFrames{true};
    int minimumY = state.p[0].y;
    bool landed = false;
    for (int tick = 1; tick <= 70; ++tick)
    {
        world.Update(keys);
        world.FillState(state);
        minimumY = std::min(minimumY, static_cast<int>(state.p[0].y));
        Check(state.p[0].y <= 560, "jump never goes below the ground");
        if (state.p[0].y == 560) { landed = true; break; }
        Check(state.p[0].frame < jumpFrames.size() && state.p[1].frame == 0,
            "airborne animation stays in range and leaves the other player idle");
        jumpFrames[state.p[0].frame] = true;
    }
    Check(landed && minimumY == 235 && state.p[0].action == Action::Move, "gravity, jump apex and walking landing");
    Check(std::ranges::all_of(jumpFrames, [](bool shown) { return shown; }) && state.p[0].frame == 0,
        "the full jump uses all poses and returns to the first walking frame");
    for (int tick = 0; tick < 5; ++tick) world.Update(keys);
    world.FillState(state);
    Check(state.p[0].y == 560, "holding up across landing must not trigger another jump");
    keys[0] = 0; world.Update(keys);
    keys[0] = UP; world.Update(keys); world.FillState(state);
    Check(state.p[0].y == 535 && state.p[0].frame == 0, "a second jump resets velocity and the takeoff frame");
    keys[1] = UP; world.Update(keys); world.FillState(state);
    Check(state.p[0].y == 511 && state.p[1].y == 535, "players have independent jump velocities");

    PlayerState player;
    player.x = 30; player.y = 560;
    PlayerInput request; request.jump = true;
    for (Action locked : {Action::Slide, Action::Win, Action::Lose})
    {
        player.action = locked;
        int velocity = 0;
        UpdatePlayerJump(player, request, velocity);
        Check(player.y == 560 && velocity == 0 && player.action == locked, "locked action must block jumping");
    }
    player.y = 559; player.action = Action::Jump;
    int velocity = 4;
    UpdatePlayerJump(player, PlayerInput{}, velocity);
    Check(player.y == 560 && velocity == 0 && player.action == Action::Idle, "landing clamps overshoot and clears velocity");
    std::cout << "PASS: jump ascent/gravity/landing, held-key suppression, air movement and independent players\n";
}

void SessionThreadTest()
{
    Check(ClientSession::ClientThread(nullptr) == ERROR_INVALID_PARAMETER, "null thread argument rejected");
    {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        Check(session.Start(), "start a real session worker");
        Check(!session.WaitForFinish(30), "worker must block waiting for a packet");
        InputPacket input; input.keys = RIGHT;
        const char* bytes = reinterpret_cast<const char*>(&input);
        stream.Send(bytes, 1);
        Check(!session.WaitForFinish(30), "fragmented header must not finish the worker");
        NextStagePacket next;
        std::array<char, sizeof(input) - 1 + sizeof(next)> rest{};
        std::memcpy(rest.data(), bytes + 1, sizeof(input) - 1);
        std::memcpy(rest.data() + sizeof(input) - 1, &next, sizeof(next));
        stream.Send(rest.data(), static_cast<int>(rest.size()));
        shutdown(stream.sender.value, SD_SEND);
        Check(session.WaitForFinish(2000), "worker finishes on graceful EOF");
        Check(session.GetLastError() == 0 && session.GetReceivedCount() == 2, "loop receives both frames with separate boundaries");
        char tail;
        Check(recv(stream.sender.value, &tail, 1, 0) == 0, "worker closes the accepted socket on exit");
    }
    auto reject = [](const char* bytes, int length, DWORD expected) {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        Check(session.Start(), "start malformed packet worker");
        stream.Send(bytes, length);
        shutdown(stream.sender.value, SD_SEND);
        Check(session.WaitForFinish(2000), "malformed or truncated packet must end the worker");
        Check(session.GetLastError() == expected && session.GetReceivedCount() == 0, "bad frame must not reach ProcessPacket");
    };
    const PacketHeader tooSmall{2, PacketType::INPUT};
    const PacketHeader tooLarge{MAX_PACKET_SIZE + 1, PacketType::INPUT};
    const PacketHeader unknown{3, static_cast<PacketType>(0xff)};
    const PacketHeader mismatch{3, PacketType::INPUT};
    reject(reinterpret_cast<const char*>(&tooSmall), sizeof(tooSmall), WSAEMSGSIZE);
    reject(reinterpret_cast<const char*>(&tooLarge), sizeof(tooLarge), WSAEMSGSIZE);
    reject(reinterpret_cast<const char*>(&unknown), sizeof(unknown), WSAEINVAL);
    reject(reinterpret_cast<const char*>(&mismatch), sizeof(mismatch), WSAEINVAL);
    InputPacket input;
    reject(reinterpret_cast<const char*>(&input), 1, WSAECONNRESET);
    reject(reinterpret_cast<const char*>(&input), sizeof(PacketHeader), WSAECONNRESET);
    {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        Check(session.Start(), "start cancellable session worker");
        stream.Send(reinterpret_cast<const char*>(&input), 1);
        const auto started = std::chrono::steady_clock::now();
        session.Stop();
        Check(std::chrono::steady_clock::now() - started < std::chrono::seconds(2), "Stop must unblock partial header reception");
        Check(session.GetLastError() == 0 && session.GetReceivedCount() == 0, "local cancellation is a clean exit");
    }
    std::cout << "PASS: session receive loop, TCP framing, malformed/truncated packets, EOF and cancellation\n";
}

void SessionSendFrameIntegrityTest()
{
    {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        Check(!session.SendPacket(nullptr, 3) && !session.SendPacket("x", 0), "invalid send arguments are rejected");
        InputPacket input; input.keys = LEFT | UP;
        Check(session.SendPacket(reinterpret_cast<const char*>(&input), sizeof(input)), "send one packet");
        std::array<unsigned char, sizeof(InputPacket)> bytes{};
        Check(recvn(stream.sender.value, reinterpret_cast<char*>(bytes.data()), sizeof(input), 0) == sizeof(input) &&
              std::memcmp(bytes.data(), &input, sizeof(input)) == 0, "peer receives the same bytes");
    }
    {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        constexpr int perThread = 300;
        constexpr int frameSize = sizeof(GameStatePacket);
        auto writer = [&](char fill, bool& ok) {
            std::array<char, frameSize> frame{};
            PacketHeader header{static_cast<std::uint16_t>(frameSize), PacketType::GAME_STATE};
            std::memcpy(frame.data(), &header, sizeof(header));
            std::memset(frame.data() + sizeof(header), fill, frameSize - sizeof(header));
            ok = true;
            for (int i = 0; i < perThread; ++i) ok = session.SendPacket(frame.data(), frameSize) && ok;
        };
        bool okA = false, okB = false, framesIntact = true;
        int countA = 0, countB = 0;
        std::thread reader([&] {
            std::array<char, frameSize> frame{};
            for (int i = 0; i < perThread * 2; ++i)
            {
                if (recvn(stream.sender.value, frame.data(), frameSize, 0) != frameSize) { framesIntact = false; return; }
                const PacketHeader* header = reinterpret_cast<const PacketHeader*>(frame.data());
                const char fill = frame[sizeof(PacketHeader)];
                bool same = header->size == frameSize && header->type == PacketType::GAME_STATE && (fill == 'A' || fill == 'B');
                for (int j = sizeof(PacketHeader); j < frameSize; ++j) same = same && frame[j] == fill;
                if (!same) { framesIntact = false; return; }
                (fill == 'A' ? countA : countB)++;
            }
        });
        std::thread a(writer, 'A', std::ref(okA));
        std::thread b(writer, 'B', std::ref(okB));
        a.join(); b.join(); reader.join();
        Check(okA && okB, "concurrent sends succeed");
        Check(framesIntact && countA == perThread && countB == perThread, "concurrent sends arrive as whole frames");
    }
    std::cout << "PASS: session send, argument checks and concurrent sends arriving as whole frames\n";
}

void SessionDisconnectTest()
{
    {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        Check(session.Start(), "start session for disconnect");
        Check(!session.WaitForFinish(30), "worker waits for packets");
        const auto started = std::chrono::steady_clock::now();
        session.Disconnect();
        Check(session.WaitForFinish(2000) && std::chrono::steady_clock::now() - started < std::chrono::seconds(2),
              "disconnect from another thread ends the blocked worker");
        Check(session.GetLastError() == 0, "local disconnect is a clean exit");
        char tail;
        const int closeResult = recv(stream.sender.value, &tail, 1, 0);
        const int closeError = closeResult == SOCKET_ERROR ? WSAGetLastError() : 0;
        Check(closeResult == 0, ("peer sees the connection closed; recv=" + std::to_string(closeResult) +
            ", Winsock=" + std::to_string(closeError)).c_str());
        InputPacket input;
        Check(!session.SendPacket(reinterpret_cast<const char*>(&input), sizeof(input)), "send after disconnect fails");
        session.Disconnect();
    }
    for (int round = 0; round < 20; ++round)
    {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        Check(session.Start(), "start session for concurrent disconnect");
        std::thread first([&] { session.Disconnect(); });
        std::thread second([&] { session.Disconnect(); });
        first.join(); second.join();
        Check(session.WaitForFinish(2000) && session.GetLastError() == 0, "simultaneous disconnects close once");
        char tail;
        Check(recv(stream.sender.value, &tail, 1, 0) == 0, "concurrent disconnect sends a clean end of stream");
    }
    {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        Check(session.Start(), "start session for final frame delivery");
        GameStatePacket state; state.tick = 42;
        Check(session.SendPacket(reinterpret_cast<const char*>(&state), sizeof(state)), "send final frame before disconnect");
        session.Disconnect();
        Check(session.WaitForFinish(2000), "final frame session ends");
        GameStatePacket received;
        Check(recvn(stream.sender.value, reinterpret_cast<char*>(&received), sizeof(received), 0) == sizeof(received) &&
            std::memcmp(&state, &received, sizeof(state)) == 0, "queued final frame arrives intact before EOF");
        char tail;
        Check(recv(stream.sender.value, &tail, 1, 0) == 0, "final frame is followed by a clean end of stream");
    }
    {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        Check(session.Start(), "start session for peer close");
        closesocket(std::exchange(stream.sender.value, INVALID_SOCKET));
        Check(session.WaitForFinish(2000), "peer close ends the worker");
        session.Disconnect();
    }
    std::cout << "PASS: disconnect once, from another thread or concurrently, and after peer close\n";
}

void JoinRoomSendTest()
{
    NetworkClient client;
    client.SendJoinRoom(1, "test");
    Check(client.GetLastError() == WSAENOTCONN, "join requires a connected socket");
    Socket listener;
    listener.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Check(listener.value != INVALID_SOCKET, "create join receiver listener");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Check(bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind join receiver");
    Check(listen(listener.value, 1) == 0, "listen for join request");
    int length = sizeof(address);
    Check(getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &length) == 0, "join receiver port");
    Check(client.Connect("127.0.0.1", ntohs(address.sin_port)), "connect join client");
    Socket receiver;
    receiver.value = accept(listener.value, nullptr, nullptr);
    Check(receiver.value != INVALID_SOCKET, "accept join client");
    const DWORD timeout = 2000;
    Check(setsockopt(receiver.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0,
          "join receiver timeout");
    client.SendJoinRoom(0, "test"); Check(client.GetLastError() == WSAEINVAL, "zero room rejected");
    client.SendJoinRoom(-1, "test"); Check(client.GetLastError() == WSAEINVAL, "negative room rejected");
    for (const char* nick : {static_cast<const char*>(nullptr), "", "1234567890123456", "\xff", "\xe3\x81"})
    {
        client.SendJoinRoom(1, nick);
        Check(client.GetLastError() == WSAEINVAL && client.IsConnected(), "invalid nickname sends nothing and preserves transport");
    }
    client.SendJoinRoom(27, "피카츄");
    Check(client.GetLastError() == 0, "send UTF-8 nickname");
    JoinRoomPacket packet;
    Check(recvn(receiver.value, reinterpret_cast<char*>(&packet), sizeof(packet)) == sizeof(packet), "receive complete JOIN_ROOM bytes");
    Check(packet.type == PacketType::JOIN_ROOM && packet.size == 23 && packet.roomNo == 27, "join wire header and room number");
    Check(std::strcmp(packet.nickname, "피카츄") == 0, "UTF-8 nickname is preserved");
    for (int i = 9; i < MAX_NICK; ++i) Check(packet.nickname[i] == '\0', "unused nickname bytes must be zero");
    client.SendJoinRoom(28, "123456789012345");
    Check(client.GetLastError() == 0, "maximum length nickname");
    Check(recvn(receiver.value, reinterpret_cast<char*>(&packet), sizeof(packet)) == sizeof(packet), "second join frame");
    Check(packet.roomNo == 28 && std::strcmp(packet.nickname, "123456789012345") == 0 && packet.nickname[MAX_NICK - 1] == '\0',
          "maximum nickname is NUL terminated");
    client.Disconnect();
    ExpectClientLeave(receiver.value);
    std::cout << "PASS: JOIN_ROOM wire bytes, UTF-8/length validation and disconnected client handling\n";
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
    Check(!world.GetPlayerInput(0).jump, "release and press cannot restart an airborne jump");
    keys[0] = 0;
    for (int tick = 0; tick < 70; ++tick) world.Update(keys);
    keys[0] = UP;
    world.Update(keys);
    Check(world.GetPlayerInput(0).jump, "release and press after landing allows another jump request");
    std::cout << "PASS: move/jump/slide/spike input, press edges, action locks and independent players\n";
}

void SlideTest()
{
    GameWorld world;
    GameStatePacket state;
    std::uint8_t keys[2] = {RIGHT | ACTION, 0};
    world.Update(keys);
    world.FillState(state);
    Check(state.p[0].action == Action::Slide && state.p[0].frame == 0 &&
          state.p[0].x == 44 && state.p[0].y == 557,
          "slide starts on ACTION press with a direction and moves 14px without walking");
    int ticks = 1, minimumY = state.p[0].y;
    while (state.p[0].action == Action::Slide && ticks < 60)
    {
        world.Update(keys);
        world.FillState(state);
        minimumY = std::min(minimumY, static_cast<int>(state.p[0].y));
        ++ticks;
        if (ticks <= GameConst::SlideFrames)
        {
            const int expected = ticks <= 10 ? 0 : ticks <= 24 ? 1 : 2;
            Check(state.p[0].action == Action::Slide && state.p[0].frame == expected,
                "slide advances through the original three poses at the correct ticks");
        }
    }
    Check(ticks == 41 && state.p[0].action == Action::Idle && state.p[0].frame == 0,
        "slide returns to the idle pose on its ending tick");
    Check(state.p[0].x == 30 + 15 * 14 + 9 * 7 && state.p[0].y == 560 && minimumY == 530,
          "original slide distance, 30px lift and landing");
    world.Update(keys);
    world.FillState(state);
    Check(state.p[0].x == 313 && state.p[0].action == Action::Move, "held direction walks again; held ACTION does not slide again");

    GameWorld wall;
    keys[0] = LEFT | ACTION; keys[1] = RIGHT | ACTION;
    for (int tick = 0; tick < 41; ++tick) wall.Update(keys);
    wall.FillState(state);
    Check(state.p[0].x == -12 && state.p[1].x == 992, "wall check happens before each step, as in the original");

    GameWorld locked;
    keys[0] = RIGHT | ACTION; keys[1] = 0;
    locked.Update(keys);
    keys[0] = RIGHT | ACTION | UP;
    for (int tick = 0; tick < 40; ++tick) locked.Update(keys);
    locked.FillState(state);
    Check(state.p[0].y == 560 && state.p[0].action == Action::Idle, "jump pressed during a slide is ignored");
    keys[0] = 0; locked.Update(keys);
    keys[0] = ACTION; locked.Update(keys); locked.FillState(state);
    Check(state.p[0].action != Action::Slide, "ACTION without a direction does not slide");
    keys[0] = UP; locked.Update(keys);
    keys[0] = UP | RIGHT | ACTION; locked.Update(keys); locked.FillState(state);
    Check(state.p[0].action != Action::Slide && state.p[0].y < 560, "no slide while airborne");
    std::cout << "PASS: slide timing/distance/height, wall rule, walk resume and action locks\n";
}

void InputSendTest()
{
    NetworkClient client;
    client.SendInput(LEFT);
    Check(client.GetLastError() == WSAENOTCONN, "input requires a connected socket");
    Socket listener;
    listener.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Check(bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind input receiver");
    Check(listen(listener.value, 1) == 0, "listen for input");
    int length = sizeof(address);
    Check(getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &length) == 0, "input receiver port");
    Check(client.Connect("127.0.0.1", ntohs(address.sin_port)), "connect input client");
    Socket receiver;
    receiver.value = accept(listener.value, nullptr, nullptr);
    Check(receiver.value != INVALID_SOCKET, "accept input client");
    const DWORD timeout = 2000;
    setsockopt(receiver.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    client.SendInput(LEFT | ACTION);
    Check(client.GetLastError() == 0, "send input");
    std::array<unsigned char, 4> bytes{};
    Check(recvn(receiver.value, reinterpret_cast<char*>(bytes.data()), 4) == 4, "receive INPUT frame");
    Check(bytes == std::array<unsigned char, 4>{4, 0, 2, LEFT | ACTION}, "INPUT wire bytes");
    client.SendInput(0x10);
    Check(client.GetLastError() == WSAEINVAL && client.IsConnected(), "unknown key bits are rejected without sending");
    client.SendInput(0);
    Check(recvn(receiver.value, reinterpret_cast<char*>(bytes.data()), 4) == 4 &&
          bytes == std::array<unsigned char, 4>{4, 0, 2, 0}, "all keys released is sent as 0");

    ClientSession session(std::exchange(receiver.value, INVALID_SOCKET));
    Check(session.Start(), "start session for client frames");
    client.SendJoinRoom(7, "pika");
    client.SendInput(RIGHT);
    client.SendInput(RIGHT | UP);
    WaitUntil([&] { return session.GetReceivedCount() == 3; }, "session receives JOIN_ROOM and two INPUT frames");
    client.Disconnect();
    Check(session.WaitForFinish(2000) && session.GetLastError() == 0, "client close ends the session cleanly");
    std::cout << "PASS: INPUT wire bytes, key validation and client-to-session delivery\n";
}

void SessionSendTest()
{
    EventPacket packet;
    ClientSession disconnected;
    Check(!disconnected.SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet)) &&
          disconnected.GetLastError() == WSAENOTCONN, "session send requires a live socket");
    Stream stream;
    const DWORD timeout = 2000;
    setsockopt(stream.sender.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
    Check(session.Start(), "start bidirectional session");
    Check(!session.SendPacket(nullptr, 0) && session.GetLastError() == WSAEINVAL, "null send rejected");
    Check(!session.SendPacket(reinterpret_cast<const char*>(&packet), 2), "short send rejected");
    packet.size = 4;
    Check(!session.SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet)), "mismatched send length rejected");
    packet.size = sizeof(packet);
    packet.type = static_cast<PacketType>(0xff);
    Check(!session.SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet)), "unknown send type rejected");

    std::atomic_bool sent{true};
    auto writer = [&](std::uint8_t who) {
        EventPacket event;
        event.kind = EventKind::Hit; event.who = who;
        for (int i = 0; i < 100; ++i)
            if (!session.SendPacket(reinterpret_cast<const char*>(&event), sizeof(event))) { sent = false; return; }
    };
    std::jthread first(writer, 0), second(writer, 1);
    int count[2]{};
    for (int i = 0; i < 200; ++i)
    {
        EventPacket received;
        Check(recvn(stream.sender.value, reinterpret_cast<char*>(&received), sizeof(received)) == sizeof(received), "receive server frame");
        Check(received.size == sizeof(received) && received.type == PacketType::EVENT &&
              received.kind == EventKind::Hit && received.who < 2, "concurrent server frames must remain intact");
        ++count[received.who];
    }
    first.join(); second.join();
    Check(sent && count[0] == 100 && count[1] == 100, "deliver both concurrent writers without mixing or dropping frames");
    InputPacket input; input.keys = LEFT;
    stream.Send(reinterpret_cast<const char*>(&input), sizeof(input));
    WaitUntil([&] { return session.GetReceivedCount() == 1; }, "receive remains active while sending");
    shutdown(stream.sender.value, SD_SEND);
    Check(session.WaitForFinish(2000) && session.GetLastError() == 0, "sender session exits after peer EOF");
    std::cout << "PASS: server SendPacket validation, 200 concurrent frames and bidirectional traffic\n";
}

void BallGravityTest()
{
    auto close = [](float actual, float expected) { return std::fabs(actual - expected) < 0.001f; };
    GameWorld world;
    GameStatePacket state;
    world.FillState(state);
    Check(state.ball.x == 120 && state.ball.y == 0 && state.ball.vx == 0 && state.ball.vy == 400 && state.ball.spike == 0,
          "initial ball serve state");
    std::uint8_t keys[2]{};
    world.Update(keys); world.FillState(state);
    Check(state.ball.vy == 416 && close(state.ball.y, 15.3088f) && state.ball.x == 120, "one tick applies gravity before movement");
    world.Update(keys); world.FillState(state);
    Check(state.ball.vy == 432 && close(state.ball.y, 31.2064f), "gravity accumulates over ticks");
    world.ResetBall(1); world.FillState(state);
    Check(state.ball.x == 980 && state.ball.y == 0 && state.ball.vy == 400, "reset for right-side serve");
    world.ResetBall(0); world.FillState(state);
    Check(state.ball.x == 120 && state.ball.y == 0 && state.ball.vy == 400, "reset for left-side serve");
    bool rejected = false;
    try { world.ResetBall(2); } catch (const std::out_of_range&) { rejected = true; }
    world.FillState(state);
    Check(rejected && state.ball.x == 120, "invalid serve side does not change the ball");
    BallState ball;
    ball.x = 10; ball.y = 100; ball.vx = 100; ball.vy = -100; ball.spike = 1;
    MoveBallWithGravity(ball);
    Check(close(ball.x, 13.68f) && close(ball.y, 96.9088f) && ball.vy == -84, "horizontal velocity and upward gravity");
    for (int tick = 0; tick < 6; ++tick) MoveBallWithGravity(ball);
    Check(ball.vy == 12, "gravity changes ascent to descent");
    ResetBallForServe(ball, 1);
    Check(ball.x == 980 && ball.y == 0 && ball.vx == 0 && ball.vy == 400 && ball.spike == 0, "serve reset clears old velocity and spike");
    std::cout << "PASS: ball serve reset, gravity, accumulated motion and original time scale\n";
}

void BallWallTest()
{
    GameWorld world;
    auto close = [](float a, float b) { return std::fabs(a - b) < 0.001f; };
    BallState ball{};
    ball.x = -5; ball.y = 100; ball.vx = -200; ball.vy = -300; ball.spike = 1;
    ReflectBallAtWalls(ball);
    Check(ball.x == 0 && close(ball.vx, 180) && ball.vy == -300 && ball.spike == 1,
        "left wall clamps and reflects horizontal speed without changing spike");
    ReflectBallAtWalls(ball);
    Check(close(ball.vx, 180), "left wall does not reflect a departing ball twice");
    ball.x = 1070; ball.vx = 200;
    ReflectBallAtWalls(ball);
    Check(ball.x == 1060 && close(ball.vx, -180), "right wall includes the ball collision size");
    ReflectBallAtWalls(ball);
    Check(close(ball.vx, -180), "right wall does not reflect a departing ball twice");
    ball.x = -2; ball.vx = 50; ball.y = -30; ball.vy = -300;
    ReflectBallAtWalls(ball);
    Check(ball.x == 0 && ball.vx == 50 && ball.y == -25 && ball.vy == 300,
        "ceiling reflects upward motion and left wall preserves inward motion");
    ReflectBallAtWalls(ball);
    Check(ball.vy == 300, "ceiling does not reflect downward motion");
    ball.x = 1060; ball.vx = 200; ball.y = -25; ball.vy = -100;
    ReflectBallAtWalls(ball);
    Check(close(ball.vx, -180) && ball.vy == 100, "exact wall and ceiling corner contact reflects both axes");
    ball = BallState{}; ball.x = 1; ball.y = -24; ball.vx = -200; ball.vy = -300;
    AdvanceWorldBall(world, ball);
    Check(ball.x == 0 && ball.y == -25 && close(ball.vx, 180) && ball.vy == 284,
        "tick resolves wall and ceiling overshoot after applying gravity");
    AdvanceWorldBall(world, ball);
    Check(ball.x > 0 && ball.y > -25 && close(ball.vx, 180) && ball.vy == 300,
        "next tick moves away from the corner without repeated damping");
    ball = BallState{}; ball.x = 100; ball.y = 800; ball.vy = 100;
    AdvanceWorldBall(world, ball);
    Check(ball.y > 800 && ball.vy == 116, "wall reflection does not add a floor bounce");
    std::cout << "PASS: ball wall/ceiling reflection, damping, exact contact and tick integration\n";
}

void BallNetTest()
{
    GameWorld world;
    auto close = [](float a, float b) { return std::fabs(a - b) < 0.001f; };
    auto impact = [](BallState previous, float x, float y) {
        BallState next = previous; next.x = x; next.y = y;
        Check(ReflectBallAtNet(next, previous), "ball path must hit the net");
        return next;
    };
    BallState ball = impact({450, 500, 400, -200, 1}, 470, 490);
    Check(ball.x == 460 && ball.y == 495 && close(ball.vx, -380) && close(ball.vy, -180) && ball.spike == 1,
        "left net face reflects horizontally while preserving ascent and spike");
    ball = impact({580, 500, -400, 200, 0}, 560, 510);
    Check(ball.x == 565 && close(ball.vx, 380) && close(ball.vy, 180), "right net face reflects to the right");
    ball = impact({500, 350, 100, 400, 0}, 505, 370);
    Check(ball.y == 360 && close(ball.x, 502.5f) && close(ball.vx, 95) && close(ball.vy, -360),
        "net top reflects downward motion upward without reversing horizontal direction");
    ball = impact({500, 730, 0, -400, 0}, 500, 700);
    Check(ball.y == 715 && close(ball.vy, 360), "net lower edge uses the original collider inset");
    ball = impact({400, 500, 400, 0, 0}, 650, 500);
    Check(ball.x == 460 && close(ball.vx, -380), "fast left-to-right path cannot tunnel through the net");
    ball = impact({650, 500, -400, 0, 0}, 400, 500);
    Check(ball.x == 565 && close(ball.vx, 380), "fast right-to-left path cannot tunnel through the net");
    ball = impact({500, 250, 0, 400, 0}, 500, 750);
    Check(ball.y == 360 && close(ball.vy, -360), "fast falling path cannot cross the whole net");
    for (float y : {300.0f, 360.0f, 715.0f, 750.0f})
    {
        BallState previous{400, y, 400, 0, 0};
        ball = previous; ball.x = 650;
        Check(!ReflectBallAtNet(ball, previous) && ball.x == 650 && ball.vx == 400,
            "paths above, below or grazing the net are untouched");
    }
    BallState previous{450, 370, 100, -100, 0};
    ball = previous; ball.x = 470; ball.y = 350;
    Check(!ReflectBallAtNet(ball, previous), "single corner graze does not bounce");
    ball = impact({450, 350, 100, 100, 0}, 470, 370);
    Check(ball.x == 460 && ball.y == 360 && close(ball.vy, -90), "diagonal corner entry resolves at first contact");
    ball = impact({450, 500, 400, 0, 0}, 460, 500);
    Check(ball.x == 460 && close(ball.vx, -380), "exact endpoint contact reflects an incoming ball");
    previous = ball; ball.x -= 10;
    Check(!ReflectBallAtNet(ball, previous) && close(ball.vx, -380), "departing side contact does not bounce again");
    previous = {500, 360, 0, -200, 0}; ball = previous; ball.y = 350;
    Check(!ReflectBallAtNet(ball, previous) && ball.vy == -200, "departing top contact does not bounce again");
    previous = {461, 500, -100, 0, 0}; ball = previous;
    Check(ReflectBallAtNet(ball, previous) && ball.x == 460 && ball.vx == -100,
        "existing overlap is corrected without slowing a ball already leaving");
    ball = impact({461, 500, 100, 0, 0}, 462, 500);
    Check(ball.x == 460 && close(ball.vx, -95), "existing inward overlap is corrected and reflected");
    previous = {461, 500, -100, 0, 0}; ball = previous; ball.x = 450;
    Check(!ReflectBallAtNet(ball, previous) && ball.x == 450 && ball.vx == -100,
        "an overlapping ball can leave the net freely");

    ball = {459, 500, 400, -200, 0};
    AdvanceWorldBall(world, ball);
    Check(ball.x == 460 && close(ball.vx, -380) && close(ball.vy, -165.6f),
        "tick applies gravity then reflects at the net side");
    AdvanceWorldBall(world, ball);
    Check(ball.x < 460 && close(ball.vx, -380) && close(ball.vy, -149.6f),
        "following tick leaves the net without repeated reflection");
    ball = {500, 359, 100, 400, 0};
    AdvanceWorldBall(world, ball);
    Check(ball.y == 360 && close(ball.vx, 95) && close(ball.vy, -374.4f), "tick bounces on the net top");
    AdvanceWorldBall(world, ball);
    Check(ball.y < 360 && close(ball.vy, -358.4f), "top bounce rises away on the next tick");
    ball = {400, 500, 40000, -16, 0};
    AdvanceWorldBall(world, ball);
    Check(ball.x == 460 && close(ball.vx, -38000), "net is hit before a distant wall on a fast crossing tick");
    std::cout << "PASS: ball net side/top reflection, swept collision, grazing, overlap recovery and successive ticks\n";
}

void BallPlayerHitTest()
{
    auto close = [](float a, float b) { return std::fabs(a - b) < 0.001f; };
    PlayerState players[2]{};
    players[0].x = 30; players[1].x = 950;
    players[0].y = players[1].y = 560;
    auto hit = [&](BallState previous, float x, float y) {
        BallState next = previous; next.x = x; next.y = y;
        float time = -1;
        Check(ReflectBallAtPlayers(next, previous, players, players, time) && time >= 0 && time <= 1,
            "incoming ball must contact a player head during this tick");
        return next;
    };
    BallState ball = hit({65, 450, 0, 400, 1}, 65, 480);
    Check(ball.x == 65 && ball.y == 470 && close(ball.vx, 450) && ball.vy == -600 && ball.spike == 0,
        "left normal return uses original force and clears an incoming spike");
    ball = hit({985, 450, 0, 400, 0}, 985, 480);
    Check(ball.y == 470 && close(ball.vx, -450) && ball.vy == -600, "right normal return aims toward the other court");
    ball = hit({95, 450, 0, 400, 0}, 95, 480);
    Check(close(ball.vx, 456), "off-center contact adjusts horizontal force");
    players[0].action = Action::Spike;
    ball = hit({65, 450, 0, 400, 0}, 65, 480);
    Check(close(ball.vx, 720) && ball.vy == -800 && ball.spike == 1, "left spike uses original spike force");
    players[1].action = Action::Spike;
    ball = hit({985, 450, 0, 400, 0}, 985, 480);
    Check(close(ball.vx, -720) && ball.vy == -800 && ball.spike == 1, "right spike is mirrored");
    players[0].evil = 1;
    ball = hit({65, 450, 0, 400, 0}, 65, 480);
    Check(close(ball.vx, 1120) && ball.vy == -1000 && ball.spike == 1, "awakened spike uses stronger original force");
    players[0].action = Action::Jump;
    ball = hit({95, 450, 0, 400, 1}, 95, 480);
    Check(close(ball.vx, 612) && ball.vy == -800 && ball.spike == 0, "awakened normal hit and offset do not retain spike state");
    players[0].evil = 0; players[0].action = Action::Slide;
    ball = hit({65, 450, 0, 400, 0}, 65, 480);
    Check(close(ball.vx, 450) && ball.spike == 0, "sliding player can return the ball without a spike");
    players[0].action = players[1].action = Action::Idle;
    ball = hit({65, 100, 0, 20000, 0}, 65, 900);
    Check(ball.y == 470 && ball.vy == -600, "fast ball cannot tunnel through the head band");
    ball = hit({65, 450, 0, 400, 0}, 65, 470);
    Check(ball.y == 470 && ball.vy == -600, "exact head-top contact is a hit");
    ball = hit({65, 480, 0, 400, 0}, 65, 490);
    Check(ball.y == 470, "shallow incoming overlap is recovered above the head");
    auto miss = [&](BallState previous, float x, float y) {
        BallState next = previous; next.x = x; next.y = y;
        float time = -1;
        Check(!ReflectBallAtPlayers(next, previous, players, players, time), "non-head contact must not return the ball");
        Check(next.x == x && next.y == y && next.vx == previous.vx && next.vy == previous.vy && next.spike == previous.spike,
            "a missed head contact leaves the ball unchanged");
    };
    miss({-30, 450, 0, 400, 0}, -30, 480);
    miss({160, 450, 0, 400, 0}, 160, 480);
    miss({65, 600, 0, -400, 0}, 65, 530);
    miss({65, 600, 0, 400, 0}, 65, 650);
    miss({65, 470, 0, -600, 0}, 65, 440);
    miss({65, 480, 0, -600, 0}, 65, 470);
    miss({65, 450, 0, 0, 0}, 65, 450);
    for (Action locked : {Action::Win, Action::Lose})
    {
        players[0].action = locked;
        miss({65, 450, 0, 400, 0}, 65, 480);
    }
    players[0].action = Action::Idle;
    PlayerState previousPlayers[2] = {players[0], players[1]};
    players[0].y = 510;
    BallState previous{65, 440, 0, 0, 0}; ball = previous;
    float time = -1;
    Check(ReflectBallAtPlayers(ball, previous, players, previousPlayers, time) && close(time, 0.6f) && ball.y == 420,
        "rising head catches a stationary ball using relative vertical motion");
    players[0].y = 560; players[0].x = 200; previousPlayers[0].x = 0;
    previous = {120, 450, 0, 400, 0}; ball = previous; ball.y = 490;
    Check(ReflectBallAtPlayers(ball, previous, players, previousPlayers, time) && close(time, 0.5f) && close(ball.vx, 447),
        "moving head is tested at contact time rather than only at its endpoint");

    previousPlayers[0].x = players[0].x = 385;
    previousPlayers[0].y = 440; players[0].y = 458;
    previous = {480, 340, 0, 1000, 0}; ball = previous; ball.y = 378;
    Check(ReflectBallAtPlayers(ball, previous, players, previousPlayers, time) && close(time, 0.5f) && ball.y == 359,
        "a descending head returns the ball at contact height above the net");

    GameWorld world;
    ball = {65, 469, 0, 400, 0};
    AdvanceWorldBall(world, ball);
    Check(ball.y == 470 && close(ball.vx, 450) && ball.vy == -600, "ball tick includes player collision after gravity");
    AdvanceWorldBall(world, ball);
    Check(ball.y < 470 && ball.vy == -584 && close(ball.vx, 450), "following tick leaves the head without hitting twice");
    players[0].y = 560; players[0].action = Action::Spike;
    GameWorldTestAccess::SetPlayer(world, 0, players[0]);
    ball = {350, 400, 8000, 6500, 0};
    AdvanceWorldBall(world, ball);
    Check(ball.x < 460 && ball.y == 470 && ball.vx > 0 && ball.vy == -800 && ball.spike == 1,
        "head before net determines the collision response");
    ball = {350, 300, 8000, 9500, 0};
    AdvanceWorldBall(world, ball);
    Check(ball.x == 460 && close(ball.vx, -7600) && ball.vy > 0 && ball.spike == 0,
        "net before head does not apply a later spike");
    std::cout << "PASS: player head returns/spikes, awakened force, moving heads, fast paths, misses and collision order\n";
}

void BallWallAndHeadTest()
{
    for (int side = 0; side < 2; ++side)
    {
        GameWorld world;
        const float wall = side == 0 ? 0.0f : 1060.0f;
        const float inward = side == 0 ? 1.0f : -1.0f;
        BallState ball{wall + 5 * inward, 460, -450 * inward, 600, 1};
        AdvanceWorldBall(world, ball);
        Check(ball.x == wall && std::fabs(ball.vx - 405 * inward) < 0.001f && ball.vy == 616 && ball.spike == 1,
            "wall reflection takes precedence over a head contact outside the court");
        AdvanceWorldBall(world, ball);
        Check(ball.y == 470 && ball.vx * inward > 0 && ball.vy == -600 && ball.spike == 0,
            "the head can return the ball after the wall bounce");

        ball = {wall + 10 * inward, 469, -450 * inward, 600, 1};
        AdvanceWorldBall(world, ball);
        Check((ball.x - wall) * inward > 0 && ball.y == 470 && ball.vx * inward > 0 && ball.vy == -600 && ball.spike == 0,
            "a head contact still wins when it occurs inside the court before the wall");
    }
    std::cout << "PASS: GameWorld wall and head collision order on both sides of the court\n";
}

void BallPlayerWorldTest()
{
    for (int side = 0; side < 2; ++side)
    {
        GameWorld world;
        world.ResetBall(side);
        GameStatePacket state;
        bool hit = false;
        for (int tick = 0; tick < 60; ++tick)
        {
            world.UpdateBall(); world.FillState(state);
            if (state.ball.vy < 0) { hit = true; break; }
        }
        Check(hit && state.ball.y == 470 && state.ball.vy == -600 && state.ball.spike == 0,
            "GameWorld UpdateBall returns a served ball on either player's head");
        Check(std::fabs(state.ball.vx - (side == 0 ? 461.0f : -451.0f)) < 0.001f,
            "world hit uses the proper side and original off-center force");
    }
    GameWorld world;
    GameStatePacket state;
    std::uint8_t keys[2] = {UP | ACTION, 0};
    world.Update(keys); world.FillState(state);
    Check(state.p[0].action == Action::Spike && state.p[0].y == 535, "jump plus action starts a spike");
    keys[0] = 0;
    bool hit = false;
    for (int tick = 0; tick < 40; ++tick)
    {
        world.Update(keys); world.FillState(state);
        if (state.ball.spike) { hit = true; break; }
        Check(state.p[0].action == Action::Spike, "spike intent persists after key release while airborne");
    }
    Check(hit && state.ball.vy == -800 && std::fabs(state.ball.vx - 731) < 0.001f,
        "input, jumping player motion and server ball tick produce an actual spike");
    for (int tick = 0; tick < 60 && state.p[0].y != 560; ++tick)
    {
        world.Update(keys); world.FillState(state);
    }
    Check(state.p[0].y == 560 && state.p[0].action == Action::Idle, "landing ends spike action");
    keys[0] = ACTION; world.Update(keys); world.FillState(state);
    Check(state.p[0].action == Action::Idle, "ground action without a direction does not become a spike");
    std::cout << "PASS: GameWorld serve-to-head hits, jump/spike input, release persistence and landing cleanup\n";
}

void StageResetTest()
{
    for (int side = 0; side < GameConst::PlayerCount; ++side)
    {
        GameWorld world;
        const std::uint8_t active[2] = {UP | ACTION, RIGHT | ACTION};
        world.Update(active);
        world.Update(active);
        world.Reset(1, side);
        GameStatePacket state;
        world.FillState(state);
        Check(state.size == sizeof(GameStatePacket) && state.type == PacketType::GAME_STATE &&
            state.phase == Phase::Ready && state.tick == 0, "reset prepares a fresh ready-state packet");
        Check(GameWorldTestAccess::StageIsFresh(world, 1, side),
            "reset records stage/serve and clears scores and pending events");
        for (int i = 0; i < GameConst::PlayerCount; ++i)
        {
            Check(state.p[i].x == GameConst::PlayerStartX[i] && state.p[i].y == GameConst::PlayerGroundY &&
                state.p[i].action == Action::Idle && state.p[i].frame == 0 && state.p[i].evil == 0,
                "first-stage reset restores both players and clears animation and awakening");
            const PlayerInput& input = world.GetPlayerInput(i);
            Check(input.held == 0 && input.pressed == 0 && input.released == 0 &&
                input.moveDirection == 0 && !input.jump && !input.spike && input.slideDirection == 0 &&
                input.action == Action::Idle, "reset clears pending player intentions");
        }
        Check(state.ball.x == GameConst::ServeX[side] && state.ball.y == GameConst::ServeY &&
            state.ball.vx == 0 && state.ball.vy == GameConst::ServeVelocityY && state.ball.spike == 0,
            "reset selects either serve and clears old ball velocity/spike");
        world.Update(active);
        world.FillState(state);
        Check(state.p[0].action == Action::Spike && state.p[0].y == 535 &&
            state.p[1].action == Action::Slide && state.p[1].x == 964 && state.p[1].y == 557,
            "same held keys immediately after reset count as fresh jump/spike and slide requests");
        world.Reset(1, side);
        const std::uint8_t idle[2]{};
        world.Update(idle);
        world.FillState(state);
        Check(state.p[0].x == 30 && state.p[1].x == 950 && state.p[0].y == 560 && state.p[1].y == 560 &&
            state.p[0].action == Action::Idle && state.p[1].action == Action::Idle,
            "old jumping and sliding do not continue after reset");
    }

    for (int loser = 0; loser < GameConst::PlayerCount; ++loser)
    {
        for (int side = 0; side < GameConst::PlayerCount; ++side)
        {
            GameWorld world;
            const std::uint8_t active[2] = {UP | ACTION, LEFT | ACTION};
            world.Update(active);
            GameWorldTestAccess::FinishFirstStage(world, loser);
            world.Reset(2, side);
            GameStatePacket state;
            world.FillState(state);
            Check(GameWorldTestAccess::StageIsFresh(world, 2, side) &&
                state.tick == 0 && state.phase == Phase::Ready,
                "stage transition clears old score, events, tick and phase");
            Check(state.p[loser].evil == 1 && state.p[1 - loser].evil == 0 &&
                state.p[0].action == Action::Idle && state.p[1].action == Action::Idle &&
                state.p[0].frame == 0 && state.p[1].frame == 0,
                "only the first-stage loser awakens, independent of serve side");
            const std::uint8_t move[2] = {RIGHT, RIGHT};
            world.Update(move);
            world.FillState(state);
            for (int i = 0; i < GameConst::PlayerCount; ++i)
                Check(state.p[i].x == GameConst::PlayerStartX[i] +
                    (i == loser ? GameConst::AwakenedMoveSpeed : GameConst::MoveSpeed) && state.p[i].y == 560,
                    "reset awakening feeds existing movement without stale jumping or sliding");
            world.Reset(2, side);
            world.FillState(state);
            Check(state.p[loser].evil == 1 && state.p[1 - loser].evil == 0 &&
                GameWorldTestAccess::StageIsFresh(world, 2, side),
                "repeating a second-stage reset preserves its first-stage loser");
            world.Reset(1, 1 - side);
            world.FillState(state);
            Check(state.p[0].evil == 0 && state.p[1].evil == 0 &&
                GameWorldTestAccess::StageIsFresh(world, 1, 1 - side),
                "a new first stage removes awakening from the previous match");
        }
    }
    GameWorld noResult;
    noResult.Reset(2, 0);
    GameStatePacket state;
    noResult.FillState(state);
    Check(state.p[0].evil == 0 && state.p[1].evil == 0,
        "a tied first-stage score does not invent an awakened loser");

    const int invalid[][2] = {{0, 0}, {-1, 0}, {3, 0}, {1, -1}, {1, 2}, {2, 2}};
    for (const auto& args : invalid)
    {
        GameWorld world;
        const std::uint8_t keys[2] = {UP | ACTION, LEFT | ACTION};
        world.Update(keys);
        GameWorld control = world;
        GameStatePacket before, after;
        world.FillState(before);
        bool rejected = false;
        try { world.Reset(args[0], args[1]); }
        catch (const std::out_of_range&) { rejected = true; }
        world.FillState(after);
        Check(rejected && std::memcmp(&before, &after, sizeof(before)) == 0,
            "invalid stage or serve is rejected before changing game state");
        world.Update(keys); control.Update(keys);
        world.FillState(after); control.FillState(before);
        Check(std::memcmp(&before, &after, sizeof(before)) == 0,
            "a rejected reset also preserves input history and movement counters");
    }
    std::cout << "PASS: stage/serve reset, score/event cleanup, loser awakening, restart and invalid arguments\n";
}

void GameWorldTickTest()
{
    GameWorld world;
    world.Reset(1, 1);
    GameStatePacket before, after;
    world.FillState(before);
    bool rejected = false;
    try { world.Update(nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    world.FillState(after);
    Check(rejected && std::memcmp(&before, &after, sizeof(before)) == 0,
        "invalid input must not start or advance a tick");

    const std::uint8_t keys[2] = {RIGHT | UP, LEFT | ACTION};
    world.Update(keys);
    world.FillState(after);
    Check(after.phase == Phase::Playing && after.tick == 1 &&
        after.p[0].x == 40 && after.p[0].y == 535 && after.p[0].action == Action::Jump &&
        after.p[1].x == 936 && after.p[1].y == 557 && after.p[1].action == Action::Slide &&
        after.ball.x == 980 && after.ball.vy == 416 && std::fabs(after.ball.y - 15.3088f) < 0.001f,
        "the first playing tick advances both players and the served ball once");
    world.Update(keys);
    world.FillState(after);
    Check(after.phase == Phase::Playing && after.tick == 2 &&
        after.p[0].x == 50 && after.p[0].y == 511 && after.p[1].x == 922 && after.p[1].y == 554 &&
        after.ball.vy == 432 && std::fabs(after.ball.y - 31.2064f) < 0.001f,
        "consecutive ticks continue the jump and slide without restarting them");

    const std::uint8_t idle[2]{};
    for (Phase phase : {Phase::ServeWait, Phase::StageOver, Phase::GameOver})
    {
        GameWorldTestAccess::SetPhase(world, phase);
        world.FillState(before);
        world.Update(idle);
        world.FillState(after);
        Check(std::memcmp(&before, &after, sizeof(before)) == 0 &&
            world.GetPlayerInput(0).held == keys[0] && world.GetPlayerInput(1).held == keys[1],
            "paused or finished play must preserve the world and input history");
    }
    world.Reset(2, 0);
    world.Update(idle);
    world.FillState(after);
    Check(after.phase == Phase::Playing && after.tick == 1 && after.ball.x == 120,
        "a new stage restarts tick numbering with its own serve");
    std::cout << "PASS: GameWorld tick order, ready-to-playing transition, paused phases and stage restart\n";
}

void GameWorldEventsTest()
{
    GameWorld world;
    Check(world.PopEvents().empty(), "new world has no pending events");

    const std::uint8_t keys[2] = {UP | ACTION, LEFT | ACTION};
    world.Update(keys);
    world.Update(keys);
    const auto actions = world.PopEvents();
    Check(actions.size() == 3 &&
        actions[0].kind == EventKind::Slide && actions[0].who == 1 &&
        actions[1].kind == EventKind::Jump && actions[1].who == 0 &&
        actions[2].kind == EventKind::Spike && actions[2].who == 0,
        "action starts are drained in tick order without repeats from held keys");
    Check(world.PopEvents().empty(), "popping twice does not return the same events");

    world.Reset(1, 0);
    const std::uint8_t idle[2]{};
    world.Update(idle);
    GameWorldTestAccess::SetBall(world, {985, 465, 0, 300, 0});
    world.Update(idle);
    GameWorldTestAccess::SetBall(world, {650, 634, 0, 300, 0});
    world.Update(idle);
    const auto rally = world.PopEvents();
    Check(rally.size() == 2 &&
        rally[0].kind == EventKind::Hit && rally[0].who == 1 &&
        rally[1].kind == EventKind::Score && rally[1].who == 0,
        "head contact and floor score preserve their player numbers and order across ticks");
    Check(world.PopEvents().empty(), "events from the last rally have been drained");
    Check(actions.size() == 3 && actions[2].kind == EventKind::Spike,
        "later updates and resets do not change an earlier event batch");

    GameWorldTestAccess::SetBall(world, {350, 300, 8000, 9500, 0});
    PlayerState nearNet;
    nearNet.x = GameConst::PlayerMaxX[0];
    nearNet.y = GameConst::PlayerGroundY;
    GameWorldTestAccess::SetPlayer(world, 0, nearNet);
    world.Update(idle);
    Check(world.PopEvents().empty(), "net bounce suppresses the later candidate head contact");

    world.Update(keys);
    world.Reset(1, 0);
    Check(world.PopEvents().empty(), "reset discards pending events");
    std::cout << "PASS: game events for action starts, accepted hits and scores are drained once\n";
}

void GameWorldScoreTest()
{
    GameWorld world;
    const std::uint8_t idle[2]{};
    GameStatePacket state;
    const BallState onLeftFloor{450, 635, 0, 500, 1};
    GameWorldTestAccess::SetBall(world, onLeftFloor);
    world.CheckScore();
    Check(GameWorldTestAccess::Events(world).empty(), "ready state does not score");

    world.Reset(1, 0);
    world.Update(idle);
    GameWorldTestAccess::SetBall(world, {450, 634, 0, 0, 0});
    world.CheckScore();
    Check(GameWorldTestAccess::Events(world).empty(), "ball above the floor does not score");
    GameWorldTestAccess::SetBall(world, onLeftFloor);
    world.CheckScore();
    world.CheckScore();
    world.FillState(state);
    const auto& events = GameWorldTestAccess::Events(world);
    Check(GameWorldTestAccess::Score(world, 0) == 0 && GameWorldTestAccess::Score(world, 1) == 1 &&
        events.size() == 1 && events.back().kind == EventKind::Score && events.back().who == 1,
        "exact left floor contact awards one point to the right player");
    Check(state.phase == Phase::Playing && state.ball.x == 980 && state.ball.y == 0 &&
        state.ball.vx == 0 && state.ball.vy == 400 && state.ball.spike == 0,
        "the right scorer serves the next ball with fresh velocity");

    GameWorld unfinished = world;
    unfinished.Reset(2, 0);
    GameStatePacket nextStage;
    unfinished.FillState(nextStage);
    Check(nextStage.p[0].evil == 0 && nextStage.p[1].evil == 0,
        "an unfinished first-stage lead cannot awaken a loser");

    GameWorldTestAccess::SetBall(world, {565, 634, 0, 500, 0});
    world.Update(idle);
    world.FillState(state);
    Check(GameWorldTestAccess::Score(world, 0) == 1 && GameWorldTestAccess::Score(world, 1) == 1 &&
        events.size() == 2 && events.back().who == 0 && state.ball.x == 120 && state.ball.y == 0,
        "a ball landing next to the right net edge scores through the normal tick");

    for (int firstLoser = 0; firstLoser < GameConst::PlayerCount; ++firstLoser)
    {
        for (int stage = 1; stage <= GameConst::StageCount; ++stage)
        {
            world.Reset(stage, 0);
            world.FillState(state);
            Check(!world.IsStageOver() && world.GetLoser() == -1, "reset clears the stage result");
            if (stage == 2)
            {
                Check(state.p[firstLoser].evil == 1 && state.p[1 - firstLoser].evil == 0,
                    "the actual first-stage loser awakens at the next stage");
            }
            world.Update(idle);
            // Swap winners in stage two to verify that the result comes from its own score.
            const int loser = stage == 1 ? firstLoser : 1 - firstLoser;
            const BallState landed{loser == 0 ? 450.0f : 600.0f, 635, 0, 500, 1};
            for (int point = 0; point < GameConst::WinningScore; ++point)
            {
                Check(!world.IsStageOver() && world.GetLoser() == -1,
                    "no stage result before the fifth point");
                GameWorldTestAccess::SetBall(world, landed);
                world.CheckScore();
            }
            world.FillState(state);
            Check(world.IsStageOver() && world.GetLoser() == loser &&
                state.phase == (stage == 1 ? Phase::StageOver : Phase::GameOver) &&
                state.ball.y == 635 && state.ball.vx == 0 && state.ball.vy == 0 && state.ball.spike == 0,
                "the fifth point ends play and identifies the losing player");
            world.CheckScore();
            world.Update(idle);
            GameStatePacket stopped;
            world.FillState(stopped);
            Check(GameWorldTestAccess::Score(world, 1 - loser) == 5 && events.size() == 5 &&
                std::memcmp(&state, &stopped, sizeof(state)) == 0,
                "finished play cannot advance or award duplicate points");
        }
    }
    std::cout << "PASS: floor scoring, five-point stage results, loser awakening and reset\n";
}

void ClientControlPacketTest()
{
    Listener listener;
    NetworkClient client;
    Check(!client.SendNextStage() && client.GetLastError() == WSAENOTCONN,
        "NEXT_STAGE requires a connected socket");
    for (int cycle = 0; cycle < 6; ++cycle)
    {
        Check(client.Connect("127.0.0.1", listener.port), "reconnect control-packet client");
        Socket peer; peer.value = listener.Accept();
        const DWORD timeout = 2000;
        setsockopt(peer.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        Check(client.SendNextStage() && client.GetLastError() == 0, "send NEXT_STAGE through the shared send path");
        std::array<unsigned char, 3> ready{};
        Check(recvn(peer.value, reinterpret_cast<char*>(ready.data()), static_cast<int>(ready.size())) == static_cast<int>(ready.size()) &&
            ready == std::array<unsigned char, 3>{3, 0, 3}, "NEXT_STAGE has the exact three-byte wire format");
        client.SendInput(LEFT);
        InputPacket input;
        Check(recvn(peer.value, reinterpret_cast<char*>(&input), sizeof(input)) == sizeof(input) && input.keys == LEFT,
            "sending readiness keeps the connection usable");
        if (cycle % 2)
        {
            const ScorePacket partial;
            SendBytes(peer.value, reinterpret_cast<const char*>(&partial), 1);
        }
        else
        {
            const std::array<GameStatePacket, 3> queued{};
            SendBytes(peer.value, reinterpret_cast<const char*>(queued.data()), static_cast<int>(sizeof(queued)));
        }
        const auto started = std::chrono::steady_clock::now();
        client.Disconnect();
        client.Disconnect();
        Check(std::chrono::steady_clock::now() - started < std::chrono::seconds(2) &&
            !client.IsConnected() && !client.WaitForReceiveEnd(0), "disconnect reaps idle/partial reception promptly");
        if (cycle % 2 == 0) Check(client.GetReceivedCount() == 3, "in-flight game states are drained before socket closure");
        ExpectClientLeave(peer.value);
        Check(!client.SendNextStage() && client.GetLastError() == WSAENOTCONN,
            "NEXT_STAGE after disconnect cannot append another frame");
    }
    Socket destructorPeer;
    {
        NetworkClient temporary;
        Check(temporary.Connect("127.0.0.1", listener.port), "connect a client for automatic cleanup");
        destructorPeer.value = listener.Accept();
        const DWORD timeout = 2000;
        setsockopt(destructorPeer.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    }
    ExpectClientLeave(destructorPeer.value);

    Check(client.Connect("127.0.0.1", listener.port), "connect before remote EOF");
    Socket closed; closed.value = listener.Accept();
    shutdown(closed.value, SD_SEND);
    Check(client.WaitForReceiveEnd(2000) && client.GetLastError() == 0, "remote EOF ends reception first");
    client.Disconnect();
    char tail;
    Check(recv(closed.value, &tail, 1, 0) == 0 && client.GetLastError() == 0,
        "cleanup after remote EOF stays clean without sending to a closed connection");
    std::cout << "PASS: NEXT_STAGE bytes, final LEAVE before EOF, repeated/destructor cleanup, partial receive and reconnect\n";
}

void ClientLeaveRoomTest()
{
    RoomManager rooms;
    Listener listener;
    NetworkClient client;
    Check(client.Connect("127.0.0.1", listener.port), "connect before leaving a waiting room");
    ClientSession session(listener.Accept(), &rooms);
    Check(session.Start() && client.SendJoinRoom(26, "leaver"), "join a real server-side waiting room");
    WaitUntil([&] { return session.GetReceivedCount() == 1 && rooms.GetRoomCount() == 1; }, "the waiting room is created");
    client.Disconnect();
    Check(session.WaitForFinish(2000) && session.GetReceivedCount() == 2 && session.GetLastError() == 0 &&
        rooms.GetRoomCount() == 0 && !session.IsInRoom(),
        "LEAVE is dispatched before EOF and removes the empty waiting room");
    std::cout << "PASS: client Disconnect sends LEAVE to the real session and removes its waiting room\n";
}

void ClientReceiveTest()
{
    Check(NetworkClient::RecvThread(nullptr) == ERROR_INVALID_PARAMETER, "null receive thread argument rejected");
    Listener listener;
    NetworkClient client;
    Check(client.Connect("127.0.0.1", listener.port), "connect receiving client");
    Socket server; server.value = listener.Accept();
    Check(!client.WaitForReceiveEnd(30), "client receiver waits for data");
    GameStartPacket start;
    GameStatePacket state; state.tick = 24;
    ScorePacket score; score.scorer = 1; score.score[1] = 3;
    const char* bytes = reinterpret_cast<const char*>(&start);
    SendBytes(server.value, bytes, 1);
    Check(!client.WaitForReceiveEnd(30), "partial server header must not finish receive thread");
    std::array<char, sizeof(start) - 1 + sizeof(state) + sizeof(score)> rest{};
    std::memcpy(rest.data(), bytes + 1, sizeof(start) - 1);
    std::memcpy(rest.data() + sizeof(start) - 1, &state, sizeof(state));
    std::memcpy(rest.data() + sizeof(start) - 1 + sizeof(state), &score, sizeof(score));
    SendBytes(server.value, rest.data(), static_cast<int>(rest.size()));
    WaitUntil([&] { return client.GetReceivedCount() == 3; }, "receive fragmented and coalesced server packets");
    shutdown(server.value, SD_SEND);
    Check(client.WaitForReceiveEnd(2000) && !client.IsConnected() && client.GetLastError() == 0, "server EOF closes client receive thread cleanly");

    auto reject = [&](const char* data, int length, int expected) {
        Check(client.Connect("127.0.0.1", listener.port), "reconnect after receive end");
        Socket peer; peer.value = listener.Accept();
        SendBytes(peer.value, data, length);
        shutdown(peer.value, SD_SEND);
        Check(client.WaitForReceiveEnd(2000) && !client.IsConnected(), "bad packet ends reception");
        Check(client.GetLastError() == expected && client.GetReceivedCount() == 0, "bad server frame must not be dispatched");
    };
    const PacketHeader tooSmall{2, PacketType::SCORE};
    const PacketHeader tooLarge{MAX_PACKET_SIZE + 1, PacketType::SCORE};
    const PacketHeader unknown{3, static_cast<PacketType>(0xff)};
    const PacketHeader mismatch{3, PacketType::SCORE};
    reject(reinterpret_cast<const char*>(&tooSmall), sizeof(tooSmall), WSAEMSGSIZE);
    reject(reinterpret_cast<const char*>(&tooLarge), sizeof(tooLarge), WSAEMSGSIZE);
    reject(reinterpret_cast<const char*>(&unknown), sizeof(unknown), WSAEINVAL);
    reject(reinterpret_cast<const char*>(&mismatch), sizeof(mismatch), WSAEINVAL);
    reject(reinterpret_cast<const char*>(&score), 1, WSAECONNRESET);
    reject(reinterpret_cast<const char*>(&score), sizeof(PacketHeader), WSAECONNRESET);
    client.Disconnect();

    DWORD before = 0, after = 0;
    Check(GetProcessHandleCount(GetCurrentProcess(), &before) != 0, "client handle baseline");
    for (int cycle = 0; cycle < 8; ++cycle)
    {
        Check(client.Connect("127.0.0.1", listener.port) && client.GetReceivedCount() == 0, "fresh receive connection state");
        Socket peer; peer.value = listener.Accept();
        SendBytes(peer.value, reinterpret_cast<const char*>(&score), 1);
        const auto started = std::chrono::steady_clock::now();
        client.Disconnect();
        Check(std::chrono::steady_clock::now() - started < std::chrono::seconds(2), "disconnect cancels a partial receive without hanging");
        Check(!client.IsConnected() && client.GetLastError() == 0, "cancelled reception preserves clean status");
        client.Disconnect();
    }
    Check(GetProcessHandleCount(GetCurrentProcess(), &after) != 0 && after <= before + 2, "reconnect must not leak receive-thread/socket handles");
    std::cout << "PASS: client receive framing/errors, EOF, partial-receive cancellation and reconnect cleanup\n";
}

bool TakeNetEvent(HWND window, PacketType expected, std::vector<char>& packet)
{
    MSG message{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!PeekMessage(&message, window, WM_NET_EVENT, WM_NET_EVENT, PM_REMOVE))
    {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    char* data = reinterpret_cast<char*>(message.lParam);
    if (data == nullptr) return false;
    PacketHeader header{};
    std::memcpy(&header, data, sizeof(header));
    packet.assign(data, data + header.size);
    delete[] data;
    return message.wParam == static_cast<WPARAM>(expected) && header.type == expected;
}

bool HasNetEvent(HWND window)
{
    MSG message{};
    return PeekMessage(&message, window, WM_NET_EVENT, WM_NET_EVENT, PM_NOREMOVE) != FALSE;
}

void GameClientTimerTest()
{
    struct Window
    {
        HWND handle = CreateWindowExW(0, L"STATIC", nullptr, WS_POPUP,
            0, 0, 580, 415, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        ~Window() { DestroyWindow(handle); }
    } window;
    Check(window.handle != nullptr, "create a game client rendering window");
    Listener listener;
    GameClient client;
    client.SetWindow(window.handle);
    Check(!client.OnTimer() && !client.StartRendering(window.handle), "the connect scene does not render gameplay");
    Check(client.ConnectAndJoin(L"127.0.0.1", L"18", L"timer", listener.port) == JoinRequestResult::Sent,
        "connect game client for timer rendering");
    Socket peer; peer.value = listener.Accept();

    const auto receive = [&](PacketType type)
    {
        std::vector<char> packet;
        Check(TakeNetEvent(window.handle, type, packet), "receive rendering scene event");
        return client.OnNetEvent(static_cast<WPARAM>(type), reinterpret_cast<LPARAM>(packet.data()));
    };
    JoinResultPacket reply; reply.result = JoinResult::Matched; reply.myIndex = 0;
    GameStartPacket start;
    SendBytes(peer.value, reinterpret_cast<const char*>(&reply), sizeof(reply));
    SendBytes(peer.value, reinterpret_cast<const char*>(&start), sizeof(start));
    Check(receive(PacketType::JOIN_RESULT) == NetEventResult::Matched &&
        receive(PacketType::GAME_START) == NetEventResult::GameStarted, "start the rendering scene from server replies");
    Check(!client.StartRendering(nullptr) && client.StartRendering(window.handle), "initialize renderer and frame timer");
    Check(!client.OnTimer(), "do not render default coordinates before the first GAME_STATE");

    const auto frame = [&]
    {
        bool painted = false;
        WaitUntil([&]
        {
            MSG message{};
            if (PeekMessage(&message, window.handle, WM_TIMER, WM_TIMER, PM_REMOVE) &&
                message.wParam == GameConst::FrameTimerId)
                painted = client.OnTimer();
            return painted;
        }, "the Win32 frame timer delivers a renderable frame");
    };
    GameStatePacket state;
    state.phase = Phase::Playing;
    state.p[0].x = 100; state.p[0].y = 520;
    state.p[1].x = 900; state.p[1].y = 520;
    state.ball.x = 380; state.ball.y = 280;
    SendBytes(peer.value, reinterpret_cast<const char*>(&state), sizeof(state));
    frame();
    const auto pixel = [&](int x, int y) { return GameClientTestAccess::Pixel(client, x, y); };
    const COLORREF ball = RGB(255, 247, 247);
    Check(pixel(430, 330) == ball && pixel(159, 627) == RGB(159, 0, 0),
        "the timer renders the first received state, including valid tick zero");
    const COLORREF player = pixel(185, 605);

    state.tick = 1; state.ball.x = 450; state.p[0].x = 250;
    SendBytes(peer.value, reinterpret_cast<const char*>(&state), sizeof(state));
    WaitUntil([&]
    {
        frame();
        return pixel(500, 330) == ball;
    }, "a later server snapshot reaches the timer renderer");
    Check(pixel(430, 330) != ball && pixel(185, 605) != player && pixel(309, 627) == RGB(159, 0, 0),
        "redraw the latest player and ball positions without stale sprites");
    Check(!HasNetEvent(window.handle), "GAME_STATE does not flood the UI with posted events");

    ScorePacket score; score.scorer = 1; score.score[0] = 2; score.score[1] = 3;
    const auto sendScore = [&]
    {
        SendBytes(peer.value, reinterpret_cast<const char*>(&score), sizeof(score));
        return receive(PacketType::SCORE);
    };
    Check(sendScore() == NetEventResult::ScoreUpdated &&
        GameClientTestAccess::Scores(client) == std::array<std::uint8_t, 2>{2, 3},
        "SCORE replaces both totals with the server's values");
    Check(sendScore() == NetEventResult::ScoreUpdated &&
        GameClientTestAccess::Scores(client) == std::array<std::uint8_t, 2>{2, 3},
        "a repeated SCORE does not add points twice");
    score.scorer = NO_PLAYER;
    Check(sendScore() == NetEventResult::Ignored, "ignore a SCORE with an invalid scorer");
    score.scorer = 0; score.score[0] = GameConst::WinningScore + 1;
    Check(sendScore() == NetEventResult::Ignored &&
        GameClientTestAccess::Scores(client) == std::array<std::uint8_t, 2>{2, 3},
        "invalid score totals do not replace the last valid scoreboard");
    frame();
    {
        Renderer reference;
        Check(reference.Initialize(window.handle), "initialize a reference scoreboard");
        reference.Render(state, {2, 3});
        for (const int edge : {50, GameConst::WindowWidth - 50 - 64})
            for (int y = 50; y < 114; y += 4)
                for (int x = edge; x < edge + 64; x += 4)
                    Check(pixel(x, y) == RendererTestAccess::Pixel(reference, x, y),
                        "the timer displays both received scores using the correct digit sprites");
    }

    const auto resultPixels = [&](int side)
    {
        std::vector<COLORREF> pixels;
        for (int y = GameConst::PlayerGroundY; y < GameConst::PlayerGroundY + 170; y += 3)
            for (int x = GameConst::PlayerStartX[side]; x < GameConst::PlayerStartX[side] + 170; x += 3)
                pixels.push_back(pixel(x, y));
        return pixels;
    };
    const auto finished = [&](Phase phase, int winner)
    {
        state.phase = phase;
        state.p[winner].action = Action::Win;
        state.p[1 - winner].action = Action::Lose;
        SendBytes(peer.value, reinterpret_cast<const char*>(&state), sizeof(state));
        WaitUntil([&]
        {
            frame();
            return pixel(500, 330) != ball;
        }, "render the final server snapshot without the ball");
        const auto firstWinner = resultPixels(winner);
        const auto firstLoser = resultPixels(1 - winner);
        WaitUntil([&]
        {
            frame();
            return resultPixels(winner) != firstWinner && resultPixels(1 - winner) != firstLoser;
        }, "the timer animates both result poses without receiving another server snapshot");
    };
    finished(Phase::StageOver, 0);

    client.ChangeScene(Scene::StageOver);
    start.stage = 2;
    SendBytes(peer.value, reinterpret_cast<const char*>(&start), sizeof(start));
    Check(receive(PacketType::GAME_START) == NetEventResult::GameStarted &&
        GameClientTestAccess::Scores(client) == std::array<std::uint8_t, 2>{0, 0},
        "a new stage starts with a fresh scoreboard");
    Check(client.StartRendering(window.handle), "restart the rendering timer for the new stage");
    score.score[0] = 1; score.score[1] = 0;
    Check(sendScore() == NetEventResult::ScoreUpdated, "receive a score in the new stage");
    state.phase = Phase::Playing;
    state.p[0].action = state.p[1].action = Action::Idle;
    SendBytes(peer.value, reinterpret_cast<const char*>(&state), sizeof(state));
    WaitUntil([&] { frame(); return pixel(500, 330) == ball; }, "the new stage renders gameplay again");
    finished(Phase::GameOver, 1);
    client.Disconnect();
    Check(!client.OnTimer() && client.GetScene() == Scene::Connect &&
        GameClientTestAccess::Scores(client) == std::array<std::uint8_t, 2>{0, 0},
        "disconnect stops online rendering and clears the scoreboard");
    MSG message{};
    while (PeekMessage(&message, window.handle, WM_TIMER, WM_TIMER, PM_REMOVE)) {}
    std::this_thread::sleep_for(std::chrono::milliseconds(GameConst::TickMs * 3));
    Check(!PeekMessage(&message, window.handle, WM_TIMER, WM_TIMER, PM_REMOVE), "disconnect cancels the frame timer");

    Check(client.ConnectAndJoin(L"127.0.0.1", L"18", L"retry", listener.port) == JoinRequestResult::Sent,
        "reconnect the same rendering client");
    Socket retry; retry.value = listener.Accept();
    SendBytes(retry.value, reinterpret_cast<const char*>(&score), sizeof(score));
    Check(receive(PacketType::SCORE) == NetEventResult::Ignored &&
        GameClientTestAccess::Scores(client) == std::array<std::uint8_t, 2>{0, 0},
        "a score received before joining a game does not change the scoreboard");
    client.ChangeScene(Scene::Playing);
    Check(client.StartRendering(window.handle) && !client.OnTimer(), "reconnect does not render the previous connection's state");
    client.ChangeScene(Scene::Waiting);
    Check(!client.OnTimer(), "leaving the playing scene stops rendering");
    std::cout << "PASS: frame timer renders latest TCP state, clears old positions, stops and resets on reconnect\n";
    std::cout << "PASS: SCORE updates both rendered totals without double counting, validates and resets between stages/connections\n";
    std::cout << "PASS: client animates win and loss from final stage and game snapshots\n";
}

struct HiddenChatPlayer
{
    HWND window = CreateWindowExW(0, L"STATIC", nullptr, WS_POPUP | WS_CLIPCHILDREN,
        0, 0, GameConst::WindowWidth, GameConst::WindowHeight, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    GameClient client;
    ChatPanel panel;
    HiddenChatPlayer()
    {
        Check(window != nullptr && panel.Create(window, GetModuleHandleW(nullptr)), "create hidden native chat controls");
        client.SetWindow(window);
    }
    ~HiddenChatPlayer()
    {
        client.Disconnect();
        panel.Destroy();
        MSG message{};
        while (PeekMessage(&message, window, WM_NET_EVENT, WM_NET_EVENT, PM_REMOVE))
            delete[] reinterpret_cast<char*>(message.lParam);
        DestroyWindow(window);
    }
    NetEventResult Receive(PacketType expected)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline)
        {
            MSG message{};
            if (!PeekMessage(&message, window, WM_NET_EVENT, WM_NET_EVENT, PM_REMOVE))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            const std::unique_ptr<char[]> packet(reinterpret_cast<char*>(message.lParam));
            const auto result = client.OnNetEvent(message.wParam, message.lParam);
            if (result == NetEventResult::ChatReceived) panel.Refresh(client);
            if (message.wParam == static_cast<WPARAM>(expected)) return result;
        }
        Check(false, "receive the expected chat/room window event");
        return NetEventResult::Ignored;
    }
    void Key(WPARAM key, bool inEdit = false)
    {
        MSG message{};
        message.hwnd = inEdit ? GetDlgItem(window, ChatPanel::Input) : window;
        message.message = WM_KEYDOWN;
        message.wParam = key;
        Check(panel.HandleMessage(message, client), "chat panel handles its keyboard command");
    }
};

void ChatPanelLayoutTest()
{
    HiddenChatPlayer player;
    ConnectScreen screen;
    Check(screen.Create(player.window, GetModuleHandleW(nullptr)), "create hidden connection and waiting forms");
    SetDlgItemTextW(player.window, ConnectScreen::RoomNumber, L"22");
    SetDlgItemTextW(player.window, ConnectScreen::Nickname, L"테스트");
    for (const auto size : {std::pair{1160, 830}, std::pair{446, 692}, std::pair{800, 760}})
    {
        MoveWindow(player.window, 0, 0, size.first, size.second, FALSE);
        screen.ShowWaiting(player.window, false);
        player.panel.Show(true);
        screen.Layout(player.window);
        player.panel.Layout();
        RECT button{}, history{}, input{}, bounds{};
        GetWindowRect(GetDlgItem(player.window, ConnectScreen::LeaveButton), &button);
        GetWindowRect(GetDlgItem(player.window, ChatPanel::History), &history);
        GetWindowRect(GetDlgItem(player.window, ChatPanel::Input), &input);
        GetWindowRect(player.window, &bounds);
        Check(button.bottom <= history.top && history.bottom < input.top && input.bottom <= bounds.bottom &&
            history.left >= bounds.left && input.right <= bounds.right,
            "chat controls stay inside the hidden window and below the waiting screen at all supported sizes");
        Check(!IsWindowVisible(player.window), "layout verification never displays a window");
    }
    screen.Destroy(player.window);
    std::cout << "PASS: hidden native chat controls fit the waiting screen at normal and minimum window sizes\n";
}

void ChatSendTest()
{
    Listener listener;
    NetworkClient client;
    Check(!client.SendChat("offline") && client.GetLastError() == WSAENOTCONN, "chat requires a connection");
    Check(client.Connect("127.0.0.1", listener.port), "connect chat sender");
    Socket peer; peer.value = listener.Accept();
    const DWORD timeout = 2000;
    setsockopt(peer.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    const std::string ascii(63, 'a'), tooLong(64, 'a');
    std::string korean;
    for (int i = 0; i < 21; ++i) korean += "가";
    const char invalidUtf8[] = {static_cast<char>(0xff), '\0'};
    for (const char* invalid : {static_cast<const char*>(nullptr), "", tooLong.c_str(), invalidUtf8})
        Check(!client.SendChat(invalid) && client.GetLastError() == WSAEINVAL, "reject empty, oversized and invalid UTF-8 chat");
    for (const auto& text : {ascii, korean})
    {
        Check(client.SendChat(text.c_str()), "accept exactly 63 UTF-8 bytes");
        ChatPacket packet;
        Check(recvn(peer.value, reinterpret_cast<char*>(&packet), sizeof(packet)) == sizeof(packet) &&
            packet.type == PacketType::CHAT && packet.sender == NO_PLAYER &&
            std::strcmp(packet.msg, text.c_str()) == 0 && packet.msg[MAX_CHAT - 1] == '\0',
            "send a whole, terminated chat frame without claiming a player identity");
    }
    client.Disconnect();
    ExpectClientLeave(peer.value);
    std::cout << "PASS: CHAT framing, UTF-8 63-byte boundaries, rejection and disconnected sending\n";
}

void RoomChatTest()
{
    Room room(1), otherRoom(2);
    Stream first, second, outside;
    ClientSession player0(std::exchange(first.receiver.value, INVALID_SOCKET));
    ClientSession player1(std::exchange(second.receiver.value, INVALID_SOCKET));
    ClientSession other(std::exchange(outside.receiver.value, INVALID_SOCKET));
    room.AddPlayer(&player0); room.AddPlayer(&player1); otherRoom.AddPlayer(&other);
    RoomTestAccess::Bind(player0, room, 0); RoomTestAccess::Bind(player1, room, 1);
    RoomTestAccess::Bind(other, otherRoom, 0);
    const DWORD timeout = 2000;
    for (Stream* stream : {&first, &second, &outside})
        setsockopt(stream->sender.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    for (const int sender : {0, 1})
    {
        ChatPacket forged; forged.sender = static_cast<std::uint8_t>(1 - sender);
        std::memcpy(forged.msg, "hello", 6);
        (sender == 0 ? player0 : player1).ProcessPacket(reinterpret_cast<char*>(&forged));
        for (Stream* stream : {&first, &second})
        {
            ChatPacket received;
            Check(recvn(stream->sender.value, reinterpret_cast<char*>(&received), sizeof(received)) == sizeof(received) &&
                received.sender == sender && std::strcmp(received.msg, "hello") == 0,
                "room chat echoes to both players with the actual session's sender index");
        }
    }
    ChatPacket invalid;
    std::memset(invalid.msg, 'a', sizeof(invalid.msg));
    player0.ProcessPacket(reinterpret_cast<char*>(&invalid));
    invalid.msg[0] = '\0'; player0.ProcessPacket(reinterpret_cast<char*>(&invalid));
    invalid.msg[0] = static_cast<char>(0xff); invalid.msg[1] = '\0';
    player0.ProcessPacket(reinterpret_cast<char*>(&invalid));
    room.OnChat(-1, "bad"); room.OnChat(2, "bad"); room.OnChat(0, nullptr);
    room.OnPlayerLeave(1); room.OnChat(1, "departed");
    for (Stream* stream : {&first, &second, &outside})
    {
        fd_set readSet;
        FD_ZERO(&readSet); FD_SET(stream->sender.value, &readSet);
        const timeval immediate{};
        Check(select(0, &readSet, nullptr, nullptr, &immediate) == 0,
            "other rooms, malformed messages and departed senders receive no chat traffic");
    }
    std::cout << "PASS: room-only chat, sender spoof prevention, malformed frames and departed senders\n";
}

void GameClientChatTest()
{
    HiddenChatPlayer first, second, outside;
    Server server;
    Check(server.InitServer(0), "start the real server for end-to-end chat");
    AcceptRunner running(server);
    const auto join = [&](HiddenChatPlayer& player, const wchar_t* room, const wchar_t* name)
    {
        Check(player.client.ConnectAndJoin(L"127.0.0.1", room, name, server.GetPort()) == JoinRequestResult::Sent,
            "join a real room for chat");
        player.panel.Show(true);
    };
    join(first, L"22", L"경준");
    Check(first.Receive(PacketType::JOIN_RESULT) == NetEventResult::Waiting, "first chat player waits alone");
    first.Key(VK_RETURN);
    SetDlgItemTextW(first.window, ChatPanel::Input, L"대기 중 채팅");
    first.Key(VK_RETURN, true);
    Check(first.Receive(PacketType::CHAT) == NetEventResult::ChatReceived &&
        first.client.GetChatHistory().back() == L"경준: 대기 중 채팅", "chat works in a waiting room with the user's nickname");

    join(outside, L"23", L"other");
    Check(outside.Receive(PacketType::JOIN_RESULT) == NetEventResult::Waiting, "another room stays separate");
    join(second, L"22", L"peer");
    Check(second.Receive(PacketType::JOIN_RESULT) == NetEventResult::Matched &&
        first.Receive(PacketType::JOIN_RESULT) == NetEventResult::Matched, "chat players match in the same room");
    Check(first.Receive(PacketType::GAME_START) == NetEventResult::GameStarted &&
        second.Receive(PacketType::GAME_START) == NetEventResult::GameStarted, "game start keeps the waiting-room chat history");
    first.Key(VK_RETURN);
    SetDlgItemTextW(first.window, ChatPanel::Input, L"안녕하세요 Z ←");
    first.client.OnKey(WM_KEYDOWN, 'Z'); first.client.OnKey(WM_KEYDOWN, VK_LEFT);
    first.Key(VK_RETURN, true);
    Check(!first.client.IsChatting() && GetWindowTextLengthW(GetDlgItem(first.window, ChatPanel::Input)) == 0,
        "Enter sends a message and closes/clears the editor");
    for (HiddenChatPlayer* player : {&first, &second})
    {
        Check(player->Receive(PacketType::CHAT) == NetEventResult::ChatReceived &&
            player->client.GetChatHistory().back() == L"경준: 안녕하세요 Z ←", "both clients receive the same Korean chat line");
        wchar_t history[512]{};
        GetDlgItemTextW(player->window, ChatPanel::History, history, ARRAYSIZE(history));
        Check(std::wstring(history).find(L"경준: 안녕하세요 Z ←") != std::wstring::npos,
            "the real native history control displays the received text");
    }
    Check(first.client.GetChatHistory().size() == 2, "the sender waits for the server echo instead of displaying twice");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    Check(!HasNetEvent(outside.window) && outside.client.GetChatHistory().empty(), "chat never leaks into another room");

    first.Key(VK_RETURN);
    const std::wstring oversized(22, L'가');
    SetDlgItemTextW(first.window, ChatPanel::Input, oversized.c_str());
    first.Key(VK_RETURN, true);
    Check(first.client.IsChatting() && GetWindowTextLengthW(GetDlgItem(first.window, ChatPanel::Input)) == 22,
        "an oversized Korean message stays editable without truncation");
    first.Key(VK_ESCAPE, true);
    Check(!first.client.IsChatting() && GetWindowTextLengthW(GetDlgItem(first.window, ChatPanel::Input)) == 0,
        "Esc cancels chat without sending it");
    Check(first.client.SendChat(L"   ") == ChatSendResult::Empty &&
        first.client.SendChat(L"line\nbreak") == ChatSendResult::InvalidText,
        "blank and multiline messages are rejected locally");
    const wchar_t badSurrogate[] = {static_cast<wchar_t>(0xd800), L'\0'};
    Check(first.client.SendChat(badSurrogate) == ChatSendResult::InvalidText, "invalid UTF-16 is rejected");
    SetDlgItemTextW(second.window, ChatPanel::Input, L"버튼 전송");
    Check(second.panel.HandleCommand(MAKEWPARAM(ChatPanel::SendButton, BN_CLICKED), second.client), "send button submits chat");
    for (HiddenChatPlayer* player : {&first, &second})
        Check(player->Receive(PacketType::CHAT) == NetEventResult::ChatReceived &&
            player->client.GetChatHistory().back() == L"peer: 버튼 전송", "the other player's nickname and button message are preserved");

    ChatPacket packet; packet.sender = 0; std::memcpy(packet.msg, "bounded", 8);
    for (int i = 0; i < 105; ++i)
        first.client.OnNetEvent(static_cast<WPARAM>(PacketType::CHAT), reinterpret_cast<LPARAM>(&packet));
    Check(first.client.GetChatHistory().size() == 100, "chat history is bounded to its most recent 100 lines");
    packet.sender = NO_PLAYER;
    Check(first.client.OnNetEvent(static_cast<WPARAM>(PacketType::CHAT), reinterpret_cast<LPARAM>(&packet)) == NetEventResult::Ignored,
        "a malformed server sender does not enter chat history");
    first.client.Disconnect(); first.panel.Reset(first.client);
    Check(first.client.GetChatHistory().empty() && !first.client.IsChatting() &&
        GetWindowTextLengthW(GetDlgItem(first.window, ChatPanel::History)) == 0,
        "leaving a room clears both the model and native chat controls");
    Check(!IsWindowVisible(first.window) && !IsWindowVisible(second.window) && !IsWindowVisible(outside.window),
        "end-to-end chat verification never shows a window");
    std::cout << "PASS: hidden end-to-end waiting/game chat, Korean input, native history, Enter/Esc/button, byte limits and reset\n";
}

void GameClientNetEventTest()
{
    struct Player
    {
        HWND window = CreateWindowExW(0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
        GameClient client;

        Player()
        {
            Check(window != nullptr, "create message window for the game client");
            client.SetWindow(window);
        }
        ~Player()
        {
            client.Disconnect();
            MSG message{};
            while (PeekMessage(&message, window, WM_NET_EVENT, WM_NET_EVENT, PM_REMOVE))
                delete[] reinterpret_cast<char*>(message.lParam);
            DestroyWindow(window);
        }
        NetEventResult Receive(PacketType type)
        {
            std::vector<char> packet;
            Check(TakeNetEvent(window, type, packet), "receive the expected game client window event");
            return client.OnNetEvent(static_cast<WPARAM>(type), reinterpret_cast<LPARAM>(packet.data()));
        }
    } first, second, third;

    Server server;
    Check(server.InitServer(0), "start server for online scene transitions");
    AcceptRunner running(server);
    const auto join = [&](Player& player, const wchar_t* room, const wchar_t* nick)
    {
        Check(player.client.ConnectAndJoin(L"127.0.0.1", room, nick, server.GetPort()) == JoinRequestResult::Sent,
            "send a room join request for the online scene test");
    };

    join(first, L"17", L"first");
    Check(first.Receive(PacketType::JOIN_RESULT) == NetEventResult::Waiting && first.client.GetScene() == Scene::Waiting,
        "first player waits for an opponent");
    join(second, L"17", L"second");
    for (Player* player : {&first, &second})
    {
        Check(player->Receive(PacketType::JOIN_RESULT) == NetEventResult::Matched && player->client.GetScene() == Scene::Waiting,
            "matching does not start the game by itself");
        Check(player->Receive(PacketType::GAME_START) == NetEventResult::GameStarted && player->client.GetScene() == Scene::Playing,
            "the server's GAME_START starts the game for both players");
    }

    join(third, L"17", L"third");
    Check(third.Receive(PacketType::JOIN_RESULT) == NetEventResult::RoomFull && third.client.GetScene() == Scene::Connect,
        "the third client receives a full-room reply and stays on the connect scene");
    third.client.Disconnect();
    join(third, L"18", L"third");
    Check(third.Receive(PacketType::JOIN_RESULT) == NetEventResult::Waiting && third.client.GetScene() == Scene::Waiting,
        "a client can retry in another room");
    first.client.Disconnect();
    Check(first.client.GetScene() == Scene::Connect, "disconnect returns a player to the connect scene");
    std::cout << "PASS: online join, match, game start, full room and retry through real server replies\n";
}

void ClientProcessPacketTest()
{
    const HWND window = CreateWindowExA(0, "STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    Check(window != nullptr, "create message-only window");
    NetworkClient client;
    client.ProcessPacket(nullptr);

    GameStatePacket state; state.tick = 77; state.phase = Phase::Playing; state.p[1].x = 940; state.ball.x = 120.5f;
    client.ProcessPacket(reinterpret_cast<char*>(&state));
    GameStatePacket latest;
    Check(client.GetLatestState(latest) && std::memcmp(&latest, &state, sizeof(state)) == 0,
        "GAME_STATE stores the latest state");

    ScorePacket score; score.scorer = 1; score.score[1] = 4;
    client.ProcessPacket(reinterpret_cast<char*>(&score));
    Check(!HasNetEvent(window), "no window means no posted event");

    client.SetWindow(window);
    client.ProcessPacket(reinterpret_cast<char*>(&state));
    Check(!HasNetEvent(window), "GAME_STATE is polled, not posted");
    std::vector<char> received;
    client.ProcessPacket(reinterpret_cast<char*>(&score));
    Check(TakeNetEvent(window, PacketType::SCORE, received) && received.size() == sizeof(score) &&
        std::memcmp(received.data(), &score, sizeof(score)) == 0, "SCORE is posted as a copied packet");

    ChatPacket chat; chat.sender = 0; std::memcpy(chat.msg, "hi", 3);
    client.ProcessPacket(reinterpret_cast<char*>(&chat));
    Check(TakeNetEvent(window, PacketType::CHAT, received) && std::memcmp(received.data(), &chat, sizeof(chat)) == 0,
        "CHAT is posted with sender and message");

    InputPacket input; input.keys = LEFT;
    JoinRoomPacket join; join.roomNo = 1;
    PacketHeader broken{3, PacketType::SCORE};
    client.ProcessPacket(reinterpret_cast<char*>(&input));
    client.ProcessPacket(reinterpret_cast<char*>(&join));
    client.ProcessPacket(reinterpret_cast<char*>(&broken));
    client.GetLatestState(latest);
    Check(!HasNetEvent(window) && latest.tick == 77, "client-to-server and malformed packets are ignored");

    Listener listener;
    Check(client.Connect("127.0.0.1", listener.port), "connect dispatching client");
    Socket server; server.value = listener.Accept();
    Check(!client.GetLatestState(latest) && latest.tick == 0, "connect clears the previous state and marks it unavailable");
    JoinResultPacket result; result.result = JoinResult::Matched; result.myIndex = 1;
    GameStartPacket start; start.stage = 2; start.serveSide = 1; std::memcpy(start.nick[0], "pika", 5);
    EventPacket event; event.kind = EventKind::Spike; event.who = 0;
    StageOverPacket stageOver; stageOver.stage = 1; stageOver.winner = 0;
    GameOverPacket gameOver; gameOver.winner = 1; gameOver.reason = GameOverReason::Quit;
    state.tick = 5;
    SendBytes(server.value, reinterpret_cast<const char*>(&result), sizeof(result));
    SendBytes(server.value, reinterpret_cast<const char*>(&start), sizeof(start));
    SendBytes(server.value, reinterpret_cast<const char*>(&state), sizeof(state));
    SendBytes(server.value, reinterpret_cast<const char*>(&event), sizeof(event));
    SendBytes(server.value, reinterpret_cast<const char*>(&stageOver), sizeof(stageOver));
    SendBytes(server.value, reinterpret_cast<const char*>(&gameOver), sizeof(gameOver));
    Check(TakeNetEvent(window, PacketType::JOIN_RESULT, received) && std::memcmp(received.data(), &result, sizeof(result)) == 0,
        "receive thread posts JOIN_RESULT");
    Check(TakeNetEvent(window, PacketType::GAME_START, received) && std::memcmp(received.data(), &start, sizeof(start)) == 0,
        "receive thread posts GAME_START in order");
    Check(TakeNetEvent(window, PacketType::EVENT, received) && std::memcmp(received.data(), &event, sizeof(event)) == 0,
        "receive thread posts EVENT");
    Check(TakeNetEvent(window, PacketType::STAGE_OVER, received) && std::memcmp(received.data(), &stageOver, sizeof(stageOver)) == 0,
        "receive thread posts STAGE_OVER");
    Check(TakeNetEvent(window, PacketType::GAME_OVER, received) && std::memcmp(received.data(), &gameOver, sizeof(gameOver)) == 0,
        "receive thread posts GAME_OVER");
    WaitUntil([&] { return client.GetReceivedCount() == 6; }, "all six server packets are dispatched");
    client.GetLatestState(latest);
    Check(latest.tick == 5 && !HasNetEvent(window), "receive thread stores GAME_STATE without posting");
    client.Disconnect();
    DestroyWindow(window);
    std::cout << "PASS: client ProcessPacket stores GAME_STATE and posts other server packets to the window\n";
}

void RoomStageOverTest()
{
    Stream first, second;
    ClientSession player0(std::exchange(first.receiver.value, INVALID_SOCKET));
    ClientSession player1(std::exchange(second.receiver.value, INVALID_SOCKET));
    Room room;
    room.AddPlayer(&player0);
    room.AddPlayer(&player1);
    for (Stream* stream : {&first, &second})
    {
        const DWORD timeout = 2000;
        Check(setsockopt(stream->sender.value, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0, "stage result receive timeout");
    }
    for (int stage = 1; stage <= GameConst::StageCount; ++stage)
    {
        for (int loser = 0; loser < GameConst::PlayerCount; ++loser)
        {
            GameWorld& world = RoomTestAccess::World(room);
            world.Reset(stage, 0);
            RoomTestAccess::SetStage(room, stage);
            const std::uint8_t idle[2]{};
            world.Update(idle);
            for (int point = 0; point < GameConst::WinningScore; ++point)
            {
                GameWorldTestAccess::SetBall(world, {loser == 0 ? 450.0f : 600.0f, 635, 0, 400, 0});
                world.CheckScore();
            }
            room.BroadcastStageOver();
            StageOverPacket expected;
            expected.stage = static_cast<std::uint8_t>(stage);
            expected.winner = static_cast<std::uint8_t>(1 - loser);
            for (Stream* stream : {&first, &second})
            {
                StageOverPacket received;
                Check(recvn(stream->sender.value, reinterpret_cast<char*>(&received), sizeof(received), 0) == sizeof(received) &&
                    std::memcmp(&received, &expected, sizeof(received)) == 0,
                    "both players receive the completed stage and correct winner");
            }
        }
    }
    std::cout << "PASS: STAGE_OVER packets for either winner in both stages over TCP\n";
}

void RoomGameOverTest()
{
    for (int loser = 0; loser < GameConst::PlayerCount; ++loser)
    {
        Stream first, second;
        ClientSession player0(std::exchange(first.receiver.value, INVALID_SOCKET));
        ClientSession player1(std::exchange(second.receiver.value, INVALID_SOCKET));
        Room room;
        room.AddPlayer(&player0);
        room.AddPlayer(&player1);
        for (Stream* stream : {&first, &second})
        {
            const DWORD timeout = 2000;
            Check(setsockopt(stream->sender.value, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0, "game result receive timeout");
        }
        auto receiveBoth = [&](GameOverReason reason) {
            GameOverPacket expected;
            expected.winner = static_cast<std::uint8_t>(1 - loser);
            expected.reason = reason;
            for (Stream* stream : {&first, &second})
            {
                GameOverPacket received;
                Check(recvn(stream->sender.value, reinterpret_cast<char*>(&received), sizeof(received), 0) == sizeof(received) &&
                    std::memcmp(&received, &expected, sizeof(received)) == 0,
                    "GAME_OVER contains the final winner and requested reason");
            }
        };
        GameWorld& world = RoomTestAccess::World(room);
        room.BroadcastGameOver(static_cast<std::uint8_t>(GameOverReason::Normal));
        room.BroadcastGameOver(static_cast<std::uint8_t>(GameOverReason::Quit));
        room.BroadcastGameOver(255);
        RoomTestAccess::SetStage(room, 2);
        world.Reset(2, 0);
        const std::uint8_t idle[2]{};
        world.Update(idle);
        for (int point = 0; point < GameConst::WinningScore; ++point)
        {
            GameWorldTestAccess::SetBall(world, {loser == 0 ? 450.0f : 600.0f, 635, 0, 400, 0});
            world.CheckScore();
        }
        room.BroadcastGameOver(static_cast<std::uint8_t>(GameOverReason::Normal));
        receiveBoth(GameOverReason::Normal);
        room.OnPlayerLeave(loser);
        room.BroadcastGameOver(static_cast<std::uint8_t>(GameOverReason::Quit));
        room.EndByQuit();
        room.OnPlayerLeave(1 - loser);
        room.BroadcastGameOver(static_cast<std::uint8_t>(GameOverReason::Quit));
        for (Stream* stream : {&first, &second})
        {
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(stream->sender.value, &readable);
            const timeval immediate{};
            Check(select(0, &readable, nullptr, nullptr, &immediate) == 0,
                "unfinished or invalid results and two departures do not invent a winner");
        }
    }
    std::cout << "PASS: GAME_OVER packets preserve final results and suppress invalid or duplicate outcomes\n";
}

struct NextStageWaiter
{
    Room& room;
    HANDLE thread = nullptr;
    bool ready = false;

    explicit NextStageWaiter(Room& target) : room(target)
    {
        thread = CreateThread(nullptr, 0, Run, this, 0, nullptr);
        Check(thread != nullptr, "start next-stage waiter");
    }
    ~NextStageWaiter()
    {
        if (thread != nullptr)
        {
            room.OnPlayerLeave(0);
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
        }
    }
    static DWORD WINAPI Run(LPVOID arg)
    {
        auto& waiter = *static_cast<NextStageWaiter*>(arg);
        waiter.ready = waiter.room.WaitNextStage();
        if (waiter.ready)
        {
            GameWorld& world = RoomTestAccess::World(waiter.room);
            const int serve = 1 - world.GetLoser();
            world.Reset(2, serve);
            RoomTestAccess::SetStage(waiter.room, 2);
            waiter.room.BroadcastGameStart(2, serve);
        }
        return 0;
    }
    void Finish()
    {
        Check(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0, "next-stage wait finishes promptly");
        CloseHandle(thread);
        thread = nullptr;
    }
};

void ClientNextStageTest()
{
    Room room;
    Listener listener;
    NetworkClient first, second;
    Check(first.Connect("127.0.0.1", listener.port), "connect the first readiness client");
    ClientSession player0(listener.Accept());
    Check(second.Connect("127.0.0.1", listener.port), "connect the second readiness client");
    ClientSession player1(listener.Accept());
    room.AddPlayer(&player0); room.AddPlayer(&player1);
    RoomTestAccess::Bind(player0, room, 0); RoomTestAccess::Bind(player1, room, 1);
    Check(player0.Start() && player1.Start(), "start real server session receivers for NEXT_STAGE");
    GameWorldTestAccess::FinishFirstStage(RoomTestAccess::World(room), 0);
    room.BroadcastStageOver();
    WaitUntil([&] { return first.GetReceivedCount() == 1 && second.GetReceivedCount() == 1; }, "both clients receive stage end");
    NextStageWaiter waiting(room);
    Check(first.SendNextStage() && first.SendNextStage(), "send first player's readiness twice through NetworkClient");
    WaitUntil([&] { return player0.GetReceivedCount() == 2; }, "the first player's readiness reaches the server");
    Check(WaitForSingleObject(waiting.thread, 30) == WAIT_TIMEOUT, "duplicate readiness cannot stand in for the other player");
    Check(second.SendNextStage(), "send the second player's readiness through NetworkClient");
    waiting.Finish();
    Check(waiting.ready && GameWorldTestAccess::StageIsFresh(RoomTestAccess::World(room), 2, 1),
        "both real clients' requests advance the next-stage server flow");
    WaitUntil([&] { return first.GetReceivedCount() == 2 && second.GetReceivedCount() == 2; }, "both clients receive GAME_START");
    first.Disconnect(); second.Disconnect();
    Check(player0.WaitForFinish(2000) && player1.WaitForFinish(2000) &&
        player0.GetReceivedCount() == 3 && player1.GetReceivedCount() == 2,
        "both sessions receive one final LEAVE after readiness");
    std::cout << "PASS: NetworkClient NEXT_STAGE reaches both real sessions, advances the room and closes with LEAVE\n";
}

void RoomNextStageTest()
{
    for (int cancel = 0; cancel < 2; ++cancel)
    {
        Stream first, second;
        Room room;
        ClientSession player0(std::exchange(first.receiver.value, INVALID_SOCKET));
        ClientSession player1(std::exchange(second.receiver.value, INVALID_SOCKET));
        room.AddPlayer(&player0);
        room.AddPlayer(&player1);
        RoomTestAccess::Bind(player0, room, 0);
        RoomTestAccess::Bind(player1, room, 1);
        Check(player0.Start() && player1.Start(), "start NEXT_STAGE session receivers");
        for (Stream* stream : {&first, &second})
        {
            const DWORD timeout = 2000;
            Check(setsockopt(stream->sender.value, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0, "next-stage receive timeout");
        }
        room.OnNextStage(0);
        room.OnNextStage(1);
        Check(!room.WaitNextStage(), "ready requests during play do not start a stage");
        GameWorldTestAccess::FinishFirstStage(RoomTestAccess::World(room), 0);
        room.BroadcastStageOver();
        for (Stream* stream : {&first, &second})
        {
            StageOverPacket packet;
            Check(recvn(stream->sender.value, reinterpret_cast<char*>(&packet), sizeof(packet), 0) == sizeof(packet) &&
                packet.type == PacketType::STAGE_OVER && packet.winner == 1, "stage result precedes readiness");
        }
        NextStageWaiter waiting(room);
        Check(WaitForSingleObject(waiting.thread, 30) == WAIT_TIMEOUT, "wait for both players");
        room.OnNextStage(-1);
        room.OnNextStage(2);
        NextStagePacket request;
        first.Send(reinterpret_cast<const char*>(&request), sizeof(request));
        first.Send(reinterpret_cast<const char*>(&request), sizeof(request));
        WaitUntil([&] { return player0.GetReceivedCount() == 2; }, "duplicate ready packets are dispatched");
        Check(WaitForSingleObject(waiting.thread, 30) == WAIT_TIMEOUT, "one player cannot confirm both seats");
        room.SetInput(0, RIGHT | ACTION);
        if (cancel)
        {
            room.OnPlayerLeave(1);
            waiting.Finish();
            Check(!waiting.ready, "departure cancels the readiness wait");
        }
        else
        {
            second.Send(reinterpret_cast<const char*>(&request), sizeof(request));
            for (Stream* stream : {&first, &second})
            {
                GameStartPacket packet;
                Check(recvn(stream->sender.value, reinterpret_cast<char*>(&packet), sizeof(packet), 0) == sizeof(packet) &&
                    packet.type == PacketType::GAME_START && packet.stage == 2 && packet.serveSide == 1,
                    "both players start stage two with the previous winner serving");
            }
            waiting.Finish();
            Check(waiting.ready, "both readiness confirmations release the waiter");
            GameStatePacket state;
            RoomTestAccess::World(room).FillState(state);
            Check(state.tick == 0 && state.p[0].evil == 1 && state.p[1].evil == 0,
                "stage two resets the world and awakens its first-stage loser");
            std::uint8_t inputs[2]{};
            room.CopyInputs(inputs);
            Check(inputs[0] == 0 && inputs[1] == 0 && !room.WaitNextStage(),
                "stale inputs and readiness do not carry into the new stage");
        }
        player0.Disconnect();
        player1.Disconnect();
        Check(player0.WaitForFinish(2000) && player1.WaitForFinish(2000), "readiness session workers stop");
    }

    Room preconfirmed;
    GameWorldTestAccess::FinishFirstStage(RoomTestAccess::World(preconfirmed), 1);
    preconfirmed.BroadcastStageOver();
    preconfirmed.OnNextStage(0);
    preconfirmed.OnNextStage(1);
    Check(preconfirmed.WaitNextStage(), "confirmations arriving before the wait are retained");
    std::cout << "PASS: NEXT_STAGE over TCP, duplicate suppression, stage-two start and departure cancellation\n";
}

void RoomQuitTest()
{
    for (int betweenStages = 0; betweenStages < 2; ++betweenStages)
    {
        for (int leaver = 0; leaver < GameConst::PlayerCount; ++leaver)
        {
            Stream first, second;
            for (Stream* stream : {&first, &second})
            {
                const DWORD noTimeout = 0;
                Check(setsockopt(stream->receiver.value, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&noTimeout), sizeof(noTimeout)) == 0,
                    "keep idle session receivers alive through the stage");
            }
            Room room;
            ClientSession player0(std::exchange(first.receiver.value, INVALID_SOCKET));
            ClientSession player1(std::exchange(second.receiver.value, INVALID_SOCKET));
            room.AddPlayer(&player0);
            room.AddPlayer(&player1);
            RoomTestAccess::Bind(player0, room, 0);
            RoomTestAccess::Bind(player1, room, 1);
            Check(player0.Start() && player1.Start(), "start quit test session workers");
            for (Stream* stream : {&first, &second})
            {
                const DWORD timeout = 2000;
                Check(setsockopt(stream->sender.value, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0, "quit result timeout");
            }
            auto read = [](SOCKET socket) {
                std::array<char, MAX_PACKET_SIZE> bytes{};
                PacketHeader header{};
                Check(recvn(socket, reinterpret_cast<char*>(&header), sizeof(header), 0) == sizeof(header) &&
                    header.size >= sizeof(header) && header.size <= MAX_PACKET_SIZE && PacketSize(header.type) == header.size,
                    "receive valid match frame while waiting for a quit result");
                std::memcpy(bytes.data(), &header, sizeof(header));
                const int body = header.size - sizeof(header);
                Check(recvn(socket, bytes.data() + sizeof(header), body, 0) == body, "receive complete quit test frame");
                return bytes;
            };
            room.Start();
            for (Stream* stream : {&first, &second})
            {
                const auto bytes = read(stream->sender.value);
                GameStartPacket start;
                std::memcpy(&start, bytes.data(), sizeof(start));
                Check(start.type == PacketType::GAME_START && start.stage == 1, "quit test starts a real match");
            }
            if (betweenStages)
            {
                InputPacket move; move.keys = LEFT;
                first.Send(reinterpret_cast<const char*>(&move), sizeof(move));
                second.Send(reinterpret_cast<const char*>(&move), sizeof(move));
                const ULONGLONG deadline = GetTickCount64() + 20000;
                bool finished = false;
                while (!finished)
                {
                    Check(GetTickCount64() < deadline, "reach the readiness wait before quitting");
                    const auto a = read(first.sender.value);
                    const auto b = read(second.sender.value);
                    PacketHeader header{};
                    std::memcpy(&header, a.data(), sizeof(header));
                    Check(std::memcmp(a.data(), b.data(), header.size) == 0, "both players reach the same stage result");
                    finished = header.type == PacketType::STAGE_OVER;
                }
            }
            Stream& leaving = leaver == 0 ? first : second;
            Stream& survivor = leaver == 0 ? second : first;
            if (leaver == 0)
            {
                LeavePacket leave;
                leaving.Send(reinterpret_cast<const char*>(&leave), sizeof(leave));
            }
            else
            {
                const linger reset{1, 0};
                Check(setsockopt(leaving.sender.value, SOL_SOCKET, SO_LINGER,
                    reinterpret_cast<const char*>(&reset), sizeof(reset)) == 0, "simulate a reset connection");
                closesocket(std::exchange(leaving.sender.value, INVALID_SOCKET));
            }
            bool received = false;
            const ULONGLONG deadline = GetTickCount64() + 3000;
            while (!received)
            {
                Check(GetTickCount64() < deadline, "survivor receives the quit result promptly");
                const auto bytes = read(survivor.sender.value);
                PacketHeader header{};
                std::memcpy(&header, bytes.data(), sizeof(header));
                if (header.type == PacketType::GAME_OVER)
                {
                    GameOverPacket result;
                    std::memcpy(&result, bytes.data(), sizeof(result));
                    Check(result.winner == 1 - leaver && result.reason == GameOverReason::Quit,
                        "the remaining player wins after LEAVE or connection reset");
                    received = true;
                }
            }
            Check(RoomTestAccess::WaitForEnd(room), "quit ends the room worker during play or readiness wait");
            room.EndByQuit();
            room.EndByQuit();
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(survivor.sender.value, &readable);
            const timeval brief{0, 30000};
            Check(select(0, &readable, nullptr, nullptr, &brief) == 0, "quit result is sent only once");
            player0.Disconnect();
            player1.Disconnect();
            Check(player0.WaitForFinish(2000) && player1.WaitForFinish(2000), "quit session workers stop");
        }
    }
    std::cout << "PASS: real room quit wins after LEAVE/reset during play or readiness, exactly once\n";
}

void RoomQuitSuppressionTest()
{
    Stream first, second;
    ClientSession player0(std::exchange(first.receiver.value, INVALID_SOCKET));
    ClientSession player1(std::exchange(second.receiver.value, INVALID_SOCKET));
    Room room;
    room.AddPlayer(&player0);
    room.AddPlayer(&player1);
    room.EndByQuit();
    room.OnPlayerLeave(0);
    std::thread a([&] { room.EndByQuit(); });
    std::thread b([&] { room.EndByQuit(); });
    a.join(); b.join();
    const DWORD timeout = 2000;
    setsockopt(second.sender.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    GameOverPacket result;
    Check(recvn(second.sender.value, reinterpret_cast<char*>(&result), sizeof(result), 0) == sizeof(result) &&
        result.type == PacketType::GAME_OVER && result.winner == 1 && result.reason == GameOverReason::Quit,
        "concurrent quit handling sends one result to the survivor");
    room.OnPlayerLeave(1);
    room.EndByQuit();
    for (Stream* stream : {&first, &second})
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(stream->sender.value, &readable);
        const timeval immediate{};
        Check(select(0, &readable, nullptr, nullptr, &immediate) == 0,
            "no result goes to the departed player and duplicate results are suppressed");
    }
    Room empty;
    empty.EndByQuit();
    empty.OnPlayerLeave(0);
    empty.OnPlayerLeave(1);
    empty.EndByQuit();
    Check(!RoomTestAccess::World(empty).IsStageOver(), "empty or fully departed rooms do not invent a score winner");
    std::cout << "PASS: quit notifications handle repeated/concurrent calls and empty rooms\n";
}

void RoomEventsBroadcastTest()
{
    Stream first, second, outside;
    ClientSession player0(std::exchange(first.receiver.value, INVALID_SOCKET));
    ClientSession player1(std::exchange(second.receiver.value, INVALID_SOCKET));
    ClientSession otherPlayer(std::exchange(outside.receiver.value, INVALID_SOCKET));
    Room room(1), otherRoom(2);
    Check(room.AddPlayer(&player0) == 0 && room.AddPlayer(&player1) == 1 &&
        otherRoom.AddPlayer(&otherPlayer) == 0, "event recipients belong to their own rooms");
    const DWORD timeout = 2000;
    for (Stream* stream : {&first, &second})
        Check(setsockopt(stream->sender.value, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0, "event receive timeout");

    auto receiveBoth = [&]<typename Packet>(const Packet& expected) {
        for (Stream* stream : {&first, &second})
        {
            Packet received;
            Check(recvn(stream->sender.value, reinterpret_cast<char*>(&received), sizeof(received), 0) == sizeof(received) &&
                std::memcmp(&received, &expected, sizeof(received)) == 0,
                "both clients receive the expected whole event/score packet in order");
        }
    };
    auto expectEvent = [&](EventKind kind, std::uint8_t who) {
        EventPacket packet;
        packet.kind = kind;
        packet.who = who;
        receiveBoth(packet);
    };

    GameWorld& world = RoomTestAccess::World(room);
    const std::uint8_t active[2] = {UP | ACTION, LEFT | ACTION};
    world.Update(active);
    world.Update(active);
    room.BroadcastEvents();
    expectEvent(EventKind::Slide, 1);
    expectEvent(EventKind::Jump, 0);
    expectEvent(EventKind::Spike, 0);

    world.Reset(1, 0);
    const std::uint8_t idle[2]{};
    world.Update(idle);
    GameWorldTestAccess::SetBall(world, {985, 469, 0, 400, 0});
    world.Update(idle);
    GameWorldTestAccess::SetBall(world, {450, 635, 0, 400, 1});
    world.CheckScore();
    GameWorldTestAccess::SetBall(world, {600, 635, 0, 400, 0});
    world.CheckScore();
    room.BroadcastEvents();
    expectEvent(EventKind::Hit, 1);
    expectEvent(EventKind::Score, 1);
    ScorePacket firstScore;
    firstScore.scorer = 1;
    firstScore.score[1] = 1;
    receiveBoth(firstScore);
    expectEvent(EventKind::Score, 0);
    ScorePacket secondScore;
    secondScore.scorer = 0;
    secondScore.score[0] = secondScore.score[1] = 1;
    receiveBoth(secondScore);

    room.BroadcastEvents();
    otherRoom.BroadcastEvents();
    for (Stream* stream : {&first, &second, &outside})
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(stream->sender.value, &readSet);
        const timeval immediate{};
        Check(select(0, &readSet, nullptr, nullptr, &immediate) == 0,
            "events are drained once and do not reach a different room");
    }
    std::cout << "PASS: room EVENT/SCORE packets, per-score totals, ordering and no duplicate/other-room delivery\n";
}

void GameStateSnapshotTest()
{
    Listener listener;
    NetworkClient client;
    Check(client.Connect("127.0.0.1", listener.port), "connect client for world snapshot");
    ClientSession session(listener.Accept());

    GameWorld world;
    const std::uint8_t keys[2] = {RIGHT | UP, LEFT | ACTION};
    world.Update(keys);
    GameStatePacket snapshot;
    snapshot.size = 0;
    snapshot.type = PacketType::INPUT;
    std::as_const(world).FillState(snapshot);
    Check(snapshot.size == 49 && snapshot.type == PacketType::GAME_STATE &&
        snapshot.tick == 1 && snapshot.phase == Phase::Playing &&
        snapshot.p[0].x == 40 && snapshot.p[0].y == 535 && snapshot.p[0].action == Action::Jump &&
        snapshot.p[1].x == 936 && snapshot.p[1].y == 557 && snapshot.p[1].action == Action::Slide &&
        snapshot.ball.x == 120.0f && snapshot.ball.vy == 416.0f,
        "snapshot contains both players and the ball from the completed tick");

    world.Update(keys);
    Check(session.SendPacket(reinterpret_cast<const char*>(&snapshot), sizeof(snapshot)), "send captured world snapshot");
    WaitUntil([&] { return client.GetReceivedCount() == 1; }, "client receives the snapshot");
    GameStatePacket received;
    client.GetLatestState(received);
    Check(received.tick == 1 && std::memcmp(&received, &snapshot, sizeof(snapshot)) == 0,
        "client receives all 49 bytes of the captured tick after the world advances");
    std::cout << "PASS: GameWorld snapshot captured and received over TCP\n";
}

void ClientSessionRoundTripTest()
{
    Listener listener;
    NetworkClient client;
    Check(client.Connect("127.0.0.1", listener.port), "round trip client connects");
    ClientSession session(listener.Accept());
    Check(session.Start(), "round trip server worker starts");
    client.SendJoinRoom(9, "pika");
    client.SendInput(LEFT | ACTION);
    WaitUntil([&] { return session.GetReceivedCount() == 2; }, "server receives client join and input");
    GameStartPacket start;
    GameStatePacket state; state.tick = 42;
    ScorePacket score; score.scorer = 0; score.score[0] = 1;
    Check(session.SendPacket(reinterpret_cast<const char*>(&start), sizeof(start)), "send game start to client");
    Check(session.SendPacket(reinterpret_cast<const char*>(&state), sizeof(state)), "send game state to client");
    Check(session.SendPacket(reinterpret_cast<const char*>(&score), sizeof(score)), "send score to client");
    WaitUntil([&] { return client.GetReceivedCount() == 3; }, "client receives all server frames");
    client.SendInput(0);
    WaitUntil([&] { return session.GetReceivedCount() == 3; }, "client can send while receiver runs");
    session.Disconnect();
    Check(session.WaitForFinish(2000), "Disconnect ends the server worker during a real client connection");
    session.Disconnect();
    Check(client.WaitForReceiveEnd(2000) && client.GetLastError() == 0 && !client.IsConnected(), "server Disconnect propagates EOF to client");
    client.Disconnect();
    std::cout << "PASS: real client/session bidirectional JOIN/INPUT/START/STATE/SCORE exchange\n";
}

void ProcessPacketTest()
{
    ClientSession idle;
    idle.ProcessPacket(nullptr);
    {
        Stream stream;
        RoomManager rooms;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET), &rooms);
        Check(session.Start(), "start session for packet dispatch");
        JoinRoomPacket join; join.roomNo = 3; std::memcpy(join.nickname, "pika", 5);
        InputPacket input; input.keys = RIGHT;
        ChatPacket chat; std::memset(chat.msg, 'a', sizeof(chat.msg));
        NextStagePacket next;
        stream.Send(reinterpret_cast<const char*>(&input), sizeof(input));
        stream.Send(reinterpret_cast<const char*>(&chat), sizeof(chat));
        stream.Send(reinterpret_cast<const char*>(&next), sizeof(next));
        WaitUntil([&] { return session.GetReceivedCount() == 3; }, "room packets without a room are ignored");
        Check(!session.WaitForFinish(30), "ignored packets keep the session open");
        stream.Send(reinterpret_cast<const char*>(&join), sizeof(join));
        JoinResultPacket reply;
        Check(recvn(stream.sender.value, reinterpret_cast<char*>(&reply), sizeof(reply), 0) == sizeof(reply) &&
              reply.type == PacketType::JOIN_RESULT && reply.result == JoinResult::Waiting && reply.myIndex == 0,
              "JOIN_ROOM is answered after joining");
        stream.Send(reinterpret_cast<const char*>(&input), sizeof(input));
        WaitUntil([&] { return session.GetReceivedCount() == 5; }, "room packets after joining are accepted");
        LeavePacket leave;
        stream.Send(reinterpret_cast<const char*>(&leave), sizeof(leave));
        Check(session.WaitForFinish(2000) && session.GetLastError() == 0 && session.GetReceivedCount() == 6,
              "LEAVE ends the session cleanly");
        char tail;
        Check(recv(stream.sender.value, &tail, 1, 0) == 0, "LEAVE closes the connection");
    }
    {
        Stream stream;
        ClientSession session(std::exchange(stream.receiver.value, INVALID_SOCKET));
        Check(session.Start(), "start session for unexpected type");
        JoinResultPacket wrongWay;
        stream.Send(reinterpret_cast<const char*>(&wrongWay), sizeof(wrongWay));
        Check(session.WaitForFinish(2000), "server-to-client packet from a client ends the session");
        char tail;
        Check(recv(stream.sender.value, &tail, 1, 0) == 0, "unexpected type closes the connection");
    }
    std::cout << "PASS: packet type dispatch, ignored room packets, LEAVE and unexpected types\n";
}

void RoomJoinTest()
{
    {
        RoomManager rooms;
        ClientSession a, b, c, d;
        int seat = -1;
        std::uint8_t result = 0;
        Check(rooms.JoinRoom(1, nullptr, seat, result) == nullptr && seat == -1, "null session is rejected");
        Room* first = rooms.JoinRoom(7, &a, seat, result);
        Check(first != nullptr && seat == 0 && result == static_cast<std::uint8_t>(JoinResult::Waiting), "first player waits in seat 0");
        Room* second = rooms.JoinRoom(7, &b, seat, result);
        Check(second == first && seat == 1 && result == static_cast<std::uint8_t>(JoinResult::Matched), "second player matches in seat 1");
        Check(first->GetPlayer(0) == &a && first->GetPlayer(1) == &b && first->GetPlayer(2) == nullptr, "room keeps both players");
        Check(rooms.JoinRoom(7, &c, seat, result) == nullptr && seat == -1 &&
              result == static_cast<std::uint8_t>(JoinResult::Full), "third player finds the room full");
        Room* other = rooms.JoinRoom(8, &d, seat, result);
        Check(other != nullptr && other != first && seat == 0, "another room number makes another room");
    }
    for (int round = 0; round < 20; ++round)
    {
        RoomManager rooms;
        std::array<ClientSession, 8> sessions;
        std::array<int, 8> seats{};
        std::array<Room*, 8> joined{};
        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i)
            threads.emplace_back([&, i] { std::uint8_t r = 0; joined[i] = rooms.JoinRoom(42, &sessions[i], seats[i], r); });
        for (auto& thread : threads) thread.join();
        int seat0 = 0, seat1 = 0, full = 0;
        Room* room = nullptr;
        for (int i = 0; i < 8; ++i)
        {
            if (joined[i] == nullptr) { ++full; continue; }
            if (room == nullptr) room = joined[i];
            Check(joined[i] == room, "simultaneous joins share one room");
            (seats[i] == 0 ? seat0 : seat1)++;
        }
        Check(seat0 == 1 && seat1 == 1 && full == 6, "csRooms gives each seat to exactly one of simultaneous joins");
    }
    std::cout << "PASS: room join seats, full room, separate rooms and simultaneous joins\n";
}

void JoinResultTest()
{
    Server server;
    Check(server.InitServer(0), "listener for join results");
    AcceptRunner running(server);
    WaitUntil([&] { return server.IsAccepting(); }, "accept loop for join results");
    auto connectRaw = [&](Socket& s) {
        s.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(server.GetPort());
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Check(connect(s.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "raw client connects");
        const DWORD timeout = 2000;
        setsockopt(s.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    };
    auto join = [](Socket& s, int roomNo, const char* nick) {
        JoinRoomPacket packet; packet.roomNo = roomNo;
        std::memcpy(packet.nickname, nick, std::strlen(nick) + 1);
        Check(send(s.value, reinterpret_cast<const char*>(&packet), sizeof(packet), 0) == sizeof(packet), "send JOIN_ROOM");
    };
    auto receive = [](Socket& s) {
        JoinResultPacket packet;
        Check(recvn(s.value, reinterpret_cast<char*>(&packet), sizeof(packet), 0) == sizeof(packet) &&
              packet.size == sizeof(packet) && packet.type == PacketType::JOIN_RESULT, "receive JOIN_RESULT");
        return packet;
    };
    Socket first, second, lone;
    connectRaw(first); connectRaw(second); connectRaw(lone);
    join(first, 12, "red");
    join(second, 12, "피카츄피카");
    const JoinResultPacket firstReply = receive(first);
    const JoinResultPacket secondReply = receive(second);
    Socket& waiting = firstReply.myIndex == 0 ? first : second;
    const JoinResultPacket& waitingReply = firstReply.myIndex == 0 ? firstReply : secondReply;
    const JoinResultPacket& matchedReply = firstReply.myIndex == 0 ? secondReply : firstReply;
    Check(waitingReply.result == JoinResult::Waiting && waitingReply.myIndex == 0, "seat 0 first gets Waiting");
    Check(matchedReply.result == JoinResult::Matched && matchedReply.myIndex == 1, "seat 1 gets Matched");
    JoinResultPacket reply = receive(waiting);
    Check(reply.result == JoinResult::Matched && reply.myIndex == 0, "seat 0 is told about the match");

    auto receiveStart = [](Socket& s) {
        GameStartPacket packet;
        Check(recvn(s.value, reinterpret_cast<char*>(&packet), sizeof(packet), 0) == sizeof(packet) &&
            packet.size == 37 && packet.type == PacketType::GAME_START, "receive GAME_START after JOIN_RESULT");
        return packet;
    };
    const auto firstStart = receiveStart(first);
    const auto secondStart = receiveStart(second);
    Check(std::memcmp(&firstStart, &secondStart, sizeof(firstStart)) == 0 &&
        firstStart.stage == 1 && firstStart.serveSide == 0,
        "both players receive the same starting stage and serve side");
    Check(std::memcmp(firstStart.nick[firstReply.myIndex], "red", sizeof("red")) == 0 &&
        std::memcmp(firstStart.nick[secondReply.myIndex], "피카츄피카", MAX_NICK) == 0,
        "game start contains both nicknames in seat order, including a full UTF-8 nickname");

    auto receiveState = [](Socket& s) {
        GameStatePacket packet;
        Check(recvn(s.value, reinterpret_cast<char*>(&packet), sizeof(packet), 0) == sizeof(packet) &&
            packet.size == 49 && packet.type == PacketType::GAME_STATE, "receive GAME_STATE after GAME_START");
        return packet;
    };
    float previousBallY = static_cast<float>(GameConst::ServeY);
    for (std::uint32_t tick = 1; tick <= 4; ++tick)
    {
        const auto firstState = receiveState(first);
        const auto secondState = receiveState(second);
        Check(firstState.tick == tick && firstState.phase == Phase::Playing &&
            std::memcmp(&firstState, &secondState, sizeof(firstState)) == 0,
            "both players receive the same completed ticks in order");
        Check(firstState.p[0].x == GameConst::PlayerStartX[0] && firstState.p[1].x == GameConst::PlayerStartX[1] &&
            firstState.ball.x == GameConst::ServeX[0] && firstState.ball.y > previousBallY,
            "the packet includes both players and the updated ball position");
        previousBallY = firstState.ball.y;
    }

    join(lone, 13, "green");
    reply = receive(lone);
    Check(reply.result == JoinResult::Waiting && reply.myIndex == 0, "other room number waits alone");
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(lone.value, &readSet);
    const timeval immediate{};
    Check(select(0, &readSet, nullptr, nullptr, &immediate) == 0, "a different room does not receive match packets");

    auto receivePair = [&] {
        std::array<char, MAX_PACKET_SIZE> firstPacket{}, secondPacket{};
        auto read = [](SOCKET socket, auto& bytes) {
            PacketHeader header{};
            Check(recvn(socket, reinterpret_cast<char*>(&header), sizeof(header), 0) == sizeof(header) &&
                header.size >= sizeof(header) && header.size <= MAX_PACKET_SIZE && PacketSize(header.type) == header.size,
                "receive a valid frame during stage progression");
            std::memcpy(bytes.data(), &header, sizeof(header));
            const int body = header.size - sizeof(header);
            Check(recvn(socket, bytes.data() + sizeof(header), body, 0) == body, "receive complete result frame");
            return header;
        };
        const auto a = read(first.value, firstPacket);
        const auto b = read(second.value, secondPacket);
        Check(a.size == b.size && std::memcmp(firstPacket.data(), secondPacket.data(), a.size) == 0,
            "both clients receive identical match frames through both stages");
        return firstPacket;
    };
    for (int stage = 1; stage <= GameConst::StageCount; ++stage)
    {
        // Move both players away from the serve locations so each rally reaches the floor.
        InputPacket input; input.keys = LEFT;
        SendBytes(first.value, reinterpret_cast<const char*>(&input), sizeof(input));
        SendBytes(second.value, reinterpret_cast<const char*>(&input), sizeof(input));
        const ULONGLONG deadline = GetTickCount64() + 20000;
        int winner = -1;
        bool walkFrameReceived = false;
        GameStatePacket finalState;
        while (winner < 0)
        {
            Check(GetTickCount64() < deadline, "the actual room loop completes the stage");
            const auto bytes = receivePair();
            PacketHeader header{};
            std::memcpy(&header, bytes.data(), sizeof(header));
            if (header.type == PacketType::STAGE_OVER)
            {
                StageOverPacket result;
                std::memcpy(&result, bytes.data(), sizeof(result));
                Check(result.stage == stage && result.winner == (stage == 1 ? 1 : 0),
                    "natural floor scores produce the expected stage winner");
                winner = result.winner;
            }
            else if (header.type == PacketType::GAME_STATE)
            {
                GameStatePacket state;
                std::memcpy(&state, bytes.data(), sizeof(state));
                Check(state.p[0].evil == (stage == 2 ? 1 : 0) && state.p[1].evil == 0,
                    "only the first-stage loser awakens in the actual second stage");
                walkFrameReceived |= state.p[1].action == Action::Move && state.p[1].frame > 0;
                if (state.phase == Phase::StageOver || state.phase == Phase::GameOver)
                    finalState = state;
            }
            else Check(header.type == PacketType::EVENT || header.type == PacketType::SCORE,
                "stage frames contain state, events or scores before the result");
        }
        Check(walkFrameReceived, "both TCP peers receive changing player frames from the room");
        Check(finalState.phase == (stage == GameConst::StageCount ? Phase::GameOver : Phase::StageOver) &&
            finalState.p[winner].action == Action::Win && finalState.p[1 - winner].action == Action::Lose &&
            finalState.p[0].frame == 0 && finalState.p[1].frame == 0,
            "both TCP peers receive the server's initial winner and loser poses before the result");
        if (stage == 1)
        {
            NextStagePacket ready;
            SendBytes(first.value, reinterpret_cast<const char*>(&ready), sizeof(ready));
            SendBytes(first.value, reinterpret_cast<const char*>(&ready), sizeof(ready));
            FD_ZERO(&readSet); FD_SET(first.value, &readSet);
            const timeval brief{0, 30000};
            Check(select(0, &readSet, nullptr, nullptr, &brief) == 0,
                "the room does not start stage two after one client repeats readiness");
            SendBytes(second.value, reinterpret_cast<const char*>(&ready), sizeof(ready));
            const auto bytes = receivePair();
            GameStartPacket start;
            std::memcpy(&start, bytes.data(), sizeof(start));
            Check(start.type == PacketType::GAME_START && start.stage == 2 && start.serveSide == winner,
                "both NEXT_STAGE requests advance the real room loop with the winner serving");
        }
        else
        {
            const auto bytes = receivePair();
            GameOverPacket result;
            std::memcpy(&result, bytes.data(), sizeof(result));
            Check(result.type == PacketType::GAME_OVER && result.winner == winner &&
                result.reason == GameOverReason::Normal, "the real room loop sends the final result");
        }
    }
    running.Finish();
    std::cout << "PASS: JOIN_RESULT, GAME_START and matching GAME_STATE ticks for two TCP clients\n";
    std::cout << "PASS: full room loop through STAGE_OVER, two NEXT_STAGE requests and GAME_OVER over TCP\n";
}

void RoomRemoveTest()
{
    RoomManager rooms;
    std::array<ClientSession, 4> sessions;
    int seat = -1;
    std::uint8_t result = 0;
    Room* first = rooms.JoinRoom(1, &sessions[0], seat, result);
    rooms.JoinRoom(1, &sessions[1], seat, result);
    rooms.JoinRoom(2, &sessions[2], seat, result);
    Check(first != nullptr && rooms.GetRoomCount() == 2, "two rooms exist before removal");
    rooms.RemoveRoom(1);
    Check(rooms.GetRoomCount() == 1, "removed room leaves the list");
    rooms.RemoveRoom(1);
    rooms.RemoveRoom(99);
    Check(rooms.GetRoomCount() == 1, "removing a missing room changes nothing");
    Room* again = rooms.JoinRoom(1, &sessions[3], seat, result);
    Check(again != nullptr && seat == 0 && result == static_cast<std::uint8_t>(JoinResult::Waiting),
          "a removed room number starts as a new waiting room");
    rooms.RemoveRoom(1);
    rooms.RemoveRoom(2);
    Check(rooms.GetRoomCount() == 0, "all rooms removed");

    RoomManager shared;
    std::array<ClientSession, 64> players;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&, t] {
            for (int i = 0; i < 16; ++i)
            {
                int s = -1; std::uint8_t r = 0;
                const int roomNo = 100 + t * 16 + i;
                shared.JoinRoom(roomNo, &players[t * 16 + i], s, r);
                if (i % 2 == 0) shared.RemoveRoom(roomNo);
            }
        });
    for (auto& thread : threads) thread.join();
    Check(shared.GetRoomCount() == 32, "joins and removals from several threads keep the list consistent");
    std::cout << "PASS: room removal, missing rooms, reuse of a room number and concurrent join/remove\n";
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
    server.CloseServer(); server.AcceptLoop();
    (void)session.RecvPacket(nullptr); session.ProcessPacket(nullptr);
    (void)session.SendPacket(nullptr, 0); session.Disconnect();
    (void)ClientSession::ClientThread(nullptr); (void)Room::RoomThread(nullptr);
    int index; std::uint8_t result;
    RoomManager manager;
    Check(manager.JoinRoom(1, nullptr, index, result) == nullptr && index == -1, "no false room admission");
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
    std::cout << std::unitbuf;
    try
    {
        GameWorldScoreTest();
        GameWorldEventsTest();
        GameClientSceneTest(); RendererInitializationTest(); RendererFrameTest();
        ChatPanelLayoutTest();
        ServerConnectTest(); ServerAcceptTest();
    }
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
        ClientControlPacketTest(); ClientLeaveRoomTest(); ClientNextStageTest();
        ChatSendTest(); RoomChatTest(); GameClientChatTest();
        SessionDisconnectTest();
        RoomQuitTest();
        RoomQuitSuppressionTest();
        RoomStageOverTest();
        RoomGameOverTest();
        RoomNextStageTest();
        RoomEventsBroadcastTest();
        JoinResultTest();
        GameStateSnapshotTest();
        ProtocolTest(); FragmentedTest(); CoalescedTest(); ShortAndInvalidTest();
        ClientFailureTest(); GameClientJoinTest(); GameClientInputTest(); GameClientTimerTest(); GameClientNetEventTest();
        PlayerInputTest(); HorizontalMovementTest(); InputManagerTest();
        JumpTest(); SlideTest(); SessionThreadTest(); JoinRoomSendTest(); InputSendTest();
        SessionSendTest(); SessionSendFrameIntegrityTest(); ProcessPacketTest(); RoomJoinTest(); RoomRemoveTest();
        BallGravityTest(); BallWallTest(); BallNetTest(); BallPlayerHitTest(); BallWallAndHeadTest();
        BallPlayerWorldTest(); StageResetTest(); GameWorldTickTest();
        ClientReceiveTest(); ClientProcessPacketTest(); ClientSessionRoundTripTest(); LinkScaffolds();
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
