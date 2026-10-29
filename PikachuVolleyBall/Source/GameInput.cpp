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
				p2.jumpCnt = 60;
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
				ball.x = 980;
				ready = true;
				gameset_2 = true;
				stage_1 = true;
				mainScreen = false;
				KillTimer(hWnd, MainScreen);
				SetTimer(hWnd, 1, 16, NULL);
				SetTimer(hWnd, Ready, 70, NULL);
				SetTimer(hWnd, BallRotate, 100, NULL);
			}
			if (isLeft && !isSlide_Right_2 && !isUp)
			{
				isSlide_Left_2 = true;
				isLeft = false;
				isRight = false;
				p2.slideCnt = 40;
			}
			else if (isRight && !isSlide_Left_2 && !isUp)
			{
				isSlide_Right_2 = true;
				isLeft = false;
				isRight = false;
				p2.slideCnt = 40;
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
				p1.jumpCnt = 60;
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
				SetTimer(hWnd, 1, 16, NULL);
				SetTimer(hWnd, Ready, 70, NULL);
				SetTimer(hWnd, BallRotate, 100, NULL);
			}
			if (isD && !isSlide_Right_1 && !isR)
			{
				isSlide_Left_1 = true;
				isD = false;
				isG = false;
				p1.slideCnt = 40;
			}
			else if (isG && !isSlide_Left_1 && !isR)
			{
				isSlide_Right_1 = true;
				isD = false;
				isG = false;
				p1.slideCnt = 40;
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
			readyCnt = 30;
			p1.x = 30; p1.y = 560;
			p2.x = 950; p2.y = 560;
			winPlayer = 0;
			if (wParam == VK_RETURN)
			{
				ball.x = 980; ball.y = 0;
			}
			else
			{
				ball.x = 120; ball.y = 0;
			}
			SetTimer(hWnd, 1, 16, NULL);
			SetTimer(hWnd, Ready, 70, NULL);
			SetTimer(hWnd, BallRotate, 100, NULL);
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
			readyCnt = 30;
			p1.x = 30; p1.y = 560;
			p2.x = 950; p2.y = 560;
			winPlayer = 0;
			if (wParam == VK_RETURN)
			{
				ball.x = 980; ball.y = 0;
			}
			else
			{
				ball.x = 120; ball.y = 0;
			}
			KillTimer(hWnd, 1);
			KillTimer(hWnd, BallRotate);
			SetTimer(hWnd, MainScreen, 16, NULL);
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
