#pragma once

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
