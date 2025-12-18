#ifndef PACKET_TYPE_HPP
#define PACKET_TYPE_HPP

enum PacketType {
    // User Authentication Packets
    REQ_AUTHENTICATE,
    RES_AUTHENTICATE,
    REQ_LOGOUT,
    REQ_CHANGE_PASSWORD,
    RES_CHANGE_PASSWORD,
    // Home Game Packets
    RES_GET_PROFILE,
    REQ_UPDATE_PROFILE,
    RES_UPDATE_PROFILE,
    REQ_SEARCH_USER,
    RES_SEARCH_USER,
    // Game Room Packets
    REQ_MATCH_FIND,
    RES_MATCH_FIND,
    REQ_MATCH_DECIDE_1,
    RES_MATCH_DECIDE_1,
    RES_MATCH_DECIDE_2,
    INIT_GAME,
    // In Game Packets
    REQ_PLAY,
    RES_PLAY,
    RES_EXECUTE_PLAY,
    GAME_RESULT,
};

#endif // PACKET_TYPE_HPP