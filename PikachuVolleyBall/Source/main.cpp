#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <random>
#include "resource.h"
#include <mmsystem.h>
#define BallRotate 1000
#define MainScreen 3000
#define GameOver 2000
#define Ready 4000
#pragma comment (lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

using namespace std;

bool isR = false;
bool isD = false;
bool isG = false;
bool isZ = false;

bool isUp = false;
bool isLeft = false;
bool isRight = false;
bool isEnter = false;

bool isSlide_Left_1 = false;
bool isSlide_Right_1 = false;
bool isSlide_Left_2 = false;
bool isSlide_Right_2 = false;

bool keyLeftDown = false;
bool keyRightDown = false;
bool keyDDown = false;
bool keyGDown = false;

int velocityY_1 = 0;
int velocityY_2 = 0;
const int gravity = 1;
const int jumpPower = -25;

bool mainScreen = true;
bool stage_1 = false;
bool stage_2 = false;

bool ready = false;
bool gameover_1 = false;
bool gameover_2 = false;

int cnt = 0, prev_loc[9][2];
RECT rt;
//float g = 60.0f;
float tick = 0.016f;
bool spike = false;
bool gameset_1 = false, gameset_2 = false;
float center, prev_vy, prev_vx;
int smaller = 0;

random_device rd;
mt19937 gen(rd());

HINSTANCE g_hInst;
LPCTSTR lpszClass = L"Window Class Name";
LPCTSTR lpszWindowName = L"Pikachu_VolleyBall";

struct Pikachu
{
	int Sx, Sy, w, h; // 스프라이트 시트 위치
	int x, y;
	int moveCnt;
	int jumpCnt;
	int slideCnt;
	int score;
	int overCnt;
	bool evil = false;
};

struct BackGround
{
	int Sx, Sy, w, h; // 스프라이트 시트 위치
};

struct Ball
{
	int Sx, Sy, w, h; // 스프라이트 시트 위치
	float x, y;
	float vx = 0, vy = 0;
};

Pikachu p1, p2;
Ball ball, afterimage[2];

BackGround mountain;
BackGround sky;
BackGround groundRed;
BackGround groundYellow;
BackGround groundLine;
BackGround groundRight;
BackGround groundLeft;
BackGround shadow;
BackGround net_Top;
BackGround net;
BackGround sittingP[20];
BackGround fight;
BackGround with_Friend;
BackGround logo;
BackGround pokemon;
BackGround mark;
BackGround readyToGame;
Ball cloud[7];
int fSize = 250;
int fDir = -1;
int wfX = 450;
int wfDir = -1;
int readyCnt = 30;

int winPlayer = 0;

void moveJump()
{
	if (isR)
	{
		p1.y += velocityY_1;
		velocityY_1 += gravity;

		if (p1.y >= 560)  // 바닥 도착
		{
			p1.y = 560;
			isR = false;
			isZ = false;
			velocityY_1 = 0;
			p1.jumpCnt = 0;
		}
	}
	if (isUp)
	{
		p2.y += velocityY_2;
		velocityY_2 += gravity;

		if (p2.y >= 560)  // 바닥 도착
		{
			p2.y = 560;
			isUp = false;
			isEnter = false;
			velocityY_2 = 0;
			p2.jumpCnt = 0;
		}
	}
}

void moveSlide()
{
	if (isSlide_Left_1)
	{
		if (p1.slideCnt == 0)
		{
			p1.slideCnt = 0;
			isSlide_Left_1 = false;
		}
		if (p1.slideCnt > 16)
		{
			if (p1.slideCnt > 30) p1.y -= 3;
			else p1.y += 2;
			if (p1.slideCnt > 25)
			{
				if (p1.x > -7)
					p1.x -= 14;
			}
			else
			{
				if (p1.x > -7)
					p1.x -= 7;
			}
		}
		else p1.y = 560;
	}
	if (isSlide_Left_2)
	{
		if (p2.slideCnt == 0)
		{
			p2.slideCnt = 0;
			isSlide_Left_2 = false;
		}
		if (p2.slideCnt > 16)
		{
			if (p2.slideCnt > 30) p2.y -= 3;
			else p2.y += 2;
			if (p2.slideCnt > 25)
			{
				if (p2.x > 577)
					p2.x -= 14;
			}
			else
			{
				if (p2.x > 577)
					p2.x -= 7;
			}
		}
		else p2.y = 560;
	}
	if (isSlide_Right_1)
	{
		if (p1.slideCnt == 0)
		{
			p1.slideCnt = 0;
			isSlide_Right_1 = false;
		}
		if (p1.slideCnt > 16)
		{
			if (p1.slideCnt > 30) p1.y -= 3;
			else p1.y += 2;
			if (p1.slideCnt > 25)
			{
				if (p1.x < 381)
					p1.x += 14;
			}
			else
			{
				if (p1.x < 381)
					p1.x += 7;
			}
		}
		else p1.y = 560;
	}
	if (isSlide_Right_2)
	{
		if (p2.slideCnt == 0)
		{
			p2.slideCnt = 0;
			isSlide_Right_2 = false;
		}
		if (p2.slideCnt > 16)
		{
			if (p2.slideCnt > 30) p2.y -= 3;
			else p2.y += 2;
			if (p2.slideCnt > 25)
			{
				if (p2.x < 986)
					p2.x += 14;
			}
			else
			{
				if (p2.x < 986)
					p2.x += 7;
			}
		}
		else p2.y = 560;
	}
}

void moveLeft()
{
	if (isD && p1.x > -15)
	{
		if (!p1.evil)
			p1.x -= 10;
		else
			p1.x -= 15;
	}
	if (isLeft && p2.x > 573)
	{
		if (!p2.evil)
			p2.x -= 10;
		else
			p2.x -= 15;
	}
}

void moveRight()
{
	if (isG && p1.x < 385)
	{
		if (!p1.evil)
			p1.x += 10;
		else
			p1.x += 15;
	}
	if (isRight && p2.x < 990)
	{
		if (!p2.evil)
			p2.x += 10;
		else
			p2.x += 15;
	}
}

void BallMove_1() {
	int bottom = 725, left = 0, right = 1150;
	const float gravity = 1000.0f;
	float p_Force = 1000.0f;
	float s_Force = 1600.0f;
	static float spike_vx = 0, spike_vy = 0;
	static bool hit_top = false;
	bool hit_p1 = false, hit_p2 = false, hit_net = false;
	RECT p1_rect = { p1.x + 30, p1.y, p1.x + 140, p1.y + 40 };
	RECT p2_rect = { p2.x + 30, p2.y, p2.x + 140, p2.y + 40 };
	RECT ball_rect = { (int)ball.x, (int)ball.y, (int)ball.x + 100, (int)ball.y + 100 };
	RECT net_rect = { 550, 450, 575, 725 };

	if (ball_rect.top <= -25) {
		if (!hit_top) {
			ball.y = -25;
			ball.vy = abs(ball.vy);
			hit_top = true;
		}
	}
	else if (ball.y > 0)
		hit_top = false;

	ball.vy += gravity * tick;
	ball.x += ball.vx * tick * 2.3f;
	ball.y += ball.vy * tick * 2.3f;

	ball_rect.left = (int)ball.x + 10;
	ball_rect.top = (int)ball.y + 10;
	ball_rect.right = (int)ball.x + 90;
	ball_rect.bottom = (int)ball.y + 90;

	if (ball.y >= bottom - 90) {
		smaller = 0;
		ball.y = bottom - 90;
		ball.vy = -ball.vy * 0.9f;
		if (ball.x < 550) {
			++p2.score;
			PlaySound(TEXT("Resources\\sounds\\WAVE146_1.wav"), NULL, SND_SYNC);
			gameset_2 = true;
			cnt = 0;
		}
		else if (ball.x > 575) {
			++p1.score;
			PlaySound(TEXT("Resources\\sounds\\WAVE146_1.wav"), NULL, SND_SYNC);
			gameset_1 = true;
			cnt = 0;
		}
		if (spike) {
			ball.vy = prev_vy;
			ball.vx = prev_vx;
			spike = false;
		}
	}

	if (ball.x <= left) {
		ball.x = left;
		ball.vx = -ball.vx * 0.9f;
	}
	else if (ball.x + 90 >= right) {
		ball.x = right - 90;
		ball.vx = -ball.vx * 0.9f;
	}

	if (ball_rect.right > net_rect.left && ball_rect.left < net_rect.right &&
		ball_rect.bottom > net_rect.top && ball_rect.top < net_rect.bottom) {
		if (ball.x < 550)
			ball.x = 460;
		else
			ball.x = 565;
		ball.vy = abs(ball.vy) * 0.9f;
		ball.vx = -ball.vx * 0.95f;
		hit_net = true;
	}
	else
		hit_net = false;

	if (ball_rect.right > p1_rect.left && ball_rect.left < p1_rect.right &&
		ball_rect.bottom > p1.y && ball_rect.top < p1.y + 40) {
		hit_p1 = true;
		ball.y = p1.y - 90;
		if (spike) {
			ball.vy = prev_vy;
			ball.vx = prev_vx;
			spike = false;
		}
		if (isZ) {
			spike = true;
			if (ball.x < 550)
				ball.vx = s_Force * 0.45f;
			else
				ball.vx = -s_Force * 0.45f;
		}
		else {
			if (ball.x < 550)
				ball.vx = p_Force * 0.45f;
			else
				ball.vx = -p_Force * 0.45f;
		}
		if (spike)
			ball.vy = -800.0f;
		else
			ball.vy = -600.0f;
		float center = p1.x + 85;
		ball.vx += (ball.x + 50 - center) * 0.2f;
	}

	if (ball_rect.right > p2_rect.left && ball_rect.left < p2_rect.right &&
		ball_rect.bottom > p2.y && ball_rect.top < p2.y + 40) {
		hit_p2 = true;
		ball.y = p2.y - 90;
		if (spike) {
			ball.vy = prev_vy;
			ball.vx = prev_vx;
			spike = false;
		}
		if (isEnter) {
			spike = true;
			if (ball.x < 550)
				ball.vx = s_Force * 0.45f;
			else
				ball.vx = -s_Force * 0.45f;
		}
		else {
			if (ball.x < 550)
				ball.vx = p_Force * 0.45f;
			else
				ball.vx = -p_Force * 0.45f;
		}
		if (spike)
			ball.vy = -800.0f;
		else
			ball.vy = -600.0f;
		float center = p2.x + 85;
		ball.vx += (ball.x + 50 - center) * 0.2f;
	}
}

void BallMove_2() {
	int bottom = 725, left = 0, right = 1150;
	const float gravity = 1000.0f;
	float p_Force = 1000.0f;
	float s_Force = 1600.0f;
	static float spike_vx = 0, spike_vy = 0;
	static bool hit_top = false;
	bool hit_p1 = false, hit_p2 = false, hit_net = false;
	RECT p1_rect = { p1.x + 30, p1.y, p1.x + 140, p1.y + 40 };
	RECT p2_rect = { p2.x + 30, p2.y, p2.x + 140, p2.y + 40 };
	RECT ball_rect = { (int)ball.x, (int)ball.y, (int)ball.x + 100, (int)ball.y + 100 };
	RECT net_rect = { 550, 450, 575, 725 };

	if (ball_rect.top <= -25) {
		if (!hit_top) {
			ball.y = -25;
			ball.vy = abs(ball.vy);
			hit_top = true;
		}
	}
	else if (ball.y > 0)
		hit_top = false;

	ball.vy += gravity * tick;
	ball.x += ball.vx * tick * 2.3f;
	ball.y += ball.vy * tick * 2.3f;

	ball_rect.left = (int)ball.x + 10;
	ball_rect.top = (int)ball.y + 10;
	ball_rect.right = (int)ball.x + 90;
	ball_rect.bottom = (int)ball.y + 90;

	if (ball.y >= bottom - 90) {
		smaller = 0;
		ball.y = bottom - 90;
		ball.vy = -ball.vy * 0.9f;
		if (ball.x < 550) {
			++p2.score;
			PlaySound(TEXT("Resources\\sounds\\WAVE146_1.wav"), NULL, SND_SYNC);
			gameset_2 = true;
			cnt = 0;
		}
		else if (ball.x > 575) {
			++p1.score;
			PlaySound(TEXT("Resources\\sounds\\WAVE146_1.wav"), NULL, SND_SYNC);
			gameset_1 = true;
			cnt = 0;
		}
		if (spike) {
			ball.vy = prev_vy;
			ball.vx = prev_vx;
			spike = false;
		}
	}

	if (ball.x <= left) {
		ball.x = left;
		ball.vx = -ball.vx * 0.9f;
	}
	else if (ball.x + 90 >= right) {
		ball.x = right - 90;
		ball.vx = -ball.vx * 0.9f;
	}

	if (ball_rect.right > net_rect.left && ball_rect.left < net_rect.right &&
		ball_rect.bottom > net_rect.top && ball_rect.top < net_rect.bottom) {
		if (ball.x < 550)
			ball.x = 460;
		else
			ball.x = 565;
		ball.vy = abs(ball.vy) * 0.9f;
		ball.vx = -ball.vx * 0.95f;
		hit_net = true;
	}
	else
		hit_net = false;

	if (ball_rect.right > p1_rect.left && ball_rect.left < p1_rect.right &&
		ball_rect.bottom > p1.y && ball_rect.top < p1.y + 40) {
		hit_p1 = true;
		float center = p1.x + 85;
		ball.y = p1.y - 90;
		if (!p1.evil) {
			if (spike) {
				ball.vy = prev_vy;
				ball.vx = prev_vx;
				spike = false;
			}
			if (isZ) {
				spike = true;
				if (ball.x < 550)
					ball.vx = s_Force * 0.45f;
				else
					ball.vx = -s_Force * 0.45f;
			}
			else {
				if (ball.x < 550)
					ball.vx = p_Force * 0.45f;
				else
					ball.vx = -p_Force * 0.45f;
			}
			if (spike)
				ball.vy = -800.0f;
			else
				ball.vy = -600.0f;
			ball.vx += (ball.x + 50 - center) * 0.2f;
		}
		else {
			if (ball_rect.right > p1_rect.left && ball_rect.left < p1_rect.right &&
				ball_rect.bottom > p1.y && ball_rect.top < p1.y + 40) {
				hit_p1 = true;
				ball.y = p1.y - 90;
				if (spike) {
					ball.vy = prev_vy;
					ball.vx = prev_vx;
					spike = false;
				}
				if (isZ) {
					spike = true;
					if (ball.x < 550)
						ball.vx = s_Force * 0.7f;
					else
						ball.vx = -s_Force * 0.7f;
				}
				else {
					if (ball.x < 550)
						ball.vx = p_Force * 0.6f;
					else
						ball.vx = -p_Force * 0.6f;
				}
				if (spike)
					ball.vy = -1000.0f;
				else
					ball.vy = -800.0f;
				ball.vx += (ball.x + 50 - center) * 0.4f;
			}
		}
	}

	if (ball_rect.right > p2_rect.left && ball_rect.left < p2_rect.right &&
		ball_rect.bottom > p2.y && ball_rect.top < p2.y + 40) {
		hit_p2 = true;
		ball.y = p2.y - 90;
		float center = p2.x + 85;
		if (!p2.evil) {
			if (spike) {
				ball.vy = prev_vy;
				ball.vx = prev_vx;
				spike = false;
			}
			if (isEnter) {
				spike = true;
				if (ball.x < 550)
					ball.vx = s_Force * 0.45f;
				else
					ball.vx = -s_Force * 0.45f;
			}
			else {
				if (ball.x < 550)
					ball.vx = p_Force * 0.45f;
				else
					ball.vx = -p_Force * 0.45f;
			}
			if (spike)
				ball.vy = -800.0f;
			else
				ball.vy = -600.0f;
			float center = p2.x + 85;
			ball.vx += (ball.x + 50 - center) * 0.2f;
		}
		else {
			if (ball_rect.right > p2_rect.left && ball_rect.left < p2_rect.right &&
				ball_rect.bottom > p2.y && ball_rect.top < p2.y + 40) {
				hit_p2 = true;
				ball.y = p2.y - 90;
				if (spike) {
					ball.vy = prev_vy;
					ball.vx = prev_vx;
					spike = false;
				}
				if (isEnter) {
					spike = true;
					if (ball.x < 550)
						ball.vx = s_Force * 0.7f;
					else
						ball.vx = -s_Force * 0.7f;
				}
				else {
					if (ball.x < 550)
						ball.vx = p_Force * 0.6f;
					else
						ball.vx = -p_Force * 0.6f;
				}
				if (spike)
					ball.vy = -1000.0f;
				else
					ball.vy = -800.0f;
				float center = p2.x + 85;
				ball.vx += (ball.x + 50 - center) * 0.4f;
			}
		}
	}
}

void Afterimages(float ball_x, float ball_y, float ball_vx, float ball_vy, Ball afterimage[]) {
	float vec = sqrt(ball_vx * ball_vx + ball_vy * ball_vy);

	if (vec < 0.0001f) {
		afterimage[0].x = ball_x;
		afterimage[0].y = ball_y;
		afterimage[1].x = ball_x;
		afterimage[1].y = ball_y;
		return;
	}

	float vec_vx = ball_vx / vec;
	float vec_vy = ball_vy / vec;
	float distance = 40.0f;

	afterimage[0].x = ball_x - vec_vx * distance;
	afterimage[0].y = ball_y - vec_vy * distance;

	afterimage[1].x = ball_x - vec_vx * distance * 2.0f;
	afterimage[1].y = ball_y - vec_vy * distance * 2.0f;

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
	HWND hWnd;
	MSG Message;
	WNDCLASSEX WndClass;
	g_hInst = hInstance;
	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = (WNDPROC)WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = lpszClass;
	WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&WndClass);
	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW, 0, 0, 1160, 830, NULL, (HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	while (GetMessage(&Message, 0, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	return Message.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hDC, m1DC, m2DC, tempDC;
	PAINTSTRUCT ps;
	HBRUSH oldBrush;
	HPEN oldPen;
	static HBITMAP hBitmap, warningBit, hBitmap2, hBit, oldBit[2], tempBmp;
	// static RECT rt;
	static int direct = 1;
	static int direct2 = 1;
	static int spikeCnt_1 = 0;
	static int spikeCnt_2 = 0;
	int center[2] = { ball.x - 50, ball.y + 50 };

	if (gameset_1 || gameset_2 && smaller > 12) {
		if (gameset_1) {
			ball.x = 120;
			gameset_1 = false;
		}
		if (gameset_2) {
			ball.x = 980;
			gameset_2 = false;
		}
		ball.y = 0;
		ball.vx = 0;
		ball.vy = 400;
		p1.x = 30; p1.y = 560;
		p2.x = 950; p2.y = 560;
		spike = false;
	}

	switch (iMsg)
	{
	case WM_CREATE:
	{
		hDC = GetDC(hWnd);
		GetClientRect(hWnd, &rt);
		mountain.Sx = 20; mountain.Sy = 200; mountain.w = 432; mountain.h = 64;
		sky.Sx = 156; sky.Sy = 2; sky.w = 16; sky.h = 16;
		groundRed.Sx = 120; groundRed.Sy = 2; groundRed.w = 16; groundRed.h = 16;
		groundYellow.Sx = 138; groundYellow.Sy = 2; groundYellow.w = 16; groundYellow.h = 16;
		groundLine.Sx = 66; groundLine.Sy = 2; groundLine.w = 16; groundLine.h = 16;
		groundRight.Sx = 102; groundRight.Sy = 2; groundRight.w = 16; groundRight.h = 16;
		groundLeft.Sx = 84; groundLeft.Sy = 2; groundLeft.w = 16; groundLeft.h = 16;
		shadow.Sx = 32; shadow.Sy = 2; shadow.w = 32; shadow.h = 8;
		net.Sx = 12; net.Sy = 2; net.w = 8; net.h = 8;
		net_Top.Sx = 22; net_Top.Sy = 2; net_Top.w = 8; net_Top.h = 8;
		p1.Sx = 134; p1.Sy = 266; p1.w = 62; p1.h = 64;
		p2.Sx = 134; p2.Sy = 266; p2.w = 62; p2.h = 64;
		p1.x = 30; p1.y = 560;
		p2.x = 950; p2.y = 560;
		p1.moveCnt = 2; p2.moveCnt = 2;
		p1.score = 0;
		p2.score = 0;
		ball.Sx = 88; ball.Sy = 158; ball.w = 40; ball.h = 40;
		ball.x = 120; ball.y = 0; ball.vx = 0; ball.vy = 400;
		fight.Sx = 254; fight.Sy = 723; fight.w = 160; fight.h = 160;
		logo.Sx = 2; logo.Sy = 611; logo.w = 276; logo.h = 79;
		pokemon.Sx = 2; pokemon.Sy = 124; pokemon.w = 200; pokemon.h = 32;
		with_Friend.Sx = 2; with_Friend.Sy = 64; with_Friend.w = 120; with_Friend.h = 20;
		mark.Sx = 2; mark.Sy = 20; mark.w = 360; mark.h = 20;
		readyToGame.Sx = 222; readyToGame.Sy = 64; readyToGame.w = 80; readyToGame.h = 24;
		for (int i = 0; i < 20; ++i)
		{
			sittingP[i].Sx = 280; sittingP[i].Sy = 611; sittingP[i].w = 104; sittingP[i].h = 104;
		}
		afterimage[0].Sx = 298; afterimage[0].Sy = 158; afterimage[0].w = 40; afterimage[0].h = 40;
		afterimage[1].Sx = 382; afterimage[1].Sy = 158; afterimage[1].w = 40; afterimage[1].h = 40;
		afterimage[0].x = afterimage[1].x = ball.x; afterimage[0].y = afterimage[1].y = ball.y;
		for (int i = 0; i < 7; ++i)
		{
			cloud[i].y = rd() % 400;
			cloud[i].x = rd() % 400;
			cloud[i].vx = rd() % 5 + 1;
			cloud[i].Sx = 100;
			cloud[i].Sy = 90;
			cloud[i].w = 48;
			cloud[i].h = 24;
		}

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
		return 0;
	}
	case WM_PAINT:
	{
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
		break;
	}
	case WM_KEYDOWN:
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
		break;
	case WM_TIMER:
		switch (wParam)
		{
		case BallRotate:
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
			break;
		}
		case MainScreen:
		{
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
			break;
		}
		case Ready:
		{
			hDC = GetDC(hWnd);
			m1DC = CreateCompatibleDC(hDC);
			SelectObject(m1DC, hBit);
			m2DC = CreateCompatibleDC(m1DC);
			SelectObject(m2DC, hBitmap);

			if (readyCnt == 0)
			{
				ready = false;
				KillTimer(hWnd, Ready);
				SetTimer(hWnd, 1, 16, NULL);
			}

			p1.x = 30; p1.y = 560;
			p2.x = 950; p2.y = 560;

			readyCnt -= 1;

			TransparentBlt(m1DC, 470, 300, 200, 70, m2DC, readyToGame.Sx, readyToGame.Sy, readyToGame.w, readyToGame.h, RGB(255, 255, 255));
			BitBlt(hDC, 0, 0, rt.right, rt.bottom, m1DC, 0, 0, SRCCOPY);

			DeleteDC(m1DC);
			DeleteDC(m2DC);
			ReleaseDC(hWnd, hDC);
			InvalidateRect(hWnd, NULL, FALSE);
			break;
		}
		case 1: // 그림
		{
			hDC = GetDC(hWnd);
			m1DC = CreateCompatibleDC(hDC);
			SelectObject(m1DC, hBit);
			tempDC = CreateCompatibleDC(m1DC);
			tempBmp = CreateCompatibleBitmap(m1DC, p2.w, p2.h);
			m2DC = CreateCompatibleDC(m1DC);
			SelectObject(m2DC, hBitmap);

			if ((p1.score == 5 || p2.score == 5) && stage_1 && !gameover_1)
			{
				gameover_1 = true;
				winPlayer = p1.score > p2.score ? 1 : 2;
				if (winPlayer == 1) p2.evil = true;
				else p1.evil = true;
				p1.overCnt = 5;
				p2.overCnt = 5;
				KillTimer(hWnd, 1);
				SetTimer(hWnd, 1, 150, NULL);
				PlaySound(TEXT("Resources\\sounds\\WAVE143_1.wav"), NULL, SND_LOOP | SND_ASYNC);
			}

			if ((p1.score == 5 || p2.score == 5) && stage_2 && !gameover_2)
			{
				gameover_2 = true;
				winPlayer = p1.score > p2.score ? 1 : 2;
				if (winPlayer == 1) p2.evil = true;
				else p1.evil = true;
				p1.overCnt = 5;
				p2.overCnt = 5;
				KillTimer(hWnd, 1);
				SetTimer(hWnd, 1, 150, NULL);
				PlaySound(TEXT("Resources\\sounds\\WAVE143_1.wav"), NULL, SND_LOOP | SND_ASYNC);
			}

			if (ready) KillTimer(hWnd, 1);

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

			p1.moveCnt += direct;
			p2.moveCnt += direct2;


			if (p1.moveCnt >= 16)
			{
				p1.moveCnt = 16;
				direct = -1;
			}
			else if (p1.moveCnt <= 0)
			{
				p1.moveCnt = 0;
				direct = 1;
			}

			if (p2.moveCnt >= 16)
			{
				p2.moveCnt = 16;
				direct2 = -1;
			}
			else if (p2.moveCnt <= 0)
			{
				p2.moveCnt = 0;
				direct2 = 1;
			}

			switch (p1.moveCnt)
			{
			case 0: p1.Sx = 2;   break;
			case 4: p1.Sx = 68;  break;
			case 8: p1.Sx = 134; break;
			case 12: p1.Sx = 200; break;
			case 16: p1.Sx = 266; break;
			}
			switch (p2.moveCnt)
			{
			case 0: p2.Sx = 2;   break;
			case 4: p2.Sx = 68;  break;
			case 8: p2.Sx = 134; break;
			case 12: p2.Sx = 200; break;
			case 16: p2.Sx = 266; break;
			}
			p1.Sy = 266;
			p2.Sy = 266;

			if (isR)
			{
				switch (p1.jumpCnt)
				{
				case 60: case 59: case 58:
					p1.Sx = 332; p1.Sy = 266;
					break;

				case 57: case 56: case 53: case 52:
				case 49: case 48: case 45: case 44:
				case 41: case 40: case 37: case 36:
				case 33: case 32: case 29: case 28:
				case 25: case 24: case 21: case 20:
				case 17: case 16:
					p1.Sx = 398; p1.Sy = 266;
					break;

				case 55: case 54: case 51: case 50:
				case 47: case 46: case 43: case 42:
				case 39: case 38: case 35: case 34:
				case 31: case 30: case 27: case 26:
				case 23: case 22: case 19: case 18:
				case 15: case 14:
					p1.Sx = 2; p1.Sy = 332;
					break;

				case 13: case 12: case 11:
					p1.Sx = 68; p1.Sy = 332;
					break;

				case 10: case 9: case 8: case 7:
				case 6: case 5: case 4: case 3:
				case 2: case 1:
					p1.Sx = 134; p1.Sy = 332;
					break;
				}
				if (isZ)
				{
					if (spikeCnt_1 == 0)
					{
						p1.Sx = 200; p1.Sy = 332;
						spikeCnt_1++;
					}
					else if (spikeCnt_1 == 1)
					{
						p1.Sx = 266; p1.Sy = 332;
						spikeCnt_1++;
					}
					else if (spikeCnt_1 == 2)
					{
						p1.Sx = 332; p1.Sy = 332;
						spikeCnt_1++;
					}
					else if (spikeCnt_1 == 3)
					{
						p1.Sx = 398; p1.Sy = 332;
						spikeCnt_1++;
					}
				}
				p1.jumpCnt--;
			}

			if (isUp)
			{
				switch (p2.jumpCnt)
				{
				case 60: case 59: case 58:
					p2.Sx = 332; p2.Sy = 266;
					break;

				case 57: case 56: case 53: case 52:
				case 49: case 48: case 45: case 44:
				case 41: case 40: case 37: case 36:
				case 33: case 32: case 29: case 28:
				case 25: case 24: case 21: case 20:
				case 17: case 16:
					p2.Sx = 398; p2.Sy = 266;
					break;

				case 55: case 54: case 51: case 50:
				case 47: case 46: case 43: case 42:
				case 39: case 38: case 35: case 34:
				case 31: case 30: case 27: case 26:
				case 23: case 22: case 19: case 18:
				case 15: case 14:
					p2.Sx = 2; p2.Sy = 332;
					break;

				case 13: case 12: case 11:
					p2.Sx = 68; p2.Sy = 332;
					break;

				case 10: case 9: case 8: case 7:
				case 6: case 5: case 4: case 3:
				case 2: case 1:
					p2.Sx = 134; p2.Sy = 332;
					break;
				}
				if (isEnter)
				{
					if (spikeCnt_2 == 0)
					{
						p2.Sx = 266; p2.Sy = 332;
						spikeCnt_2++;
					}
					else if (spikeCnt_2 == 1)
					{
						p2.Sx = 266; p2.Sy = 332;
						spikeCnt_2++;
					}
					else if (spikeCnt_2 == 2)
					{
						p2.Sx = 332; p2.Sy = 332;
						spikeCnt_2++;
					}
					else if (spikeCnt_2 == 3)
					{
						p2.Sx = 332; p2.Sy = 332;
						spikeCnt_2++;
					}
				}
				p2.jumpCnt--;
			}

			if (isSlide_Left_1 || isSlide_Right_1)
			{
				switch (p1.slideCnt)
				{
				case 40: case 39: case 38: case 37:
				case 36: case 35: case 34: case 33:
				case 32: case 31:
					p1.Sx = 68; p1.Sy = 398;
					break;
				case 30: case 29: case 28: case 27:
				case 26: case 25: case 24: case 23:
				case 22: case 21: case 20: case 19:
				case 18: case 17:
					p1.Sx = 134; p1.Sy = 398;
					break;
				case 16: case 15: case 14: case 13:
				case 12: case 11: case 10: case 9:
				case 8: case 7: case 6: case 5:
				case 4: case 3: case 2: case 1:
					p1.Sx = 200; p1.Sy = 398;
					break;
				}
				p1.slideCnt--;
			}
			if (isSlide_Left_2 || isSlide_Right_2)
			{
				switch (p2.slideCnt)
				{
				case 40: case 39: case 38: case 37:
				case 36: case 35: case 34: case 33:
				case 32: case 31:
					p2.Sx = 68; p2.Sy = 398;
					break;
				case 30: case 29: case 28: case 27:
				case 26: case 25: case 24: case 23:
				case 22: case 21: case 20: case 19:
				case 18: case 17:
					p2.Sx = 134; p2.Sy = 398;
					break;
				case 16: case 15: case 14: case 13:
				case 12: case 11: case 10: case 9:
				case 8: case 7: case 6: case 5:
				case 4: case 3: case 2: case 1:
					p2.Sx = 200; p2.Sy = 398;
					break;
				}
				p2.slideCnt--;
			}
			if (gameover_1 || gameover_2)
			{
				p1.x = 30; p1.y = 560;
				p2.x = 950; p2.y = 560;

				if (winPlayer == 1)
				{
					if (p1.overCnt == 0) p1.overCnt = 5;
					if (p2.overCnt == 0) p2.overCnt = 3;

					switch (p1.overCnt)
					{
					case 5:
						p1.Sx = 266; p1.Sy = 398;
						break;
					case 4:
						p1.Sx = 332; p1.Sy = 398;
						break;
					case 3:
						p1.Sx = 398; p1.Sy = 398;
						break;
					case 2:
						p1.Sx = 2; p1.Sy = 464;
						break;
					case 1:
						p1.Sx = 68; p1.Sy = 464;
						break;
					}
					switch (p2.overCnt)
					{
					case 5:
						p2.Sx = 134; p2.Sy = 464;
						break;
					case 4:
						p2.Sx = 200; p2.Sy = 464;
						break;
					case 3:
						p2.Sx = 266; p2.Sy = 464;
						break;
					case 2:
						p2.Sx = 332; p2.Sy = 464;
						break;
					case 1:
						p2.Sx = 398; p2.Sy = 464;
						break;
					}
					p1.overCnt -= 1;
					p2.overCnt -= 1;
				}
				else
				{
					if (p1.overCnt == 0) p1.overCnt = 3;
					if (p2.overCnt == 0) p2.overCnt = 5;

					switch (p1.overCnt)
					{
					case 5:
						p1.Sx = 134; p1.Sy = 464;
						break;
					case 4:
						p1.Sx = 200; p1.Sy = 464;
						break;
					case 3:
						p1.Sx = 266; p1.Sy = 464;
						break;
					case 2:
						p1.Sx = 332; p1.Sy = 464;
						break;
					case 1:
						p1.Sx = 398; p1.Sy = 464;
						break;
					}
					switch (p2.overCnt)
					{
					case 5:
						p2.Sx = 266; p2.Sy = 398;
						break;
					case 4:
						p2.Sx = 332; p2.Sy = 398;
						break;
					case 3:
						p2.Sx = 398; p2.Sy = 398;
						break;
					case 2:
						p2.Sx = 2; p2.Sy = 464;
						break;
					case 1:
						p2.Sx = 68; p2.Sy = 464;
						break;
					}
					p1.overCnt -= 1;
					p2.overCnt -= 1;
				}
			}
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
			break;
		}
		}
		InvalidateRect(hWnd, NULL, FALSE);
		break;
	case WM_KEYUP:
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
		break;
	case WM_DESTROY:
		KillTimer(hWnd, 2);
		KillTimer(hWnd, 1);
		KillTimer(hWnd, BallRotate);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}