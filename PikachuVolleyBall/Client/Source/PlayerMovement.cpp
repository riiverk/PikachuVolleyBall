#include "PlayerMovement.h"
#include "GameState.h"

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
