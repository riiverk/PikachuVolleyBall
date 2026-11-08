#pragma once
#include "GameConst.h"
#include "Protocol.h"
#include <algorithm>
#include <limits>
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

inline bool ReflectBallAtNet(BallState& ball, const BallState& previous)
{
    // Expand the net by the original ball collider (x/y + 10 through + 90).
    // Sweeping the ball's top-left point also catches a whole-net crossing in one tick.
    constexpr float left = GameConst::NetLeft - GameConst::BallCollisionSize;
    constexpr float right = GameConst::NetRight - GameConst::BallCollisionInset;
    constexpr float top = GameConst::NetTop - GameConst::BallCollisionSize;
    constexpr float bottom = GameConst::CourtBottom - GameConst::BallCollisionInset;
    enum Face { Left, Right, Top, Bottom };
    Face face;
    auto inside = [](const BallState& b) {
        return b.x > left && b.x < right && b.y > top && b.y < bottom;
    };
    if (inside(previous))
    {
        // Recover an overlapping state without turning a ball that is already leaving.
        if (!inside(ball)) return false;
        const float distance[] = {ball.x - left, right - ball.x, ball.y - top, bottom - ball.y};
        face = static_cast<Face>(std::min_element(distance, distance + 4) - distance);
    }
    else
    {
        const float dx = ball.x - previous.x;
        const float dy = ball.y - previous.y;
        float enterX, exitX, enterY, exitY;
        auto sweepAxis = [](float start, float delta, float low, float high, float& enter, float& exit) {
            if (delta == 0)
            {
                // Parallel travel on the edge is a graze, not an impact.
                if (start <= low || start >= high) return false;
                enter = -std::numeric_limits<float>::infinity();
                exit = std::numeric_limits<float>::infinity();
            }
            else
            {
                const float a = (low - start) / delta;
                const float b = (high - start) / delta;
                enter = std::min(a, b);
                exit = std::max(a, b);
            }
            return true;
        };
        if (!sweepAxis(previous.x, dx, left, right, enterX, exitX) ||
            !sweepAxis(previous.y, dy, top, bottom, enterY, exitY)) return false;
        const float enter = std::max(enterX, enterY);
        const float exit = std::min(exitX, exitY);
        if (enter < 0 || enter > 1 || enter >= exit) return false;
        face = enterX > enterY ? (dx > 0 ? Left : Right) : (dy > 0 ? Top : Bottom);
        ball.x = previous.x + dx * enter;
        ball.y = previous.y + dy * enter;
    }

    bool incoming;
    switch (face)
    {
    case Left: ball.x = left; incoming = ball.vx > 0; break;
    case Right: ball.x = right; incoming = ball.vx < 0; break;
    case Top: ball.y = top; incoming = ball.vy > 0; break;
    default: ball.y = bottom; incoming = ball.vy < 0; break;
    }
    if (incoming)
    {
        ball.vx *= GameConst::BallNetHorizontalRestitution;
        ball.vy *= GameConst::BallNetVerticalRestitution;
        if (face == Left || face == Right) ball.vx = -ball.vx;
        else ball.vy = -ball.vy;
    }
    return true;
}

inline void AdvanceBall(BallState& ball)
{
    const BallState previous = ball;
    MoveBallWithGravity(ball);
    ReflectBallAtNet(ball, previous);
    ReflectBallAtWalls(ball);
}
