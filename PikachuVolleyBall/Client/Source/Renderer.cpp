#include "Renderer.h"
#include "resource.h"
#include <algorithm>
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

void DrawSprite(HDC target, HDC source, const Sprite& sprite, int x, int y, int width, int height)
{
    TransparentBlt(target, x, y, width, height, source,
        sprite.x, sprite.y, sprite.width, sprite.height, RGB(255, 255, 255));
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

void DrawBall(HDC target, HDC atlas, const BallState& ball, std::uint32_t tick)
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

    constexpr int frameCount = 5;
    constexpr int ticksPerFrame = GameConst::RotationTimerMs / GameConst::TickMs;
    int frame = static_cast<int>((tick / ticksPerFrame) % frameCount);
    if (ball.vx == 0) frame = 0;
    if (ball.vx < 0) frame = (frameCount - frame) % frameCount;
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
    backBuffer_ = CreateCompatibleBitmap(windowDC, GameConst::WindowWidth, GameConst::WindowHeight);
    mirroredSprite_ = CreateCompatibleBitmap(windowDC, PlayerSpriteSize, PlayerSpriteSize);
    ReleaseDC(window, windowDC);

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    spriteSheet_ = LoadBitmapW(instance, MAKEINTRESOURCEW(IDB_BITMAP1));
    warning_ = LoadBitmapW(instance, MAKEINTRESOURCEW(IDB_BITMAP2));
    if (!backBufferDC_ || !spriteDC_ || !mirroredSpriteDC_ ||
        !backBuffer_ || !spriteSheet_ || !mirroredSprite_ || !warning_)
    {
        Shutdown();
        return false;
    }

    originalBackBuffer_ = SelectObject(backBufferDC_, backBuffer_);
    originalSprite_ = SelectObject(spriteDC_, spriteSheet_);
    originalMirroredSprite_ = SelectObject(mirroredSpriteDC_, mirroredSprite_);
    if (!originalBackBuffer_ || !originalSprite_ || !originalMirroredSprite_)
    {
        Shutdown();
        return false;
    }

    window_ = window;
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

void Renderer::Render(const GameStatePacket& state)
{
    if (!window_) return;
    RECT client{};
    if (!GetClientRect(window_, &client) || client.right <= 0 || client.bottom <= 0) return;

    DrawCourt(backBufferDC_, spriteDC_, state.tick);
    for (int side = 0; side < GameConst::PlayerCount; ++side)
        DrawPlayer(state.p[side], side);
    if (state.phase != Phase::StageOver && state.phase != Phase::GameOver)
        DrawBall(backBufferDC_, spriteDC_, state.ball, state.tick);

    const HDC windowDC = GetDC(window_);
    if (!windowDC) return;
    StretchBlt(windowDC, 0, 0, client.right, client.bottom, backBufferDC_,
        0, 0, GameConst::WindowWidth, GameConst::WindowHeight, SRCCOPY);
    ReleaseDC(window_, windowDC);
}

void Renderer::Shutdown()
{
    if (originalBackBuffer_) SelectObject(backBufferDC_, originalBackBuffer_);
    if (originalSprite_) SelectObject(spriteDC_, originalSprite_);
    if (originalMirroredSprite_) SelectObject(mirroredSpriteDC_, originalMirroredSprite_);
    if (backBufferDC_) DeleteDC(backBufferDC_);
    if (spriteDC_) DeleteDC(spriteDC_);
    if (mirroredSpriteDC_) DeleteDC(mirroredSpriteDC_);
    if (backBuffer_) DeleteObject(backBuffer_);
    if (spriteSheet_) DeleteObject(spriteSheet_);
    if (warning_) DeleteObject(warning_);
    if (mirroredSprite_) DeleteObject(mirroredSprite_);

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
}
