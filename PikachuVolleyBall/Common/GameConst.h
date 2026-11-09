#pragma once
#include <cstdint>

// Logical coordinates preserve the original local game's rules and artwork.
namespace GameConst
{
inline constexpr int WindowWidth = 1160;
inline constexpr int WindowHeight = 830;
inline constexpr int TickMs = 16;
inline constexpr float TickSeconds = TickMs / 1000.0f;
inline constexpr int PlayerCount = 2;
inline constexpr int WinningScore = 5;
inline constexpr int StageCount = 2;
inline constexpr int PlayerStartX[PlayerCount] = {30, 950};
inline constexpr int PlayerGroundY = 560;
inline constexpr int PlayerMinX[PlayerCount] = {-15, 573};
inline constexpr int PlayerMaxX[PlayerCount] = {385, 990};
inline constexpr int MoveSpeed = 10;
inline constexpr int AwakenedMoveSpeed = 15;
inline constexpr int JumpGravity = 1;
inline constexpr int JumpPower = -25;
inline constexpr int SlideFrames = 40;
inline constexpr int SlideMinX[PlayerCount] = {-7, 577};
inline constexpr int SlideMaxX[PlayerCount] = {381, 986};
inline constexpr int SlideFastSpeed = 14;
inline constexpr int SlideSlowSpeed = 7;
inline constexpr int JumpFrames = 60;
inline constexpr int ServeX[PlayerCount] = {120, 980};
inline constexpr int ServeY = 0;
inline constexpr float ServeVelocityY = 400.0f;
inline constexpr int CourtLeft = 0;
inline constexpr int CourtRight = 1150;
inline constexpr int CourtBottom = 725;
inline constexpr int CeilingY = -25;
inline constexpr int NetLeft = 550;
inline constexpr int NetRight = 575;
inline constexpr int NetTop = 450;
inline constexpr int BallCollisionSize = 90;
inline constexpr float BallWallRestitution = 0.9f;
inline constexpr int BallCollisionInset = 10;
inline constexpr float BallNetHorizontalRestitution = 0.95f;
inline constexpr float BallNetVerticalRestitution = 0.9f;
inline constexpr float BallGravity = 1000.0f;
inline constexpr float BallTimeScale = 2.3f;
inline constexpr float HitForce = 1000.0f;
inline constexpr float SpikeForce = 1600.0f;
inline constexpr int PlayerHeadLeft = 30;
inline constexpr int PlayerHeadRight = 140;
inline constexpr int PlayerHeadHeight = 40;
inline constexpr int PlayerCenterX = 85;
inline constexpr int BallCenterX = 50;
inline constexpr float BallHitHorizontalRatio = 0.45f;
inline constexpr float AwakenedBallHitHorizontalRatio = 0.6f;
inline constexpr float AwakenedBallSpikeHorizontalRatio = 0.7f;
inline constexpr float BallHitVelocityY = -600.0f;
inline constexpr float BallSpikeVelocityY = -800.0f;
inline constexpr float AwakenedBallHitVelocityY = -800.0f;
inline constexpr float AwakenedBallSpikeVelocityY = -1000.0f;
inline constexpr float BallHitOffsetScale = 0.2f;
inline constexpr float AwakenedBallHitOffsetScale = 0.4f;
inline constexpr int ReadyFrames = 30;
inline constexpr int ReadyTimerMs = 70;
inline constexpr int RotationTimerMs = 100;
inline constexpr int ResultTimerMs = 150;
inline constexpr std::uintptr_t FrameTimerId = 1;
inline constexpr std::uintptr_t BallRotateTimerId = 1000;
inline constexpr std::uintptr_t GameOverTimerId = 2000;
inline constexpr std::uintptr_t MainScreenTimerId = 3000;
inline constexpr std::uintptr_t ReadyTimerId = 4000;
}
