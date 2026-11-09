#include "GameWorld.h"
#include "PlayerMovement.h"
#include "BallPhysics.h"
#include <stdexcept>

GameWorld::GameWorld()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
    {
        state_.p[i].x = GameConst::PlayerStartX[i];
        state_.p[i].y = GameConst::PlayerGroundY;
    }
    ResetBall(0);
}

void GameWorld::Reset(int, int) { /* TODO 11/10: initialize stage and serve. */ }
void GameWorld::Update(const std::uint8_t inputs[2])
{
    const PlayerState previousPlayers[GameConst::PlayerCount] = {state_.p[0], state_.p[1]};
    ApplyInputs(inputs);
    StartSlides();
    MovePlayers();
    JumpPlayers();
    SlidePlayers();
    UpdateBall(previousPlayers);
    /* TODO 11/11: integrate scoring and stage progression into the tick. */
}

void GameWorld::StartSlides()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
        StartPlayerSlide(state_.p[i], playerInputs_[i], slideCount_[i], slideDirection_[i]);
}

void GameWorld::SlidePlayers()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
        UpdatePlayerSlide(state_.p[i], i, slideCount_[i], slideDirection_[i]);
}

void GameWorld::MovePlayers()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
        MovePlayerHorizontally(state_.p[i], playerInputs_[i], i);
}

void GameWorld::JumpPlayers()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
        UpdatePlayerJump(state_.p[i], playerInputs_[i], velocityY_[i]);
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
void GameWorld::ResetBall(int serveSide)
{
    ResetBallForServe(state_.ball, serveSide);
    serveSide_ = serveSide;
}

void GameWorld::UpdateBall() { UpdateBall(state_.p); }
void GameWorld::UpdateBall(const PlayerState (&previousPlayers)[2])
{
    AdvanceBall(state_.ball, state_.p, previousPlayers);
}
void GameWorld::CheckScore() { /* TODO 11/13: scoring. */ }
void GameWorld::FillState(GameStatePacket& out) { out = state_; /* TODO 11/15 */ }
std::vector<GameEvent> GameWorld::PopEvents() { return {}; /* TODO 11/18 */ }
bool GameWorld::IsStageOver() { return false; /* TODO 11/14 */ }
int GameWorld::GetLoser() { return -1; /* No result until scoring is implemented. */ }
