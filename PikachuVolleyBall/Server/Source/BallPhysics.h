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

inline bool ReflectBallAtNet(BallState& ball, const BallState& previous, float* contactTime = nullptr)
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
        if (contactTime) *contactTime = 0;
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
        if (contactTime) *contactTime = enter;
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

inline bool FindPlayerHeadContact(const BallState& ball, const BallState& previous,
    const PlayerState& player, const PlayerState& previousPlayer, float& contactTime)
{
    if (player.action == Action::Win || player.action == Action::Lose) return false;
    const float startGap = previous.y + GameConst::BallCollisionSize - previousPlayer.y;
    const float endGap = ball.y + GameConst::BallCollisionSize - player.y;
    // Relative motion matters: a rising head can catch a stationary or rising ball.
    if (endGap <= startGap || endGap < 0) return false;
    if (startGap > 0)
    {
        if (previous.y + GameConst::BallCollisionInset >= previousPlayer.y + GameConst::PlayerHeadHeight)
            return false; // A ball underneath the head must not teleport onto it.
        contactTime = 0; // Recover a shallow overlap only while moving into the head.
    }
    else contactTime = -startGap / (endGap - startGap);
    const float ballX = previous.x + (ball.x - previous.x) * contactTime;
    const float playerX = previousPlayer.x + (player.x - previousPlayer.x) * contactTime;
    return ballX + GameConst::BallCollisionSize > playerX + GameConst::PlayerHeadLeft &&
        ballX + GameConst::BallCollisionInset < playerX + GameConst::PlayerHeadRight;
}

inline bool ReflectBallAtPlayers(BallState& ball, const BallState& previous,
    const PlayerState (&players)[GameConst::PlayerCount],
    const PlayerState (&previousPlayers)[GameConst::PlayerCount], float& contactTime)
{
    int hitter = -1;
    float earliest = 2;
    for (int i = 0; i < GameConst::PlayerCount; ++i)
    {
        float time;
        if (FindPlayerHeadContact(ball, previous, players[i], previousPlayers[i], time) && time < earliest)
        {
            hitter = i;
            earliest = time;
        }
    }
    if (hitter < 0) return false;
    const PlayerState& player = players[hitter];
    const PlayerState& previousPlayer = previousPlayers[hitter];
    ball.x = previous.x + (ball.x - previous.x) * earliest;
    // Keep the ball above the head's final position when the player is jumping.
    ball.y = static_cast<float>(player.y - GameConst::BallCollisionSize);
    const float playerX = previousPlayer.x + (player.x - previousPlayer.x) * earliest;
    const bool spike = player.action == Action::Spike;
    const bool awakened = player.evil != 0;
    const float ratio = awakened ? (spike ? GameConst::AwakenedBallSpikeHorizontalRatio :
        GameConst::AwakenedBallHitHorizontalRatio) : GameConst::BallHitHorizontalRatio;
    const float offsetScale = awakened ? GameConst::AwakenedBallHitOffsetScale : GameConst::BallHitOffsetScale;
    ball.vx = (hitter == 0 ? 1.0f : -1.0f) * (spike ? GameConst::SpikeForce : GameConst::HitForce) * ratio;
    ball.vx += (ball.x + GameConst::BallCenterX - playerX - GameConst::PlayerCenterX) * offsetScale;
    ball.vy = awakened ? (spike ? GameConst::AwakenedBallSpikeVelocityY : GameConst::AwakenedBallHitVelocityY) :
        (spike ? GameConst::BallSpikeVelocityY : GameConst::BallHitVelocityY);
    ball.spike = spike ? 1 : 0;
    contactTime = earliest;
    return true;
}

inline void AdvanceBall(BallState& ball)
{
    const BallState previous = ball;
    MoveBallWithGravity(ball);
    ReflectBallAtNet(ball, previous);
    ReflectBallAtWalls(ball);
}

inline void AdvanceBall(BallState& ball, const PlayerState (&players)[GameConst::PlayerCount],
    const PlayerState (&previousPlayers)[GameConst::PlayerCount])
{
    const BallState previous = ball;
    MoveBallWithGravity(ball);
    BallState netBall = ball, headBall = ball;
    float netTime = 2, headTime = 2;
    const bool hitNet = ReflectBallAtNet(netBall, previous, &netTime);
    const bool hitHead = ReflectBallAtPlayers(headBall, previous, players, previousPlayers, headTime);
    // Resolve the first contact so a later obstacle cannot overwrite the earlier hit.
    if (hitHead && (!hitNet || headTime < netTime)) ball = headBall;
    else if (hitNet) ball = netBall;
    ReflectBallAtWalls(ball);
}
