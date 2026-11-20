#include "GameClient.h"
#include <cstring>
#include <limits>

GameClient::~GameClient()
{
    StopRendering();
}

JoinRequestResult GameClient::ConnectAndJoin(const wchar_t* address, const wchar_t* room,
    const wchar_t* nickname, u_short port)
{
    char ip[INET_ADDRSTRLEN]{};
    if (!address || !*address || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        address, -1, ip, static_cast<int>(sizeof(ip)), nullptr, nullptr))
        return JoinRequestResult::InvalidAddress;

    int roomNumber = 0;
    if (!room) return JoinRequestResult::InvalidRoom;
    for (const wchar_t* cursor = room; *cursor; ++cursor)
    {
        const int digit = *cursor - L'0';
        if (digit < 0 || digit > 9 || roomNumber > (std::numeric_limits<int>::max() - digit) / 10)
            return JoinRequestResult::InvalidRoom;
        roomNumber = roomNumber * 10 + digit;
    }
    if (roomNumber == 0) return JoinRequestResult::InvalidRoom;

    char nick[MAX_NICK]{};
    if (!nickname || !*nickname || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        nickname, -1, nick, static_cast<int>(sizeof(nick)), nullptr, nullptr))
        return JoinRequestResult::InvalidNickname;

    if (port == 0) return JoinRequestResult::ConnectionFailed;
    if (!network_.Connect(ip, port))
        return network_.GetLastError() == WSAEINVAL ? JoinRequestResult::InvalidAddress : JoinRequestResult::ConnectionFailed;
    if (!network_.SendJoinRoom(roomNumber, nick))
    {
        network_.Disconnect();
        return JoinRequestResult::SendFailed;
    }
    input_.Reset();
    return JoinRequestResult::Sent;
}

void GameClient::Disconnect()
{
    StopRendering();
    ReleaseInput();
    network_.Disconnect();
    ChangeScene(Scene::Connect);
}

void GameClient::ChangeScene(Scene scene)
{
    if (scene_ == scene) return;

    if (scene != Scene::Playing) StopRendering();
    ReleaseInput();
    scene_ = scene;
}

void GameClient::OnKey(UINT msg, WPARAM key)
{
    if (!network_.IsConnected()) return;

    bool changed = false;
    switch (msg)
    {
    case WM_KEYDOWN: changed = input_.OnKeyDown(key); break;
    case WM_KEYUP: changed = input_.OnKeyUp(key); break;
    }
    if (changed) network_.SendInput(input_.GetKeys());
}

void GameClient::ReleaseInput()
{
    if (input_.GetKeys() == 0) return;
    input_.Reset();
    if (network_.IsConnected()) network_.SendInput(0);
}

NetEventResult GameClient::OnNetEvent(WPARAM type, LPARAM pkt)
{
    if (!pkt) return NetEventResult::Ignored;

    const auto data = reinterpret_cast<const char*>(pkt);
    switch (type)
    {
    case static_cast<WPARAM>(PacketType::JOIN_RESULT):
    {
        if (scene_ != Scene::Connect && scene_ != Scene::Waiting) break;

        JoinResultPacket reply;
        std::memcpy(&reply, data, sizeof(reply));
        switch (reply.result)
        {
        case JoinResult::Waiting:
        case JoinResult::Matched:
            if (reply.myIndex >= GameConst::PlayerCount) break;
            ChangeScene(Scene::Waiting);
            return reply.result == JoinResult::Waiting ? NetEventResult::Waiting : NetEventResult::Matched;
        case JoinResult::Full:
            return NetEventResult::RoomFull;
        case JoinResult::Rejected:
            return NetEventResult::JoinRejected;
        }
        break;
    }
    case static_cast<WPARAM>(PacketType::GAME_START):
    {
        if (scene_ != Scene::Waiting && scene_ != Scene::StageOver) break;

        GameStartPacket start;
        std::memcpy(&start, data, sizeof(start));
        if (start.stage == 0 || start.stage > GameConst::StageCount || start.serveSide >= GameConst::PlayerCount)
            break;
        ChangeScene(Scene::Playing);
        return NetEventResult::GameStarted;
    }
    }
    return NetEventResult::Ignored;
}
bool GameClient::StartRendering(HWND window)
{
    if (scene_ != Scene::Playing || !network_.IsConnected() || !IsWindow(window)) return false;
    StopRendering();
    if (!renderer_.Initialize(window)) return false;
    if (!SetTimer(window, GameConst::FrameTimerId, GameConst::TickMs, nullptr))
    {
        renderer_.Shutdown();
        return false;
    }
    renderWindow_ = window;
    return true;
}

void GameClient::StopRendering()
{
    if (renderWindow_) KillTimer(renderWindow_, GameConst::FrameTimerId);
    renderWindow_ = nullptr;
    renderer_.Shutdown();
}

bool GameClient::OnTimer()
{
    if (!renderWindow_ || scene_ != Scene::Playing || !network_.IsConnected()) return false;
    GameStatePacket state;
    if (!network_.GetLatestState(state)) return false;
    renderer_.Render(state);
    return true;
}
