#pragma once
#include "GameConst.h"
#include "PlayerInput.h"
#include <algorithm>
#include <stdexcept>

inline void MovePlayerHorizontally(PlayerState& player, const PlayerInput& input, int index)
{
    if (index < 0 || index >= GameConst::PlayerCount) throw std::out_of_range("player index");
    if (player.action == Action::Slide || player.action == Action::Win || player.action == Action::Lose) return;
    const int speed = player.evil ? GameConst::AwakenedMoveSpeed : GameConst::MoveSpeed;
    const int direction = std::clamp(input.moveDirection, -1, 1);
    const int minX = GameConst::PlayerMinX[index];
    const int maxX = GameConst::PlayerMaxX[index];
    const int position = std::clamp(static_cast<int>(player.x), minX, maxX);
    player.x = std::clamp(position + direction * speed, minX, maxX);
    if (player.action == Action::Idle || player.action == Action::Move)
        player.action = direction == 0 ? Action::Idle : Action::Move;
}
