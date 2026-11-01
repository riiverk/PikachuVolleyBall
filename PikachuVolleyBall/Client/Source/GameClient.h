#pragma once
#include "NetworkClient.h"
#include "InputManager.h"
#include "Renderer.h"

enum class Scene { Connect, Waiting, Playing, StageOver, Result };

class GameClient
{
public:
    void ChangeScene(Scene scene);
    void OnKey(UINT msg, WPARAM key);
    void OnNetEvent(WPARAM type, LPARAM pkt);
    void OnTimer();
    Scene GetScene() const { return scene_; }
private:
    Scene scene_ = Scene::Connect;
    NetworkClient network_;
    InputManager input_;
    Renderer renderer_;
};
