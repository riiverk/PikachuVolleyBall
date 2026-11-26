#include "ChatPanel.h"
#include "GameClient.h"
#include <algorithm>
#include <imm.h>
#include <string>

#pragma comment(lib, "imm32.lib")

namespace
{
constexpr const wchar_t* ChatHelp = L"Enter로 채팅 · Esc로 취소 (최대 63바이트)";
}

ChatPanel::~ChatPanel() { Destroy(); }

bool ChatPanel::Create(HWND window, HINSTANCE instance)
{
    Destroy();
    window_ = window;
    font_ = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
    const struct { int id; const wchar_t* type; const wchar_t* text; DWORD style, extended; } controls[] = {
        {History, L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE},
        {Input, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE},
        {Hint, L"STATIC", ChatHelp, SS_LEFT, 0},
        {SendButton, L"BUTTON", L"전송", BS_PUSHBUTTON | WS_TABSTOP, 0}
    };
    for (const auto& entry : controls)
    {
        const HWND control = CreateWindowExW(entry.extended, entry.type, entry.text,
            WS_CHILD | entry.style, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(entry.id)),
            instance, nullptr);
        if (!control) { Destroy(); return false; }
        const HFONT controlFont = font_ ? font_ : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(controlFont), FALSE);
    }
    SendDlgItemMessageW(window_, Input, EM_SETLIMITTEXT, MAX_CHAT - 1, 0);
    Layout();
    return true;
}

void ChatPanel::Destroy()
{
    if (window_)
        for (int id = History; id <= SendButton; ++id)
            if (const HWND control = GetDlgItem(window_, id)) DestroyWindow(control);
    if (font_) DeleteObject(font_);
    window_ = nullptr;
    font_ = nullptr;
    visible_ = false;
}

void ChatPanel::Layout() const
{
    if (!window_) return;
    RECT client{};
    GetClientRect(window_, &client);
    const int width = std::max(1L, client.right - 32);
    const int top = std::max(0L, client.bottom - GameConst::ChatPanelHeight);
    MoveWindow(GetDlgItem(window_, History), 16, top + 8, width, 92, TRUE);
    MoveWindow(GetDlgItem(window_, Hint), 16, top + 104, width, 20, TRUE);
    MoveWindow(GetDlgItem(window_, Input), 16, top + 128, std::max(1, width - 80), 32, TRUE);
    MoveWindow(GetDlgItem(window_, SendButton), 16 + width - 72, top + 128, 72, 32, TRUE);
}

void ChatPanel::Show(bool visible)
{
    visible_ = visible;
    for (int id = History; id <= SendButton; ++id)
        ShowWindow(GetDlgItem(window_, id), visible ? SW_SHOW : SW_HIDE);
    Layout();
}

void ChatPanel::SetHint(const wchar_t* text) const { SetDlgItemTextW(window_, Hint, text); }

void ChatPanel::Cancel(GameClient& client)
{
    client.EndChat();
    SetDlgItemTextW(window_, Input, L"");
    SetHint(ChatHelp);
    SetFocus(window_);
}

void ChatPanel::Reset(GameClient& client)
{
    client.EndChat();
    SetDlgItemTextW(window_, Input, L"");
    SetDlgItemTextW(window_, History, L"");
    SetHint(ChatHelp);
    Show(false);
}

void ChatPanel::Refresh(const GameClient& client) const
{
    std::wstring text;
    for (const auto& line : client.GetChatHistory()) text += line + L"\r\n";
    SetDlgItemTextW(window_, History, text.c_str());
    const HWND history = GetDlgItem(window_, History);
    SendMessageW(history, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    SendMessageW(history, EM_SCROLLCARET, 0, 0);
}

bool ChatPanel::IsComposing() const
{
    const HWND input = GetDlgItem(window_, Input);
    const HIMC context = ImmGetContext(input);
    if (!context) return false;
    const bool composing = ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0) > 0;
    ImmReleaseContext(input, context);
    return composing;
}

void ChatPanel::Submit(GameClient& client)
{
    if (IsComposing()) return;
    wchar_t text[MAX_CHAT]{};
    GetDlgItemTextW(window_, Input, text, ARRAYSIZE(text));
    switch (client.SendChat(text))
    {
    case ChatSendResult::Sent:
    case ChatSendResult::Empty:
        Cancel(client);
        return;
    case ChatSendResult::TooLong:
        SetHint(L"63바이트 초과: 한글 최대 21자");
        break;
    case ChatSendResult::InvalidText:
        SetHint(L"올바른 한 줄 메시지를 입력하세요.");
        break;
    case ChatSendResult::NotInRoom:
    case ChatSendResult::SendFailed:
        SetHint(L"전송하지 못했습니다. 서버 연결을 확인하세요.");
        break;
    }
    SetFocus(GetDlgItem(window_, Input));
}

bool ChatPanel::HandleMessage(const MSG& message, GameClient& client)
{
    if (!visible_ || client.GetScene() == Scene::Connect) return false;
    if (message.message == WM_CHAR && message.hwnd == GetDlgItem(window_, Input) &&
        (message.wParam == VK_RETURN || message.wParam == VK_ESCAPE)) return true;
    if (message.message != WM_KEYDOWN) return false;
    if (message.hwnd == GetDlgItem(window_, Input) && IsComposing()) return false;
    if (message.wParam == VK_ESCAPE && client.IsChatting()) { Cancel(client); return true; }
    if (message.wParam == VK_TAB && client.IsChatting() && message.hwnd == GetDlgItem(window_, Input))
    {
        SetFocus(GetDlgItem(window_, SendButton));
        return true;
    }
    if (message.wParam != VK_RETURN) return false;
    if (!client.IsChatting() && message.hwnd != window_ && message.hwnd != GetDlgItem(window_, Input)) return false;
    if (IsComposing()) return false; // Enter must finish Korean composition before it can submit.
    if ((message.lParam & (1LL << 30)) != 0) return true;
    if (client.IsChatting()) Submit(client);
    else if (client.BeginChat())
    {
        SetHint(L"메시지를 입력하고 Enter로 전송하세요. Esc: 취소");
        SetFocus(GetDlgItem(window_, Input));
    }
    return true;
}

bool ChatPanel::HandleCommand(WPARAM command, GameClient& client)
{
    if (!visible_) return false;
    if (LOWORD(command) == Input && HIWORD(command) == EN_SETFOCUS)
    {
        client.BeginChat();
        return true;
    }
    if (LOWORD(command) == History && HIWORD(command) == EN_SETFOCUS)
    {
        client.ReleaseInput();
        return true;
    }
    if (LOWORD(command) == SendButton && HIWORD(command) == BN_CLICKED)
    {
        if (client.BeginChat()) Submit(client);
        return true;
    }
    return false;
}
