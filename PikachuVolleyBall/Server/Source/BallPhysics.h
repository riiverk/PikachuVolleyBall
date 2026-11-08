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

inline void ReflectBallAtWalls(BallState& ball)
{
    if (ball.x <= GameConst::CourtLeft)
    {
        ball.x = static_cast<float>(GameConst::CourtLeft);
        if (ball.vx < 0) ball.vx = -ball.vx * GameConst::BallWallRestitution;
    }
    else if (ball.x >= GameConst::CourtRight - GameConst::BallCollisionSize)
    {
        ball.x = static_cast<float>(GameConst::CourtRight - GameConst::BallCollisionSize);
        if (ball.vx > 0) ball.vx = -ball.vx * GameConst::BallWallRestitution;
    }
    if (ball.y <= GameConst::CeilingY)
    {
        ball.y = static_cast<float>(GameConst::CeilingY);
        if (ball.vy < 0) ball.vy = -ball.vy;
    }
}

inline void AdvanceBall(BallState& ball)
{
    MoveBallWithGravity(ball);
    ReflectBallAtWalls(ball);
}
