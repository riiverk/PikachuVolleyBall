#include "InputManager.h"

namespace
{
std::uint8_t KeyBit(WPARAM vk)
{
    switch (vk)
    {
    case VK_LEFT: return LEFT;
    case VK_RIGHT: return RIGHT;
    case VK_UP: return UP;
    case 'Z': return ACTION;
    default: return 0;
    }
}
}

bool InputManager::OnKeyDown(WPARAM vk)
{
    const std::uint8_t bit = KeyBit(vk);
    if (bit == 0 || (keys_ & bit) != 0) return false;
    keys_ |= bit;
    return true;
}

bool InputManager::OnKeyUp(WPARAM vk)
{
    const std::uint8_t bit = KeyBit(vk);
    if (bit == 0 || (keys_ & bit) == 0) return false;
    keys_ &= static_cast<std::uint8_t>(~bit);
    return true;
}
