#pragma once
#include <bit>
#include <cstdint>
#include <type_traits>

// Windows x86/x64 TCP wire format: packed, little-endian, IEEE-754 float32.
// Text is UTF-8, NUL terminated. Capacities include the terminating NUL byte.
inline constexpr std::uint16_t SERVERPORT = 9000;
inline constexpr int MAX_NICK = 16;
inline constexpr int MAX_CHAT = 64;
inline constexpr std::uint8_t NO_PLAYER = 0xff;

enum class PacketType : std::uint8_t
{
    JOIN_ROOM = 1, INPUT, NEXT_STAGE, LEAVE, CHAT, JOIN_RESULT,
    GAME_START, GAME_STATE, EVENT, SCORE, STAGE_OVER, GAME_OVER
};
enum InputBit : std::uint8_t { LEFT = 1, RIGHT = 2, UP = 4, ACTION = 8 };
enum class Action : std::uint8_t { Idle, Move, Jump, Slide, Spike, Win, Lose };
enum class Phase : std::uint8_t { Ready, Playing, ServeWait, StageOver, GameOver };
enum class EventKind : std::uint8_t { Jump, Slide, Spike, Hit, Score };
enum class JoinResult : std::uint8_t { Waiting, Matched, Full, Rejected };
enum class GameOverReason : std::uint8_t { Normal, Quit };

#pragma pack(push, 1)
struct PacketHeader { std::uint16_t size; PacketType type; };
struct PlayerState
{
    std::int32_t x = 0, y = 0;
    Action action = Action::Idle;
    std::uint16_t frame = 0;
    std::uint8_t evil = 0;
};
struct BallState
{
    float x = 0, y = 0, vx = 0, vy = 0;
    std::uint8_t spike = 0;
};
struct JoinRoomPacket
{
    std::uint16_t size = sizeof(JoinRoomPacket);
    PacketType type = PacketType::JOIN_ROOM;
    std::int32_t roomNo = 0;
    char nickname[MAX_NICK]{};
};
struct InputPacket
{
    std::uint16_t size = sizeof(InputPacket);
    PacketType type = PacketType::INPUT;
    std::uint8_t keys = 0;
};
struct NextStagePacket
{
    std::uint16_t size = sizeof(NextStagePacket);
    PacketType type = PacketType::NEXT_STAGE;
};
struct LeavePacket
{
    std::uint16_t size = sizeof(LeavePacket);
    PacketType type = PacketType::LEAVE;
};
struct ChatPacket
{
    std::uint16_t size = sizeof(ChatPacket);
    PacketType type = PacketType::CHAT;
    std::uint8_t sender = NO_PLAYER;
    char msg[MAX_CHAT]{};
};
struct JoinResultPacket
{
    std::uint16_t size = sizeof(JoinResultPacket);
    PacketType type = PacketType::JOIN_RESULT;
    JoinResult result = JoinResult::Rejected;
    std::uint8_t myIndex = NO_PLAYER;
};
struct GameStartPacket
{
    std::uint16_t size = sizeof(GameStartPacket);
    PacketType type = PacketType::GAME_START;
    std::uint8_t stage = 1, serveSide = 0;
    char nick[2][MAX_NICK]{};
};
struct GameStatePacket
{
    std::uint16_t size = sizeof(GameStatePacket);
    PacketType type = PacketType::GAME_STATE;
    std::uint32_t tick = 0;
    Phase phase = Phase::Ready;
    PlayerState p[2]{};
    BallState ball{};
};
struct EventPacket
{
    std::uint16_t size = sizeof(EventPacket);
    PacketType type = PacketType::EVENT;
    EventKind kind = EventKind::Jump;
    std::uint8_t who = NO_PLAYER;
};
struct ScorePacket
{
    std::uint16_t size = sizeof(ScorePacket);
    PacketType type = PacketType::SCORE;
    std::uint8_t scorer = NO_PLAYER, score[2]{};
};
struct StageOverPacket
{
    std::uint16_t size = sizeof(StageOverPacket);
    PacketType type = PacketType::STAGE_OVER;
    std::uint8_t stage = 1, winner = NO_PLAYER;
};
struct GameOverPacket
{
    std::uint16_t size = sizeof(GameOverPacket);
    PacketType type = PacketType::GAME_OVER;
    std::uint8_t winner = NO_PLAYER;
    GameOverReason reason = GameOverReason::Normal;
};
#pragma pack(pop)

constexpr std::uint16_t PacketSize(PacketType type)
{
    switch (type)
    {
    case PacketType::JOIN_ROOM: return sizeof(JoinRoomPacket);
    case PacketType::INPUT: return sizeof(InputPacket);
    case PacketType::NEXT_STAGE: return sizeof(NextStagePacket);
    case PacketType::LEAVE: return sizeof(LeavePacket);
    case PacketType::CHAT: return sizeof(ChatPacket);
    case PacketType::JOIN_RESULT: return sizeof(JoinResultPacket);
    case PacketType::GAME_START: return sizeof(GameStartPacket);
    case PacketType::GAME_STATE: return sizeof(GameStatePacket);
    case PacketType::EVENT: return sizeof(EventPacket);
    case PacketType::SCORE: return sizeof(ScorePacket);
    case PacketType::STAGE_OVER: return sizeof(StageOverPacket);
    case PacketType::GAME_OVER: return sizeof(GameOverPacket);
    default: return 0;
    }
}
inline constexpr std::uint16_t MAX_PACKET_SIZE = sizeof(ChatPacket);
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(float) == 4);
static_assert(sizeof(PacketHeader) == 3 && sizeof(PlayerState) == 12 && sizeof(BallState) == 17);
static_assert(sizeof(JoinRoomPacket) == 23 && sizeof(InputPacket) == 4);
static_assert(sizeof(NextStagePacket) == 3 && sizeof(LeavePacket) == 3);
static_assert(sizeof(ChatPacket) == 68 && sizeof(JoinResultPacket) == 5);
static_assert(sizeof(GameStartPacket) == 37 && sizeof(GameStatePacket) == 49);
static_assert(sizeof(EventPacket) == 5 && sizeof(ScorePacket) == 6);
static_assert(sizeof(StageOverPacket) == 5 && sizeof(GameOverPacket) == 5);
static_assert(std::is_trivially_copyable_v<GameStatePacket>);
