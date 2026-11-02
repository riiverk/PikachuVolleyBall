#include "GameWorld.h"
#include <stdexcept>

void GameWorld::Reset(int, int) { /* TODO 11/10: initialize stage and serve. */ }
void GameWorld::Update(const std::uint8_t inputs[2])
{
    ApplyInputs(inputs);
    /* TODO 11/11: integrate movement, physics and scoring into the tick. */
}

void GameWorld::ApplyInputs(const std::uint8_t inputs[2])
{
    if (inputs == nullptr) throw std::invalid_argument("inputs must contain two player key states");
    for (int i = 0; i < 2; ++i)
    {
        playerInputs_[i] = DecodePlayerInput(inputs[i], previousInputs_[i], state_.p[i].action);
        previousInputs_[i] = playerInputs_[i].held;
    }
}

const PlayerInput& GameWorld::GetPlayerInput(int index) const
{
    return playerInputs_.at(index);
}
void GameWorld::UpdateBall() { /* TODO 11/12: ball physics. */ }
void GameWorld::CheckScore() { /* TODO 11/13: scoring. */ }
void GameWorld::FillState(GameStatePacket& out) { out = state_; /* TODO 11/15 */ }
std::vector<GameEvent> GameWorld::PopEvents() { return {}; /* TODO 11/18 */ }
bool GameWorld::IsStageOver() { return false; /* TODO 11/14 */ }
int GameWorld::GetLoser() { return -1; /* No result until scoring is implemented. */ }
