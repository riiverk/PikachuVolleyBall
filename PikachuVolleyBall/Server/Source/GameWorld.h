#pragma once
#include "Protocol.h"
#include "PlayerInput.h"
#include <array>
#include <vector>

struct GameEvent { EventKind kind; std::uint8_t who; };

// Input decoding is implemented; movement, physics and scoring follow separately.
class GameWorld
{
public:
    void Reset(int stage, int serveSide);
    void Update(const std::uint8_t inputs[2]);
    void ApplyInputs(const std::uint8_t inputs[2]);
    const PlayerInput& GetPlayerInput(int index) const;
    void UpdateBall();
    void CheckScore();
    void FillState(GameStatePacket& out);
    std::vector<GameEvent> PopEvents();
    bool IsStageOver();
    int GetLoser();
private:
    GameStatePacket state_{};
    std::vector<GameEvent> events_;
    std::uint8_t previousInputs_[2]{};
    std::array<PlayerInput, 2> playerInputs_{};
    std::uint8_t scores_[2]{};
    int stage_ = 1;
    int serveSide_ = 0;
};
