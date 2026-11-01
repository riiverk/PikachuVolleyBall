#include "PlayerAnimation.h"
#include "GameState.h"

void UpdatePlayerAnimation()
{
	static int direct = 1;
	static int direct2 = 1;

	p1.moveCnt += direct;
	p2.moveCnt += direct2;


	if (p1.moveCnt >= 16)
	{
		p1.moveCnt = 16;
		direct = -1;
	}
	else if (p1.moveCnt <= 0)
	{
		p1.moveCnt = 0;
		direct = 1;
	}

	if (p2.moveCnt >= 16)
	{
		p2.moveCnt = 16;
		direct2 = -1;
	}
	else if (p2.moveCnt <= 0)
	{
		p2.moveCnt = 0;
		direct2 = 1;
	}

	switch (p1.moveCnt)
	{
	case 0: p1.Sx = 2;   break;
	case 4: p1.Sx = 68;  break;
	case 8: p1.Sx = 134; break;
	case 12: p1.Sx = 200; break;
	case 16: p1.Sx = 266; break;
	}
	switch (p2.moveCnt)
	{
	case 0: p2.Sx = 2;   break;
	case 4: p2.Sx = 68;  break;
	case 8: p2.Sx = 134; break;
	case 12: p2.Sx = 200; break;
	case 16: p2.Sx = 266; break;
	}
	p1.Sy = 266;
	p2.Sy = 266;

	if (isR)
	{
		switch (p1.jumpCnt)
		{
		case 60: case 59: case 58:
			p1.Sx = 332; p1.Sy = 266;
			break;

		case 57: case 56: case 53: case 52:
		case 49: case 48: case 45: case 44:
		case 41: case 40: case 37: case 36:
		case 33: case 32: case 29: case 28:
		case 25: case 24: case 21: case 20:
		case 17: case 16:
			p1.Sx = 398; p1.Sy = 266;
			break;

		case 55: case 54: case 51: case 50:
		case 47: case 46: case 43: case 42:
		case 39: case 38: case 35: case 34:
		case 31: case 30: case 27: case 26:
		case 23: case 22: case 19: case 18:
		case 15: case 14:
			p1.Sx = 2; p1.Sy = 332;
			break;

		case 13: case 12: case 11:
			p1.Sx = 68; p1.Sy = 332;
			break;

		case 10: case 9: case 8: case 7:
		case 6: case 5: case 4: case 3:
		case 2: case 1:
			p1.Sx = 134; p1.Sy = 332;
			break;
		}
		if (isZ)
		{
			if (spikeCnt_1 == 0)
			{
				p1.Sx = 200; p1.Sy = 332;
				spikeCnt_1++;
			}
			else if (spikeCnt_1 == 1)
			{
				p1.Sx = 266; p1.Sy = 332;
				spikeCnt_1++;
			}
			else if (spikeCnt_1 == 2)
			{
				p1.Sx = 332; p1.Sy = 332;
				spikeCnt_1++;
			}
			else if (spikeCnt_1 == 3)
			{
				p1.Sx = 398; p1.Sy = 332;
				spikeCnt_1++;
			}
		}
		p1.jumpCnt--;
	}

	if (isUp)
	{
		switch (p2.jumpCnt)
		{
		case 60: case 59: case 58:
			p2.Sx = 332; p2.Sy = 266;
			break;

		case 57: case 56: case 53: case 52:
		case 49: case 48: case 45: case 44:
		case 41: case 40: case 37: case 36:
		case 33: case 32: case 29: case 28:
		case 25: case 24: case 21: case 20:
		case 17: case 16:
			p2.Sx = 398; p2.Sy = 266;
			break;

		case 55: case 54: case 51: case 50:
		case 47: case 46: case 43: case 42:
		case 39: case 38: case 35: case 34:
		case 31: case 30: case 27: case 26:
		case 23: case 22: case 19: case 18:
		case 15: case 14:
			p2.Sx = 2; p2.Sy = 332;
			break;

		case 13: case 12: case 11:
			p2.Sx = 68; p2.Sy = 332;
			break;

		case 10: case 9: case 8: case 7:
		case 6: case 5: case 4: case 3:
		case 2: case 1:
			p2.Sx = 134; p2.Sy = 332;
			break;
		}
		if (isEnter)
		{
			if (spikeCnt_2 == 0)
			{
				p2.Sx = 266; p2.Sy = 332;
				spikeCnt_2++;
			}
			else if (spikeCnt_2 == 1)
			{
				p2.Sx = 266; p2.Sy = 332;
				spikeCnt_2++;
			}
			else if (spikeCnt_2 == 2)
			{
				p2.Sx = 332; p2.Sy = 332;
				spikeCnt_2++;
			}
			else if (spikeCnt_2 == 3)
			{
				p2.Sx = 332; p2.Sy = 332;
				spikeCnt_2++;
			}
		}
		p2.jumpCnt--;
	}

	if (isSlide_Left_1 || isSlide_Right_1)
	{
		switch (p1.slideCnt)
		{
		case 40: case 39: case 38: case 37:
		case 36: case 35: case 34: case 33:
		case 32: case 31:
			p1.Sx = 68; p1.Sy = 398;
			break;
		case 30: case 29: case 28: case 27:
		case 26: case 25: case 24: case 23:
		case 22: case 21: case 20: case 19:
		case 18: case 17:
			p1.Sx = 134; p1.Sy = 398;
			break;
		case 16: case 15: case 14: case 13:
		case 12: case 11: case 10: case 9:
		case 8: case 7: case 6: case 5:
		case 4: case 3: case 2: case 1:
			p1.Sx = 200; p1.Sy = 398;
			break;
		}
		p1.slideCnt--;
	}
	if (isSlide_Left_2 || isSlide_Right_2)
	{
		switch (p2.slideCnt)
		{
		case 40: case 39: case 38: case 37:
		case 36: case 35: case 34: case 33:
		case 32: case 31:
			p2.Sx = 68; p2.Sy = 398;
			break;
		case 30: case 29: case 28: case 27:
		case 26: case 25: case 24: case 23:
		case 22: case 21: case 20: case 19:
		case 18: case 17:
			p2.Sx = 134; p2.Sy = 398;
			break;
		case 16: case 15: case 14: case 13:
		case 12: case 11: case 10: case 9:
		case 8: case 7: case 6: case 5:
		case 4: case 3: case 2: case 1:
			p2.Sx = 200; p2.Sy = 398;
			break;
		}
		p2.slideCnt--;
	}
	if (gameover_1 || gameover_2)
	{
		p1.x = 30; p1.y = 560;
		p2.x = 950; p2.y = 560;

		if (winPlayer == 1)
		{
			if (p1.overCnt == 0) p1.overCnt = 5;
			if (p2.overCnt == 0) p2.overCnt = 3;

			switch (p1.overCnt)
			{
			case 5:
				p1.Sx = 266; p1.Sy = 398;
				break;
			case 4:
				p1.Sx = 332; p1.Sy = 398;
				break;
			case 3:
				p1.Sx = 398; p1.Sy = 398;
				break;
			case 2:
				p1.Sx = 2; p1.Sy = 464;
				break;
			case 1:
				p1.Sx = 68; p1.Sy = 464;
				break;
			}
			switch (p2.overCnt)
			{
			case 5:
				p2.Sx = 134; p2.Sy = 464;
				break;
			case 4:
				p2.Sx = 200; p2.Sy = 464;
				break;
			case 3:
				p2.Sx = 266; p2.Sy = 464;
				break;
			case 2:
				p2.Sx = 332; p2.Sy = 464;
				break;
			case 1:
				p2.Sx = 398; p2.Sy = 464;
				break;
			}
			p1.overCnt -= 1;
			p2.overCnt -= 1;
		}
		else
		{
			if (p1.overCnt == 0) p1.overCnt = 3;
			if (p2.overCnt == 0) p2.overCnt = 5;

			switch (p1.overCnt)
			{
			case 5:
				p1.Sx = 134; p1.Sy = 464;
				break;
			case 4:
				p1.Sx = 200; p1.Sy = 464;
				break;
			case 3:
				p1.Sx = 266; p1.Sy = 464;
				break;
			case 2:
				p1.Sx = 332; p1.Sy = 464;
				break;
			case 1:
				p1.Sx = 398; p1.Sy = 464;
				break;
			}
			switch (p2.overCnt)
			{
			case 5:
				p2.Sx = 266; p2.Sy = 398;
				break;
			case 4:
				p2.Sx = 332; p2.Sy = 398;
				break;
			case 3:
				p2.Sx = 398; p2.Sy = 398;
				break;
			case 2:
				p2.Sx = 2; p2.Sy = 464;
				break;
			case 1:
				p2.Sx = 68; p2.Sy = 464;
				break;
			}
			p1.overCnt -= 1;
			p2.overCnt -= 1;
		}
	}
}
