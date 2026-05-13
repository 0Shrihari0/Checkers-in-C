/*
 * history.c — Game history implementation using SQLite.
 *
 * Shares the database connection from db.c via extern.
 */

#include "history.h"
#include "log.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

/* Shared database handle from db.c */
extern sqlite3 *g_db;

static const char *HISTORY_SCHEMA =
    "CREATE TABLE IF NOT EXISTS games ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  p1 TEXT NOT NULL,"
    "  p2 TEXT NOT NULL,"
    "  result INTEGER DEFAULT 0,"
    "  started_at TEXT DEFAULT CURRENT_TIMESTAMP,"
    "  ended_at TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS moves ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  game_id INTEGER REFERENCES games(id),"
    "  move_num INTEGER,"
    "  player INTEGER,"
    "  from_row INTEGER, from_col INTEGER,"
    "  to_row INTEGER, to_col INTEGER"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_moves_game ON moves(game_id);"
    "CREATE INDEX IF NOT EXISTS idx_games_players ON games(p1, p2);";

int history_init_tables(void) {
    if (!g_db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(g_db, HISTORY_SCHEMA, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        LOG_ERROR("[HISTORY] Schema error: %s", err);
        sqlite3_free(err);
        return -1;
    }
    LOG_INFO("[HISTORY] Tables initialized.");
    return 0;
}

int history_start_game(const char *p1, const char *p2) {
    if (!g_db) return -1;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO games (p1, p2) VALUES (?, ?);", -1, &stmt, NULL);
    if (rc != SQLITE_OK) { LOG_ERROR("[HISTORY] Start: %s", sqlite3_errmsg(g_db)); return -1; }

    sqlite3_bind_text(stmt, 1, p1, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, p2, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) { LOG_ERROR("[HISTORY] Insert game: %s", sqlite3_errmsg(g_db)); return -1; }

    int game_id = (int)sqlite3_last_insert_rowid(g_db);
    LOG_INFO("[HISTORY] Game %d started: %s vs %s", game_id, p1, p2);
    return game_id;
}

int history_record_move(int game_id, int move_num, int player,
                        int fromR, int fromC, int toR, int toC) {
    if (!g_db) return -1;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO moves (game_id, move_num, player, from_row, from_col, to_row, to_col) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, game_id);
    sqlite3_bind_int(stmt, 2, move_num);
    sqlite3_bind_int(stmt, 3, player);
    sqlite3_bind_int(stmt, 4, fromR);
    sqlite3_bind_int(stmt, 5, fromC);
    sqlite3_bind_int(stmt, 6, toR);
    sqlite3_bind_int(stmt, 7, toC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int history_end_game(int game_id, int status) {
    if (!g_db) return -1;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "UPDATE games SET result = ?, ended_at = CURRENT_TIMESTAMP WHERE id = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, status);
    sqlite3_bind_int(stmt, 2, game_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    LOG_INFO("[HISTORY] Game %d ended with status %d", game_id, status);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int history_get_recent(const char *username, GameSummary *out, int max) {
    if (!g_db || !username || !out) return 0;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT id, "
        "  CASE WHEN p1 = ? THEN p2 ELSE p1 END AS opponent, "
        "  CASE "
        "    WHEN (p1 = ? AND result = 1) OR (p2 = ? AND result = 2) THEN 1 "
        "    WHEN result = 3 THEN 2 "
        "    ELSE 0 "
        "  END AS win_loss, "
        "  COALESCE(ended_at, started_at) AS date "
        "FROM games WHERE p1 = ? OR p2 = ? "
        "ORDER BY id DESC LIMIT ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, username, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, max);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max) {
        memset(&out[count], 0, sizeof(GameSummary));
        out[count].game_id = sqlite3_column_int(stmt, 0);
        const char *opp = (const char *)sqlite3_column_text(stmt, 1);
        if (opp) strncpy(out[count].opponent, opp, LB_NAME_LEN - 1);
        out[count].result = sqlite3_column_int(stmt, 2);
        const char *dt = (const char *)sqlite3_column_text(stmt, 3);
        if (dt) strncpy(out[count].date, dt, 19);
        count++;
    }
    sqlite3_finalize(stmt);
    return count;
}

int history_get_moves(int game_id, HistoryMove *moves, int max_moves) {
    if (!g_db || !moves) return 0;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT move_num, player, from_row, from_col, to_row, to_col "
        "FROM moves WHERE game_id = ? ORDER BY move_num;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, game_id);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_moves) {
        moves[count].move_num = sqlite3_column_int(stmt, 0);
        moves[count].player   = sqlite3_column_int(stmt, 1);
        moves[count].fromR    = sqlite3_column_int(stmt, 2);
        moves[count].fromC    = sqlite3_column_int(stmt, 3);
        moves[count].toR      = sqlite3_column_int(stmt, 4);
        moves[count].toC      = sqlite3_column_int(stmt, 5);
        count++;
    }
    sqlite3_finalize(stmt);
    return count;
}
