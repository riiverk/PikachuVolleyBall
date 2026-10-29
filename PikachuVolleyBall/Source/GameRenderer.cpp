#include "GameRenderer.h"
#include "GameState.h"
#include "GameInitialization.h"
#include "GameUpdate.h"
#include "PlayerAnimation.h"
#include "BallPhysics.h"
#include "resource.h"

namespace
{
    HBITMAP hBitmap, warningBit, hBitmap2, hBit, oldBit[2], tempBmp;
}

void InitializeGame(HWND hWnd)
{
	HDC hDC, m1DC, m2DC;
	hDC = GetDC(hWnd);
	InitializeGameState(hWnd);

	hBitmap = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP1));
	warningBit = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP2));
	hBit = CreateCompatibleBitmap(hDC, rt.right, rt.bottom);

	m1DC = CreateCompatibleDC(hDC);
	SelectObject(m1DC, hBit);

	m2DC = CreateCompatibleDC(hDC);
	SelectObject(m2DC, hBitmap);

	DeleteDC(m1DC);
	DeleteDC(m2DC);
	ReleaseDC(hWnd, hDC);
	SetTimer(hWnd, MainScreen, 16, NULL);
}

void PaintScores(HWND hWnd)
{
	HDC hDC, m1DC;
	PAINTSTRUCT ps;

	hDC = BeginPaint(hWnd, &ps);
	m1DC = CreateCompatibleDC(hDC);
	SelectObject(m1DC, hBitmap);

	if ((stage_1 || stage_2) && !gameover_1 && !gameover_2)
	{
		// p1 점수
		if (p1.score >= 0 && p1.score <= 7)
			TransparentBlt(hDC, 50, 50, 70, 70, m1DC, 204 + (34 * p1.score), 124, 32, 32, RGB(255, 255, 255));
		else if (p1.score > 7 && p1.score <= 9)
			TransparentBlt(hDC, 50, 50, 70, 70, m1DC, 2 + (34 * (p1.score - 8)), 158, 32, 32, RGB(255, 255, 255));
		else if (p1.score > 9 && p1.score < 16)
		{
			TransparentBlt(hDC, 50, 50, 70, 70, m1DC, 238, 124, 32, 32, RGB(255, 255, 255));
			TransparentBlt(hDC, 90, 50, 70, 70, m1DC, 204 + (34 * (p1.score - 10)), 124, 32, 32, RGB(255, 255, 255));
		}
		// p2 점수
		if (p2.score >= 0 && p2.score <= 7)
			TransparentBlt(hDC, 980, 50, 70, 70, m1DC, 204 + (34 * p2.score), 124, 32, 32, RGB(255, 255, 255));
		else if (p2.score > 7 && p2.score <= 9)
			TransparentBlt(hDC, 980, 50, 70, 70, m1DC, 2 + (34 * (p2.score - 8)), 158, 32, 32, RGB(255, 255, 255));
		else if (p2.score > 9 && p2.score < 16)
		{
			TransparentBlt(hDC, 980, 50, 70, 70, m1DC, 238, 124, 32, 32, RGB(255, 255, 255));
			TransparentBlt(hDC, 1020, 50, 70, 70, m1DC, 204 + (34 * (p2.score - 10)), 124, 32, 32, RGB(255, 255, 255));
		}
	}

	SelectObject(m1DC, oldBit[0]);
	DeleteDC(m1DC);
	EndPaint(hWnd, &ps);
}

void RenderMainScreen(HWND hWnd)
{
	HDC hDC, m1DC, m2DC;

	hDC = GetDC(hWnd);
	m1DC = CreateCompatibleDC(hDC);
	SelectObject(m1DC, hBit);
	m2DC = CreateCompatibleDC(m1DC);
	SelectObject(m2DC, hBitmap);
	FillRect(m1DC, &rt, (HBRUSH)GetStockObject(WHITE_BRUSH));

	if (fSize >= 300 || fSize <= 250)
		fDir *= -1;
	if (wfX >= 460 || wfX <= 400)
		wfDir *= -1;

	fSize += 6 * fDir;
	wfX += 3 * wfDir;

	for (int i = 0; i < 20; ++i)
	{
		StretchBlt(m1DC, 0, 250 * i, 250, 250, m2DC, sittingP[i].Sx, sittingP[i].Sy, sittingP[i].w, sittingP[i].h, SRCCOPY);
		StretchBlt(m1DC, 250, 250 * i, 250, 250, m2DC, sittingP[i].Sx, sittingP[i].Sy, sittingP[i].w, sittingP[i].h, SRCCOPY);
		StretchBlt(m1DC, 500, 250 * i, 250, 250, m2DC, sittingP[i].Sx, sittingP[i].Sy, sittingP[i].w, sittingP[i].h, SRCCOPY);
		StretchBlt(m1DC, 750, 250 * i, 250, 250, m2DC, sittingP[i].Sx, sittingP[i].Sy, sittingP[i].w, sittingP[i].h, SRCCOPY);
		StretchBlt(m1DC, 1000, 250 * i, 250, 250, m2DC, sittingP[i].Sx, sittingP[i].Sy, sittingP[i].w, sittingP[i].h, SRCCOPY);
	}

	TransparentBlt(m1DC, 100, 30, fSize, fSize, m2DC, fight.Sx, fight.Sy, fight.w, fight.h, RGB(255, 255, 255));
	TransparentBlt(m1DC, 500, 100, 450, 80, m2DC, pokemon.Sx, pokemon.Sy, pokemon.w, pokemon.h, RGB(255, 255, 255));
	TransparentBlt(m1DC, 450, 210, 600, 170, m2DC, logo.Sx, logo.Sy, logo.w, logo.h, RGB(255, 255, 255));

	TransparentBlt(m1DC, 400, 500, wfX, 80, m2DC, with_Friend.Sx, with_Friend.Sy, with_Friend.w, with_Friend.h, RGB(255, 255, 255));

	TransparentBlt(m1DC, 100, 700, 900, 50, m2DC, mark.Sx, mark.Sy, mark.w, mark.h, RGB(255, 255, 255));

	BitBlt(hDC, 0, 0, rt.right, rt.bottom, m1DC, 0, 0, SRCCOPY);

	DeleteDC(m1DC);
	DeleteDC(m2DC);
	ReleaseDC(hWnd, hDC);
	InvalidateRect(hWnd, NULL, FALSE);
}

void RenderReadyScreen(HWND hWnd)
{
	HDC hDC, m1DC, m2DC;

	hDC = GetDC(hWnd);
	m1DC = CreateCompatibleDC(hDC);
	SelectObject(m1DC, hBit);
	m2DC = CreateCompatibleDC(m1DC);
	SelectObject(m2DC, hBitmap);

	UpdateReadyState(hWnd);

	TransparentBlt(m1DC, 470, 300, 200, 70, m2DC, readyToGame.Sx, readyToGame.Sy, readyToGame.w, readyToGame.h, RGB(255, 255, 255));
	BitBlt(hDC, 0, 0, rt.right, rt.bottom, m1DC, 0, 0, SRCCOPY);

	DeleteDC(m1DC);
	DeleteDC(m2DC);
	ReleaseDC(hWnd, hDC);
	InvalidateRect(hWnd, NULL, FALSE);
}

static void DrawCourt(HDC m1DC, HDC m2DC)
{
	if (stage_1)
		StretchBlt(m1DC, 0, 500, 1200, 150, m2DC, mountain.Sx, mountain.Sy, mountain.w, mountain.h, SRCCOPY);
	else
		StretchBlt(m1DC, 0, 500, 1200, 150, m2DC, mountain.Sx, mountain.Sy, mountain.w, mountain.h, NOTSRCCOPY);
	for (int i = 0; i < 23; ++i)
	{
		for (int j = 0; j < 10; ++j)
		{
			if (stage_1)
				StretchBlt(m1DC, 50 * i, 50 * j, 50, 50, m2DC, sky.Sx, sky.Sy, sky.w, sky.h, SRCCOPY);
			else
				StretchBlt(m1DC, 50 * i, 50 * j, 50, 50, m2DC, sky.Sx, sky.Sy, sky.w, sky.h, NOTSRCCOPY);
		}
		StretchBlt(m1DC, 50 * i, 647, 50, 50, m2DC, groundRed.Sx, groundRed.Sy, groundRed.w, groundRed.h, SRCCOPY);
		StretchBlt(m1DC, 50 * i, 747, 50, 50, m2DC, groundYellow.Sx, groundYellow.Sy, groundYellow.w, groundYellow.h, SRCCOPY);
		if (i >= 1 && i < 22) StretchBlt(m1DC, 50 * i, 697, 50, 50, m2DC, groundLine.Sx, groundLine.Sy, groundLine.w, groundLine.h, SRCCOPY);
		if (i == 0) StretchBlt(m1DC, 50 * i, 697, 50, 50, m2DC, groundLeft.Sx, groundLeft.Sy, groundLeft.w, groundLeft.h, SRCCOPY);
		else if (i == 22)StretchBlt(m1DC, 50 * i, 697, 50, 50, m2DC, groundRight.Sx, groundRight.Sy, groundRight.w, groundRight.h, SRCCOPY);
	}

	for (int i = 0; i < 7; ++i) {
		TransparentBlt(m1DC, cloud[i].x, cloud[i].y, 96, 48, m2DC, cloud[i].Sx, cloud[i].Sy, cloud[i].w, cloud[i].h, RGB(255, 255, 255));
		if (cloud[i].x > 1180)
			cloud[i].x = -100;
		cloud[i].x += cloud[i].vx;
	}

	for (int i = 0; i < 11; ++i)
		TransparentBlt(m1DC, 550, 725 - 25 * i, 25, 25, m2DC, net.Sx, net.Sy, net.w, net.h, RGB(255, 255, 255));
	TransparentBlt(m1DC, 550, 450, 25, 25, m2DC, net_Top.Sx, net_Top.Sy, net_Top.w, net_Top.h, RGB(255, 255, 255));
}

void RenderGameFrame(HWND hWnd)
{
	HDC hDC, m1DC, m2DC, tempDC;

	hDC = GetDC(hWnd);
	m1DC = CreateCompatibleDC(hDC);
	SelectObject(m1DC, hBit);
	tempDC = CreateCompatibleDC(m1DC);
	tempBmp = CreateCompatibleBitmap(m1DC, p2.w, p2.h);
	m2DC = CreateCompatibleDC(m1DC);
	SelectObject(m2DC, hBitmap);

	UpdateGameFrame(hWnd, m1DC, m2DC);
	DrawCourt(m1DC, m2DC);
	UpdatePlayerAnimation();

	// p1
	if (isSlide_Left_1)
	{
		SelectObject(tempDC, tempBmp);
		TransparentBlt(m1DC, p1.x + 35, 710, 100, 25, m2DC, shadow.Sx, shadow.Sy, shadow.w, shadow.h, RGB(255, 255, 255));
		StretchBlt(tempDC, 0, 0, p1.w, p1.h, m2DC, p1.Sx + p1.w - 1, p1.Sy, -p1.w, p1.h, SRCCOPY);
		TransparentBlt(m1DC, p1.x, p1.y, 170, 170, tempDC, 0, 0, p1.w, p1.h, RGB(255, 255, 255));
	}
	else
	{
		TransparentBlt(m1DC, p1.x + 40, 710, 100, 25, m2DC, shadow.Sx, shadow.Sy, shadow.w, shadow.h, RGB(255, 255, 255));
		TransparentBlt(m1DC, p1.x, p1.y, 170, 170, m2DC, p1.Sx, p1.Sy, p1.w, p1.h, RGB(255, 255, 255));
	}

	//p2
	if (isSlide_Right_2)
	{
		TransparentBlt(m1DC, p2.x + 40, 710, 100, 25, m2DC, shadow.Sx, shadow.Sy, shadow.w, shadow.h, RGB(255, 255, 255));
		TransparentBlt(m1DC, p2.x, p2.y, 170, 170, m2DC, p2.Sx, p2.Sy, p2.w, p2.h, RGB(255, 255, 255));
	}
	else
	{
		SelectObject(tempDC, tempBmp);
		TransparentBlt(m1DC, p2.x + 35, 710, 100, 25, m2DC, shadow.Sx, shadow.Sy, shadow.w, shadow.h, RGB(255, 255, 255));
		StretchBlt(tempDC, 0, 0, p2.w, p2.h, m2DC, p2.Sx + p2.w - 1, p2.Sy, -p2.w, p2.h, SRCCOPY);
		TransparentBlt(m1DC, p2.x, p2.y, 170, 170, tempDC, 0, 0, p2.w, p2.h, RGB(255, 255, 255));
	}

	if (!gameover_1 && !gameover_2)
	{
		if (spike) {
			Afterimages(ball.x, ball.y, ball.vx, ball.vy, afterimage);
			TransparentBlt(m1DC, afterimage[1].x, afterimage[1].y, 100, 100, m2DC, afterimage[1].Sx, afterimage[0].Sy, ball.w, ball.h, RGB(255, 255, 255));
			TransparentBlt(m1DC, afterimage[0].x, afterimage[0].y, 100, 100, m2DC, afterimage[0].Sx, afterimage[1].Sy, ball.w, ball.h, RGB(255, 255, 255));
		}
		TransparentBlt(m1DC, ball.x, ball.y, 100, 100, m2DC, ball.Sx, ball.Sy, ball.w, ball.h, RGB(255, 255, 255));
	}

	if (stage_2)
	{
		SelectObject(m2DC, warningBit);
		if (p1.evil)
			TransparentBlt(m1DC, p1.x + 100, p1.y - 40, 50, 50, m2DC, 0, 0, 512, 512, RGB(255, 255, 255));
		else if (p2.evil)
			TransparentBlt(m1DC, p2.x + 20, p2.y - 40, 50, 50, m2DC, 0, 0, 512, 512, RGB(255, 255, 255));
	}


	BitBlt(hDC, 0, 0, rt.right, rt.bottom, m1DC, 0, 0, SRCCOPY);

	DeleteDC(m1DC);
	DeleteDC(m2DC);
	DeleteDC(tempDC);
	ReleaseDC(hWnd, hDC);
}
