/*
 * room.h — Thread-safe game room management with reconnect,
 *           spectator, AI, and chat support.
 */
#ifndef ROOM_H
#define ROOM_H

#include <pthread.h>
#include "../shared/protocol.h"
#include "../src/game/board.h"

#define MAX_ROOMS      32
#define MAX_LOBBY      64
#define MAX_SPECTATORS 8

/* Lobby client — main thread only */
typedef struct {
    int  fd;
    char name[NAME_LEN];
    int  has_name;
    int  rating;          /* ELO rating for ranked matchmaking */
} LobbyClient;

/* Game room — runs in its own thread */
typedef struct {
    int        room_id;
    int        active;            /* protected by g_rooms_mutex        */
    pthread_t  thread;
    int        p1_fd, p2_fd;      /* protected by room_lock            */
    char     **board;
    PlayerInfo p1, p2;
    int        currentTurn;
    int        gameStatus;
    int        inMultiCapture;
    int        mcRow, mcCol;

    /* Reconnect support */
    char       p1_token[TOKEN_LEN];
    char       p2_token[TOKEN_LEN];
    int        p1_disconnected;   /* 1 if p1 currently disconnected    */
    int        p2_disconnected;
    int        notify_pipe[2];    /* main thread writes here to signal */
    pthread_mutex_t room_lock;    /* protects fd and disconnect fields */

    /* Spectator support */
    int        spectator_fds[MAX_SPECTATORS];
    int        spectator_count;   /* protected by room_lock            */

    /* AI support */
    int        ai_enabled;        /* 1 if P2 is AI                     */
    int        ai_difficulty;     /* AI_EASY / AI_MEDIUM / AI_HARD     */

    /* Game history */
    int        game_id;           /* SQLite game ID for history         */
    int        move_count;        /* total moves played                 */

    /* Turn tracking for room list */
    int        turn_number;       /* incremented each turn switch       */
} GameRoom;

extern pthread_mutex_t g_rooms_mutex;

/* Room lifecycle */
void rooms_init(GameRoom *rooms);
int  room_create_and_start(GameRoom *rooms,
                           int p1_fd, const char *p1_name,
                           int p2_fd, const char *p2_name);
int  room_create_ai_game(GameRoom *rooms, int p1_fd,
                         const char *p1_name, int difficulty);
void *room_thread_func(void *arg);

/* Reconnect: finds room by token, updates fd, signals thread */
int room_reconnect(GameRoom *rooms, const char *token, int new_fd);

/* Spectator: attach fd to a room, returns 1 on success */
int room_add_spectator(GameRoom *rooms, int room_id, int fd);

/* Room info: build list of active rooms for MSG_ROOM_LIST */
int room_get_list(GameRoom *rooms, RoomInfo *out, int max);

/* Lobby helpers */
void lobby_init(LobbyClient *lobby);
int  lobby_add(LobbyClient *lobby, int fd);
void lobby_remove(LobbyClient *lobby, int index);
int  lobby_find_match(LobbyClient *lobby, int *p1, int *p2);

#endif
