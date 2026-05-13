/*
 * history.h — Game history and replay system.
 *
 * Stores every game and its moves in SQLite for replay and statistics.
 * Uses the same database connection as db.c (shared g_db).
 */
#ifndef HISTORY_H
#define HISTORY_H

#include "../shared/protocol.h"

/* Initialize history tables (called from db_init). Returns 0 on success. */
int history_init_tables(void);

/* Create a new game record. Returns game_id (>0) on success, -1 on error. */
int history_start_game(const char *p1, const char *p2);

/* Record a move within a game. Returns 0 on success. */
int history_record_move(int game_id, int move_num, int player,
                        int fromR, int fromC, int toR, int toC);

/* Finalize a game with its result status. Returns 0 on success. */
int history_end_game(int game_id, int status);

/* Get a player's recent games. Returns count filled. */
int history_get_recent(const char *username, GameSummary *out, int max);

/* Move record for replay */
typedef struct {
    int move_num;
    int player;
    int fromR, fromC, toR, toC;
} HistoryMove;

/* Get all moves for a game. Returns count filled. */
int history_get_moves(int game_id, HistoryMove *moves, int max_moves);

#endif
