#pragma once
#include "Common.h"
#include <array>
#include <string>

struct StageResult
{
    std::uint8_t stage = 0, winner = NO_PLAYER, myIndex = NO_PLAYER;
    std::array<std::uint8_t, GameConst::PlayerCount> scores{};
    std::array<std::wstring, GameConst::PlayerCount> names;
    bool ready = false, ended = false;
    bool CanContinue() const { return stage > 0 && stage < GameConst::StageCount && !ended; }
};

namespace StageOverLayout
{
inline constexpr RECT Card{210, 60, 950, 770};
inline constexpr RECT Next{250, 568, 650, 625};
inline constexpr RECT Leave{670, 568, 910, 625};
inline constexpr RECT LeaveOnly{380, 568, 780, 625};
}

class StageOverScreen
{
public:
    enum Control { NextButton = 1201, LeaveButton };
    ~StageOverScreen();
    bool Create(HWND window, HINSTANCE instance);
    void Destroy();
    void Show(const StageResult& result);
    void Update(const StageResult& result);
    void Hide();
    void Layout() const;
    void PaintButton(const DRAWITEMSTRUCT& item) const;
private:
    HWND window_ = nullptr;
    HFONT font_ = nullptr;
    bool visible_ = false, canContinue_ = false;
};
