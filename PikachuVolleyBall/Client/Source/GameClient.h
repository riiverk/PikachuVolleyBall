#pragma once
#include "NetworkClient.h"
#include "InputManager.h"
#include "Renderer.h"
#include "StageOverScreen.h"
#include <array>
#include <deque>
#include <string>

enum class Scene { Connect, Waiting, Playing, StageOver, Result };
enum class JoinRequestResult { Sent, InvalidAddress, InvalidRoom, InvalidNickname, ConnectionFailed, SendFailed };
enum class NetEventResult { Ignored, Waiting, Matched, RoomFull, JoinRejected, GameStarted, ScoreUpdated, ChatReceived, StageEnded, EffectPlayed };
enum class ChatSendResult { Sent, Empty, TooLong, InvalidText, NotInRoom, SendFailed };

class GameClient
{
public:
    ~GameClient();
    JoinRequestResult ConnectAndJoin(const wchar_t* address, const wchar_t* room, const wchar_t* nickname,
        u_short port = SERVERPORT);
    void SetWindow(HWND window) { network_.SetWindow(window); }
    bool IsConnected() const { return network_.IsConnected(); }
    void Disconnect();
    void ChangeScene(Scene scene);
    // Only Playing forwards movement/action keys; other scenes keep their UI input.
    void OnKey(UINT msg, WPARAM key);
    bool RequestNextStage();
    const StageResult& GetStageResult() const { return stageResult_; }
    void ReleaseInput();
    bool BeginChat();
    void EndChat() { chatting_ = false; }
    bool IsChatting() const { return chatting_; }
    ChatSendResult SendChat(const wchar_t* text);
    const std::deque<std::wstring>& GetChatHistory() const { return chatHistory_; }
    // The UI owns the packet and keeps it alive for the duration of this call.
    NetEventResult OnNetEvent(WPARAM type, LPARAM pkt);
    bool StartRendering(HWND window);
    void StopRendering();
    bool OnTimer();
    Scene GetScene() const { return scene_; }
private:
    friend struct GameClientTestAccess;
    bool PlayEffect(EventKind kind);
    void CloseEffects();

    Scene scene_ = Scene::Connect;
    NetworkClient network_;
    InputManager input_;
    Renderer renderer_;
    std::array<std::uint8_t, GameConst::PlayerCount> scores_{};
    HWND renderWindow_ = nullptr;
    std::uint8_t playerIndex_ = NO_PLAYER, stage_ = 0;
    StageResult stageResult_;
    GameStatePacket stageState_{};
    bool readyKeyDown_ = false;
    Phase renderedPhase_ = Phase::Ready;
    ULONGLONG resultStartedAt_ = 0;
    bool chatting_ = false;
    std::wstring joinedNickname_;
    std::array<std::wstring, GameConst::PlayerCount> nicknames_;
    std::deque<std::wstring> chatHistory_;
    std::array<UINT, 5> effectDevices_{};
};
