#include "GameInput.h"
#include "GameState.h"
#include <mmsystem.h>

void HandleKeyDown(HWND hWnd, WPARAM wParam)
{
	if (!gameover_1 && !gameover_2)
	{
		if (wParam == VK_UP)
		{
			if (!isUp && !isSlide_Left_2 && !isSlide_Right_2)
			{
				mciSendString(L"open \"Resources\\sounds\\WAVE142_1.wav\" type mpegvideo alias effect", NULL, 0, NULL);
				mciSendString(L"play effect from 0", NULL, 0, NULL);
				isUp = true;
				velocityY_2 = jumpPower;
				p2.jumpCnt = GameConst::JumpFrames;
			}
		}
		else if (wParam == VK_LEFT)
		{
			keyLeftDown = true;
			if (!isSlide_Left_2 && !isSlide_Right_2)
				isLeft = true;
		}
		else if (wParam == VK_RIGHT)
		{
			keyRightDown = true;
			if (!isSlide_Left_2 && !isSlide_Right_2)
				isRight = true;
		}
		if (wParam == VK_RETURN)
		{
			if (mainScreen)
			{
				PlaySound(TEXT("Resources\\sounds\\WAVE144_1.wav"), NULL, SND_SYNC);
				mciSendString(L"open \"Resources\\sounds\\bgm.wav\" type mpegvideo alias bgm", NULL, 0, NULL);
				mciSendString(L"play bgm repeat", NULL, 0, NULL);
				ball.x = GameConst::ServeX[1];
				ready = true;
				gameset_2 = true;
				stage_1 = true;
				mainScreen = false;
				KillTimer(hWnd, MainScreen);
				SetTimer(hWnd, GameConst::FrameTimerId, GameConst::TickMs, NULL);
				SetTimer(hWnd, Ready, GameConst::ReadyTimerMs, NULL);
				SetTimer(hWnd, BallRotate, GameConst::RotationTimerMs, NULL);
			}
			if (isLeft && !isSlide_Right_2 && !isUp)
			{
				isSlide_Left_2 = true;
				isLeft = false;
				isRight = false;
				p2.slideCnt = GameConst::SlideFrames;
			}
			else if (isRight && !isSlide_Left_2 && !isUp)
			{
				isSlide_Right_2 = true;
				isLeft = false;
				isRight = false;
				p2.slideCnt = GameConst::SlideFrames;
			}
			else if (isUp && !isSlide_Left_2 && !isSlide_Right_2 && !isEnter)
			{
				PlaySound(TEXT("Resources\\sounds\\WAVE141_1.wav"), NULL, SND_ASYNC);
				isEnter = true;
				spikeCnt_2 = 0;
			}
		}
		if (wParam == 'R')
		{
			if (!isR && !isSlide_Left_1 && !isSlide_Right_1)
			{
				mciSendString(L"open \"Resources\\sounds\\WAVE142_1.wav\" type mpegvideo alias effect", NULL, 0, NULL);
				mciSendString(L"play effect from 0", NULL, 0, NULL);
				isR = true;
				velocityY_1 = jumpPower;
				p1.jumpCnt = GameConst::JumpFrames;
			}
		}
		else if (wParam == 'D')
		{
			keyDDown = true;
			if (!isSlide_Left_1 && !isSlide_Right_1)
				isD = true;
		}
		else if (wParam == 'G')
		{
			keyGDown = true;
			if (!isSlide_Left_1 && !isSlide_Right_1)
				isG = true;
		}
		if (wParam == 'Z')
		{
			if (mainScreen)
			{
				PlaySound(TEXT("Resources\\sounds\\WAVE144_1.wav"), NULL, SND_SYNC);
				mciSendString(L"open \"Resources\\sounds\\bgm.wav\" type mpegvideo alias bgm", NULL, 0, NULL);
				mciSendString(L"play bgm repeat", NULL, 0, NULL);
				ready = true;
				stage_1 = true;
				mainScreen = false;
				gameset_1 = true;
				KillTimer(hWnd, MainScreen);
				SetTimer(hWnd, GameConst::FrameTimerId, GameConst::TickMs, NULL);
				SetTimer(hWnd, Ready, GameConst::ReadyTimerMs, NULL);
				SetTimer(hWnd, BallRotate, GameConst::RotationTimerMs, NULL);
			}
			if (isD && !isSlide_Right_1 && !isR)
			{
				isSlide_Left_1 = true;
				isD = false;
				isG = false;
				p1.slideCnt = GameConst::SlideFrames;
			}
			else if (isG && !isSlide_Left_1 && !isR)
			{
				isSlide_Right_1 = true;
				isD = false;
				isG = false;
				p1.slideCnt = GameConst::SlideFrames;
			}
			if (isR && !isSlide_Left_1 && !isSlide_Right_1 && !isZ)
			{
				PlaySound(TEXT("Resources\\sounds\\WAVE141_1.wav"), NULL, SND_ASYNC);
				isZ = true;
				spikeCnt_1 = 0;
			}
		}
	}
	else if (gameover_1)
	{
		if (wParam == VK_RETURN || wParam == 'Z')
		{
			PlaySound(NULL, 0, 0);
			gameover_1 = false;
			ready = true;
			stage_1 = false;
			stage_2 = true;
			p1.score = 0;
			p2.score = 0;
			readyCnt = GameConst::ReadyFrames;
			p1.x = GameConst::PlayerStartX[0]; p1.y = GameConst::PlayerGroundY;
			p2.x = GameConst::PlayerStartX[1]; p2.y = GameConst::PlayerGroundY;
			winPlayer = 0;
			if (wParam == VK_RETURN)
			{
				ball.x = GameConst::ServeX[1]; ball.y = GameConst::ServeY;
			}
			else
			{
				ball.x = GameConst::ServeX[0]; ball.y = GameConst::ServeY;
			}
			SetTimer(hWnd, GameConst::FrameTimerId, GameConst::TickMs, NULL);
			SetTimer(hWnd, Ready, GameConst::ReadyTimerMs, NULL);
			SetTimer(hWnd, BallRotate, GameConst::RotationTimerMs, NULL);
			mciSendString(L"stop bgm", NULL, 0, NULL);
			mciSendString(L"close bgm", NULL, 0, NULL);

			PlaySound(TEXT("Resources\\sounds\\WAVE144_1.wav"), NULL, SND_SYNC);

			mciSendString(L"open \"Resources\\sounds\\Stage2_Bgm.wav\" type mpegvideo alias bgm", NULL, 0, NULL);
			mciSendString(L"play bgm repeat", NULL, 0, NULL);
		}
	}
	else if (gameover_2)
	{
		if (wParam == VK_RETURN || wParam == 'Z')
		{
			PlaySound(NULL, 0, 0);
			gameover_2 = false;
			mainScreen = true;
			stage_1 = false;
			stage_2 = false;
			p1.score = 0;
			p2.score = 0;
			readyCnt = GameConst::ReadyFrames;
			p1.x = GameConst::PlayerStartX[0]; p1.y = GameConst::PlayerGroundY;
			p2.x = GameConst::PlayerStartX[1]; p2.y = GameConst::PlayerGroundY;
			winPlayer = 0;
			if (wParam == VK_RETURN)
			{
				ball.x = GameConst::ServeX[1]; ball.y = GameConst::ServeY;
			}
			else
			{
				ball.x = GameConst::ServeX[0]; ball.y = GameConst::ServeY;
			}
			KillTimer(hWnd, GameConst::FrameTimerId);
			KillTimer(hWnd, BallRotate);
			SetTimer(hWnd, MainScreen, GameConst::TickMs, NULL);
			mciSendString(L"stop bgm", NULL, 0, NULL);
			mciSendString(L"close bgm", NULL, 0, NULL);
		}
	}
	InvalidateRect(hWnd, NULL, FALSE);
}

void HandleKeyUp(WPARAM wParam)
{
	if (!gameover_1 && !gameover_2)
	{
		if (wParam == VK_LEFT)
		{
			keyLeftDown = false;
			isLeft = false;
		}
		else if (wParam == VK_RIGHT)
		{
			keyRightDown = false;
			isRight = false;
		}

		if (wParam == 'D')
		{
			keyDDown = false;
			isD = false;
		}
		else if (wParam == 'G')
		{
			keyGDown = false;
			isG = false;
		}
	}
}
