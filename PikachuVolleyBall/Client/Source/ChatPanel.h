#pragma once
#include "Common.h"

class GameClient;

class ChatPanel
{
public:
    enum Control { History = 1101, Input, Hint, SendButton };
    ~ChatPanel();
    bool Create(HWND window, HINSTANCE instance);
    void Destroy();
    void Layout() const;
    void Show(bool visible);
    void Reset(GameClient& client);
    void Cancel(GameClient& client);
    void Refresh(const GameClient& client) const;
    bool HandleMessage(const MSG& message, GameClient& client);
    bool HandleCommand(WPARAM command, GameClient& client);
private:
    void Submit(GameClient& client);
    bool IsComposing() const;
    void SetHint(const wchar_t* text) const;
    HWND window_ = nullptr;
    HFONT font_ = nullptr;
    bool visible_ = false;
};
