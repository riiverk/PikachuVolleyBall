#pragma once
#include "Protocol.h"
#include "PlayerInput.h"
#include <array>
#include <vector>

struct GameEvent { EventKind kind; std::uint8_t who; };

class GameWorld
{
public:
    GameWorld();
    void Reset(int stage, int serveSide);
    void Update(const std::uint8_t inputs[2]);
    void ApplyInputs(const std::uint8_t inputs[2]);
    const PlayerInput& GetPlayerInput(int index) const;
    void UpdateBall();
    void ResetBall(int serveSide);
    void CheckScore();
    void FillState(GameStatePacket& out);
    std::vector<GameEvent> PopEvents();
    bool IsStageOver() const;
    int GetLoser() const; // Returns -1 until a player reaches the winning score.
private:
    friend struct GameWorldTestAccess;
    void UpdateBall(const PlayerState (&previousPlayers)[2]);
    void StartSlides();
    void MovePlayers();
    void JumpPlayers();
    void SlidePlayers();
    GameStatePacket state_{};
    std::vector<GameEvent> events_;
    std::uint8_t previousInputs_[2]{};
    std::array<PlayerInput, 2> playerInputs_{};
    int velocityY_[2]{};
    int slideCount_[2]{};
    int slideDirection_[2]{};
    std::uint8_t scores_[2]{};
    int stage_ = 1;
    int serveSide_ = 0;
};
