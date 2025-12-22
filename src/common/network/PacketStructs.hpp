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
    char username[32];
    char info[1000];
    char createdAt[20];
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
    uint32_t matchId;
} ReqMatchDecide1; // This is Request from server to client, not vice versa

typedef struct {
    bool isSuccess;
} ResMatchDecide1; // This is Response from client to server, not vice versa

typedef struct {
    uint32_t matchId;
    uint32_t playerOrder[2]; // Max player, change later
} ResMatchDecide2; // Broadcast

typedef struct {
    uint32_t matchId;
    char gameData[2000]; // Placeholder for initial game data
} InitGame;
// --------------------------------------------------------
// In Game Packets ---------------------------------------
typedef struct {
    bool orient;
    float distance;
    float angle;
    float power;
} ReqPlay;

typedef struct {
    bool isSuccess;
    char message[100];
} ResPlay;

typedef struct {
    // PlayerPos playerpos;
    // PlauyerPos opponentpos;
    // GameState gamestate;
    bool isEnd;
} ResExecutePlay;

typedef struct {
    uint32_t winner_id[32];
    int winnerScore;
    int loserScore;
    int winnerEloGain;
    int loserEloLoss;
} GameResult;

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
#pragma pack(pop)
// --------------------------------------------------------
#endif // PACKET_STRUCTS_H