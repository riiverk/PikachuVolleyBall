#include "GameWorld.h"
#include "PlayerMovement.h"
#include "BallPhysics.h"
#include <stdexcept>

GameWorld::GameWorld()
{
    Reset(1, 0);
}

void GameWorld::Reset(int stage, int serveSide)
{
    if (stage < 1 || stage > GameConst::StageCount) throw std::out_of_range("stage");
    if (serveSide < 0 || serveSide >= GameConst::PlayerCount) throw std::out_of_range("serve side");

    // Read the previous result before clearing the scores. Serve side is independent.
    std::uint8_t awakened[GameConst::PlayerCount]{};
    if (stage == 2)
    {
        if (stage_ == 1 && scores_[0] != scores_[1])
            awakened[scores_[0] < scores_[1] ? 0 : 1] = 1;
        else if (stage_ == 2)
        {
            // Resetting the same second stage keeps the first-stage loser's awakening.
            for (int i = 0; i < GameConst::PlayerCount; ++i)
                awakened[i] = state_.p[i].evil;
        }
    }

    stage_ = stage;
    state_ = GameStatePacket{};
    events_.clear();
    for (int i = 0; i < GameConst::PlayerCount; ++i)
    {
        scores_[i] = 0;
        state_.p[i].x = GameConst::PlayerStartX[i];
        state_.p[i].y = GameConst::PlayerGroundY;
        state_.p[i].evil = awakened[i];
        previousInputs_[i] = 0;
        playerInputs_[i] = PlayerInput{};
        velocityY_[i] = 0;
        slideCount_[i] = 0;
        slideDirection_[i] = 0;
    }
    ResetBall(serveSide);
}

void GameWorld::Update(const std::uint8_t inputs[2])
{
    if (inputs == nullptr) throw std::invalid_argument("inputs must contain two player key states");
    if (state_.phase != Phase::Ready && state_.phase != Phase::Playing) return;

    const PlayerState previousPlayers[GameConst::PlayerCount] = {state_.p[0], state_.p[1]};
    ApplyInputs(inputs);
    state_.phase = Phase::Playing;
    StartSlides();
    MovePlayers();
    JumpPlayers();
    SlidePlayers();
    UpdateBall(previousPlayers);
    CheckScore();
    ++state_.tick;
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
    const BallState previous = state_.ball;
    BallState next = previous;
    MoveBallWithGravity(next);

    BallState netBall = next;
    BallState headBall = next;
    float netTime = 0;
    float headTime = 0;
    const bool hitNet = ReflectBallAtNet(netBall, previous, &netTime);
    const bool hitHead = ReflectBallAtPlayers(headBall, previous, state_.p, previousPlayers, headTime);
    // Evaluate both contacts along the same movement and apply the earlier one.
    if (hitHead && (!hitNet || headTime < netTime)) next = headBall;
    else if (hitNet) next = netBall;

    ReflectBallAtWalls(next);
    state_.ball = next;
}
void GameWorld::CheckScore() { /* TODO 11/13: scoring. */ }
void GameWorld::FillState(GameStatePacket& out) { out = state_; /* TODO 11/15 */ }
std::vector<GameEvent> GameWorld::PopEvents() { return {}; /* TODO 11/18 */ }
bool GameWorld::IsStageOver() { return false; /* TODO 11/14 */ }
int GameWorld::GetLoser() { return -1; /* No result until scoring is implemented. */ }
