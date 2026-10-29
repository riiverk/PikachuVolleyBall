#include "GameState.h"

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

int spikeCnt_1 = 0;
int spikeCnt_2 = 0;
