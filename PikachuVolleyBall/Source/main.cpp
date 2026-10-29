#define _CRT_SECURE_NO_WARNINGS
#include "GameState.h"
#include "GameInput.h"
#include "GameUpdate.h"
#include "GameRenderer.h"

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

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
	int center[2] = { ball.x - 50, ball.y + 50 };

	ResetRoundIfNeeded();

	switch (iMsg)
	{
	case WM_CREATE:
		InitializeGame(hWnd);
		return 0;
	case WM_PAINT:
		PaintScores(hWnd);
		break;
	case WM_KEYDOWN:
		HandleKeyDown(hWnd, wParam);
		break;
	case WM_TIMER:
		switch (wParam)
		{
		case BallRotate:
			UpdateBallRotation();
			break;
		case MainScreen:
			RenderMainScreen(hWnd);
			break;
		case Ready:
			RenderReadyScreen(hWnd);
			break;
		case 1:
			RenderGameFrame(hWnd);
			break;
		}
		InvalidateRect(hWnd, NULL, FALSE);
		break;
	case WM_KEYUP:
		HandleKeyUp(wParam);
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
