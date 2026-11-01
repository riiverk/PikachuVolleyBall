#pragma once
#include "Common.h"

// New online renderer scaffold. Existing GameRenderer still runs local play.
class Renderer
{
public:
    bool Initialize(HWND window);
    void Render(const GameStatePacket& state);
    void Shutdown();
};
