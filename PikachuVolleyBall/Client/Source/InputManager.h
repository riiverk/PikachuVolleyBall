#pragma once
#include "Common.h"

class InputManager
{
public:
    bool OnKeyDown(WPARAM vk);
    bool OnKeyUp(WPARAM vk);
    std::uint8_t GetKeys() const { return keys_; }
    void Reset() { keys_ = 0; }
private:
    std::uint8_t keys_ = 0;
};
