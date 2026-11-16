#define _CRT_SECURE_NO_WARNINGS
#include "ConnectScreen.h"
#include "GameState.h"
#include "GameInput.h"
#include "GameUpdate.h"
#include "GameRenderer.h"
#include <filesystem>

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
ConnectScreen connectScreen;
bool localGameStarted = false;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
	// Resources are copied next to Client.exe; launching from another working
	// directory (Explorer, Visual Studio, or a terminal) must still find audio.
	wchar_t executablePath[32768]{};
	const DWORD pathLength = GetModuleFileNameW(nullptr, executablePath, ARRAYSIZE(executablePath));
	if (pathLength == 0 || pathLength >= ARRAYSIZE(executablePath) ||
		!SetCurrentDirectoryW(std::filesystem::path(executablePath).parent_path().c_str()))
	{
		MessageBoxW(nullptr, L"Cannot locate the game resource directory.", L"PikachuVolleyBall", MB_OK | MB_ICONERROR);
		return 1;
	}
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
	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		0, 0, GameConst::WindowWidth, GameConst::WindowHeight, NULL, (HMENU)NULL, hInstance, NULL);
	if (!hWnd) return 1;
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	int result;
	while ((result = GetMessage(&Message, 0, 0, 0)) > 0) {
		if (!localGameStarted && IsDialogMessageW(hWnd, &Message)) continue;
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	return result == -1 ? 1 : static_cast<int>(Message.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	if (iMsg == WM_DESTROY)
	{
		if (!localGameStarted) connectScreen.Destroy(hWnd);
		KillTimer(hWnd, 2);
		KillTimer(hWnd, GameConst::FrameTimerId);
		KillTimer(hWnd, BallRotate);
		PostQuitMessage(0);
		return 0;
	}

	if (!localGameStarted)
	{
		switch (iMsg)
		{
		case WM_CREATE:
			return connectScreen.Create(hWnd, g_hInst) ? 0 : -1;
		case WM_GETMINMAXINFO:
			connectScreen.LimitWindowSize(hWnd, *reinterpret_cast<MINMAXINFO*>(lParam));
			return 0;
		case WM_SIZE:
			connectScreen.Layout(hWnd);
			return 0;
		case WM_ERASEBKGND:
			return TRUE;
		case WM_PAINT:
			connectScreen.Paint(hWnd);
			return 0;
		case WM_CTLCOLORSTATIC:
			return reinterpret_cast<LRESULT>(connectScreen.PaintLabel(reinterpret_cast<HDC>(wParam)));
		case WM_DRAWITEM:
			connectScreen.PaintButton(*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
			return TRUE;
		case WM_SETFOCUS:
			SetFocus(GetDlgItem(hWnd, ConnectScreen::ServerIp));
			return 0;
		case WM_COMMAND:
			if (LOWORD(wParam) == ConnectScreen::LocalPlayButton && HIWORD(wParam) == BN_CLICKED)
			{
				localGameStarted = true;
				connectScreen.Destroy(hWnd);
				SetFocus(hWnd);
				InitializeGame(hWnd);
				InvalidateRect(hWnd, nullptr, TRUE);
			}
			return 0;
		}
		return DefWindowProc(hWnd, iMsg, wParam, lParam);
	}

	ResetRoundIfNeeded();
	switch (iMsg)
	{
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
		case GameConst::FrameTimerId:
			RenderGameFrame(hWnd);
			break;
		}
		InvalidateRect(hWnd, NULL, FALSE);
		break;
	case WM_KEYUP:
		HandleKeyUp(wParam);
		break;
	}
	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}
