#pragma once
#include "Protocol.h"

// One player's input for one server tick. It contains intentions only;
// position changes and animation lifetimes belong to the later movement code.
struct PlayerInput
{
    std::uint8_t held = 0;
    std::uint8_t pressed = 0;
    std::uint8_t released = 0;
    int moveDirection = 0;
    bool jump = false;
    bool spike = false;
    int slideDirection = 0;
    Action action = Action::Idle;
};

inline PlayerInput DecodePlayerInput(std::uint8_t keys, std::uint8_t previous, Action currentAction)
{
    constexpr std::uint8_t validKeys = LEFT | RIGHT | UP | ACTION;
    PlayerInput input;
    input.held = keys & validKeys;
    input.pressed = input.held & static_cast<std::uint8_t>(~previous);
    input.released = (previous & validKeys) & static_cast<std::uint8_t>(~input.held);

    // Victory/defeat and sliding cannot be interrupted by another action.
    if (currentAction == Action::Win || currentAction == Action::Lose || currentAction == Action::Slide)
    {
        input.action = currentAction;
        return input;
    }
    const bool airborne = currentAction == Action::Jump || currentAction == Action::Spike;
    input.moveDirection = ((input.held & RIGHT) != 0) - ((input.held & LEFT) != 0);
    input.jump = !airborne && (input.pressed & UP) != 0;
    input.spike = (airborne || input.jump) && (input.pressed & ACTION) != 0;
    if (!airborne && !input.jump && (input.pressed & ACTION) != 0)
        input.slideDirection = input.moveDirection;

    if (input.spike) input.action = Action::Spike;
    else if (airborne) input.action = currentAction;
    else if (input.jump) input.action = Action::Jump;
    else if (input.slideDirection != 0) input.action = Action::Slide;
    else if (input.moveDirection != 0) input.action = Action::Move;
    return input;
}
