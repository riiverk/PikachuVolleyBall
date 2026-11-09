#include "Renderer.h"
#include "resource.h"

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
    backBuffer_ = CreateCompatibleBitmap(windowDC, GameConst::WindowWidth, GameConst::WindowHeight);
    ReleaseDC(window, windowDC);

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    spriteSheet_ = LoadBitmapW(instance, MAKEINTRESOURCEW(IDB_BITMAP1));
    warning_ = LoadBitmapW(instance, MAKEINTRESOURCEW(IDB_BITMAP2));
    if (!backBufferDC_ || !spriteDC_ || !backBuffer_ || !spriteSheet_ || !warning_)
    {
        Shutdown();
        return false;
    }

    originalBackBuffer_ = SelectObject(backBufferDC_, backBuffer_);
    originalSprite_ = SelectObject(spriteDC_, spriteSheet_);
    if (!originalBackBuffer_ || !originalSprite_)
    {
        Shutdown();
        return false;
    }

    window_ = window;
    return true;
}

void Renderer::Render(const GameStatePacket&) { /* TODO 11/10-11/12 */ }

void Renderer::Shutdown()
{
    if (originalBackBuffer_) SelectObject(backBufferDC_, originalBackBuffer_);
    if (originalSprite_) SelectObject(spriteDC_, originalSprite_);
    if (backBufferDC_) DeleteDC(backBufferDC_);
    if (spriteDC_) DeleteDC(spriteDC_);
    if (backBuffer_) DeleteObject(backBuffer_);
    if (spriteSheet_) DeleteObject(spriteSheet_);
    if (warning_) DeleteObject(warning_);

    window_ = nullptr;
    backBufferDC_ = nullptr;
    backBuffer_ = nullptr;
    originalBackBuffer_ = nullptr;
    spriteDC_ = nullptr;
    spriteSheet_ = nullptr;
    originalSprite_ = nullptr;
    warning_ = nullptr;
}
