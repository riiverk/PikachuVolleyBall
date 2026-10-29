#include "GameInitialization.h"
#include "GameState.h"

void InitializeGameState(HWND hWnd)
{
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
}
