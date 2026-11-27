#include "Renderer.h"
#include "StageOverScreen.h"
#include "resource.h"
#include <algorithm>
#include <charconv>
#include <cmath>

#pragma comment(lib, "msimg32.lib")

namespace
{
struct Sprite
{
    int x, y, width, height;
};

// Source rectangles from sprite_sheet.json.
constexpr Sprite Sky{156, 2, 16, 16};
constexpr Sprite Mountain{2, 200, 432, 64};
constexpr Sprite RedGround{120, 2, 16, 16};
constexpr Sprite YellowGround{138, 2, 16, 16};
constexpr Sprite GroundLine{66, 2, 16, 16};
constexpr Sprite LeftGroundLine{84, 2, 16, 16};
constexpr Sprite RightGroundLine{102, 2, 16, 16};
constexpr Sprite Net{12, 2, 8, 8};
constexpr Sprite NetTop{22, 2, 8, 8};
constexpr Sprite Cloud{100, 90, 48, 24};
constexpr Sprite Shadow{32, 2, 32, 8};
constexpr Sprite SpikeGlow{298, 158, 40, 40};
constexpr Sprite BallTrail{382, 158, 40, 40};
constexpr Sprite Warning{0, 0, 512, 512};

constexpr int PlayerSpriteSize = 64;
constexpr int PlayerSize = 170;
constexpr int BallSize = GameConst::BallCollisionSize + GameConst::BallCollisionInset;
constexpr int BallFrameCount = 5;

void DrawSprite(HDC target, HDC source, const Sprite& sprite, int x, int y, int width, int height)
{
    TransparentBlt(target, x, y, width, height, source,
        sprite.x, sprite.y, sprite.width, sprite.height, RGB(255, 255, 255));
}

void DrawScore(HDC target, HDC atlas, std::uint8_t score, int edge, bool alignRight)
{
    constexpr int digitSize = 64;
    constexpr int top = 50;
    char digits[3]; // A uint8_t score is at most 255.
    const auto end = std::to_chars(digits, digits + sizeof(digits), score).ptr;
    const int width = static_cast<int>(end - digits) * digitSize;
    int x = alignRight ? edge - width : edge;
    for (const char* digit = digits; digit != end; ++digit, x += digitSize)
    {
        const int value = *digit - '0';
        const Sprite sprite = value < 8
            ? Sprite{204 + 34 * value, 124, 32, 32}
            : Sprite{2 + 34 * (value - 8), 158, 32, 32};
        DrawSprite(target, atlas, sprite, x, top, digitSize, digitSize);
    }
}

void DrawCourt(HDC target, HDC atlas, std::uint32_t tick)
{
    constexpr int tile = 50;
    constexpr int mountainTop = 500;
    constexpr int groundTop = 647;
    constexpr int rightEdgeX = GameConst::CourtRight - tile;
    for (int x = 0; x < GameConst::WindowWidth; x += tile)
        for (int y = 0; y < mountainTop; y += tile)
            StretchBlt(target, x, y, tile, tile, atlas, Sky.x, Sky.y, Sky.width, Sky.height, SRCCOPY);

    StretchBlt(target, 0, mountainTop, GameConst::WindowWidth, groundTop - mountainTop,
        atlas, Mountain.x, Mountain.y, Mountain.width, Mountain.height, SRCCOPY);

    for (int x = 0; x <= rightEdgeX; x += tile)
    {
        const int width = x == rightEdgeX ? GameConst::WindowWidth - x : tile;
        const Sprite& line = x == 0 ? LeftGroundLine : (x == rightEdgeX ? RightGroundLine : GroundLine);
        StretchBlt(target, x, groundTop, width, tile, atlas, RedGround.x, RedGround.y,
            RedGround.width, RedGround.height, SRCCOPY);
        StretchBlt(target, x, groundTop + tile, width, tile, atlas, line.x, line.y, line.width, line.height, SRCCOPY);
        for (int y = groundTop + 2 * tile; y < GameConst::WindowHeight; y += tile)
            StretchBlt(target, x, y, width, tile, atlas, YellowGround.x, YellowGround.y,
                YellowGround.width, YellowGround.height, SRCCOPY);
    }

    constexpr int cloudWidth = 96;
    constexpr int cloudTravel = GameConst::WindowWidth + cloudWidth;
    for (int cloud = 0; cloud < 7; ++cloud)
    {
        const int speed = 1 + cloud % 3;
        const int x = (cloud * 197 + static_cast<int>(tick % cloudTravel) * speed) % cloudTravel - cloudWidth;
        const int y = 55 + (cloud * 97) % 330;
        DrawSprite(target, atlas, Cloud, x, y, cloudWidth, 48);
    }

    constexpr int netWidth = GameConst::NetRight - GameConst::NetLeft;
    DrawSprite(target, atlas, NetTop, GameConst::NetLeft, GameConst::NetTop, netWidth, netWidth);
    for (int y = GameConst::NetTop + netWidth; y < GameConst::CourtBottom; y += netWidth)
        DrawSprite(target, atlas, Net, GameConst::NetLeft, y, netWidth, netWidth);
}

Sprite PlayerSprite(Action action, int frame)
{
    int index = 0;
    switch (action)
    {
    case Action::Idle:
    case Action::Move:
        index = frame % 5;
        break;
    case Action::Jump:
        index = 5 + std::min(frame, 4);
        break;
    case Action::Spike:
        index = 10 + std::min(frame, 4);
        break;
    case Action::Slide:
        index = 15 + std::min(frame, 2);
        break;
    case Action::Win:
        index = 18 + frame % 5;
        break;
    case Action::Lose:
        index = 23 + frame % 5;
        break;
    }
    constexpr int framesPerRow = 7;
    constexpr int stride = PlayerSpriteSize + 2;
    return {2 + (index % framesPerRow) * stride, 266 + (index / framesPerRow) * stride,
        PlayerSpriteSize, PlayerSpriteSize};
}

void DrawBallSprite(HDC target, HDC atlas, const Sprite& sprite, double x, double y)
{
    if (x > -BallSize && x < GameConst::WindowWidth && y > -BallSize && y < GameConst::WindowHeight)
        DrawSprite(target, atlas, sprite, static_cast<int>(x), static_cast<int>(y), BallSize, BallSize);
}

void DrawBall(HDC target, HDC atlas, const BallState& ball, int frame)
{
    if (ball.spike)
    {
        const double speed = std::hypot(static_cast<double>(ball.vx), static_cast<double>(ball.vy));
        if (std::isfinite(speed) && speed > 0.0001)
        {
            const double dx = 40 * (ball.vx / speed);
            const double dy = 40 * (ball.vy / speed);
            DrawBallSprite(target, atlas, BallTrail, ball.x - 2 * dx, ball.y - 2 * dy);
            DrawBallSprite(target, atlas, SpikeGlow, ball.x - dx, ball.y - dy);
        }
    }

    DrawBallSprite(target, atlas, {88 + 42 * frame, 158, 40, 40}, ball.x, ball.y);
}
}

Renderer::~Renderer()
{
    Shutdown();
}

bool Renderer::Initialize(HWND window)
{
    if (!IsWindow(window)) return false;
    if (window_ == window) return true;

    Shutdown();
    const HDC windowDC = GetDC(window);
    if (windowDC == nullptr) return false;

    backBufferDC_ = CreateCompatibleDC(windowDC);
    spriteDC_ = CreateCompatibleDC(windowDC);
    mirroredSpriteDC_ = CreateCompatibleDC(windowDC);
    shadeDC_ = CreateCompatibleDC(windowDC);
    backBuffer_ = CreateCompatibleBitmap(windowDC, GameConst::WindowWidth, GameConst::WindowHeight);
    mirroredSprite_ = CreateCompatibleBitmap(windowDC, PlayerSpriteSize, PlayerSpriteSize);
    shade_ = CreateCompatibleBitmap(windowDC, 1, 1);
    ReleaseDC(window, windowDC);

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    spriteSheet_ = LoadBitmapW(instance, MAKEINTRESOURCEW(IDB_BITMAP1));
    warning_ = LoadBitmapW(instance, MAKEINTRESOURCEW(IDB_BITMAP2));
    const std::array<int, 4> sizes{48, 20, 68, 18};
    for (std::size_t i = 0; i < sizes.size(); ++i)
        resultFonts_[i] = CreateFontW(-sizes[i], 0, 0, 0, i == 1 || i == 3 ? FW_MEDIUM : FW_BOLD,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
    if (!backBufferDC_ || !spriteDC_ || !mirroredSpriteDC_ ||
        !backBuffer_ || !spriteSheet_ || !mirroredSprite_ || !warning_ || !shadeDC_ || !shade_ ||
        std::any_of(resultFonts_.begin(), resultFonts_.end(), [](HFONT font) { return !font; }))
    {
        Shutdown();
        return false;
    }

    originalBackBuffer_ = SelectObject(backBufferDC_, backBuffer_);
    originalSprite_ = SelectObject(spriteDC_, spriteSheet_);
    originalMirroredSprite_ = SelectObject(mirroredSpriteDC_, mirroredSprite_);
    originalShade_ = SelectObject(shadeDC_, shade_);
    if (!originalBackBuffer_ || !originalSprite_ || !originalMirroredSprite_ || !originalShade_)
    {
        Shutdown();
        return false;
    }

    window_ = window;
    PatBlt(mirroredSpriteDC_, 0, 0, PlayerSpriteSize, PlayerSpriteSize, WHITENESS);
    SetPixel(shadeDC_, 0, 0, RGB(6, 14, 27));
    return true;
}

void Renderer::DrawPlayer(const PlayerState& player, int side)
{
    const int x = player.x;
    const int y = player.y;
    if (x <= -PlayerSize || x >= GameConst::WindowWidth) return;

    DrawSprite(backBufferDC_, spriteDC_, Shadow, x + (side == 0 ? 40 : 35),
        GameConst::CourtBottom - 15, 100, 25);
    if (y <= -PlayerSize || y >= GameConst::WindowHeight) return;

    const Sprite sprite = PlayerSprite(player.action, player.frame);
    if (side == 0)
        DrawSprite(backBufferDC_, spriteDC_, sprite, x, y, PlayerSize, PlayerSize);
    else
    {
        // TransparentBlt cannot mirror. Flip the sprite into a reusable bitmap first.
        StretchBlt(mirroredSpriteDC_, PlayerSpriteSize, 0, -PlayerSpriteSize, PlayerSpriteSize,
            spriteDC_, sprite.x, sprite.y, sprite.width, sprite.height, SRCCOPY);
        DrawSprite(backBufferDC_, mirroredSpriteDC_, {0, 0, PlayerSpriteSize, PlayerSpriteSize},
            x, y, PlayerSize, PlayerSize);
    }

    if (player.evil && SelectObject(spriteDC_, warning_))
    {
        DrawSprite(backBufferDC_, spriteDC_, Warning, x + (side == 0 ? 100 : 20), y - 40, 50, 50);
        SelectObject(spriteDC_, spriteSheet_);
    }
}

void Renderer::DrawStageResult(const StageResult& result, const GameStatePacket& state)
{
    const int saved = SaveDC(backBufferDC_);
    const BLENDFUNCTION blend{AC_SRC_OVER, 0, 205, 0};
    AlphaBlend(backBufferDC_, 0, 0, GameConst::WindowWidth, GameConst::WindowHeight,
        shadeDC_, 0, 0, 1, 1, blend);
    const COLORREF gold = RGB(255, 211, 76), white = RGB(239, 245, 255), muted = RGB(169, 189, 215);
    const auto box = [&](RECT rect, COLORREF fill, COLORREF border, int radius)
    {
        SelectObject(backBufferDC_, GetStockObject(DC_BRUSH));
        SelectObject(backBufferDC_, GetStockObject(DC_PEN));
        SetDCBrushColor(backBufferDC_, fill);
        SetDCPenColor(backBufferDC_, border);
        RoundRect(backBufferDC_, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    };
    const auto text = [&](const std::wstring& value, RECT rect, int font, COLORREF color)
    {
        SelectObject(backBufferDC_, resultFonts_[font]);
        SetBkMode(backBufferDC_, TRANSPARENT);
        SetTextColor(backBufferDC_, color);
        DrawTextW(backBufferDC_, value.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    };
    RECT shadow = StageOverLayout::Card;
    OffsetRect(&shadow, 12, 12);
    box(shadow, RGB(5, 12, 23), RGB(5, 12, 23), 24);
    box(StageOverLayout::Card, RGB(19, 32, 52), RGB(73, 95, 126), 24);
    box({432, 84, 728, 122}, RGB(38, 54, 77), RGB(38, 54, 77), 20);
    text(L"STAGE 0" + std::to_wstring(result.stage) + L"  COMPLETE", {432, 84, 728, 122}, 3, gold);
    const bool ownWin = result.myIndex == result.winner;
    text(ownWin ? L"스테이지 승리!" : L"다음 판을 노려요", {250, 132, 910, 190}, 0,
        ownWin ? gold : RGB(133, 211, 255));
    const std::wstring winnerName = result.names[result.winner].empty()
        ? L"플레이어 " + std::to_wstring(result.winner + 1) : result.names[result.winner];
    text(winnerName + L" 님이 이번 스테이지를 가져갔어요.", {250, 199, 910, 231}, 1, muted);
    box({250, 246, 910, 249}, gold, gold, 0);
    for (int side = 0; side < GameConst::PlayerCount; ++side)
    {
        const int left = side == 0 ? 250 : 600;
        const bool winner = side == result.winner;
        box({left, 270, left + 310, 530}, RGB(28, 45, 68), winner ? gold : RGB(60, 80, 106), 18);
        text(winner ? L"WINNER" : L"LOSER", {left + 16, 280, left + 294, 307}, 3, winner ? gold : muted);
        const Sprite pose = PlayerSprite(winner ? Action::Win : Action::Lose, state.p[side].frame);
        if (side == 0)
            DrawSprite(backBufferDC_, spriteDC_, pose, left + 107, 310, 96, 96);
        else
        {
            StretchBlt(mirroredSpriteDC_, PlayerSpriteSize, 0, -PlayerSpriteSize, PlayerSpriteSize,
                spriteDC_, pose.x, pose.y, pose.width, pose.height, SRCCOPY);
            DrawSprite(backBufferDC_, mirroredSpriteDC_, {0, 0, PlayerSpriteSize, PlayerSpriteSize}, left + 107, 310, 96, 96);
        }
        std::wstring name = result.names[side].empty() ? L"플레이어 " + std::to_wstring(side + 1) : result.names[side];
        if (side == result.myIndex) name += L" (나)";
        text(name, {left + 16, 410, left + 294, 444}, 1, white);
        text(std::to_wstring(result.scores[side]), {left + 16, 446, left + 294, 523}, 2, winner ? gold : white);
    }
    text(L"VS", {560, 348, 600, 380}, 3, muted);
    const wchar_t* hint = result.ended ? L"상대가 방을 나갔습니다." : !result.CanContinue()
        ? L"마지막 스테이지가 끝났습니다." : result.ready
        ? L"준비 완료! 상대 플레이어를 기다리고 있어요." : L"Z 또는 ‘다음 준비’를 눌러 다음 스테이지로!";
    text(hint, {250, 650, 910, 686}, 1, result.ready ? gold : white);
    text(result.CanContinue() ? L"두 플레이어가 모두 준비하면 자동으로 시작됩니다."
        : L"채팅을 계속하거나 방에서 나갈 수 있어요.", {250, 692, 910, 723}, 3, muted);
    RestoreDC(backBufferDC_, saved);
}

void Renderer::UpdateBallRotation(const GameStatePacket& state)
{
    const int direction = state.phase == Phase::Playing
        ? (state.ball.vx > 0) - (state.ball.vx < 0) : 0;
    if (direction == 0 || ballDirection_ == 0 || state.tick < lastBallTick_)
    {
        ballFrame_ = 0;
        ballRotationMs_ = 0;
    }
    else
    {
        // The previous snapshot supplies the direction up to this tick.
        const auto elapsed = static_cast<std::uint64_t>(state.tick - lastBallTick_) *
            GameConst::TickMs + ballRotationMs_;
        const int steps = static_cast<int>((elapsed / GameConst::RotationTimerMs) % BallFrameCount);
        ballFrame_ = (ballFrame_ + ballDirection_ * steps + BallFrameCount) % BallFrameCount;
        ballRotationMs_ = static_cast<int>(elapsed % GameConst::RotationTimerMs);
    }
    lastBallTick_ = state.tick;
    ballDirection_ = direction;
}

void Renderer::Render(const GameStatePacket& state, std::array<std::uint8_t, GameConst::PlayerCount> scores,
    int bottomInset, const StageResult* result)
{
    if (!window_) return;
    RECT client{};
    if (!GetClientRect(window_, &client) || client.right <= 0 || client.bottom <= 0) return;
    client.bottom -= std::clamp(bottomInset, 0, static_cast<int>(client.bottom));
    if (client.bottom <= 0) return;

    UpdateBallRotation(state);
    DrawCourt(backBufferDC_, spriteDC_, state.tick);
    for (int side = 0; side < GameConst::PlayerCount; ++side)
        DrawPlayer(state.p[side], side);
    if (state.phase != Phase::StageOver && state.phase != Phase::GameOver)
        DrawBall(backBufferDC_, spriteDC_, state.ball, ballFrame_);

    constexpr int scoreMargin = 50;
    DrawScore(backBufferDC_, spriteDC_, scores[0], scoreMargin, false);
    DrawScore(backBufferDC_, spriteDC_, scores[1], GameConst::WindowWidth - scoreMargin, true);
    if (result && result->winner < GameConst::PlayerCount) DrawStageResult(*result, state);

    const HDC windowDC = GetDC(window_);
    if (!windowDC) return;
    const int stretchMode = SetStretchBltMode(windowDC, result ? HALFTONE : COLORONCOLOR);
    POINT brushOrigin{};
    if (result) SetBrushOrgEx(windowDC, 0, 0, &brushOrigin);
    StretchBlt(windowDC, 0, 0, client.right, client.bottom, backBufferDC_,
        0, 0, GameConst::WindowWidth, GameConst::WindowHeight, SRCCOPY);
    if (result) SetBrushOrgEx(windowDC, brushOrigin.x, brushOrigin.y, nullptr);
    if (stretchMode) SetStretchBltMode(windowDC, stretchMode);
    ReleaseDC(window_, windowDC);
}

void Renderer::Shutdown()
{
    if (originalBackBuffer_) SelectObject(backBufferDC_, originalBackBuffer_);
    if (originalSprite_) SelectObject(spriteDC_, originalSprite_);
    if (originalMirroredSprite_) SelectObject(mirroredSpriteDC_, originalMirroredSprite_);
    if (originalShade_) SelectObject(shadeDC_, originalShade_);
    if (backBufferDC_) DeleteDC(backBufferDC_);
    if (spriteDC_) DeleteDC(spriteDC_);
    if (mirroredSpriteDC_) DeleteDC(mirroredSpriteDC_);
    if (backBuffer_) DeleteObject(backBuffer_);
    if (spriteSheet_) DeleteObject(spriteSheet_);
    if (warning_) DeleteObject(warning_);
    if (mirroredSprite_) DeleteObject(mirroredSprite_);
    if (shadeDC_) DeleteDC(shadeDC_);
    if (shade_) DeleteObject(shade_);
    for (HFONT font : resultFonts_) if (font) DeleteObject(font);
    resultFonts_.fill(nullptr);

    lastBallTick_ = 0;
    ballDirection_ = 0;
    ballRotationMs_ = 0;
    ballFrame_ = 0;
    window_ = nullptr;
    backBufferDC_ = nullptr;
    backBuffer_ = nullptr;
    originalBackBuffer_ = nullptr;
    spriteDC_ = nullptr;
    spriteSheet_ = nullptr;
    originalSprite_ = nullptr;
    warning_ = nullptr;
    mirroredSpriteDC_ = nullptr;
    mirroredSprite_ = nullptr;
    originalMirroredSprite_ = nullptr;
    shadeDC_ = nullptr;
    shade_ = nullptr;
    originalShade_ = nullptr;
}
