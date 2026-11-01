#include "BallPhysics.h"
#include "GameState.h"
#include <mmsystem.h>
#include <iostream>
#include <cmath>

using namespace std;

void BallMove_1() {
	const int bottom = GameConst::CourtBottom, left = GameConst::CourtLeft, right = GameConst::CourtRight;
	const float gravity = GameConst::BallGravity;
	const float p_Force = GameConst::HitForce;
	const float s_Force = GameConst::SpikeForce;
	static float spike_vx = 0, spike_vy = 0;
	static bool hit_top = false;
	bool hit_p1 = false, hit_p2 = false, hit_net = false;
	RECT p1_rect = { p1.x + 30, p1.y, p1.x + 140, p1.y + 40 };
	RECT p2_rect = { p2.x + 30, p2.y, p2.x + 140, p2.y + 40 };
	RECT ball_rect = { (int)ball.x, (int)ball.y, (int)ball.x + 100, (int)ball.y + 100 };
	RECT net_rect = { GameConst::NetLeft, GameConst::NetTop, GameConst::NetRight, GameConst::CourtBottom };

	if (ball_rect.top <= GameConst::CeilingY) {
		if (!hit_top) {
			ball.y = GameConst::CeilingY;
			ball.vy = abs(ball.vy);
			hit_top = true;
		}
	}
	else if (ball.y > 0)
		hit_top = false;

	ball.vy += gravity * tick;
	ball.x += ball.vx * tick * GameConst::BallTimeScale;
	ball.y += ball.vy * tick * GameConst::BallTimeScale;

	ball_rect.left = (int)ball.x + 10;
	ball_rect.top = (int)ball.y + 10;
	ball_rect.right = (int)ball.x + 90;
	ball_rect.bottom = (int)ball.y + 90;

	if (ball.y >= bottom - GameConst::BallCollisionSize) {
		smaller = 0;
		ball.y = bottom - GameConst::BallCollisionSize;
		ball.vy = -ball.vy * 0.9f;
		if (ball.x < GameConst::NetLeft) {
			++p2.score;
			PlaySound(TEXT("Resources\\sounds\\WAVE146_1.wav"), NULL, SND_SYNC);
			gameset_2 = true;
			cnt = 0;
		}
		else if (ball.x > GameConst::NetRight) {
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
	else if (ball.x + GameConst::BallCollisionSize >= right) {
		ball.x = right - GameConst::BallCollisionSize;
		ball.vx = -ball.vx * 0.9f;
	}

	if (ball_rect.right > net_rect.left && ball_rect.left < net_rect.right &&
		ball_rect.bottom > net_rect.top && ball_rect.top < net_rect.bottom) {
		if (ball.x < GameConst::NetLeft)
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
			if (ball.x < GameConst::NetLeft)
				ball.vx = s_Force * 0.45f;
			else
				ball.vx = -s_Force * 0.45f;
		}
		else {
			if (ball.x < GameConst::NetLeft)
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
			if (ball.x < GameConst::NetLeft)
				ball.vx = s_Force * 0.45f;
			else
				ball.vx = -s_Force * 0.45f;
		}
		else {
			if (ball.x < GameConst::NetLeft)
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
	const int bottom = GameConst::CourtBottom, left = GameConst::CourtLeft, right = GameConst::CourtRight;
	const float gravity = GameConst::BallGravity;
	const float p_Force = GameConst::HitForce;
	const float s_Force = GameConst::SpikeForce;
	static float spike_vx = 0, spike_vy = 0;
	static bool hit_top = false;
	bool hit_p1 = false, hit_p2 = false, hit_net = false;
	RECT p1_rect = { p1.x + 30, p1.y, p1.x + 140, p1.y + 40 };
	RECT p2_rect = { p2.x + 30, p2.y, p2.x + 140, p2.y + 40 };
	RECT ball_rect = { (int)ball.x, (int)ball.y, (int)ball.x + 100, (int)ball.y + 100 };
	RECT net_rect = { GameConst::NetLeft, GameConst::NetTop, GameConst::NetRight, GameConst::CourtBottom };

	if (ball_rect.top <= GameConst::CeilingY) {
		if (!hit_top) {
			ball.y = GameConst::CeilingY;
			ball.vy = abs(ball.vy);
			hit_top = true;
		}
	}
	else if (ball.y > 0)
		hit_top = false;

	ball.vy += gravity * tick;
	ball.x += ball.vx * tick * GameConst::BallTimeScale;
	ball.y += ball.vy * tick * GameConst::BallTimeScale;

	ball_rect.left = (int)ball.x + 10;
	ball_rect.top = (int)ball.y + 10;
	ball_rect.right = (int)ball.x + 90;
	ball_rect.bottom = (int)ball.y + 90;

	if (ball.y >= bottom - GameConst::BallCollisionSize) {
		smaller = 0;
		ball.y = bottom - GameConst::BallCollisionSize;
		ball.vy = -ball.vy * 0.9f;
		if (ball.x < GameConst::NetLeft) {
			++p2.score;
			PlaySound(TEXT("Resources\\sounds\\WAVE146_1.wav"), NULL, SND_SYNC);
			gameset_2 = true;
			cnt = 0;
		}
		else if (ball.x > GameConst::NetRight) {
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
	else if (ball.x + GameConst::BallCollisionSize >= right) {
		ball.x = right - GameConst::BallCollisionSize;
		ball.vx = -ball.vx * 0.9f;
	}

	if (ball_rect.right > net_rect.left && ball_rect.left < net_rect.right &&
		ball_rect.bottom > net_rect.top && ball_rect.top < net_rect.bottom) {
		if (ball.x < GameConst::NetLeft)
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
				if (ball.x < GameConst::NetLeft)
					ball.vx = s_Force * 0.45f;
				else
					ball.vx = -s_Force * 0.45f;
			}
			else {
				if (ball.x < GameConst::NetLeft)
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
					if (ball.x < GameConst::NetLeft)
						ball.vx = s_Force * 0.7f;
					else
						ball.vx = -s_Force * 0.7f;
				}
				else {
					if (ball.x < GameConst::NetLeft)
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
				if (ball.x < GameConst::NetLeft)
					ball.vx = s_Force * 0.45f;
				else
					ball.vx = -s_Force * 0.45f;
			}
			else {
				if (ball.x < GameConst::NetLeft)
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
					if (ball.x < GameConst::NetLeft)
						ball.vx = s_Force * 0.7f;
					else
						ball.vx = -s_Force * 0.7f;
				}
				else {
					if (ball.x < GameConst::NetLeft)
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
