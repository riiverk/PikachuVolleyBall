#define _CRT_SECURE_NO_WARNINGS
#include "GameState.h"
#include "GameInput.h"
#include "GameUpdate.h"
#include "GameRenderer.h"
#include <algorithm>
#include <filesystem>

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
enum ConnectControl { ConnectTitle = 1001, ConnectButton, LocalPlayButton };
bool localGameStarted = false;

HWND CreateConnectControl(HWND window, ConnectControl id, const wchar_t* type, const wchar_t* label, DWORD style)
{
	const HWND control = CreateWindowW(type, label, WS_CHILD | WS_VISIBLE | style,
		0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInst, nullptr);
	if (control)
		SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), FALSE);
	return control;
}

void LayoutConnectScreen(HWND window)
{
	RECT client{};
	GetClientRect(window, &client);
	const int width = std::min(280L, client.right);
	const int x = (client.right - width) / 2;
	const int y = std::max(0L, (client.bottom - 126) / 2);
	MoveWindow(GetDlgItem(window, ConnectTitle), x, y, width, 24, TRUE);
	MoveWindow(GetDlgItem(window, ConnectButton), x, y + 34, width, 38, TRUE);
	MoveWindow(GetDlgItem(window, LocalPlayButton), x, y + 84, width, 38, TRUE);
}
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
			if (!CreateConnectControl(hWnd, ConnectTitle, L"STATIC", L"온라인 대전 접속", SS_CENTER) ||
				!CreateConnectControl(hWnd, ConnectButton, L"BUTTON", L"서버 접속 (준비 중)", WS_DISABLED) ||
				!CreateConnectControl(hWnd, LocalPlayButton, L"BUTTON", L"로컬 2인 대전", WS_TABSTOP | BS_DEFPUSHBUTTON))
				return -1;
			LayoutConnectScreen(hWnd);
			return 0;
		case WM_SIZE:
			LayoutConnectScreen(hWnd);
			return 0;
		case WM_SETFOCUS:
			SetFocus(GetDlgItem(hWnd, LocalPlayButton));
			return 0;
		case DM_GETDEFID:
			return MAKELRESULT(LocalPlayButton, DC_HASDEFID);
		case WM_COMMAND:
			if (LOWORD(wParam) == LocalPlayButton && HIWORD(wParam) == BN_CLICKED)
			{
				localGameStarted = true;
				DestroyWindow(GetDlgItem(hWnd, ConnectTitle));
				DestroyWindow(GetDlgItem(hWnd, ConnectButton));
				DestroyWindow(GetDlgItem(hWnd, LocalPlayButton));
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
