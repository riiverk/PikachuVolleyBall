#include "GameClient.h"

void GameClient::ChangeScene(Scene scene)
{
    if (scene_ == scene) return;

    scene_ = scene;
    input_.Reset();
}

void GameClient::OnKey(UINT, WPARAM) { /* TODO 11/19 */ }
void GameClient::OnNetEvent(WPARAM, LPARAM) { /* TODO 11/17 */ }
void GameClient::OnTimer() { /* TODO 11/18 */ }
