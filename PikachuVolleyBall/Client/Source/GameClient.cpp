#include "GameClient.h"
#include <limits>

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
    ReleaseInput();
    network_.Disconnect();
}

void GameClient::ChangeScene(Scene scene)
{
    if (scene_ == scene) return;

    scene_ = scene;
    input_.Reset();
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

void GameClient::OnNetEvent(WPARAM, LPARAM) { /* TODO 11/17 */ }
void GameClient::OnTimer() { /* TODO 11/18 */ }
