#include "GameUpdate.h"
#include "GameState.h"
#include "PlayerMovement.h"
#include "BallPhysics.h"
#include <mmsystem.h>

void ResetRoundIfNeeded()
{
	if ((gameset_1 || gameset_2) && smaller > 12) {
		if (gameset_1) {
			ball.x = GameConst::ServeX[0];
			gameset_1 = false;
		}
		if (gameset_2) {
			ball.x = GameConst::ServeX[1];
			gameset_2 = false;
		}
		ball.y = GameConst::ServeY;
		ball.vx = 0;
		ball.vy = GameConst::ServeVelocityY;
		p1.x = GameConst::PlayerStartX[0]; p1.y = GameConst::PlayerGroundY;
		p2.x = GameConst::PlayerStartX[1]; p2.y = GameConst::PlayerGroundY;
		spike = false;
	}
}

void UpdateBallRotation()
{
	if (ball.vx > 0) {
		if (ball.Sx > 256)
			ball.Sx = 88;
		else
			ball.Sx += 42;
	}
	else if (ball.vx < 0) {
		if (ball.Sx == 88)
			ball.Sx = 256;
		else
			ball.Sx -= 42;
	}
	else
		ball.Sx = 88;
}

void UpdateReadyState(HWND hWnd)
{
	if (readyCnt == 0)
	{
		ready = false;
		KillTimer(hWnd, Ready);
		SetTimer(hWnd, GameConst::FrameTimerId, GameConst::TickMs, NULL);
	}

	p1.x = GameConst::PlayerStartX[0]; p1.y = GameConst::PlayerGroundY;
	p2.x = GameConst::PlayerStartX[1]; p2.y = GameConst::PlayerGroundY;

	readyCnt -= 1;
}

void UpdateGameFrame(HWND hWnd, HDC m1DC, HDC m2DC)
{
	if ((p1.score == GameConst::WinningScore || p2.score == GameConst::WinningScore) && stage_1 && !gameover_1)
	{
		gameover_1 = true;
		winPlayer = p1.score > p2.score ? 1 : 2;
		if (winPlayer == 1) p2.evil = true;
		else p1.evil = true;
		p1.overCnt = 5;
		p2.overCnt = 5;
		KillTimer(hWnd, GameConst::FrameTimerId);
		SetTimer(hWnd, GameConst::FrameTimerId, GameConst::ResultTimerMs, NULL);
		PlaySound(TEXT("Resources\\sounds\\WAVE143_1.wav"), NULL, SND_LOOP | SND_ASYNC);
	}

	if ((p1.score == GameConst::WinningScore || p2.score == GameConst::WinningScore) && stage_2 && !gameover_2)
	{
		gameover_2 = true;
		winPlayer = p1.score > p2.score ? 1 : 2;
		if (winPlayer == 1) p2.evil = true;
		else p1.evil = true;
		p1.overCnt = 5;
		p2.overCnt = 5;
		KillTimer(hWnd, GameConst::FrameTimerId);
		SetTimer(hWnd, GameConst::FrameTimerId, GameConst::ResultTimerMs, NULL);
		PlaySound(TEXT("Resources\\sounds\\WAVE143_1.wav"), NULL, SND_LOOP | SND_ASYNC);
	}

	if (ready) KillTimer(hWnd, GameConst::FrameTimerId);

	if (isR || isUp) moveJump();
	if (isD || isLeft) moveLeft();
	if (isSlide_Left_1 || isSlide_Left_2 || isSlide_Right_1 || isSlide_Right_2) moveSlide();
	if (isG || isRight) moveRight();

	if (stage_1)
	{
		if (!gameset_1 && !gameset_2)
			BallMove_1();
		else {
			TransparentBlt(m1DC, ball.x, ball.y - 110, 40 - smaller, 40 - smaller, m2DC, 340, ball.Sy, ball.w, ball.h, RGB(255, 255, 255));
			smaller += 3;
		}
	}
	else if (stage_2)
	{
		if (!gameset_1 && !gameset_2)
			BallMove_2();
		else {
			TransparentBlt(m1DC, ball.x, ball.y - 110, 40 - smaller, 40 - smaller, m2DC, 340, ball.Sy, ball.w, ball.h, RGB(255, 255, 255));
			smaller += 3;
		}
	}

	if (!isSlide_Left_2 && !isSlide_Right_2 && !isUp)
	{
		if (keyLeftDown)
			isLeft = true;
		else if (keyRightDown)
			isRight = true;
		else
		{
			isLeft = false;
			isRight = false;
		}
	}

	if (!isSlide_Left_1 && !isSlide_Right_1 && !isR)
	{
		if (keyDDown)
			isD = true;
		else if (keyGDown)
			isG = true;
		else
		{
			isD = false;
			isG = false;
		}
	}
}
