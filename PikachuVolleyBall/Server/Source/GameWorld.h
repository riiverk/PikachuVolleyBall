#pragma once
#include "Protocol.h"
#include <vector>

struct GameEvent { EventKind kind; std::uint8_t who; };

// 11/1 scaffold. Physics and scoring are scheduled for 11/2-11/14.
class GameWorld
{
public:
    void Reset(int stage, int serveSide);
    void Update(const std::uint8_t inputs[2]);
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
    std::uint8_t scores_[2]{};
    int stage_ = 1;
    int serveSide_ = 0;
};
