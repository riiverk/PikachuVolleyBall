#pragma once

#include <windows.h>

void ResetRoundIfNeeded();
void UpdateBallRotation();
void UpdateReadyState(HWND hWnd);
// Retains the scoring effect at its original point in the frame.
void UpdateGameFrame(HWND hWnd, HDC m1DC, HDC m2DC);
