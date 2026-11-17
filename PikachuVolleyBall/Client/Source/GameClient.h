#pragma once
#include "NetworkClient.h"
#include "InputManager.h"
#include "Renderer.h"

enum class Scene { Connect, Waiting, Playing, StageOver, Result };
enum class JoinRequestResult { Sent, InvalidAddress, InvalidRoom, InvalidNickname, ConnectionFailed, SendFailed };

class GameClient
{
public:
    JoinRequestResult ConnectAndJoin(const wchar_t* address, const wchar_t* room, const wchar_t* nickname,
        u_short port = SERVERPORT);
    bool IsConnected() const { return network_.IsConnected(); }
    void Disconnect();
    void ChangeScene(Scene scene);
    void OnKey(UINT msg, WPARAM key);
    void ReleaseInput();
    void OnNetEvent(WPARAM type, LPARAM pkt);
    void OnTimer();
    Scene GetScene() const { return scene_; }
private:
    Scene scene_ = Scene::Connect;
    NetworkClient network_;
    InputManager input_;
    Renderer renderer_;
};
