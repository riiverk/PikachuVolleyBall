#pragma once
#include "Common.h"
#include <array>
struct StageResult;

// Online renderer for server game states.
class Renderer
{
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Initialize(HWND window);
    void Render(const GameStatePacket& state, std::array<std::uint8_t, GameConst::PlayerCount> scores = {},
        int bottomInset = 0, const StageResult* result = nullptr);
    void Shutdown();

private:
    friend struct RendererTestAccess;

    void DrawPlayer(const PlayerState& player, int side);
    void DrawStageResult(const StageResult& result, const GameStatePacket& state);

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
    HDC shadeDC_ = nullptr;
    HBITMAP shade_ = nullptr;
    HGDIOBJ originalShade_ = nullptr;
    std::array<HFONT, 4> resultFonts_{};
};
