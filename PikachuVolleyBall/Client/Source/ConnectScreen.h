#pragma once
#include "Common.h"

class ConnectScreen
{
public:
    enum Control { IpLabel = 1001, ServerIp, RoomLabel, RoomNumber, NickLabel, Nickname, JoinButton, LocalPlayButton, Status };

    void LimitWindowSize(HWND window, MINMAXINFO& limits) const;
    bool Create(HWND window, HINSTANCE instance);
    void Layout(HWND window) const;
    void Paint(HWND window) const;
    HBRUSH PaintLabel(HDC dc) const;
    void PaintButton(const DRAWITEMSTRUCT& item) const;
    void Destroy(HWND window);

private:
    HBITMAP sprites_ = nullptr;
    HFONT font_ = nullptr;
};
