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

inline void StartPlayerSlide(PlayerState& player, const PlayerInput& input, int& slideCount, int& slideDirection)
{
    if (input.slideDirection == 0 || player.action == Action::Slide) return;
    if (player.action == Action::Win || player.action == Action::Lose) return;
    if (player.action == Action::Jump || player.action == Action::Spike) return;
    player.action = Action::Slide;
    slideCount = GameConst::SlideFrames;
    slideDirection = input.slideDirection < 0 ? -1 : 1;
}

inline void UpdatePlayerSlide(PlayerState& player, int index, int& slideCount, int& slideDirection)
{
    if (index < 0 || index >= GameConst::PlayerCount) throw std::out_of_range("player index");
    if (player.action != Action::Slide) return;
    if (slideCount <= 0)
    {
        player.y = GameConst::PlayerGroundY;
        player.action = Action::Idle;
        slideCount = 0;
        slideDirection = 0;
        return;
    }
    if (slideCount > 16)
    {
        player.y += slideCount > 30 ? -3 : 2;
        const int step = slideCount > 25 ? GameConst::SlideFastSpeed : GameConst::SlideSlowSpeed;
        if (slideDirection < 0 && player.x > GameConst::SlideMinX[index]) player.x -= step;
        if (slideDirection > 0 && player.x < GameConst::SlideMaxX[index]) player.x += step;
    }
    else player.y = GameConst::PlayerGroundY;
    --slideCount;
}

inline void UpdatePlayerJump(PlayerState& player, const PlayerInput& input, int& velocityY)
{
    if (player.action == Action::Slide || player.action == Action::Win || player.action == Action::Lose) return;
    if (input.jump && player.y == GameConst::PlayerGroundY &&
        player.action != Action::Jump && player.action != Action::Spike)
    {
        velocityY = GameConst::JumpPower;
        player.action = Action::Jump;
    }
    if (player.action != Action::Jump && player.action != Action::Spike) return;
    // The original spike stays active until landing, even after the key is released.
    if (input.spike) player.action = Action::Spike;
    player.y += velocityY;
    velocityY += GameConst::JumpGravity;
    if (player.y >= GameConst::PlayerGroundY)
    {
        player.y = GameConst::PlayerGroundY;
        velocityY = 0;
        player.action = input.moveDirection == 0 ? Action::Idle : Action::Move;
    }
}
