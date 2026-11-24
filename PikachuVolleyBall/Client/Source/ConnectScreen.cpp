#include "ConnectScreen.h"
#include "resource.h"
#include <algorithm>
#include <string>

namespace
{
constexpr int panelWidth = 414;
constexpr int panelHeight = 484;
constexpr int padding = 30;
constexpr COLORREF ink = RGB(24, 42, 69);
constexpr COLORREF paper = RGB(255, 252, 237);
constexpr COLORREF gold = RGB(255, 212, 72);

RECT PanelRect(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);
    const int x = (client.right - panelWidth) / 2;
    const int y = (client.bottom - panelHeight) / 2;
    return {x, y, x + panelWidth, y + panelHeight};
}

void PaintRect(HDC dc, const RECT& rect, COLORREF color)
{
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

void DrawBackdrop(HDC dc, HDC sprites, const RECT& client, const RECT& panel)
{
    const int horizon = client.bottom * 3 / 4;
    TRIVERTEX colors[] = {{0, 0, 0x4800, 0x9a00, 0xf000, 0}, {client.right, horizon, 0xc600, 0xec00, 0xff00, 0}};
    GRADIENT_RECT gradient{0, 1};
    GradientFill(dc, colors, 2, &gradient, 1, GRADIENT_FILL_RECT_V);
    PaintRect(dc, {0, horizon, client.right, client.bottom}, RGB(242, 199, 104));
    PaintRect(dc, {0, horizon, client.right, horizon + 5}, ink);
    PaintRect(dc, {0, horizon + 22, client.right, horizon + 26}, RGB(255, 231, 158));

    if (!sprites) return;

    constexpr COLORREF transparent = RGB(255, 255, 255);
    TransparentBlt(dc, client.right / 12, client.bottom / 7, 108, 54, sprites, 100, 90, 48, 24, transparent);
    TransparentBlt(dc, client.right * 4 / 5, client.bottom / 9, 132, 66, sprites, 100, 90, 48, 24, transparent);
    StretchBlt(dc, 0, horizon - 108, client.right, 108, sprites, 2, 200, 432, 64, SRCCOPY);
    if (panel.left >= 150)
    {
        TransparentBlt(dc, panel.left - 147, horizon - 104, 128, 128,
            sprites, 2, 266, 64, 64, transparent);
        TransparentBlt(dc, panel.right + 19, horizon - 104, 128, 128,
            sprites, 266, 398, 64, 64, transparent);
    }
}
}

void ConnectScreen::LimitWindowSize(HWND window, MINMAXINFO& limits) const
{
    RECT minimum{0, 0, panelWidth + 32, panelHeight + 32};
    AdjustWindowRectEx(&minimum, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)),
        FALSE, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)));
    limits.ptMinTrackSize.x = std::max(limits.ptMinTrackSize.x, minimum.right - minimum.left);
    limits.ptMinTrackSize.y = std::max(limits.ptMinTrackSize.y, minimum.bottom - minimum.top);
}

bool ConnectScreen::Create(HWND window, HINSTANCE instance)
{
    sprites_ = LoadBitmapW(instance, MAKEINTRESOURCEW(IDB_BITMAP1));
    font_ = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
    const struct { Control id; const wchar_t* type; const wchar_t* text; DWORD style; DWORD extendedStyle; } controls[] = {
        {IpLabel, L"STATIC", L"서버 IP", SS_LEFT, 0},
        {ServerIp, L"EDIT", L"127.0.0.1", WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE},
        {RoomLabel, L"STATIC", L"방 번호", SS_LEFT, 0},
        {RoomNumber, L"EDIT", L"1", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE},
        {NickLabel, L"STATIC", L"닉네임", SS_LEFT, 0},
        {Nickname, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE},
        {JoinButton, L"BUTTON", L"서버 접속", BS_OWNERDRAW | WS_TABSTOP, 0},
        {LocalPlayButton, L"BUTTON", L"로컬 2인 대전", BS_OWNERDRAW | WS_TABSTOP, 0},
        {Status, L"STATIC", L"", SS_LEFT, 0},
        {WaitingTitle, L"STATIC", L"", SS_CENTER, 0},
        {WaitingRoom, L"STATIC", L"", SS_CENTER, 0},
        {WaitingPlayer, L"STATIC", L"", SS_LEFT, 0},
        {WaitingOpponent, L"STATIC", L"", SS_LEFT, 0},
        {WaitingHint, L"STATIC", L"", SS_CENTER, 0},
        {LeaveButton, L"BUTTON", L"방 나가기", BS_OWNERDRAW | WS_TABSTOP, 0}
    };

    const HFONT controlFont = font_ ? font_ : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    for (const auto& entry : controls)
    {
        const HWND control = CreateWindowExW(entry.extendedStyle, entry.type, entry.text,
            WS_CHILD | (entry.id <= Status ? WS_VISIBLE : 0) | entry.style, 0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(entry.id)), instance, nullptr);
        if (!control)
        {
            Destroy(window);
            return false;
        }
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(controlFont), FALSE);
    }
    SendDlgItemMessageW(window, ServerIp, EM_SETLIMITTEXT, 15, 0);
    SendDlgItemMessageW(window, RoomNumber, EM_SETLIMITTEXT, 10, 0);
    SendDlgItemMessageW(window, Nickname, EM_SETLIMITTEXT, MAX_NICK - 1, 0);
    for (const auto edit : {ServerIp, RoomNumber, Nickname})
        SendDlgItemMessageW(window, edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));

    Layout(window);
    return true;
}

void ConnectScreen::Layout(HWND window) const
{
    const RECT panel = PanelRect(window);
    const int x = panel.left + padding;
    constexpr int width = panelWidth - 2 * padding;
    const auto move = [&](Control id, int y, int height)
    {
        MoveWindow(GetDlgItem(window, id), x, panel.top + y, width, height, TRUE);
    };
    move(IpLabel, 113, 24);
    move(ServerIp, 142, 32);
    move(RoomLabel, 188, 24);
    move(RoomNumber, 217, 32);
    move(NickLabel, 263, 24);
    move(Nickname, 292, 32);
    move(JoinButton, 342, 38);
    move(LocalPlayButton, 390, 34);
    move(Status, 437, 30);
    move(WaitingTitle, 120, 30);
    move(WaitingRoom, 158, 26);
    move(WaitingPlayer, 212, 56);
    move(WaitingOpponent, 288, 56);
    move(WaitingHint, 361, 52);
    move(LeaveButton, 427, 34);
    InvalidateRect(window, nullptr, FALSE);
}

void ConnectScreen::Hide(HWND window) const
{
    for (int id = IpLabel; id <= LeaveButton; ++id)
        ShowWindow(GetDlgItem(window, id), SW_HIDE);
}

void ConnectScreen::ShowConnect(HWND window)
{
    waiting_ = false;
    Hide(window);
    for (int id = IpLabel; id <= Status; ++id)
        ShowWindow(GetDlgItem(window, id), SW_SHOW);
    InvalidateRect(window, nullptr, FALSE);
}

void ConnectScreen::ShowWaiting(HWND window, bool matched)
{
    wchar_t room[11]{}, nickname[MAX_NICK]{};
    GetDlgItemTextW(window, RoomNumber, room, ARRAYSIZE(room));
    GetDlgItemTextW(window, Nickname, nickname, ARRAYSIZE(nickname));
    SetDlgItemTextW(window, WaitingTitle, matched ? L"매칭 완료!" : L"상대를 기다리고 있어요");
    const std::wstring roomText = std::wstring(L"대기실 · 방 ") + room;
    SetDlgItemTextW(window, WaitingRoom, roomText.c_str());
    const std::wstring playerText = std::wstring(L"나 · 입장 완료\n") + nickname;
    SetDlgItemTextW(window, WaitingPlayer, playerText.c_str());
    SetDlgItemTextW(window, WaitingOpponent, matched
        ? L"상대 플레이어 · 입장 완료\n곧 경기가 시작됩니다."
        : L"상대 플레이어 · 대기 중\n같은 방 번호로 접속하면 만날 수 있어요.");
    SetDlgItemTextW(window, WaitingHint, matched
        ? L"경기 시작을 준비하고 있어요."
        : L"상대 입장 후 자동으로 시작됩니다.");
    const bool entering = !waiting_;
    waiting_ = true;
    Hide(window);
    for (int id = WaitingTitle; id <= LeaveButton; ++id)
        ShowWindow(GetDlgItem(window, id), SW_SHOW);
    if (entering) SetFocus(window);
    InvalidateRect(window, nullptr, FALSE);
}

void ConnectScreen::Paint(HWND window) const
{
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);
    const RECT panel = PanelRect(window);
    const HDC sprites = sprites_ ? CreateCompatibleDC(dc) : nullptr;
    const HGDIOBJ original = sprites ? SelectObject(sprites, sprites_) : nullptr;
    DrawBackdrop(dc, original ? sprites : nullptr, client, panel);

    RECT shadow = panel;
    OffsetRect(&shadow, 8, 8);
    PaintRect(dc, shadow, RGB(55, 96, 144));
    PaintRect(dc, panel, ink);
    PaintRect(dc, {panel.left + 3, panel.top + 99, panel.right - 3, panel.bottom - 3}, paper);
    PaintRect(dc, {panel.left + 3, panel.top + 94, panel.right - 3, panel.top + 99}, gold);
    if (waiting_)
    {
        PaintRect(dc, {panel.left + padding, panel.top + 196, panel.right - padding, panel.top + 199}, gold);
        PaintRect(dc, {panel.left + padding, panel.top + 274, panel.right - padding, panel.top + 275}, RGB(209, 217, 225));
    }
    if (original)
    {
        TransparentBlt(dc, panel.left + (panelWidth - 276) / 2, panel.top + 4, 276, 79,
            sprites, 2, 611, 276, 79, RGB(255, 255, 255));
        SelectObject(sprites, original);
    }
    else
    {
        RECT title{panel.left, panel.top, panel.right, panel.top + 94};
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, gold);
        DrawTextW(dc, L"피카츄 배구", -1, &title, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (sprites) DeleteDC(sprites);
    EndPaint(window, &paint);
}

HBRUSH ConnectScreen::PaintLabel(HDC dc) const
{
    SetTextColor(dc, ink);
    SetBkMode(dc, TRANSPARENT);
    SetDCBrushColor(dc, paper);
    return static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
}

void ConnectScreen::PaintButton(const DRAWITEMSTRUCT& item) const
{
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool primary = item.CtlID == JoinButton;
    const COLORREF border = disabled ? RGB(180, 162, 106) : ink;
    const COLORREF surface = disabled ? RGB(240, 220, 150) :
        (primary ? gold : RGB(237, 246, 255));

    PaintRect(item.hDC, item.rcItem, border);
    RECT face = item.rcItem;
    InflateRect(&face, -2, -2);
    if (!pressed) face.bottom -= 2;
    PaintRect(item.hDC, face, surface);
    const HGDIOBJ oldFont = font_ ? SelectObject(item.hDC, font_) : nullptr;
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, disabled ? RGB(98, 77, 33) : ink);
    wchar_t label[40]{};
    GetWindowTextW(item.hwndItem, label, ARRAYSIZE(label));
    DrawTextW(item.hDC, label, -1, &face, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (item.itemState & ODS_FOCUS)
    {
        InflateRect(&face, -3, -3);
        DrawFocusRect(item.hDC, &face);
    }
    if (oldFont) SelectObject(item.hDC, oldFont);
}

void ConnectScreen::Destroy(HWND window)
{
    for (int id = IpLabel; id <= LeaveButton; ++id)
        if (const HWND control = GetDlgItem(window, id)) DestroyWindow(control);
    if (font_) DeleteObject(font_);
    if (sprites_) DeleteObject(sprites_);
    font_ = nullptr;
    sprites_ = nullptr;
}
