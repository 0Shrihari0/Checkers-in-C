/*
 * protocol.c — Packet serialization, deserialization, and transport.
 */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

/* ------------------------------------------------------------------ */
/*  Serializers                                                        */
/* ------------------------------------------------------------------ */

static void pkt_init(Packet *p, int type, int len) {
    memset(p, 0, sizeof(Packet));
    p->type   = type;
    p->length = len;
}

void proto_pack_join(Packet *pkt, const char *name) {
    JoinData d; memset(&d, 0, sizeof(d));
    strncpy(d.name, name, NAME_LEN - 1);
    pkt_init(pkt, MSG_JOIN, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_welcome(Packet *pkt, int playerNum,
                        const char *p1Name, const char *p2Name,
                        char p1Sym, char p2Sym, char p1King, char p2King,
                        const char *token) {
    WelcomeData d; memset(&d, 0, sizeof(d));
    d.playerNum = playerNum;
    strncpy(d.p1Name, p1Name, NAME_LEN - 1);
    strncpy(d.p2Name, p2Name, NAME_LEN - 1);
    d.p1Symbol = p1Sym; d.p2Symbol = p2Sym;
    d.p1King = p1King;  d.p2King = p2King;
    if (token) strncpy(d.token, token, TOKEN_LEN - 1);
    pkt_init(pkt, MSG_WELCOME, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_wait(Packet *pkt) {
    pkt_init(pkt, MSG_WAIT, 0);
}

void proto_pack_board(Packet *pkt, const char board[BOARD_SZ][BOARD_SZ],
                      int p1Score, int p2Score, int turn,
                      int mustCap, int multiCap, int capRow, int capCol,
                      const char *message) {
    BoardData d; memset(&d, 0, sizeof(d));
    memcpy(d.board, board, BOARD_SZ * BOARD_SZ);
    d.p1Score = p1Score; d.p2Score = p2Score;
    d.currentTurn = turn; d.mustCapture = mustCap;
    d.multiCapture = multiCap;
    d.captureRow = capRow; d.captureCol = capCol;
    if (message) strncpy(d.message, message, MSG_LEN - 1);
    pkt_init(pkt, MSG_BOARD, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_move(Packet *pkt, int fromR, int fromC, int toR, int toC) {
    MoveData d; memset(&d, 0, sizeof(d));
    d.fromRow = fromR; d.fromCol = fromC;
    d.toRow = toR;     d.toCol = toC;
    pkt_init(pkt, MSG_MOVE, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_turn(Packet *pkt, int currentTurn, const char *message) {
    TurnData d; memset(&d, 0, sizeof(d));
    d.currentTurn = currentTurn;
    if (message) strncpy(d.message, message, MSG_LEN - 1);
    pkt_init(pkt, MSG_TURN, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_win(Packet *pkt, int status, int p1Score, int p2Score,
                    const char *message) {
    WinData d; memset(&d, 0, sizeof(d));
    d.status = status; d.p1Score = p1Score; d.p2Score = p2Score;
    if (message) strncpy(d.message, message, MSG_LEN - 1);
    pkt_init(pkt, MSG_WIN, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_error(Packet *pkt, const char *message) {
    ErrorData d; memset(&d, 0, sizeof(d));
    if (message) strncpy(d.message, message, MSG_LEN - 1);
    pkt_init(pkt, MSG_ERROR, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_disconnect(Packet *pkt, const char *reason) {
    DisconnectData d; memset(&d, 0, sizeof(d));
    if (reason) strncpy(d.reason, reason, MSG_LEN - 1);
    pkt_init(pkt, MSG_DISCONNECT, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_reconnect(Packet *pkt, const char *token) {
    ReconnectData d; memset(&d, 0, sizeof(d));
    if (token) strncpy(d.token, token, TOKEN_LEN - 1);
    pkt_init(pkt, MSG_RECONNECT, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_leaderboard(Packet *pkt, const LBEntry *entries, int count) {
    LeaderboardData d; memset(&d, 0, sizeof(d));
    if (count > LB_MAX_ENTRIES) count = LB_MAX_ENTRIES;
    d.count = count;
    for (int i = 0; i < count; i++) d.entries[i] = entries[i];
    pkt_init(pkt, MSG_LEADERBOARD, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_stats_req(Packet *pkt) {
    pkt_init(pkt, MSG_STATS_REQ, 0);
}

void proto_pack_stats_resp(Packet *pkt, const StatsData *stats) {
    pkt_init(pkt, MSG_STATS_RESP, sizeof(StatsData));
    memcpy(pkt->payload, stats, sizeof(StatsData));
}

void proto_pack_lb_req(Packet *pkt) {
    pkt_init(pkt, MSG_LB_REQ, 0);
}

void proto_pack_chat(Packet *pkt, const char *sender, const char *message) {
    ChatData d; memset(&d, 0, sizeof(d));
    if (sender) strncpy(d.sender, sender, NAME_LEN - 1);
    if (message) strncpy(d.message, message, CHAT_MSG_LEN - 1);
    pkt_init(pkt, MSG_CHAT, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_spectate(Packet *pkt, int room_id) {
    SpectateData d; memset(&d, 0, sizeof(d));
    d.room_id = room_id;
    pkt_init(pkt, MSG_SPECTATE, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_room_list(Packet *pkt, const RoomInfo *rooms, int count) {
    RoomListData d; memset(&d, 0, sizeof(d));
    if (count > MAX_ROOMS_LIST) count = MAX_ROOMS_LIST;
    d.count = count;
    for (int i = 0; i < count; i++) d.rooms[i] = rooms[i];
    pkt_init(pkt, MSG_ROOM_LIST, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_room_req(Packet *pkt) {
    pkt_init(pkt, MSG_ROOM_REQ, 0);
}

void proto_pack_history_req(Packet *pkt) {
    pkt_init(pkt, MSG_HISTORY_REQ, 0);
}

void proto_pack_history_resp(Packet *pkt, const GameSummary *games, int count) {
    HistoryData d; memset(&d, 0, sizeof(d));
    if (count > MAX_HISTORY) count = MAX_HISTORY;
    d.count = count;
    for (int i = 0; i < count; i++) d.games[i] = games[i];
    pkt_init(pkt, MSG_HISTORY_RESP, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

void proto_pack_ai_req(Packet *pkt, int difficulty) {
    AIRequestData d; memset(&d, 0, sizeof(d));
    d.difficulty = difficulty;
    pkt_init(pkt, MSG_AI_REQ, sizeof(d));
    memcpy(pkt->payload, &d, sizeof(d));
}

/* ------------------------------------------------------------------ */
/*  Deserializers                                                      */
/* ------------------------------------------------------------------ */

#define UNPACK(pkt, expected_type, out) \
    do { if ((pkt)->type != (expected_type)) return -1; \
         memcpy((out), (pkt)->payload, sizeof(*(out))); \
         return 0; } while(0)

int proto_unpack_join(const Packet *p, JoinData *o)       { UNPACK(p, MSG_JOIN, o); }
int proto_unpack_welcome(const Packet *p, WelcomeData *o) { UNPACK(p, MSG_WELCOME, o); }
int proto_unpack_board(const Packet *p, BoardData *o)     { UNPACK(p, MSG_BOARD, o); }
int proto_unpack_move(const Packet *p, MoveData *o)       { UNPACK(p, MSG_MOVE, o); }
int proto_unpack_turn(const Packet *p, TurnData *o)       { UNPACK(p, MSG_TURN, o); }
int proto_unpack_win(const Packet *p, WinData *o)         { UNPACK(p, MSG_WIN, o); }
int proto_unpack_error(const Packet *p, ErrorData *o)     { UNPACK(p, MSG_ERROR, o); }
int proto_unpack_disconnect(const Packet *p, DisconnectData *o) { UNPACK(p, MSG_DISCONNECT, o); }
int proto_unpack_reconnect(const Packet *p, ReconnectData *o)   { UNPACK(p, MSG_RECONNECT, o); }
int proto_unpack_leaderboard(const Packet *p, LeaderboardData *o) { UNPACK(p, MSG_LEADERBOARD, o); }
int proto_unpack_stats_resp(const Packet *p, StatsData *o)        { UNPACK(p, MSG_STATS_RESP, o); }
int proto_unpack_chat(const Packet *p, ChatData *o)               { UNPACK(p, MSG_CHAT, o); }
int proto_unpack_spectate(const Packet *p, SpectateData *o)       { UNPACK(p, MSG_SPECTATE, o); }
int proto_unpack_room_list(const Packet *p, RoomListData *o)      { UNPACK(p, MSG_ROOM_LIST, o); }
int proto_unpack_history_resp(const Packet *p, HistoryData *o)    { UNPACK(p, MSG_HISTORY_RESP, o); }
int proto_unpack_ai_req(const Packet *p, AIRequestData *o)        { UNPACK(p, MSG_AI_REQ, o); }

/* ------------------------------------------------------------------ */
/*  Protocol utilities                                                 */
/* ------------------------------------------------------------------ */

const char *proto_type_name(int type) {
    switch (type) {
        case MSG_JOIN:         return "JOIN";
        case MSG_WELCOME:      return "WELCOME";
        case MSG_WAIT:         return "WAIT";
        case MSG_BOARD:        return "BOARD";
        case MSG_MOVE:         return "MOVE";
        case MSG_TURN:         return "TURN";
        case MSG_WIN:          return "WIN";
        case MSG_ERROR:        return "ERROR";
        case MSG_DISCONNECT:   return "DISCONNECT";
        case MSG_RECONNECT:    return "RECONNECT";
        case MSG_LEADERBOARD:  return "LEADERBOARD";
        case MSG_STATS_REQ:    return "STATS_REQ";
        case MSG_STATS_RESP:   return "STATS_RESP";
        case MSG_LB_REQ:       return "LB_REQ";
        case MSG_CHAT:         return "CHAT";
        case MSG_SPECTATE:     return "SPECTATE";
        case MSG_ROOM_LIST:    return "ROOM_LIST";
        case MSG_ROOM_REQ:     return "ROOM_REQ";
        case MSG_HISTORY_REQ:  return "HISTORY_REQ";
        case MSG_HISTORY_RESP: return "HISTORY_RESP";
        case MSG_AI_REQ:       return "AI_REQ";
        default:               return "UNKNOWN";
    }
}

int proto_validate(const Packet *pkt) {
    if (pkt->type < MSG_JOIN || pkt->type > MSG_LAST) return 0;
    if (pkt->length < 0 || pkt->length > MAX_PAYLOAD) return 0;
    return 1;
}

static int token_seeded = 0;
void proto_generate_token(char *token) {
    if (!token_seeded) { srand((unsigned)time(NULL) ^ (unsigned)getpid()); token_seeded = 1; }
    for (int i = 0; i < 16; i++)
        snprintf(token + i, 2, "%x", rand() % 16);
    token[16] = '\0';
}

/* ------------------------------------------------------------------ */
/*  Transport — reliable TCP send/receive                              */
/* ------------------------------------------------------------------ */

int send_all(int fd, const void *buf, int len) {
    const char *ptr = (const char *)buf;
    int remaining = len;
    while (remaining > 0) {
        ssize_t sent = send(fd, ptr, (size_t)remaining, 0);
        if (sent <= 0) { perror("send_all"); return -1; }
        ptr += sent; remaining -= (int)sent;
    }
    return 0;
}

int recv_all(int fd, void *buf, int len) {
    char *ptr = (char *)buf;
    int remaining = len;
    while (remaining > 0) {
        ssize_t received = recv(fd, ptr, (size_t)remaining, 0);
        if (received < 0) { perror("recv_all"); return -1; }
        if (received == 0) return -1; /* peer closed */
        ptr += received; remaining -= (int)received;
    }
    return 0;
}

int send_packet(int fd, const Packet *pkt) {
    return send_all(fd, pkt, sizeof(Packet));
}

int recv_packet(int fd, Packet *pkt) {
    int rc = recv_all(fd, pkt, sizeof(Packet));
    if (rc < 0) return -1;
    if (!proto_validate(pkt)) return -1;
    return 0;
}
