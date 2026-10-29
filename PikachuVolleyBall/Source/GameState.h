#pragma once

#include <windows.h>
#include <tchar.h>
#include <random>
#include "GameTypes.h"

#define BallRotate 1000
#define MainScreen 3000
#define GameOver 2000
#define Ready 4000

extern bool isR;
extern bool isD;
extern bool isG;
extern bool isZ;

extern bool isUp;
extern bool isLeft;
extern bool isRight;
extern bool isEnter;

extern bool isSlide_Left_1;
extern bool isSlide_Right_1;
extern bool isSlide_Left_2;
extern bool isSlide_Right_2;

extern bool keyLeftDown;
extern bool keyRightDown;
extern bool keyDDown;
extern bool keyGDown;

extern int velocityY_1;
extern int velocityY_2;
extern const int gravity;
extern const int jumpPower;

extern bool mainScreen;
extern bool stage_1;
extern bool stage_2;

extern bool ready;
extern bool gameover_1;
extern bool gameover_2;

extern int cnt, prev_loc[9][2];
extern RECT rt;
//float g = 60.0f;
extern float tick;
extern bool spike;
extern bool gameset_1, gameset_2;
extern float center, prev_vy, prev_vx;
extern int smaller;

extern std::random_device rd;
extern std::mt19937 gen;

extern HINSTANCE g_hInst;
extern LPCTSTR lpszClass;
extern LPCTSTR lpszWindowName;

extern Pikachu p1, p2;
extern Ball ball, afterimage[2];

extern BackGround mountain;
extern BackGround sky;
extern BackGround groundRed;
extern BackGround groundYellow;
extern BackGround groundLine;
extern BackGround groundRight;
extern BackGround groundLeft;
extern BackGround shadow;
extern BackGround net_Top;
extern BackGround net;
extern BackGround sittingP[20];
extern BackGround fight;
extern BackGround with_Friend;
extern BackGround logo;
extern BackGround pokemon;
extern BackGround mark;
extern BackGround readyToGame;
extern Ball cloud[7];
extern int fSize;
extern int fDir;
extern int wfX;
extern int wfDir;
extern int readyCnt;

extern int winPlayer;

// Shared by input handling and sprite animation.
extern int spikeCnt_1;
extern int spikeCnt_2;
