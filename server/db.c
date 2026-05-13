/*
 * db.c — SQLite database implementation with ELO rating system.
 *
 * ELO formula:
 *   Expected = 1 / (1 + 10^((opponent_rating - player_rating) / 400))
 *   New rating = old + K * (actual - expected)
 *   actual: 1.0 for win, 0.0 for loss, 0.5 for draw
 */

#include "db.h"
#include "history.h"
#include "log.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

sqlite3 *g_db = NULL;

/* ---- Schema ---- */
static const char *SCHEMA =
    "CREATE TABLE IF NOT EXISTS players ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  username TEXT UNIQUE NOT NULL,"
    "  wins INTEGER DEFAULT 0,"
    "  losses INTEGER DEFAULT 0,"
    "  draws INTEGER DEFAULT 0,"
    "  rating INTEGER DEFAULT 1200,"
    "  created_at TEXT DEFAULT CURRENT_TIMESTAMP"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_players_rating ON players(rating DESC);";

/* ---- Init / Close ---- */

int db_init(const char *path) {
    if (!path) path = DB_DEFAULT_PATH;
    int rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("[DB] Failed to open %s: %s", path, sqlite3_errmsg(g_db));
        return -1;
    }
    /* Enable WAL mode for better concurrent read performance */
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);

    char *err = NULL;
    rc = sqlite3_exec(g_db, SCHEMA, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        LOG_ERROR("[DB] Schema error: %s", err);
        sqlite3_free(err);
        return -1;
    }
    LOG_INFO("[DB] Initialized: %s", path);

    /* Initialize history tables (games, moves) */
    if (history_init_tables() < 0) {
        LOG_WARN("[DB] History tables init failed (non-fatal)");
    }

    return 0;
}

void db_close(void) {
    if (g_db) { sqlite3_close(g_db); g_db = NULL; }
    LOG_INFO("[DB] Closed.");
}

/* ---- Player CRUD ---- */

int db_get_or_create(const char *username, PlayerStats *stats) {
    if (!g_db || !username || !stats) return -1;

    /* Try to find existing player */
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT id, username, wins, losses, draws, rating FROM players WHERE username = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) { LOG_ERROR("[DB] Prepare: %s", sqlite3_errmsg(g_db)); return -1; }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        stats->id      = sqlite3_column_int(stmt, 0);
        strncpy(stats->name, (const char *)sqlite3_column_text(stmt, 1), LB_NAME_LEN - 1);
        stats->wins    = sqlite3_column_int(stmt, 2);
        stats->losses  = sqlite3_column_int(stmt, 3);
        stats->draws   = sqlite3_column_int(stmt, 4);
        stats->rating  = sqlite3_column_int(stmt, 5);
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);

    /* Create new player */
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO players (username, rating) VALUES (?, ?);", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, ELO_START);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) { LOG_ERROR("[DB] Insert: %s", sqlite3_errmsg(g_db)); return -1; }

    stats->id     = (int)sqlite3_last_insert_rowid(g_db);
    strncpy(stats->name, username, LB_NAME_LEN - 1);
    stats->wins   = 0;
    stats->losses = 0;
    stats->draws  = 0;
    stats->rating = ELO_START;

    LOG_INFO("[DB] New player: %s (rating %d)", username, ELO_START);
    return 0;
}

/* ---- ELO calculation ---- */

static double elo_expected(int rating_a, int rating_b) {
    return 1.0 / (1.0 + pow(10.0, (rating_b - rating_a) / 400.0));
}

static int elo_new(int rating, double expected, double actual) {
    return rating + (int)(ELO_K * (actual - expected) + 0.5);
}

/* ---- Record results ---- */

int db_record_win(const char *winner, const char *loser) {
    if (!g_db) return -1;

    PlayerStats ws, ls;
    if (db_get_or_create(winner, &ws) < 0) return -1;
    if (db_get_or_create(loser, &ls) < 0) return -1;

    double ew = elo_expected(ws.rating, ls.rating);
    double el = elo_expected(ls.rating, ws.rating);
    int new_wr = elo_new(ws.rating, ew, 1.0);
    int new_lr = elo_new(ls.rating, el, 0.0);

    /* Minimum rating floor */
    if (new_lr < 100) new_lr = 100;

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "UPDATE players SET wins = wins + 1, rating = ? WHERE id = ?;", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, new_wr);
        sqlite3_bind_int(stmt, 2, ws.id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    rc = sqlite3_prepare_v2(g_db,
        "UPDATE players SET losses = losses + 1, rating = ? WHERE id = ?;", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, new_lr);
        sqlite3_bind_int(stmt, 2, ls.id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    LOG_INFO("[DB] %s beat %s: %d(+%d) vs %d(%d)",
             winner, loser, new_wr, new_wr - ws.rating, new_lr, new_lr - ls.rating);
    return 0;
}

int db_record_draw(const char *p1, const char *p2) {
    if (!g_db) return -1;

    PlayerStats s1, s2;
    if (db_get_or_create(p1, &s1) < 0) return -1;
    if (db_get_or_create(p2, &s2) < 0) return -1;

    double e1 = elo_expected(s1.rating, s2.rating);
    double e2 = elo_expected(s2.rating, s1.rating);
    int nr1 = elo_new(s1.rating, e1, 0.5);
    int nr2 = elo_new(s2.rating, e2, 0.5);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "UPDATE players SET draws = draws + 1, rating = ? WHERE id = ?;", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, nr1); sqlite3_bind_int(stmt, 2, s1.id);
        sqlite3_step(stmt); sqlite3_reset(stmt);
        sqlite3_bind_int(stmt, 1, nr2); sqlite3_bind_int(stmt, 2, s2.id);
        sqlite3_step(stmt); sqlite3_finalize(stmt);
    }
    LOG_INFO("[DB] Draw: %s(%d) vs %s(%d)", p1, nr1, p2, nr2);
    return 0;
}

/* ---- Leaderboard ---- */

int db_get_leaderboard(LeaderboardEntry *entries, int max_entries) {
    if (!g_db || !entries) return 0;

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT username, wins, losses, rating FROM players "
        "ORDER BY rating DESC LIMIT ?;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, max_entries);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_entries) {
        memset(&entries[count], 0, sizeof(LeaderboardEntry));
        strncpy(entries[count].name,
                (const char *)sqlite3_column_text(stmt, 0), LB_NAME_LEN - 1);
        entries[count].wins   = sqlite3_column_int(stmt, 1);
        entries[count].losses = sqlite3_column_int(stmt, 2);
        entries[count].rating = sqlite3_column_int(stmt, 3);
        count++;
    }
    sqlite3_finalize(stmt);
    return count;
}

int db_get_stats(const char *username, PlayerStats *stats) {
    return db_get_or_create(username, stats);
}

int db_get_rank(const char *username) {
    if (!g_db || !username) return -1;

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT COUNT(*) + 1 FROM players WHERE rating > "
        "(SELECT rating FROM players WHERE username = ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) { LOG_ERROR("[DB] Rank prepare: %s", sqlite3_errmsg(g_db)); return -1; }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    int rank = -1;
    if (rc == SQLITE_ROW) {
        rank = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return rank;
}
