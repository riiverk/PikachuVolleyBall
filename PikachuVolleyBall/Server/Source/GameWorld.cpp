#include "GameWorld.h"
#include "PlayerMovement.h"
#include <stdexcept>

GameWorld::GameWorld()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
    {
        state_.p[i].x = GameConst::PlayerStartX[i];
        state_.p[i].y = GameConst::PlayerGroundY;
    }
}

void GameWorld::Reset(int, int) { /* TODO 11/10: initialize stage and serve. */ }
void GameWorld::Update(const std::uint8_t inputs[2])
{
    ApplyInputs(inputs);
    MovePlayers();
    /* TODO 11/11: integrate jumping, physics and scoring into the tick. */
}

void GameWorld::MovePlayers()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
        MovePlayerHorizontally(state_.p[i], playerInputs_[i], i);
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
