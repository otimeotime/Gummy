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
    REQ_GET_PROFILE,
    RES_GET_PROFILE,
    REQ_UPDATE_PROFILE,
    RES_UPDATE_PROFILE,
    REQ_SEARCH_USER,
    RES_SEARCH_USER,
    REQ_GET_USER_LIST,
    RES_GET_USER_LIST,
    // Game Room Packets
    REQ_MATCH_FIND,
    RES_MATCH_FIND,
    REQ_MATCH_CANCEL,
    RES_MATCH_CANCEL,
    REQ_MATCH_DECIDE_1,
    RES_MATCH_DECIDE_1,
    RES_MATCH_DECIDE_2,
    INIT_GAME,
    // In Game Packets
    REQ_PLAY,
    RES_PLAY,
    RES_EXECUTE_PLAY,
    GAME_RESULT,

    // In-game realtime (authoritative ingame server)
    REQ_INGAME_JOIN,
    RES_INGAME_JOIN,
    REQ_INGAME_INPUT,
    RES_INGAME_STATE,

    // In-game pause (authoritative ingame server)
    REQ_INGAME_PAUSE_REQUEST,
    RES_INGAME_PAUSE_RESULT,
    RES_INGAME_PAUSE_SIGNAL,
    REQ_INGAME_PAUSE_END_EARLY,
    RES_INGAME_PAUSE_END,

    // In-game draw offer (authoritative ingame server)
    REQ_INGAME_DRAW_REQUEST,
    RES_INGAME_DRAW_SIGNAL,
    REQ_INGAME_DRAW_DECISION,
    RES_INGAME_DRAW_RESULT,

    // In-game surrender (authoritative ingame server)
    REQ_INGAME_SURRENDER,
    RES_INGAME_SURRENDER_RESULT,

    // In-game rematch handshake (authoritative ingame server)
    REQ_INGAME_REMATCH_REQUEST,
    RES_INGAME_REMATCH_STATUS,

    // Replay control (for replay-mode ingame server)
    REQ_REPLAY_CONTROL,
    RES_REPLAY_STATUS,
    RES_REPLAY_INFO,
};

#endif // PACKET_TYPE_HPP