#include "GameWorld.h"

void GameWorld::Reset(int, int) { /* TODO 11/10: initialize stage and serve. */ }
void GameWorld::Update(const std::uint8_t[2]) { /* TODO 11/11: simulation tick. */ }
void GameWorld::UpdateBall() { /* TODO 11/12: ball physics. */ }
void GameWorld::CheckScore() { /* TODO 11/13: scoring. */ }
void GameWorld::FillState(GameStatePacket& out) { out = state_; /* TODO 11/15 */ }
std::vector<GameEvent> GameWorld::PopEvents() { return {}; /* TODO 11/18 */ }
bool GameWorld::IsStageOver() { return false; /* TODO 11/14 */ }
int GameWorld::GetLoser() { return -1; /* No result until scoring is implemented. */ }
