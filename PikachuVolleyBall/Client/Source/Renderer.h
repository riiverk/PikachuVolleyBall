#pragma once
#include "Common.h"
#include <array>

// Online renderer for server game states.
class Renderer
{
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Initialize(HWND window);
    void Render(const GameStatePacket& state, std::array<std::uint8_t, GameConst::PlayerCount> scores = {});
    void Shutdown();

private:
    friend struct RendererTestAccess;

    void DrawPlayer(const PlayerState& player, int side);

    HWND window_ = nullptr;
    HDC backBufferDC_ = nullptr;
    HBITMAP backBuffer_ = nullptr;
    HGDIOBJ originalBackBuffer_ = nullptr;
    HDC spriteDC_ = nullptr;
    HBITMAP spriteSheet_ = nullptr;
    HGDIOBJ originalSprite_ = nullptr;
    HBITMAP warning_ = nullptr;
    HDC mirroredSpriteDC_ = nullptr;
    HBITMAP mirroredSprite_ = nullptr;
    HGDIOBJ originalMirroredSprite_ = nullptr;
};
