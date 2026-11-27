#include "StageOverScreen.h"
#include <algorithm>

StageOverScreen::~StageOverScreen() { Destroy(); }

bool StageOverScreen::Create(HWND window, HINSTANCE instance)
{
    Destroy();
    window_ = window;
    font_ = CreateFontW(-17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
    for (const int id : {NextButton, LeaveButton})
    {
        const HWND button = CreateWindowExW(0, L"BUTTON", id == NextButton ? L"다음 준비 [Z]" : L"방 나가기",
            WS_CHILD | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        if (!button) { Destroy(); return false; }
        const HFONT font = font_ ? font_ : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    }
    return true;
}

void StageOverScreen::Destroy()
{
    if (window_)
        for (const int id : {NextButton, LeaveButton})
            if (const HWND button = GetDlgItem(window_, id)) DestroyWindow(button);
    if (font_) DeleteObject(font_);
    window_ = nullptr;
    font_ = nullptr;
    visible_ = false;
}

void StageOverScreen::Show(const StageResult& result)
{
    visible_ = true;
    Update(result);
}

void StageOverScreen::Update(const StageResult& result)
{
    canContinue_ = result.CanContinue();
    SetDlgItemTextW(window_, NextButton, result.ready ? L"준비 완료" : L"다음 준비 [Z]");
    EnableWindow(GetDlgItem(window_, NextButton), canContinue_ && !result.ready);
    ShowWindow(GetDlgItem(window_, NextButton), visible_ && canContinue_ ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(window_, LeaveButton), visible_ ? SW_SHOW : SW_HIDE);
    Layout();
    InvalidateRect(GetDlgItem(window_, NextButton), nullptr, FALSE);
}

void StageOverScreen::Hide()
{
    visible_ = false;
    for (const int id : {NextButton, LeaveButton}) ShowWindow(GetDlgItem(window_, id), SW_HIDE);
}

void StageOverScreen::Layout() const
{
    if (!window_) return;
    RECT client{};
    GetClientRect(window_, &client);
    const int height = std::max(1L, client.bottom - GameConst::ChatPanelHeight);
    const auto move = [&](int id, const RECT& logical)
    {
        const int x = MulDiv(logical.left, client.right, GameConst::WindowWidth);
        const int y = MulDiv(logical.top, height, GameConst::WindowHeight);
        const int right = MulDiv(logical.right, client.right, GameConst::WindowWidth);
        const int bottom = MulDiv(logical.bottom, height, GameConst::WindowHeight);
        MoveWindow(GetDlgItem(window_, id), x, y, right - x, bottom - y, TRUE);
    };
    move(NextButton, StageOverLayout::Next);
    move(LeaveButton, canContinue_ ? StageOverLayout::Leave : StageOverLayout::LeaveOnly);
}

void StageOverScreen::PaintButton(const DRAWITEMSTRUCT& item) const
{
    const bool primary = item.CtlID == NextButton;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    SetDCBrushColor(item.hDC, RGB(19, 32, 52));
    FillRect(item.hDC, &item.rcItem, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    SetDCBrushColor(item.hDC, disabled ? RGB(61, 76, 99) : primary
        ? (pressed ? RGB(238, 184, 51) : RGB(255, 211, 76)) : RGB(41, 60, 86));
    SetDCPenColor(item.hDC, disabled ? RGB(81, 99, 122) : primary ? RGB(255, 226, 138) : RGB(93, 116, 145));
    const auto oldBrush = SelectObject(item.hDC, GetStockObject(DC_BRUSH));
    const auto oldPen = SelectObject(item.hDC, GetStockObject(DC_PEN));
    const auto oldFont = font_ ? SelectObject(item.hDC, font_) : nullptr;
    RoundRect(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom, 10, 10);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, primary && !disabled ? RGB(26, 34, 49) : RGB(229, 237, 248));
    wchar_t text[32]{};
    GetWindowTextW(item.hwndItem, text, ARRAYSIZE(text));
    RECT label = item.rcItem;
    if (pressed) OffsetRect(&label, 0, 1);
    DrawTextW(item.hDC, text, -1, &label, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (item.itemState & ODS_FOCUS) { InflateRect(&label, -5, -5); DrawFocusRect(item.hDC, &label); }
    if (oldFont) SelectObject(item.hDC, oldFont);
    SelectObject(item.hDC, oldPen);
    SelectObject(item.hDC, oldBrush);
}
