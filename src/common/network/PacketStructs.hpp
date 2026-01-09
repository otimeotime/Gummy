#ifndef PACKET_STRUCTS_H
#define PACKET_STRUCTS_H

#include "PacketType.hpp"
#include <cstdint>

// User Authentication Packets ----------------------------
typedef struct {
    char username[32];
    char password[32];
    bool isLogin;
} ReqAuthenticate;

typedef struct {
    bool isLogin;
    bool isSuccess;
    uint32_t userId;
    int32_t elo;
    char message[100];
} ResAuthenticate;

typedef struct {
    char currentPassword[32];
    char newPassword[32];
} ReqChangePassword;

typedef struct {
    bool isSuccess;
    char message[100];
} ResChangePassword;
// --------------------------------------------------------
// Home Game Packets --------------------------------------
typedef struct {
    char query_username[32];
} ReqSearchUser;

typedef struct {
    bool isSuccess;
    char matchedUsers[32 * 10]; // Assuming max 10 users, each with 32 chars for username
    uint32_t userCount;
    char message[100];
} ResSearchUser;

// --- Get User List (Dashboard) ---
typedef struct {
    int dummy;
} ReqGetUserList;

// --- Challenge Packets ---
typedef struct {
    char targetUsername[32];
} ReqChallengeUser;

typedef struct {
    char challengerUsername[32];
    int32_t challengerElo;
} ReqChallengeRequest;

typedef struct {
    bool accept;
    char challengerUsername[32]; // Echo back to verify
} ResChallengeResponse;

typedef struct {
    char opponentUsername[32];
} ReqChallengeFinalConfirm;

typedef struct {
    bool accept;
} ResChallengeFinalConfirm;

typedef struct {
    char reason[100];
} ResChallengeDeclined;

// -------------------------

typedef struct {
    char username[32];
    bool isOnline;
    int elo;
} PlayerStatusInfo;

typedef struct {
    int count;
    PlayerStatusInfo players[20]; // Limit 20
} ResGetUserList;

typedef struct {
    uint32_t userId[32];
    char info[1000];
} ReqUpdateProfile;

typedef struct {
    bool isSuccess;
    char message[100];
} ResUpdateProfile;

typedef struct {
    char username[32];
} ReqGetProfile;

typedef struct {
    uint32_t matchId;
    char endedAt[20];   // "YYYY-MM-DD HH:MM:SS" (19 chars + null)
    char opponent[32];  // empty if unknown
    int32_t myScore;
    int32_t oppScore;
    uint8_t result;     // 0 = loss, 1 = win, 2 = draw
    char replayPath[256];
} ProfileGameEntry;

typedef struct {
    bool isSuccess;
    char username[32];
    char info[1000];
    char createdAt[20];
    int32_t elo;
    uint32_t gameCount;
    ProfileGameEntry games[20];
    char message[100];
} ResGetProfile;
// --------------------------------------------------------
// Game Room Packets --------------------------------------
typedef struct {
    uint32_t userId;
} ReqMatchFind;

typedef struct {
    bool isSuccess;
    char message[100];
} ResMatchFind;

typedef struct {
    uint32_t userId;
} ReqMatchCancel;

typedef struct {
    bool isSuccess;
    char message[100];
} ResMatchCancel;

typedef struct {
    uint32_t matchId;
} ReqMatchDecide1; // This is Request from server to client, not vice versa

typedef struct {
    uint32_t matchId;
    bool isSuccess;
} ResMatchDecide1; // This is Response from client to server, not vice versa

typedef struct {
    uint32_t matchId;
    uint32_t playerOrder[2]; // Max player, change later
} ResMatchDecide2; // Broadcast

typedef struct {
    uint32_t matchId;
    char host[64];       // e.g. "127.0.0.1"
    uint16_t port;       // ingame server TCP port
    char mapPath[128];   // e.g. "assets/maps/flatmap.txt"
} InitGame;
// --------------------------------------------------------

// Realtime Ingame Server Packets -------------------------
// Keep these fixed-size for the current Packet (memcpy) serializer.

#define INGAME_MAX_PLAYERS 2
#define INGAME_MAX_PROJECTILES 64

typedef enum {
    INGAME_CMD_MOVE_LEFT = 0,
    INGAME_CMD_MOVE_RIGHT = 1,
    INGAME_CMD_STOP = 2,
    INGAME_CMD_ADJUST_ANGLE = 3,
    INGAME_CMD_ADJUST_POWER = 4,
    INGAME_CMD_FIRE = 5,
    INGAME_CMD_POWER_UP = 6,
} InGameCommand;

#pragma pack(push, 1)
typedef struct {
    uint32_t matchId;
    uint32_t userId;
    char mapName[64]; // optional; empty = default map
} ReqIngameJoin;

typedef struct {
    bool isSuccess;
    uint32_t matchId;
    uint32_t playerId;
    char message[100];
} ResIngameJoin;

typedef struct {
    uint32_t matchId;
    uint32_t playerId;
    uint32_t seq;
    uint32_t command; // InGameCommand
    float value;
} ReqIngameInput;

typedef struct {
    uint32_t id;
    char name[32];
    int32_t hp;
    uint8_t isAlive;
    uint8_t isMyTurn;
    uint8_t orient;
    float x;
    float y;
    float angle;
    float power;
} NetPlayerState;

typedef struct {
    uint8_t isActive;
    uint8_t isPowerUp;
    float x;
    float y;
    float vx;
    float vy;
} NetProjectileState;

typedef struct {
    uint32_t matchId;
    uint32_t tick;
    uint32_t roomState;
    float turnTimer;
    float wind;
    uint8_t terrainModified;
    uint8_t hasExplosion;
    float explosionX;
    float explosionY;
    float explosionRadius;
    uint8_t playerCount;
    uint8_t projectileCount;
    NetPlayerState players[INGAME_MAX_PLAYERS];
    NetProjectileState projectiles[INGAME_MAX_PROJECTILES];
} ResIngameState;

// In-game pause packets -------------------------
// Pausing is coordinated by the authoritative ingame server.

typedef struct {
    uint32_t matchId;
} ReqIngamePauseRequest;

typedef struct {
    uint8_t isSuccess;
    uint8_t remainingUses; // how many pauses the requester has left (0..3)
    char message[100];
} ResIngamePauseResult;

// Broadcast to both players when a pause starts.
typedef struct {
    uint32_t matchId;
    uint32_t requesterPlayerId;
    uint32_t durationMs; // typically 30000
    uint8_t requesterRemainingUses;
} ResIngamePauseSignal;

typedef struct {
    uint32_t matchId;
} ReqIngamePauseEndEarly;

// Broadcast to both players when the pause ends (either naturally or early).
typedef struct {
    uint32_t matchId;
    uint32_t endedByPlayerId; // UINT32_MAX means ended by timeout
} ResIngamePauseEnd;

// In-game draw offer packets -------------------------

typedef struct {
    uint32_t matchId;
} ReqIngameDrawRequest;

// In-game rematch handshake -------------------------

typedef struct {
    uint32_t matchId;
} ReqIngameRematchRequest;

// status: 0 waiting, 1 start, 2 timeout/failed
typedef struct {
    uint8_t status;
    uint8_t acceptedMask; // bit i = player i has accepted
    uint16_t reserved;
    char message[64];
} ResIngameRematchStatus;

// Broadcast to both players when a draw offer is active.
typedef struct {
    uint32_t matchId;
    uint32_t requesterPlayerId;
    uint32_t durationMs; // typically 10000
} ResIngameDrawSignal;

typedef struct {
    uint32_t matchId;
    uint8_t accept; // 0 = decline, 1 = accept
} ReqIngameDrawDecision;

typedef enum {
    DRAW_RESULT_ACCEPTED = 0,
    DRAW_RESULT_DECLINED = 1,
    DRAW_RESULT_TIMEOUT = 2,
    DRAW_RESULT_DENIED = 3,
} IngameDrawResult;

typedef struct {
    uint32_t matchId;
    uint32_t requesterPlayerId;
    uint32_t responderPlayerId; // UINT32_MAX for timeout/denied
    uint32_t result; // IngameDrawResult
    char message[100];
} ResIngameDrawResult;

// In-game surrender packets -------------------------

typedef struct {
    uint32_t matchId;
} ReqIngameSurrender;

typedef struct {
    uint8_t isSuccess;
    char message[100];
} ResIngameSurrenderResult;

// Replay control packets -------------------------
// These are only meaningful when the ingame server is started in replay mode.

typedef enum {
    REPLAY_CMD_SET_PAUSED = 0,   // value: 0 = play, 1 = pause
    REPLAY_CMD_SET_SPEED = 1,    // value: playback speed (e.g. 0.5, 1.0, 2.0)
    REPLAY_CMD_SEEK_TICK = 2,    // tick: absolute tick to jump to
} ReplayControlCommand;

typedef struct {
    uint32_t command; // ReplayControlCommand
    uint32_t tick;    // used by SEEK
    float value;      // used by SET_PAUSED / SET_SPEED
} ReqReplayControl;

typedef struct {
    uint32_t currentTick;
    uint32_t firstTick;
    uint32_t lastTick;
    uint8_t isPaused;
    float speed;
} ResReplayStatus;

// Sent by replay-mode ingame server after a spectator joins.
// Allows the viewer to load the same map that was recorded.
typedef struct {
    char mapPath[256];
} ResReplayInfo;
#pragma pack(pop)
// --------------------------------------------------------
#endif // PACKET_STRUCTS_H