#pragma once
#include "NetworkClient.h"
#include "InputManager.h"
#include "Renderer.h"
#include <array>

enum class Scene { Connect, Waiting, Playing, StageOver, Result };
enum class JoinRequestResult { Sent, InvalidAddress, InvalidRoom, InvalidNickname, ConnectionFailed, SendFailed };
enum class NetEventResult { Ignored, Waiting, Matched, RoomFull, JoinRejected, GameStarted, ScoreUpdated };

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
    void ReleaseInput();
    // The UI owns the packet and keeps it alive for the duration of this call.
    NetEventResult OnNetEvent(WPARAM type, LPARAM pkt);
    bool StartRendering(HWND window);
    void StopRendering();
    bool OnTimer();
    Scene GetScene() const { return scene_; }
private:
    friend struct GameClientTestAccess;
    Scene scene_ = Scene::Connect;
    NetworkClient network_;
    InputManager input_;
    Renderer renderer_;
    std::array<std::uint8_t, GameConst::PlayerCount> scores_{};
    HWND renderWindow_ = nullptr;
};
