#define _CRT_SECURE_NO_WARNINGS
#include "ConnectScreen.h"
#include "GameClient.h"
#include "GameState.h"
#include "GameInput.h"
#include "GameUpdate.h"
#include "GameRenderer.h"
#include <chrono>
#include <exception>
#include <filesystem>
#include <future>

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
ConnectScreen connectScreen;
bool localGameStarted = false;
GameClient gameClient;
// Destroy the future before gameClient so the worker cannot outlive the network client.
std::future<JoinRequestResult> joinAttempt;
constexpr UINT_PTR joinTimer = 5;

void SetJoinStatus(HWND window, const wchar_t* text)
{
	SetDlgItemTextW(window, ConnectScreen::Status, text);
}

void EnableJoinFields(HWND window, bool enabled)
{
	for (const auto control : {ConnectScreen::ServerIp, ConnectScreen::RoomNumber, ConnectScreen::Nickname, ConnectScreen::JoinButton})
		EnableWindow(GetDlgItem(window, control), enabled);
}

void StartJoin(HWND window)
{
	if (joinAttempt.valid() || gameClient.IsConnected()) return;

	struct JoinFields
	{
		wchar_t ip[INET_ADDRSTRLEN]{};
		wchar_t room[11]{};
		wchar_t nickname[MAX_NICK]{};
	} fields;
	GetDlgItemTextW(window, ConnectScreen::ServerIp, fields.ip, ARRAYSIZE(fields.ip));
	GetDlgItemTextW(window, ConnectScreen::RoomNumber, fields.room, ARRAYSIZE(fields.room));
	GetDlgItemTextW(window, ConnectScreen::Nickname, fields.nickname, ARRAYSIZE(fields.nickname));

	if (!SetTimer(window, joinTimer, 50, nullptr))
	{
		SetJoinStatus(window, L"접속을 시작하지 못했습니다. 다시 시도하세요.");
		return;
	}
	try
	{
		joinAttempt = std::async(std::launch::async, [fields]
		{
			return gameClient.ConnectAndJoin(fields.ip, fields.room, fields.nickname);
		});
	}
	catch (const std::exception&)
	{
		KillTimer(window, joinTimer);
		SetJoinStatus(window, L"접속을 시작하지 못했습니다. 다시 시도하세요.");
		return;
	}

	EnableJoinFields(window, false);
	EnableWindow(GetDlgItem(window, ConnectScreen::LocalPlayButton), FALSE);
	SetJoinStatus(window, L"서버에 접속하는 중…");
}

void CheckJoin(HWND window)
{
	if (!joinAttempt.valid())
	{
		if (gameClient.IsConnected()) return;
		KillTimer(window, joinTimer);
		EnableJoinFields(window, true);
		SetJoinStatus(window, L"서버 연결이 끊겼습니다. 다시 접속하세요.");
		return;
	}
	if (joinAttempt.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return;

	const JoinRequestResult result = joinAttempt.get();
	EnableWindow(GetDlgItem(window, ConnectScreen::LocalPlayButton), TRUE);
	if (result == JoinRequestResult::Sent)
	{
		SetJoinStatus(window, L"방 입장 요청을 보냈습니다.");
		SetFocus(GetDlgItem(window, ConnectScreen::LocalPlayButton));
		return;
	}

	KillTimer(window, joinTimer);
	EnableJoinFields(window, true);
	int focus = ConnectScreen::JoinButton;
	const wchar_t* status = L"접속 실패. IP와 서버 상태를 확인하세요.";
	switch (result)
	{
	case JoinRequestResult::InvalidAddress:
		status = L"올바른 IPv4 주소를 입력하세요.";
		focus = ConnectScreen::ServerIp;
		break;
	case JoinRequestResult::InvalidRoom:
		status = L"방 번호는 1~2147483647 사이로 입력하세요.";
		focus = ConnectScreen::RoomNumber;
		break;
	case JoinRequestResult::InvalidNickname:
		status = L"닉네임은 UTF-8로 1~15바이트여야 합니다.";
		focus = ConnectScreen::Nickname;
		break;
	case JoinRequestResult::SendFailed:
		status = L"입장 요청을 보내지 못했습니다. 다시 접속하세요.";
		break;
	default:
		break;
	}
	SetJoinStatus(window, status);
	SetFocus(GetDlgItem(window, focus));
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
		KillTimer(hWnd, joinTimer);
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
			if (const HWND ipField = GetDlgItem(hWnd, ConnectScreen::ServerIp); IsWindowEnabled(ipField))
				SetFocus(ipField);
			return 0;
		case DM_GETDEFID:
			return IsWindowEnabled(GetDlgItem(hWnd, ConnectScreen::JoinButton))
				? MAKELRESULT(ConnectScreen::JoinButton, DC_HASDEFID) : 0;
		case WM_TIMER:
			if (wParam == joinTimer && !IsWindowEnabled(GetDlgItem(hWnd, ConnectScreen::JoinButton)))
				CheckJoin(hWnd);
			return 0;
		case WM_COMMAND:
			if (HIWORD(wParam) != BN_CLICKED) return 0;
			if (LOWORD(wParam) == ConnectScreen::JoinButton)
				StartJoin(hWnd);
			if (LOWORD(wParam) == ConnectScreen::LocalPlayButton && !joinAttempt.valid())
			{
				KillTimer(hWnd, joinTimer);
				gameClient.Disconnect();
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
