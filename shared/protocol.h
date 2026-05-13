/*
 * protocol.h — Structured packet protocol for Checkers multiplayer.
 *
 * All communication uses a generic Packet with a type tag and a
 * fixed-size payload buffer. Serializer/deserializer functions
 * pack typed data into/from the payload, keeping the protocol
 * extensible for future message types.
 *
 * Message flow:
 *   Client → Server:  MSG_JOIN, MSG_MOVE, MSG_CHAT, MSG_STATS_REQ,
 *                      MSG_LB_REQ, MSG_ROOM_REQ, MSG_SPECTATE,
 *                      MSG_HISTORY_REQ, MSG_AI_REQ
 *   Server → Client:  MSG_WELCOME, MSG_WAIT, MSG_BOARD, MSG_WIN,
 *                      MSG_ERROR, MSG_LEADERBOARD, MSG_STATS_RESP,
 *                      MSG_ROOM_LIST, MSG_HISTORY_RESP, MSG_CHAT
 *   Either  → Either: MSG_DISCONNECT, MSG_RECONNECT
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define DEFAULT_PORT   8080
#define BOARD_SZ       8
#define NAME_LEN       50
#define MSG_LEN        128
#define MAX_PAYLOAD    512
#define TOKEN_LEN      17     /* 16 hex chars + null              */
#define LB_NAME_LEN    24
#define LB_MAX_ENTRIES 5
#define MOVE_TIMEOUT   120    /* seconds before move timeout loss  */
#define RECONNECT_TIMEOUT 30  /* seconds to reconnect after DC     */
#define MAX_ROOMS_LIST 8      /* max rooms in a room list packet   */
#define MAX_HISTORY    10     /* max games in a history packet     */
#define CHAT_MSG_LEN   100   /* max chat message length           */

/* ------------------------------------------------------------------ */
/*  Message types                                                      */
/* ------------------------------------------------------------------ */

#define MSG_JOIN         1   /* client → server : player name            */
#define MSG_WELCOME      2   /* server → client : player/game assignment */
#define MSG_WAIT         3   /* server → client : in matchmaking queue   */
#define MSG_BOARD        4   /* server → client : board state update     */
#define MSG_MOVE         5   /* client → server : move coordinates       */
#define MSG_TURN         6   /* server → client : turn notification      */
#define MSG_WIN          7   /* server → client : game over              */
#define MSG_ERROR        8   /* server → client : error/rejection        */
#define MSG_DISCONNECT   9   /* either direction : graceful disconnect   */
#define MSG_RECONNECT   10   /* client → server : reconnect with token   */
#define MSG_LEADERBOARD 11   /* server → client : top players            */
#define MSG_STATS_REQ   12   /* client → server : request own stats      */
#define MSG_STATS_RESP  13   /* server → client : player stats response  */
#define MSG_LB_REQ      14   /* client → server : request leaderboard    */
#define MSG_CHAT        15   /* either → either : in-game chat message   */
#define MSG_SPECTATE    16   /* client → server : request to spectate    */
#define MSG_ROOM_LIST   17   /* server → client : list of active rooms   */
#define MSG_ROOM_REQ    18   /* client → server : request room list      */
#define MSG_HISTORY_REQ  19  /* client → server : request game history   */
#define MSG_HISTORY_RESP 20  /* server → client : game history list      */
#define MSG_AI_REQ       21  /* client → server : request AI game        */
#define MSG_LAST         21  /* keep in sync with last message type      */

/* ------------------------------------------------------------------ */
/*  Game status codes                                                  */
/* ------------------------------------------------------------------ */

#define STATUS_PLAYING   0
#define STATUS_P1_WINS   1
#define STATUS_P2_WINS   2
#define STATUS_DRAW      3

/* AI difficulty levels */
#define AI_EASY   1
#define AI_MEDIUM 2
#define AI_HARD   3

/* ------------------------------------------------------------------ */
/*  Generic Packet — the unit of communication over TCP                */
/*                                                                     */
/*  Every message is exactly sizeof(Packet) bytes on the wire.         */
/*  'type' identifies how to interpret 'payload'.                      */
/*  'length' is the meaningful bytes in payload (for validation).      */
/* ------------------------------------------------------------------ */

typedef struct {
    int  type;
    int  length;
    char payload[MAX_PAYLOAD];
} Packet;

/* ------------------------------------------------------------------ */
/*  Payload structures — one per message type                          */
/*  These are serialized into / deserialized from Packet.payload.      */
/* ------------------------------------------------------------------ */

/* MSG_JOIN: client sends their name */
typedef struct {
    char name[NAME_LEN];          /* 50 bytes */
} JoinData;

/* MSG_WELCOME: server assigns player number + game info (sent once) */
typedef struct {
    int  playerNum;               /* 1 or 2 */
    char p1Name[NAME_LEN];
    char p2Name[NAME_LEN];
    char p1Symbol, p2Symbol;
    char p1King, p2King;
    char token[TOKEN_LEN];        /* reconnect token */
} WelcomeData;                    /* ~125 bytes */

/* MSG_BOARD: full board state (sent after every valid move) */
typedef struct {
    char board[BOARD_SZ][BOARD_SZ]; /* 64 bytes */
    int  p1Score, p2Score;
    int  currentTurn;
    int  mustCapture;
    int  multiCapture;
    int  captureRow, captureCol;
    char message[MSG_LEN];
} BoardData;                      /* ~220 bytes */

/* MSG_MOVE: client sends move coordinates */
typedef struct {
    int fromRow, fromCol;
    int toRow, toCol;
} MoveData;                       /* 16 bytes */

/* MSG_TURN: lightweight turn notification */
typedef struct {
    int  currentTurn;
    char message[MSG_LEN];
} TurnData;                       /* ~132 bytes */

/* MSG_WIN: game over announcement */
typedef struct {
    int  status;                  /* STATUS_P1_WINS / P2_WINS / DRAW */
    int  p1Score, p2Score;
    char message[MSG_LEN];
} WinData;                        /* ~140 bytes */

/* MSG_ERROR: error/rejection message */
typedef struct {
    char message[MSG_LEN];
} ErrorData;                      /* 128 bytes */

/* MSG_DISCONNECT: graceful disconnect */
typedef struct {
    char reason[MSG_LEN];
} DisconnectData;                 /* 128 bytes */

/* MSG_RECONNECT: client sends token to rejoin a paused game */
typedef struct {
    char token[TOKEN_LEN];
} ReconnectData;                  /* 17 bytes */

/* MSG_LEADERBOARD: top players by rating */
typedef struct {
    char name[LB_NAME_LEN];
    int  wins;
    int  losses;
    int  rating;
} LBEntry;                        /* 36 bytes */

typedef struct {
    int     count;
    LBEntry entries[LB_MAX_ENTRIES];
} LeaderboardData;                /* 184 bytes */

/* MSG_STATS_RESP: individual player stats */
typedef struct {
    char name[LB_NAME_LEN];
    int  wins;
    int  losses;
    int  draws;
    int  rating;
    int  rank;                    /* 1-based leaderboard position */
} StatsData;                      /* ~44 bytes */

/* MSG_CHAT: in-game chat message */
typedef struct {
    char sender[NAME_LEN];
    char message[CHAT_MSG_LEN];
} ChatData;                       /* ~150 bytes */

/* MSG_SPECTATE: request to spectate a specific room */
typedef struct {
    int room_id;
} SpectateData;                   /* 4 bytes */

/* MSG_ROOM_LIST: list of active game rooms */
typedef struct {
    int  room_id;
    char p1_name[LB_NAME_LEN];
    char p2_name[LB_NAME_LEN];
    int  turn_number;
} RoomInfo;                       /* ~56 bytes */

typedef struct {
    int      count;
    RoomInfo rooms[MAX_ROOMS_LIST];
} RoomListData;                   /* ~452 bytes */

/* MSG_HISTORY_RESP: recent games for a player */
typedef struct {
    int  game_id;
    char opponent[LB_NAME_LEN];
    int  result;                  /* 1=win, 0=loss, 2=draw */
    char date[20];
} GameSummary;

typedef struct {
    int         count;
    GameSummary games[MAX_HISTORY];
} HistoryData;                    /* fits in MAX_PAYLOAD */

/* MSG_AI_REQ: request a game against the AI */
typedef struct {
    int difficulty;               /* AI_EASY / AI_MEDIUM / AI_HARD */
} AIRequestData;                  /* 4 bytes */

/* ------------------------------------------------------------------ */
/*  Serializers — pack typed data into a Packet                        */
/* ------------------------------------------------------------------ */

void proto_pack_join(Packet *pkt, const char *name);
void proto_pack_welcome(Packet *pkt, int playerNum,
                        const char *p1Name, const char *p2Name,
                        char p1Sym, char p2Sym, char p1King, char p2King,
                        const char *token);
void proto_pack_wait(Packet *pkt);
void proto_pack_board(Packet *pkt, const char board[BOARD_SZ][BOARD_SZ],
                      int p1Score, int p2Score, int turn,
                      int mustCap, int multiCap, int capRow, int capCol,
                      const char *message);
void proto_pack_move(Packet *pkt, int fromR, int fromC, int toR, int toC);
void proto_pack_turn(Packet *pkt, int currentTurn, const char *message);
void proto_pack_win(Packet *pkt, int status, int p1Score, int p2Score,
                    const char *message);
void proto_pack_error(Packet *pkt, const char *message);
void proto_pack_disconnect(Packet *pkt, const char *reason);
void proto_pack_reconnect(Packet *pkt, const char *token);
void proto_pack_leaderboard(Packet *pkt, const LBEntry *entries, int count);
void proto_pack_stats_req(Packet *pkt);
void proto_pack_stats_resp(Packet *pkt, const StatsData *stats);
void proto_pack_lb_req(Packet *pkt);
void proto_pack_chat(Packet *pkt, const char *sender, const char *message);
void proto_pack_spectate(Packet *pkt, int room_id);
void proto_pack_room_list(Packet *pkt, const RoomInfo *rooms, int count);
void proto_pack_room_req(Packet *pkt);
void proto_pack_history_req(Packet *pkt);
void proto_pack_history_resp(Packet *pkt, const GameSummary *games, int count);
void proto_pack_ai_req(Packet *pkt, int difficulty);

/* ------------------------------------------------------------------ */
/*  Deserializers — unpack typed data from a Packet                    */
/*  Return 0 on success, -1 if packet type doesn't match.             */
/* ------------------------------------------------------------------ */

int proto_unpack_join(const Packet *pkt, JoinData *out);
int proto_unpack_welcome(const Packet *pkt, WelcomeData *out);
int proto_unpack_board(const Packet *pkt, BoardData *out);
int proto_unpack_move(const Packet *pkt, MoveData *out);
int proto_unpack_turn(const Packet *pkt, TurnData *out);
int proto_unpack_win(const Packet *pkt, WinData *out);
int proto_unpack_error(const Packet *pkt, ErrorData *out);
int proto_unpack_disconnect(const Packet *pkt, DisconnectData *out);
int proto_unpack_reconnect(const Packet *pkt, ReconnectData *out);
int proto_unpack_leaderboard(const Packet *pkt, LeaderboardData *out);
int proto_unpack_stats_resp(const Packet *pkt, StatsData *out);
int proto_unpack_chat(const Packet *pkt, ChatData *out);
int proto_unpack_spectate(const Packet *pkt, SpectateData *out);
int proto_unpack_room_list(const Packet *pkt, RoomListData *out);
int proto_unpack_history_resp(const Packet *pkt, HistoryData *out);
int proto_unpack_ai_req(const Packet *pkt, AIRequestData *out);

/* ------------------------------------------------------------------ */
/*  Protocol utilities                                                 */
/* ------------------------------------------------------------------ */

/* Get human-readable name for a message type (for logging). */
const char *proto_type_name(int type);

/* Validate a packet's type and length. Returns 1 if valid, 0 if not. */
int proto_validate(const Packet *pkt);

/* Generate a random 16-char hex token. */
void proto_generate_token(char *token);

/* ------------------------------------------------------------------ */
/*  Transport helpers — reliable TCP send/receive                      */
/* ------------------------------------------------------------------ */

int send_all(int fd, const void *buf, int len);
int recv_all(int fd, void *buf, int len);
int send_packet(int fd, const Packet *pkt);
int recv_packet(int fd, Packet *pkt);

#endif
