#include "Common.h"
#include "../../Server/Source/Server.h"
#include "../../Server/Source/ClientSession.h"
#include "../../Server/Source/Room.h"
#include "../../Server/Source/PlayerMovement.h"
#include "../../Server/Source/BallPhysics.h"
#include "../../Client/Source/GameClient.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

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
    for (int tick = 0; tick < 100; ++tick) world.Update(keys);
    world.FillState(state);
    Check(state.p[0].x == GameConst::PlayerMaxX[0] && state.p[1].x == GameConst::PlayerMinX[1], "players cannot cross the net");
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
    int minimumY = state.p[0].y;
    bool landed = false;
    for (int tick = 1; tick <= 70; ++tick)
    {
        world.Update(keys);
        world.FillState(state);
        minimumY = std::min(minimumY, static_cast<int>(state.p[0].y));
        Check(state.p[0].y <= 560, "jump never goes below the ground");
        if (state.p[0].y == 560) { landed = true; break; }
    }
    Check(landed && minimumY == 235 && state.p[0].action == Action::Move, "gravity, jump apex and walking landing");
    for (int tick = 0; tick < 5; ++tick) world.Update(keys);
    world.FillState(state);
    Check(state.p[0].y == 560, "holding up across landing must not trigger another jump");
    keys[0] = 0; world.Update(keys);
    keys[0] = UP; world.Update(keys); world.FillState(state);
    Check(state.p[0].y == 535, "release and press starts a second jump with reset velocity");
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
        Check(recv(stream.sender.value, &tail, 1, 0) == 0, "peer sees the connection closed");
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
    char tail;
    Check(recv(receiver.value, &tail, 1, 0) == 0, "invalid requests must not leave extra wire data");
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
    Check(state.p[0].action == Action::Slide && state.p[0].x == 44 && state.p[0].y == 557,
          "slide starts on ACTION press with a direction and moves 14px without walking");
    int ticks = 1, minimumY = state.p[0].y;
    while (state.p[0].action == Action::Slide && ticks < 60)
    {
        world.Update(keys);
        world.FillState(state);
        minimumY = std::min(minimumY, static_cast<int>(state.p[0].y));
        ++ticks;
    }
    Check(ticks == 41 && state.p[0].action == Action::Idle, "slide lasts 40 moving ticks plus the ending tick");
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
    AdvanceBall(ball);
    Check(ball.x == 0 && ball.y == -25 && close(ball.vx, 180) && ball.vy == 284,
        "tick resolves wall and ceiling overshoot after applying gravity");
    AdvanceBall(ball);
    Check(ball.x > 0 && ball.y > -25 && close(ball.vx, 180) && ball.vy == 300,
        "next tick moves away from the corner without repeated damping");
    ball = BallState{}; ball.x = 100; ball.y = 800; ball.vy = 100;
    AdvanceBall(ball);
    Check(ball.y > 800 && ball.vy == 116, "wall reflection does not add a floor bounce");
    std::cout << "PASS: ball wall/ceiling reflection, damping, exact contact and tick integration\n";
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

void ClientProcessPacketTest()
{
    const HWND window = CreateWindowExA(0, "STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    Check(window != nullptr, "create message-only window");
    NetworkClient client;
    client.ProcessPacket(nullptr);

    GameStatePacket state; state.tick = 77; state.phase = Phase::Playing; state.p[1].x = 940; state.ball.x = 120.5f;
    client.ProcessPacket(reinterpret_cast<char*>(&state));
    GameStatePacket latest;
    client.GetLatestState(latest);
    Check(std::memcmp(&latest, &state, sizeof(state)) == 0, "GAME_STATE stores the latest state");

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
    client.GetLatestState(latest);
    Check(latest.tick == 0, "connect clears the previous state");
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
    std::cout << std::unitbuf;
    try { ServerConnectTest(); ServerAcceptTest(); }
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
        ClientFailureTest(); PlayerInputTest(); HorizontalMovementTest(); InputManagerTest();
        JumpTest(); SlideTest(); SessionThreadTest(); JoinRoomSendTest(); InputSendTest();
        SessionSendTest(); SessionSendFrameIntegrityTest(); SessionDisconnectTest(); BallGravityTest(); BallWallTest(); ClientReceiveTest(); ClientProcessPacketTest(); ClientSessionRoundTripTest(); LinkScaffolds();
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
