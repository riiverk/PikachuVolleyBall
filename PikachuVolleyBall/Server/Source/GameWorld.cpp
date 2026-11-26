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
        if (stage_ == 1)
        {
            const int loser = GetLoser();
            if (loser >= 0) awakened[loser] = 1;
        }
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
        walkTicks_[i] = 0;
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
    UpdatePlayerFrames(previousPlayers);
    UpdateBall(previousPlayers);
    CheckScore();
    ++state_.tick;
}

void GameWorld::StartSlides()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
    {
        const Action before = state_.p[i].action;
        StartPlayerSlide(state_.p[i], playerInputs_[i], slideCount_[i], slideDirection_[i]);
        if (before != Action::Slide && state_.p[i].action == Action::Slide)
            events_.push_back({EventKind::Slide, static_cast<std::uint8_t>(i)});
    }
}

void GameWorld::SlidePlayers()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
        UpdatePlayerSlide(state_.p[i], i, slideCount_[i], slideDirection_[i]);
}

void GameWorld::UpdatePlayerFrames(const PlayerState (&previousPlayers)[GameConst::PlayerCount])
{
    constexpr std::array walkFrames{0, 1, 2, 3, 4, 3, 2, 1};
    constexpr int ticksPerWalkFrame = 4;
    constexpr int walkCycleTicks = static_cast<int>(walkFrames.size()) * ticksPerWalkFrame;
    constexpr int jumpDurationTicks = 1 - 2 * GameConst::JumpPower / GameConst::JumpGravity;
    for (int i = 0; i < GameConst::PlayerCount; ++i)
    {
        PlayerState& player = state_.p[i];
        const PlayerState& previous = previousPlayers[i];
        if (player.action == Action::Move && player.x != previous.x)
        {
            player.frame = static_cast<std::uint16_t>(walkFrames[walkTicks_[i] / ticksPerWalkFrame]);
            walkTicks_[i] = (walkTicks_[i] + 1) % walkCycleTicks;
            continue;
        }

        walkTicks_[i] = 0;
        if (player.action == Action::Jump)
        {
            // Velocity advances once per tick, so each jump starts its own frame sequence.
            const int airborneTicks = (velocityY_[i] - GameConst::JumpPower) / GameConst::JumpGravity;
            const int ticksToLand = jumpDurationTicks - airborneTicks;
            if (airborneTicks <= 3) player.frame = 0;
            else if (ticksToLand <= 8) player.frame = ticksToLand <= 5 ? 4 : 3;
            else player.frame = static_cast<std::uint16_t>(1 + ((airborneTicks - 4) / 2) % 2);
        }
        else if (player.action == Action::Slide)
        {
            // The slide update has already consumed this tick from the remaining count.
            const int remaining = slideCount_[i];
            player.frame = remaining >= 30 ? 0 : remaining >= 16 ? 1 : 2;
        }
        else if (player.action == Action::Idle || player.action == Action::Move || player.action != previous.action)
            player.frame = 0;
    }
}

void GameWorld::MovePlayers()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
        MovePlayerHorizontally(state_.p[i], playerInputs_[i], i);
}

void GameWorld::JumpPlayers()
{
    for (int i = 0; i < GameConst::PlayerCount; ++i)
    {
        const Action before = state_.p[i].action;
        UpdatePlayerJump(state_.p[i], playerInputs_[i], velocityY_[i]);
        const Action after = state_.p[i].action;
        const auto player = static_cast<std::uint8_t>(i);
        if (playerInputs_[i].jump && (after == Action::Jump || after == Action::Spike))
            events_.push_back({EventKind::Jump, player});
        if (before != Action::Spike && after == Action::Spike)
            events_.push_back({EventKind::Spike, player});
    }
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
    std::uint8_t hitter = NO_PLAYER;
    const bool hitNet = ReflectBallAtNet(netBall, previous, &netTime);
    const bool hitHead = ReflectBallAtPlayers(headBall, previous, state_.p, previousPlayers, headTime, &hitter);
    // Evaluate both contacts along the same movement and apply the earlier one.
    if (hitHead && (!hitNet || headTime < netTime))
    {
        next = headBall;
        events_.push_back({EventKind::Hit, hitter});
    }
    else if (hitNet) next = netBall;

    ReflectBallAtWalls(next);
    state_.ball = next;
}

void GameWorld::CheckScore()
{
    constexpr float floorY = GameConst::CourtBottom - GameConst::BallCollisionSize;
    if (state_.phase != Phase::Playing || state_.ball.y < floorY) return;

    constexpr float netCenterX = (GameConst::NetLeft + GameConst::NetRight) / 2.0f;
    const float ballCenterX = state_.ball.x + GameConst::BallCenterX;
    const std::uint8_t scorer = ballCenterX < netCenterX ? 1 : 0;
    GameEvent event{EventKind::Score, scorer, {scores_[0], scores_[1]}};
    ++event.score[scorer];
    events_.push_back(event);
    ++scores_[scorer];

    if (scores_[scorer] < GameConst::WinningScore)
    {
        ResetBall(scorer);
        return;
    }

    state_.ball.y = floorY;
    state_.ball.vx = 0;
    state_.ball.vy = 0;
    state_.ball.spike = 0;
    state_.phase = stage_ == GameConst::StageCount ? Phase::GameOver : Phase::StageOver;
    for (int i = 0; i < GameConst::PlayerCount; ++i)
    {
        state_.p[i].action = i == scorer ? Action::Win : Action::Lose;
        state_.p[i].frame = 0;
    }
}

void GameWorld::FillState(GameStatePacket& out) const
{
    out = state_;
}

std::vector<GameEvent> GameWorld::PopEvents()
{
    std::vector<GameEvent> pending;
    pending.swap(events_);
    return pending;
}

bool GameWorld::IsStageOver() const
{
    return GetLoser() >= 0;
}

int GameWorld::GetLoser() const
{
    if (scores_[0] >= GameConst::WinningScore) return 1;
    if (scores_[1] >= GameConst::WinningScore) return 0;
    return -1;
}
