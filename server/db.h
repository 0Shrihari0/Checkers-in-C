/*
 * db.h — SQLite database layer for persistent player data and leaderboards.
 *
 * Provides ELO-rated player profiles with win/loss tracking.
 * All functions are thread-safe (SQLite serialized mode).
 */
#ifndef DB_H
#define DB_H

#define DB_DEFAULT_PATH  "checkers.db"
#define ELO_START        1200
#define ELO_K            32

#include "../shared/protocol.h"

/* Player stats returned by queries */
typedef struct {
    int  id;
    char name[LB_NAME_LEN];
    int  wins;
    int  losses;
    int  draws;
    int  rating;
} PlayerStats;

/* Leaderboard entry (compact, for protocol serialization) */
typedef struct {
    char name[LB_NAME_LEN];
    int  wins;
    int  losses;
    int  rating;
} LeaderboardEntry;

/* Initialize database (create tables if needed). Returns 0 on success. */
int db_init(const char *path);

/* Close database. */
void db_close(void);

/* Get or create a player by username. Returns 0 on success, fills stats. */
int db_get_or_create(const char *username, PlayerStats *stats);

/* Record a game result and update ELO ratings.
   winner/loser are usernames. Returns 0 on success. */
int db_record_win(const char *winner, const char *loser);

/* Record a draw and update ELO. Returns 0 on success. */
int db_record_draw(const char *p1, const char *p2);

/* Get top N players by rating. Returns count of entries filled. */
int db_get_leaderboard(LeaderboardEntry *entries, int max_entries);

/* Get a single player's stats. Returns 0 on success, -1 if not found. */
int db_get_stats(const char *username, PlayerStats *stats);

/* Get a player's rank (1-based position by rating). Returns rank, or -1 on error. */
int db_get_rank(const char *username);

#endif
