#include "GameClient.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <cwctype>

namespace
{
bool DecodeText(const char* text, int capacity, std::wstring& out)
{
    int length = 0;
    while (length < capacity && text[length] != '\0') ++length;
    if (length == 0 || length == capacity) return false;
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, nullptr, 0);
    if (!count) return false;
    out.resize(count);
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, out.data(), count) == count;
}
}

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
    joinedNickname_ = nickname;
    return JoinRequestResult::Sent;
}

void GameClient::Disconnect()
{
    StopRendering();
    ReleaseInput();
    network_.Disconnect();
    ChangeScene(Scene::Connect);
    scores_.fill(0);
    EndChat();
    chatHistory_.clear();
    nicknames_.fill({});
    joinedNickname_.clear();
}

void GameClient::ChangeScene(Scene scene)
{
    if (scene_ == scene) return;

    if (scene != Scene::Playing) StopRendering();
    ReleaseInput();
    EndChat();
    scene_ = scene;
}

void GameClient::OnKey(UINT msg, WPARAM key)
{
    if (chatting_ || scene_ != Scene::Playing || !network_.IsConnected()) return;

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

bool GameClient::BeginChat()
{
    if (scene_ == Scene::Connect || !network_.IsConnected()) return false;
    ReleaseInput();
    chatting_ = true;
    return true;
}

ChatSendResult GameClient::SendChat(const wchar_t* text)
{
    if (scene_ == Scene::Connect || !network_.IsConnected()) return ChatSendResult::NotInRoom;
    if (!text || !*text) return ChatSendResult::Empty;
    bool blank = true;
    for (const wchar_t* cursor = text; *cursor; ++cursor)
    {
        if (*cursor < L' ') return ChatSendResult::InvalidText;
        if (!std::iswspace(*cursor)) blank = false;
    }
    if (blank) return ChatSendResult::Empty;
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, nullptr, 0, nullptr, nullptr);
    if (bytes == 0) return ChatSendResult::InvalidText;
    if (bytes > MAX_CHAT) return ChatSendResult::TooLong;
    char utf8[MAX_CHAT]{};
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, utf8, MAX_CHAT, nullptr, nullptr))
        return ChatSendResult::InvalidText;
    return network_.SendChat(utf8) ? ChatSendResult::Sent : ChatSendResult::SendFailed;
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
            if (scene_ == Scene::Connect)
            {
                chatHistory_.clear();
                nicknames_.fill({});
                nicknames_[reply.myIndex] = joinedNickname_;
            }
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
        scores_.fill(0);
        for (int i = 0; i < GameConst::PlayerCount; ++i)
        {
            std::wstring nickname;
            if (DecodeText(start.nick[i], MAX_NICK, nickname)) nicknames_[i] = std::move(nickname);
        }
        ChangeScene(Scene::Playing);
        return NetEventResult::GameStarted;
    }
    case static_cast<WPARAM>(PacketType::SCORE):
    {
        if (scene_ != Scene::Playing) break;

        ScorePacket score;
        std::memcpy(&score, data, sizeof(score));
        if (score.scorer >= GameConst::PlayerCount ||
            score.score[0] > GameConst::WinningScore || score.score[1] > GameConst::WinningScore)
            break;
        // The server sends both totals. Copy them rather than counting received notifications.
        std::copy_n(score.score, scores_.size(), scores_.begin());
        return NetEventResult::ScoreUpdated;
    }
    case static_cast<WPARAM>(PacketType::CHAT):
    {
        if (scene_ == Scene::Connect) break;
        ChatPacket chat;
        std::memcpy(&chat, data, sizeof(chat));
        std::wstring message;
        if (chat.sender >= GameConst::PlayerCount || !DecodeText(chat.msg, MAX_CHAT, message) ||
            std::any_of(message.begin(), message.end(), [](wchar_t ch) { return ch < L' '; }))
            break;
        const std::wstring name = nicknames_[chat.sender].empty()
            ? L"플레이어 " + std::to_wstring(chat.sender + 1) : nicknames_[chat.sender];
        if (chatHistory_.size() == 100) chatHistory_.pop_front();
        chatHistory_.push_back(name + L": " + message);
        return NetEventResult::ChatReceived;
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
    renderer_.Render(state, scores_, GameConst::ChatPanelHeight);
    return true;
}
