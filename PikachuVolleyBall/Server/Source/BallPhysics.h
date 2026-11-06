#pragma once
#include "GameConst.h"
#include "Protocol.h"
#include <stdexcept>

inline void ResetBallForServe(BallState& ball, int serveSide)
{
    if (serveSide < 0 || serveSide >= GameConst::PlayerCount) throw std::out_of_range("serve side");
    ball = BallState{};
    ball.x = static_cast<float>(GameConst::ServeX[serveSide]);
    ball.y = static_cast<float>(GameConst::ServeY);
    ball.vy = GameConst::ServeVelocityY;
}

inline void MoveBallWithGravity(BallState& ball)
{
    // Match the original game's order and time scale: gravity, then position.
    ball.vy += GameConst::BallGravity * GameConst::TickSeconds;
    ball.x += ball.vx * GameConst::TickSeconds * GameConst::BallTimeScale;
    ball.y += ball.vy * GameConst::TickSeconds * GameConst::BallTimeScale;
}
